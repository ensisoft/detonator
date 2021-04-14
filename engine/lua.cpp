// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

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
    sol::protected_function func = (*mLuaState)["LoadGame"];
    if (!func.valid())
        return;
    auto result = func();
    if (!result.valid())
    {
        const sol::error err = result;
        ERROR(err.what());
    }
}
void LuaGame::LoadBackgroundDone(Scene* background)
{
    sol::protected_function func = (*mLuaState)["LoadBackgroundDone"];
    if (!func.valid())
        return;
    auto result = func(background);
    if (!result.valid())
    {
        const sol::error err = result;
        ERROR(err.what());
    }
}

void LuaGame::Tick(double wall_time, double tick_time, double dt)
{
    sol::protected_function func = (*mLuaState)["Tick"];
    if (!func.valid())
        return;
    auto result = func(wall_time, tick_time, dt);
    if (!result.valid())
    {
        const sol::error err = result;
        ERROR(err.what());
    }
}
void LuaGame::Update(double wall_time, double game_time, double dt)
{
    sol::protected_function func = (*mLuaState)["Update"];
    if (!func.valid())
        return;
    auto result = func(wall_time, game_time, dt);
    if (!result.valid())
    {
        const sol::error err = result;
        ERROR(err.what());
    }
}
void LuaGame::BeginPlay(Scene* scene)
{
    mScene = scene;
    (*mLuaState)["Scene"] = mScene;

    sol::protected_function func = (*mLuaState)["BeginPlay"];
    if (!func.valid())
        return;
    auto result = func(scene);
    if (!result.valid())
    {
        const sol::error err = result;
        ERROR(err.what());
    }
}
void LuaGame::EndPlay()
{
    mScene = nullptr;

    sol::protected_function func = (*mLuaState)["EndPlay"];
    if (!func.valid())
        return;
    auto result = func();
    if (!result.valid())
    {
        const sol::error err = result;
        ERROR(err.what());
    }
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
        sol::protected_function func = (*mLuaState)["OnBeginContact"];
        if (!func.valid())
            return;
        auto result = func(entityA, entityB, nodeA, nodeB);
        if (!result.valid())
        {
            const sol::error err = result;
            ERROR(err.what());
        }
    }
    else if (contact.type == ContactEvent::Type::EndContact)
    {
        sol::protected_function func = (*mLuaState)["OnEndContact"];
        if (!func.valid())
            return;
        auto result = func(entityA, entityB, nodeA, nodeB);
        if (!result.valid())
        {
            const sol::error err = result;
            ERROR(err.what());
        }
    }
}

