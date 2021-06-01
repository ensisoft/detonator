// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "config.h"

#include "warnpush.h"
#  define SOL_ALL_SAFETIES_ON 1
#  include <sol/sol.hpp>
#  include <glm/glm.hpp>
#  include <glm/gtx/matrix_decompose.hpp>
#  include <neargye/magic_enum.hpp>
#include "warnpop.h"

#include <unordered_set>

#include "base/assert.h"
#include "base/logging.h"
#include "base/types.h"
#include "base/color4f.h"
#include "base/format.h"
#include "engine/entity.h"
#include "engine/game.h"
#include "engine/scene.h"
#include "engine/classlib.h"
#include "engine/physics.h"
#include "engine/lua.h"
#include "engine/transform.h"
#include "engine/util.h"
#include "wdk/keys.h"
#include "wdk/system.h"

namespace {
template<typename... Args>
void CallLua(const sol::protected_function& func, const Args&... args)
{
    if (!func.valid())
        return;
    const auto& result = func(args...);
    if (result.valid())
        return;
    const sol::error err = result;
    ERROR(err.what());
}
} // namespace

namespace game
{

LuaGame::LuaGame(std::shared_ptr<sol::state> state)
  : mLuaState(state)
{ }

LuaGame::LuaGame(const std::string& lua_path)
{
    mLuaState = std::make_shared<sol::state>();
    // todo: should this specify which libraries to load?
    mLuaState->open_libraries();
    // ? is a wildcard (usually denoted by kleene star *)
    // todo: setup a package loader instead of messing with the path?
    // https://github.com/ThePhD/sol2/issues/90
    std::string path = (*mLuaState)["package"]["path"];
    path = path + ";" + lua_path + "/?.lua";
    path = path + ";" + lua_path + "/?/?.lua";
    (*mLuaState)["package"]["path"] = path;
    BindBase(*mLuaState);
    BindUtil(*mLuaState);
    BindGLM(*mLuaState);
    BindGFX(*mLuaState);
    BindWDK(*mLuaState);
    BindGameLib(*mLuaState);

    // bind engine interface.
    auto table  = (*mLuaState)["game"].get_or_create<sol::table>();
    auto engine = table.new_usertype<LuaGame>("Engine");
    engine["PlayScene"] = [](LuaGame& self, ClassHandle<SceneClass> klass) {
        if (!klass)
            throw std::runtime_error("nil scene class");
        PlaySceneAction play;
        play.klass = klass;
        self.mActionQueue.push(play);
    };
    engine["SetViewport"] = [](LuaGame& self, const FRect& view) {
        self.mView = view;
    };
    engine["LoadBackground"] = [](LuaGame* self, ClassHandle<SceneClass> klass) {
        if (!klass)
            throw std::runtime_error("nil scene class");
        LoadBackgroundAction action;
        action.klass = klass;
        self->mActionQueue.push(action);
    };
    engine["DebugPrint"] = [](LuaGame* self, std::string message) {
        PrintDebugStrAction action;
        action.message = std::move(message);
        self->mActionQueue.push(std::move(action));
    };

    // todo: maybe this needs some configuring or whatever?
    mLuaState->script_file(lua_path + "/game.lua");
}

void LuaGame::SetPhysicsEngine(const PhysicsEngine* engine)
{
    mPhysicsEngine = engine;
}

void LuaGame::LoadGame(const ClassLibrary* loader)
{
    mClasslib = loader;
    (*mLuaState)["Physics"]  = mPhysicsEngine;
    (*mLuaState)["ClassLib"] = mClasslib;
    (*mLuaState)["Game"]     = this;
    CallLua((*mLuaState)["LoadGame"]);
}
void LuaGame::LoadBackgroundDone(Scene* background)
{
    CallLua((*mLuaState)["LoadBackgroundDone"], background);
}

void LuaGame::Tick(double wall_time, double tick_time, double dt)
{
    CallLua((*mLuaState)["Tick"], wall_time, tick_time, dt);
}
void LuaGame::Update(double wall_time, double game_time, double dt)
{
    CallLua((*mLuaState)["Update"], wall_time, game_time, dt);
}
void LuaGame::BeginPlay(Scene* scene)
{
    mScene = scene;
    (*mLuaState)["Scene"] = mScene;

    CallLua((*mLuaState)["BeginPlay"], scene);
}
void LuaGame::EndPlay()
{
    mScene = nullptr;
    CallLua((*mLuaState)["EndPlay"]);
}

void LuaGame::SaveGame()
{

}

bool LuaGame::GetNextAction(Action* out)
{
    if (mActionQueue.empty())
        return false;
    *out = mActionQueue.front();
    mActionQueue.pop();
    return true;
}

FRect LuaGame::GetViewport() const
{ return mView; }

void LuaGame::OnContactEvent(const ContactEvent& contact)
{
    Entity* entityA = mScene->FindEntityByInstanceId(contact.entityA);
    Entity* entityB = mScene->FindEntityByInstanceId(contact.entityB);
    if (entityA == nullptr || entityB == nullptr)  {
        WARN("Contact event ignored, entity was not found.");
        return;
    }
    EntityNode* nodeA = entityA->FindNodeByInstanceId(contact.nodeA);
    EntityNode* nodeB = entityB->FindNodeByInstanceId(contact.nodeB);
    if (nodeA == nullptr || nodeB == nullptr) {
        WARN("Contact event ignored, entity node was not found.");
        return;
    }

    if (contact.type == ContactEvent::Type::BeginContact)
    {
        CallLua((*mLuaState)["OnBeginContact"], entityA, entityB, nodeA, nodeB);
    }
    else if (contact.type == ContactEvent::Type::EndContact)
    {
        CallLua((*mLuaState)["OnEndContact"], entityA, entityB, nodeA, nodeB);
    }
}

void LuaGame::OnKeyDown(const wdk::WindowEventKeydown& key)
{
    CallLua((*mLuaState)["OnKeyDown"],
            static_cast<int>(key.symbol),
            static_cast<int>(key.modifiers.value()));
}
void LuaGame::OnKeyUp(const wdk::WindowEventKeyup& key)
{
    CallLua((*mLuaState)["OnKeyUp"],
            static_cast<int>(key.symbol),
            static_cast<int>(key.modifiers.value()));
}
void LuaGame::OnChar(const wdk::WindowEventChar& text)
{

}
void LuaGame::OnMouseMove(const wdk::WindowEventMouseMove& mouse)
{

}
void LuaGame::OnMousePress(const wdk::WindowEventMousePress& mouse)
{
}
void LuaGame::OnMouseRelease(const wdk::WindowEventMouseRelease& mouse)
{
}

ScriptEngine::ScriptEngine(const std::string& lua_path) : mLuaPath(lua_path)
{}

void ScriptEngine::BeginPlay(Scene* scene)
{
    // When the game play begins we create fresh new lua state
    // and environments for all scriptable entity classes.

    auto state = std::make_unique<sol::state>();
    state->open_libraries();
    // ? is a wildcard (usually denoted by kleene star *)
    // todo: setup a package loader instead of messing with the path?
    // https://github.com/ThePhD/sol2/issues/90
    std::string path = (*state)["package"]["path"];
    path = path + ";" + mLuaPath + "/?.lua";
    path = path + ";" + mLuaPath + "/?/?.lua";
    (*state)["package"]["path"] = path;

    BindBase(*state);
    BindUtil(*state);
    BindGLM(*state);
    BindGFX(*state);
    BindWDK(*state);
    BindGameLib(*state);

    // table that maps entity types to their scripting
    // environments. then we later invoke the script per
    // each instance's type on each instance of that type.
    // In other words if there's an EntityClass 'foobar'
    // it has a "foobar.lua" script and there are 2 entities
    // a and b, the same script foobar.lua will be invoked
    // for a total of two times (per script function), once
    // per each instance.
    std::unordered_map<std::string, std::unique_ptr<sol::environment>> envs;
    std::unordered_set<std::string> ids;

    for (size_t i=0; i<scene->GetNumEntities(); ++i)
    {
        const auto& entity = scene->GetEntity(i);
        const auto& klass  = entity.GetClass();
        if (ids.find(klass.GetId()) != ids.end())
            continue;
        ids.insert(klass.GetId());
        if (!klass.HasScriptFile())
            continue;
        else if (envs.find(klass.GetId()) != envs.end())
            continue;
        const auto& script = klass.GetScriptFileId();
        const auto& file = base::JoinPath(mLuaPath, script + ".lua");
        if (!base::FileExists(file)) {
            ERROR("Entity '%1' Lua file '%2' was not found.", klass.GetName(), file);
            continue;
        }
        auto env = std::make_unique<sol::environment>(*state, sol::create, state->globals());
        state->script_file(file, *env);
        envs[klass.GetId()] = std::move(env);
        DEBUG("Entity class '%1' script loaded.", klass.GetName());
    }
    // careful here, make sure to clean up the environment objects
    // first since they depend on lua state. changing the order
    // of these two lines will crash.
    mTypeEnvs = std::move(envs);
    mLuaState = std::move(state);

    mScene    = scene;
    (*mLuaState)["Physics"]  = mPhysicsEngine;
    (*mLuaState)["ClassLib"] = mClassLib;
    (*mLuaState)["Scene"]    = mScene;
    (*mLuaState)["Game"]     = this;
    auto table = (*mLuaState)["game"].get_or_create<sol::table>();
    auto engine = table.new_usertype<ScriptEngine>("Engine");
    engine["DebugPrint"] = [](ScriptEngine* self, std::string message) {
        PrintDebugStrAction action;
        action.message = std::move(message);
        self->mActionQueue.push(std::move(action));
    };

    for (size_t i=0; i<scene->GetNumEntities(); ++i)
    {
        auto* entity = &mScene->GetEntity(i);
        const auto& klass   = entity->GetClass();
        const auto& klassId = klass.GetId();
        auto it = mTypeEnvs.find(klassId);
        if (it == mTypeEnvs.end())
            continue;
        auto& env = it->second;
        CallLua((*env)["BeginPlay"], entity, scene);
    }
}

void ScriptEngine::EndPlay()
{

}

void ScriptEngine::Tick(double wall_time, double tick_time, double dt)
{
    for (size_t i=0; i<mScene->GetNumEntities(); ++i)
    {
        auto* entity = &mScene->GetEntity(i);
        if (auto* env = GetTypeEnv(entity->GetClass()))
        {
            CallLua((*env)["Tick"], entity, wall_time, tick_time, dt);
        }
    }
}
void ScriptEngine::Update(double wall_time, double game_time, double dt)
{
    for (size_t i=0; i<mScene->GetNumEntities(); ++i)
    {
        auto* entity = &mScene->GetEntity(i);
        if (auto* env = GetTypeEnv(entity->GetClass()))
        {
            CallLua((*env)["Update"], entity, wall_time, game_time, dt);
        }
    }
}

bool ScriptEngine::GetNextAction(Action* out)
{
    if (mActionQueue.empty())
        return false;
    *out = mActionQueue.front();
    mActionQueue.pop();
    return true;
}

void ScriptEngine::OnContactEvent(const ContactEvent& contact)
{
    Entity* entityA = mScene->FindEntityByInstanceId(contact.entityA);
    Entity* entityB = mScene->FindEntityByInstanceId(contact.entityB);
    if (entityA == nullptr || entityB == nullptr)  {
        WARN("Contact event ignored, entity was not found.");
        return;
    }
    EntityNode* nodeA = entityA->FindNodeByInstanceId(contact.nodeA);
    EntityNode* nodeB = entityB->FindNodeByInstanceId(contact.nodeB);
    if (nodeA == nullptr || nodeB == nullptr) {
        WARN("Contact event ignored, entity node was not found.");
        return;
    }
    const auto* function = contact.type == ContactEvent::Type::BeginContact
            ? "OnBeginContact" : "OnEndContact";

    // there's a little problem here that needs to be fixed regarding the
    // lifetimes of objects. calling into the script may choose to for
    // example delete the object from the scene which would invalidate
    // the pointers above. This needs to be fixed somehow.
    const auto& klassA = entityA->GetClass();
    const auto& klassB = entityB->GetClass();

    if (auto* env = GetTypeEnv(klassA))
    {
        CallLua((*env)[function], entityA, nodeA, entityB, nodeB);
    }
    if (auto* env = GetTypeEnv(klassB))
    {
        CallLua((*env)[function], entityB, nodeB, entityA, nodeA);
    }
}
void ScriptEngine::OnKeyDown(const wdk::WindowEventKeydown& key)
{
    for (size_t i=0; i<mScene->GetNumEntities(); ++i)
    {
        auto* entity = &mScene->GetEntity(i);
        if (auto* env = GetTypeEnv(entity->GetClass()))
        {
            CallLua((*env)["OnKeyDown"], entity,
                    static_cast<int>(key.symbol) ,
                    static_cast<int>(key.modifiers.value()));
        }
    }
}
void ScriptEngine::OnKeyUp(const wdk::WindowEventKeyup& key)
{
    for (size_t i=0; i<mScene->GetNumEntities(); ++i)
    {
        auto* entity = &mScene->GetEntity(i);
        if (auto* env = GetTypeEnv(entity->GetClass()))
        {
            CallLua((*env)["OnKeyUp"], entity,
                    static_cast<int>(key.symbol) ,
                    static_cast<int>(key.modifiers.value()));
        }
    }
}
void ScriptEngine::OnChar(const wdk::WindowEventChar& text)
{

}
void ScriptEngine::OnMouseMove(const wdk::WindowEventMouseMove& mouse)
{

}
void ScriptEngine::OnMousePress(const wdk::WindowEventMousePress& mouse)
{

}
void ScriptEngine::OnMouseRelease(const wdk::WindowEventMouseRelease& mouse)
{

}

sol::environment* ScriptEngine::GetTypeEnv(const EntityClass& klass)
{
    if (!klass.HasScriptFile())
        return nullptr;
    const auto& klassId = klass.GetId();
    auto it = mTypeEnvs.find(klassId);
    if (it != mTypeEnvs.end())
        return it->second.get();

    const auto& script = klass.GetScriptFileId();
    const auto& file   = base::JoinPath(mLuaPath, script + ".lua");
    if (!base::FileExists(file))
        return nullptr;
    auto env = std::make_unique<sol::environment>(*mLuaState, sol::create, mLuaState->globals());
    mLuaState->script_file(file, *env);
    it = mTypeEnvs.insert({klassId, std::move(env)}).first;
    return it->second.get();
}

void BindUtil(sol::state& L)
{
    auto util = L.create_named_table("util");
    {
        util["GetRotationFromMatrix"]    = GetRotationFromMatrix;
        util["GetScaleFromMatrix"]       = GetScaleFromMatrix;
        util["GetTranslationFromMatrix"] = GetTranslationFromMatrix;
        util["RotateVector"]             = RotateVector;
    }

    {
        sol::constructors<FBox(), FBox(float, float), FBox(const glm::mat4& mat, float, float), FBox(const glm::mat4& mat)> ctors;
        auto box = util.new_usertype<FBox>("FBox", ctors);
        box["GetWidth"]    = &FBox::GetWidth;
        box["GetHeight"]   = &FBox::GetHeight;
        box["GetTopLeft"]  = &FBox::GetTopLeft;
        box["GetTopRight"] = &FBox::GetTopRight;
        box["GetBotRight"] = &FBox::GetTopRight;
        box["GetBotLeft"]  = &FBox::GetBotLeft;
        box["GetCenter"]   = &FBox::GetCenter;
        box["GetSize"]     = &FBox::GetSize;
        box["GetRotation"] = &FBox::GetRotation;
        box["Transform"]   = &FBox::Transform;
        box["Reset"]       = &FBox::Reset;
    }

}

void BindBase(sol::state& L)
{
    auto table = L.create_named_table("base");
    table["debug"] = [](const std::string& str) { DEBUG(str); };
    table["warn"]  = [](const std::string& str) { WARN(str); };
    table["error"] = [](const std::string& str) { ERROR(str); };
    table["info"]  = [](const std::string& str) { INFO(str); };

    // FRect
    {
        sol::constructors<base::FRect(), base::FRect(float, float, float, float)> ctors;
        auto rect = table.new_usertype<base::FRect>("FRect", ctors);
        rect["GetHeight"] = &base::FRect::GetHeight;
        rect["GetWidth"]  = &base::FRect::GetWidth;
        rect["GetX"]      = &base::FRect::GetX;
        rect["GetY"]      = &base::FRect::GetY;
        rect["SetX"]      = &base::FRect::SetX;
        rect["SetY"]      = &base::FRect::SetY;
        rect["SetWidth"]  = &base::FRect::SetWidth;
        rect["SetHeight"] = &base::FRect::SetHeight;
        rect["Resize"]    = (void(base::FRect::*)(float, float))&base::FRect::Resize;
        rect["Grow"]      = (void(base::FRect::*)(float, float))&base::FRect::Grow;
        rect["Move"]      = (void(base::FRect::*)(float, float))&base::FRect::Move;
        rect["Translate"] = (void(base::FRect::*)(float, float))&base::FRect::Translate;
        rect["IsEmpty"]   = &base::FRect::IsEmpty;
        // global functions for FRect
        table["CombineRects"]     = base::Union<float>;
        table["IntersectRects"]   = base::Intersect<float>;
        table["DoRectsIntersect"] = base::DoesIntersect<float>;
    }

    // FSize
    {
        sol::constructors<base::FSize(), base::FSize(float, float)> ctors;
        auto size = table.new_usertype<base::FSize>("FSize", ctors);
        size["GetWidth"]  = &base::FSize::GetWidth;
        size["GetHeight"] = &base::FSize::GetHeight;
        size.set_function(sol::meta_function::multiplication, [](const base::FSize& size, float scalar) { return size * scalar; });
        size.set_function(sol::meta_function::addition, [](const base::FSize& lhs, const base::FSize& rhs) { return lhs + rhs; });
        size.set_function(sol::meta_function::subtraction, [](const base::FSize& lhs, const base::FSize& rhs) { return lhs - rhs; });
    }

    // FPoint
    {
        sol::constructors<base::FPoint(), base::FPoint(float, float)> ctors;
        auto point = table.new_usertype<base::FPoint>("FPoint", ctors);
        point["GetX"] = &base::FPoint::GetX;
        point["GetY"] = &base::FPoint::GetY;
        point.set_function(sol::meta_function::addition, [](const base::FPoint& lhs, const base::FPoint& rhs) { return lhs + rhs; });
        point.set_function(sol::meta_function::subtraction, [](const base::FPoint& lhs, const base::FPoint& rhs) { return lhs - rhs; });
    }

    // build table for color names
    {
        for (const auto& color : magic_enum::enum_values<base::Color>())
        {
            const std::string name(magic_enum::enum_name(color));
            table[sol::create_if_nil]["Colors"][name] = magic_enum::enum_integer(color);
        }
    }

    // Color4f
    {
        // todo: figure out a way to construct from color name, is that possible?
        sol::constructors<base::Color4f(),
                base::Color4f(float, float, float, float),
                base::Color4f(int, int, int, int)> ctors;
        auto color = table.new_usertype<base::Color4f>("Color4f", ctors);
        color["GetRed"]     = &base::Color4f::Red;
        color["GetGreen"]   = &base::Color4f::Green;
        color["GetBlue"]    = &base::Color4f::Blue;
        color["GetAlpha"]   = &base::Color4f::Alpha;
        color["SetRed"]     = (void(base::Color4f::*)(float))&base::Color4f::SetRed;
        color["SetGreen"]   = (void(base::Color4f::*)(float))&base::Color4f::SetGreen;
        color["SetBlue"]    = (void(base::Color4f::*)(float))&base::Color4f::SetBlue;
        color["SetAlpha"]   = (void(base::Color4f::*)(float))&base::Color4f::SetAlpha;
        color["SetColor"]   = [](base::Color4f& color, int value) {
            const auto color_value = magic_enum::enum_cast<base::Color>(value);
            if (!color_value.has_value())
                return;
            color = base::Color4f(color_value.value());
        };
    }
}

void BindGLM(sol::state& L)
{
    sol::constructors<glm::vec2(), glm::vec2(float, float)> ctors;
    auto glm  = L.create_named_table("glm");
    auto vec2 = glm.new_usertype<glm::vec2>("vec2", ctors,
        sol::meta_function::index, [](const glm::vec2& vec, int index) {
        if (index >= 2)
            throw std::runtime_error("glm.vec2 access out of bounds");
        return vec[index];
            });
    vec2["x"]   = &glm::vec2::x;
    vec2["y"]   = &glm::vec2::y;
    vec2["length"]    = [](const glm::vec2& vec) { return glm::length(vec); };
    vec2["normalize"] = [](const glm::vec2& vec) { return glm::normalize(vec); };
    // adding meta functions for operators
    // setting the index through set_function seems to be broken! (sol 3.2.3)
    //type.set_function(sol::meta_function::index, [](const glm::vec2& vec, int index) {
    //    return vec[index];
    //});
    vec2.set_function(sol::meta_function::addition, [](const glm::vec2& a, const glm::vec2& b) {
        return a + b;
    });
    vec2.set_function(sol::meta_function::subtraction, [](const glm::vec2& a, const glm::vec2& b) {
        return a - b;
    });
    vec2.set_function(sol::meta_function::multiplication, [](const glm::vec2& vector, float scalar) {
        return vector * scalar;
    });
    vec2.set_function(sol::meta_function::multiplication, [](float scalar, const glm::vec2& vector) {
        return vector * scalar;
    });
    vec2.set_function(sol::meta_function::division, [](const glm::vec2& vector, float scalar) {
        return vector / scalar;
    });
    vec2.set_function(sol::meta_function::to_string, [](const glm::vec2& vector) {
        return std::to_string(vector.x) + "," + std::to_string(vector.y);
    });
    glm["dot"]       = [](const glm::vec2 &a, const glm::vec2 &b) { return glm::dot(a, b); };
    glm["length"]    = [](const glm::vec2& vec) { return glm::length(vec); };
    glm["normalize"] = [](const glm::vec2& vec) { return glm::normalize(vec); };

    auto mat4 = glm.new_usertype<glm::mat4>("mat4");
    mat4["decompose"] = [](const glm::mat4& mat) {
        glm::vec3 scale;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::quat orientation;
        glm::decompose(mat, scale, orientation, translation, skew, perspective);
        return std::make_tuple(glm::vec2(translation.x, translation.y),
                               glm::vec2(scale.x, scale.y),
                               glm::angle(orientation));
    };
}

void BindGFX(sol::state& L)
{

}

void BindWDK(sol::state& L)
{
    auto table = L["wdk"].get_or_create<sol::table>();
    table["KeyStr"] = [](int value) {
        const auto key = magic_enum::enum_cast<wdk::Keysym>(value);
        if (key.has_value())
            return std::string(magic_enum::enum_name(key.value()));
        return std::string("");
    };
    table["BtnStr"] = [](int value) {
        const auto key = magic_enum::enum_cast<wdk::MouseButton>(value);
        if (key.has_value())
            return std::string(magic_enum::enum_name(key.value()));
        return std::string("");
    };
    table["ModStr"] = [](int value) {
        std::string ret;
        wdk::bitflag<wdk::Keymod> mods;
        mods.set_from_value(value);
        if (mods.test(wdk::Keymod::Control))
            ret += "Ctrl+";
        if (mods.test(wdk::Keymod::Shift))
            ret += "Shift+";
        if (mods.test(wdk::Keymod::Alt))
            ret += "Alt+";
        if (!ret.empty())
            ret.pop_back();
        return ret;
    };
    table["TestKeyDown"] = [](int value) {
        const auto key = magic_enum::enum_cast<wdk::Keysym>(value);
        if (!key.has_value())
            return false;
        return wdk::TestKeyDown(key.value());
    };

    // build table for key names.
    for (const auto& key : magic_enum::enum_values<wdk::Keysym>())
    {
        const std::string key_name(magic_enum::enum_name(key));
        table[sol::create_if_nil]["Keys"][key_name] = magic_enum::enum_integer(key);
    }

    // build table for modifiers
    for (const auto& mod : magic_enum::enum_values<wdk::Keymod>())
    {
        const std::string mod_name(magic_enum::enum_name(mod));
        table[sol::create_if_nil]["Mods"][mod_name] = magic_enum::enum_integer(mod);
    }

    // build table for mouse buttons.
    for (const auto& btn : magic_enum::enum_values<wdk::MouseButton>())
    {
        const std::string btn_name(magic_enum::enum_name(btn));
        table[sol::create_if_nil]["Buttons"][btn_name] = magic_enum::enum_integer(btn);
    }
}

void BindGameLib(sol::state& L)
{
    auto table = L["game"].get_or_create<sol::table>();

    auto classlib = table.new_usertype<ClassLibrary>("ClassLibrary");
    classlib["FindEntityClassByName"] = &ClassLibrary::FindEntityClassByName;
    classlib["FindEntityClassById"]   = &ClassLibrary::FindEntityClassById;
    classlib["FindSceneClassByName"]  = &ClassLibrary::FindSceneClassByName;
    classlib["FindSceneClassById"]    = &ClassLibrary::FindSceneClassById;

    auto drawable = table.new_usertype<DrawableItem>("Drawable");
    drawable["GetMaterialId"] = &DrawableItem::GetMaterialId;
    drawable["GetDrawableId"] = &DrawableItem::GetDrawableId;
    drawable["GetLayer"]      = &DrawableItem::GetLayer;
    drawable["GetLineWidth"]  = &DrawableItem::GetLineWidth;
    drawable["GetTimeScale"]  = &DrawableItem::GetTimeScale;
    drawable["GetAlpha"]      = &DrawableItem::GetAlpha;
    drawable["SetAlpha"]      = &DrawableItem::SetAlpha;
    drawable["SetTimeScale"]  = &DrawableItem::SetTimeScale;
    drawable["TestFlag"]      = [](const DrawableItem* item, const std::string& str) {
        const auto enum_val = magic_enum::enum_cast<DrawableItem::Flags>(str);
        if (!enum_val.has_value())
            throw std::runtime_error("no such drawable item flag:" + str);
        return item->TestFlag(enum_val.value());
    };
    drawable["SetFlag"]       = [](DrawableItem* item, const std::string& str, bool on_off) {
        const auto enum_val = magic_enum::enum_cast<DrawableItem::Flags>(str);
        if (!enum_val.has_value())
            throw std::runtime_error("no such drawable item flag: " + str);
        item->SetFlag(enum_val.value(), on_off);
    };

    auto body = table.new_usertype<RigidBodyItem>("RigidBody");
    body["GetFriction"]        = &RigidBodyItem::GetFriction;
    body["GetRestitution"]     = &RigidBodyItem::GetRestitution;
    body["GetAngularDamping"]  = &RigidBodyItem::GetAngularDamping;
    body["GetLinearDamping"]   = &RigidBodyItem::GetLinearDamping;
    body["GetDensity"]         = &RigidBodyItem::GetDensity;
    body["GetPolygonShapeId"]  = &RigidBodyItem::GetPolygonShapeId;
    body["GetLinearVelocity"]  = &RigidBodyItem::GetLinearVelocity;
    body["GetAngularVelocity"] = &RigidBodyItem::GetAngularVelocity;
    body["SetLinearVelocity"]  = &RigidBodyItem::SetLinearVelocity;
    body["SetAngularVelocity"] = &RigidBodyItem::SetAngularVelocity;
    body["TestFlag"] = [](const RigidBodyItem* item, const std::string& str) {
        const auto enum_val = magic_enum::enum_cast<RigidBodyItem::Flags>(str);
        if (!enum_val.has_value())
            throw std::runtime_error("no such rigid body flag:" + str);
        return item->TestFlag(enum_val.value());
    };
    body["SetFlag"] = [](RigidBodyItem* item, const std::string& str, bool on_off) {
        const auto enum_val = magic_enum::enum_cast<RigidBodyItem::Flags>(str);
        if (!enum_val.has_value())
            throw std::runtime_error("no such rigid body flag: " + str);
        item->SetFlag(enum_val.value(), on_off);
    };
    body["GetSimulationType"] = [](const RigidBodyItem* item) {
        return magic_enum::enum_name(item->GetSimulation());
    };
    body["GetCollisionShapeType"] = [](const RigidBodyItem* item) {
        return magic_enum::enum_name(item->GetCollisionShape());
    };

    auto entity_node = table.new_usertype<EntityNode>("EntityNode");
    entity_node["GetId"]          = &EntityNode::GetId;
    entity_node["GetName"]        = &EntityNode::GetName;
    entity_node["GetClassId"]     = &EntityNode::GetClassId;
    entity_node["GetClassName"]   = &EntityNode::GetClassName;
    entity_node["GetTranslation"] = &EntityNode::GetTranslation;
    entity_node["GetScale"]       = &EntityNode::GetScale;
    entity_node["GetRotation"]    = &EntityNode::GetRotation;
    entity_node["HasRigidBody"]   = &EntityNode::HasRigidBody;
    entity_node["HasDrawable"]    = &EntityNode::HasDrawable;
    entity_node["GetDrawable"]    = (DrawableItem*(EntityNode::*)(void))&EntityNode::GetDrawable;
    entity_node["GetRigidBody"]   = (RigidBodyItem*(EntityNode::*)(void))&EntityNode::GetRigidBody;
    entity_node["SetScale"]       = &EntityNode::SetScale;
    entity_node["SetSize"]        = &EntityNode::SetSize;
    entity_node["SetTranslation"] = &EntityNode::SetTranslation;
    entity_node["SetName"]        = &EntityNode::SetName;
    entity_node["Translate"]      = (void(EntityNode::*)(const glm::vec2&))&EntityNode::Translate;
    entity_node["Rotate"]         = (void(EntityNode::*)(float))&EntityNode::Rotate;

    auto entity = table.new_usertype<Entity>("Entity",
        sol::meta_function::index, [&L](const Entity* entity, const char* key) {
                sol::state_view lua(L);
                const ScriptVar* var = entity->FindScriptVar(key);
                if (var && var->GetType() == ScriptVar::Type::Boolean)
                    return sol::make_object(lua, var->GetValue<bool>());
                else if (var && var->GetType() == ScriptVar::Type::Float)
                    return sol::make_object(lua, var->GetValue<float>());
                else if (var && var->GetType() == ScriptVar::Type::String)
                    return sol::make_object(lua, var->GetValue<std::string>());
                else if (var && var->GetType() == ScriptVar::Type::Integer)
                    return sol::make_object(lua, var->GetValue<int>());
                else if (var && var->GetType() == ScriptVar::Type::Vec2)
                    return sol::make_object(lua, var->GetValue<glm::vec2>());
                else if (var) BUG("Unhandled ScriptVar type.");
                WARN("No such script variable: '%1'", key);
                return sol::make_object(lua, sol::lua_nil);
            },
            sol::meta_function::new_index, [](const Entity* entity, const char* key, sol::object value) {
                const ScriptVar* var = entity->FindScriptVar(key);
                if (var == nullptr) {
                    WARN("No such script variable: '%1'", key);
                    return;
                } else if (var->IsReadOnly()) {
                    WARN("Trying to write to a read only variable: '%1'", key);
                    return;
                }
                if (value.is<int>() && var->HasType<int>())
                    var->SetValue(value.as<int>());
                else if (value.is<float>() && var->HasType<float>())
                    var->SetValue(value.as<float>());
                else if (value.is<bool>() && var->HasType<bool>())
                    var->SetValue(value.as<bool>());
                else if (value.is<std::string>() && var->HasType<std::string>())
                    var->SetValue(value.as<std::string>());
                else if (value.is<glm::vec2>() && var->HasType<glm::vec2>())
                    var->SetValue(value.as<glm::vec2>());
                else WARN("Script value type mismatch. '%1' expects: '%2'", key, var->GetType());
            });
    entity["GetName"]              = &Entity::GetName;
    entity["GetId"]                = &Entity::GetId;
    entity["GetClassName"]         = &Entity::GetClassName;
    entity["GetClassId"]           = &Entity::GetClassId;
    entity["GetNumNodes"]          = &Entity::GetNumNodes;
    entity["GetTime"]              = &Entity::GetTime;
    entity["GetLayer"]             = &Entity::GetLayer;
    entity["SetLayer"]             = &Entity::SetLayer;
    entity["IsPlaying"]            = &Entity::IsPlaying;
    entity["HasExpired"]           = &Entity::HasExpired;
    entity["GetNode"]              = (EntityNode&(Entity::*)(size_t))&Entity::GetNode;
    entity["FindNodeByClassName"]  = (EntityNode*(Entity::*)(const std::string&))&Entity::FindNodeByClassName;
    entity["FindNodeByClassId"]    = (EntityNode*(Entity::*)(const std::string&))&Entity::FindNodeByClassId;
    entity["FindNodeByInstanceId"] = (EntityNode*(Entity::*)(const std::string&))&Entity::FindNodeByInstanceId;
    entity["PlayIdle"]             = &Entity::PlayIdle;
    entity["PlayAnimationByName"]  = &Entity::PlayAnimationByName;
    entity["PlayAnimationById"]    = &Entity::PlayAnimationById;
    entity["TestFlag"]             = [](const Entity* entity, const std::string& str) {
        const auto enum_val = magic_enum::enum_cast<Entity::Flags>(str);
        if (!enum_val.has_value())
            throw std::runtime_error("no such drawable item flag:" + str);
        return entity->TestFlag(enum_val.value());
    };

    auto entity_args = table.new_usertype<EntityArgs>("EntityArgs", sol::constructors<EntityArgs()>());
    entity_args["class"]    = sol::property(&EntityArgs::klass);
    entity_args["name"]     = sol::property(&EntityArgs::name);
    entity_args["scale"]    = sol::property(&EntityArgs::scale);
    entity_args["position"] = sol::property(&EntityArgs::position);
    entity_args["rotation"] = sol::property(&EntityArgs::rotation);

    // todo: cleanup the copy pasta regarding script vars index/newindex
    auto scene = table.new_usertype<Scene>("Scene",
       sol::meta_function::index, [&L](const Scene* scene, const char* key) {
                sol::state_view lua(L);
                const ScriptVar* var = scene->FindScriptVar(key);
                if (var && var->GetType() == ScriptVar::Type::Boolean)
                    return sol::make_object(lua, var->GetValue<bool>());
                else if (var && var->GetType() == ScriptVar::Type::Float)
                    return sol::make_object(lua, var->GetValue<float>());
                else if (var && var->GetType() == ScriptVar::Type::String)
                    return sol::make_object(lua, var->GetValue<std::string>());
                else if (var && var->GetType() == ScriptVar::Type::Integer)
                    return sol::make_object(lua, var->GetValue<int>());
                else if (var && var->GetType() == ScriptVar::Type::Vec2)
                    return sol::make_object(lua, var->GetValue<glm::vec2>());
                else if (var) BUG("Unhandled ScriptVar type.");
                WARN("No such script variable: '%1'", key);
                return sol::make_object(lua, sol::lua_nil);
            },
       sol::meta_function::new_index, [](const Scene* scene, const char* key, sol::object value) {
                const ScriptVar* var = scene->FindScriptVar(key);
                if (var == nullptr) {
                    WARN("No such script variable: '%1'", key);
                    return;
                } else if (var->IsReadOnly()) {
                    WARN("Trying to write to a read only variable: '%1'", key);
                    return;
                }
                if (value.is<int>() && var->HasType<int>())
                    var->SetValue(value.as<int>());
                else if (value.is<float>() && var->HasType<float>())
                    var->SetValue(value.as<float>());
                else if (value.is<bool>() && var->HasType<bool>())
                    var->SetValue(value.as<bool>());
                else if (value.is<std::string>() && var->HasType<std::string>())
                    var->SetValue(value.as<std::string>());
                else if (value.is<glm::vec2>() && var->HasType<glm::vec2>())
                    var->SetValue(value.as<glm::vec2>());
                else WARN("Script value type mismatch. '%1' expects: '%2'", key, var->GetType());
            });
    scene["GetNumEntities"]           = &Scene::GetNumEntities;
    scene["FindEntityByInstanceId"]   = (Entity*(Scene::*)(const std::string&))&Scene::FindEntityByInstanceId;
    scene["FindEntityByInstanceName"] = (Entity*(Scene::*)(const std::string&))&Scene::FindEntityByInstanceName;
    scene["GetEntity"]                = (Entity&(Scene::*)(size_t))&Scene::GetEntity;
    scene["KillEntity"]               = &Scene::KillEntity;
    scene["SpawnEntity"]              = &Scene::SpawnEntity;
    scene["FindEntityTransform"]      = &Scene::FindEntityTransform;
    scene["FindEntityNodeTransform"]  = &Scene::FindEntityNodeTransform;
    scene["GetTime"]                  = &Scene::GetTime;

    auto physics = table.new_usertype<PhysicsEngine>("Physics");
    physics["ApplyImpulseToCenter"] = (void(PhysicsEngine::*)(const std::string&, const glm::vec2&) const)&PhysicsEngine::ApplyImpulseToCenter;
    physics["ApplyImpulseToCenter"] = (void(PhysicsEngine::*)(const EntityNode&, const glm::vec2&) const)&PhysicsEngine::ApplyImpulseToCenter;

}


} // namespace