void LuaGame::OnKeyDown(const wdk::WindowEventKeydown& key)
{
    sol::protected_function func = (*mLuaState)["OnKeyDown"];
    if (!func.valid())
        return;
    auto result = func(static_cast<int>(key.symbol), static_cast<int>(key.modifiers.value()));
    if (!result.valid())
    {
        const sol::error err = result;
        ERROR(err.what());
    }
}
void LuaGame::OnKeyUp(const wdk::WindowEventKeyup& key)
{
    sol::protected_function func = (*mLuaState)["OnKeyUp"];
    if (!func.valid())
        return;
    auto result = func(static_cast<int>(key.symbol), static_cast<int>(key.modifiers.value()));
    if (!result.valid())
    {
        const sol::error err = result;
        ERROR(err.what());
    }
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
        sol::protected_function func = (*env)["BeginPlay"];
        if (!func.valid())
            continue;
        auto result = func(entity, scene);
        if (!result.valid())
        {
            const sol::error err = result;
            ERROR(err.what());
        }
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
            sol::protected_function func = (*env)["Tick"];
            if (!func.valid())
                continue;
            auto result = func(entity, wall_time, tick_time, dt);
            if (!result.valid())
            {
                const sol::error err = result;
                ERROR(err.what());
            }
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
            sol::protected_function func = (*env)["Update"];
            if (!func.valid())
                continue;
            auto result = func(entity , wall_time , game_time , dt);
            if (!result.valid())
            {
                const sol::error err = result;
                ERROR(err.what());
            }
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
    const auto* function_name = contact.type == ContactEvent::Type::BeginContact
            ? "OnBeginContact" : "OnEndContact";

    // there's a little problem here that needs to be fixed regarding the
    // lifetimes of objects. calling into the script may choose to for
    // example delete the object from the scene which would invalidate
    // the pointers above. This needs to be fixed somehow.
    const auto& klassA = entityA->GetClass();
    const auto& klassB = entityB->GetClass();

    if (auto* env = GetTypeEnv(klassA))
    {
        sol::protected_function func = (*env)[function_name];
        if (func.valid())
        {
            auto result = func(entityA, nodeA, entityB, nodeB);
            if (!result.valid())
            {
                const sol::error err = result;
                ERROR(err.what());
            }
        }
    }
    if (auto* env = GetTypeEnv(klassB))
    {
        sol::protected_function func = (*env)[function_name];
        if (func.valid())
        {
            auto result = func(entityB, nodeB, entityA, nodeA);
            if (!result.valid())
            {
                const sol::error err = result;
                ERROR(err.what());
            }
        }
    }
}
void ScriptEngine::OnKeyDown(const wdk::WindowEventKeydown& key)
{
    for (size_t i=0; i<mScene->GetNumEntities(); ++i)
    {
        auto* entity = &mScene->GetEntity(i);
        if (auto* env = GetTypeEnv(entity->GetClass()))
        {
            sol::protected_function func = (*env)["OnKeyDown"];
            if (!func.valid())
                continue;
            auto result = func(entity , static_cast<int>(key.symbol) , static_cast<int>(key.modifiers.value()));
            if (!result.valid())
            {
                const sol::error err = result;
                ERROR(err.what());
            }
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
            sol::protected_function func = (*env)["OnKeyUp"];
            if (!func.valid())
                continue;
            auto result = func(entity , static_cast<int>(key.symbol) , static_cast<int>(key.modifiers.value()));
            if (!result.valid())
            {
                const sol::error err = result;
                ERROR(err.what());
            }
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

void BindBase(sol::state& L)
{
    auto table = L.create_named_table("base");
    table["debug"] = [](const std::string& str) {
        DEBUG(str);
    };
    table["warn"] = [](const std::string& str) {
        WARN(str);
    };
    table["error"] = [](const std::string& str) {
        ERROR(str);
    };
    table["info"] = [](const std::string& str) {
        INFO(str);
    };
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

    {
        auto util = table["util"].get_or_create<sol::table>();
        util["GetRotationFromMatrix"]    = GetRotationFromMatrix;
        util["GetScaleFromMatrix"]       = GetScaleFromMatrix;
        util["GetTranslationFromMatrix"] = GetTranslationFromMatrix;
        util["RotateVector"]             = RotateVector;
    }
    {
        sol::constructors<FRect(), FRect(float, float, float, float)> ctors;
        auto rect = table.new_usertype<FRect>("FRect", ctors);
        rect["GetHeight"] = &FRect::GetHeight;
        rect["GetWidth"]  = &FRect::GetWidth;
        rect["GetX"]      = &FRect::GetX;
        rect["GetY"]      = &FRect::GetY;
        rect["SetX"]      = &FRect::SetX;
        rect["SetY"]      = &FRect::SetY;
        rect["SetWidth"]  = &FRect::SetWidth;
        rect["SetHeight"] = &FRect::SetHeight;
        rect["Resize"]    = (void(FRect::*)(float, float))&FRect::Resize;
        rect["Grow"]      = (void(FRect::*)(float, float))&FRect::Grow;
        rect["Move"]      = (void(FRect::*)(float, float))&FRect::Move;
        rect["Translate"] = (void(FRect::*)(float, float))&FRect::Translate;
        rect["IsEmpty"]   = &FRect::IsEmpty;
    }
    {
        sol::constructors<FBox(), FBox(float, float), FBox(const glm::mat4& mat, float, float), FBox(const glm::mat4& mat)> ctors;
        auto box = table.new_usertype<FBox>("FBox", ctors);
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
