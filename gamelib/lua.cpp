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
#  include <neargye/magic_enum.hpp>
#include "warnpop.h"

#include "base/assert.h"
#include "base/logging.h"
#include "gamelib/entity.h"
#include "gamelib/game.h"
#include "gamelib/scene.h"
#include "gamelib/classlib.h"
#include "gamelib/physics.h"
#include "gamelib/lua.h"
#include "wdk/keys.h"

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
void LuaGame::Tick(double current_time)
{
    sol::protected_function func = (*mLuaState)["Tick"];
    if (!func.valid())
        return;
    auto result = func(current_time);
    if (!result.valid())
    {
        const sol::error err = result;
        ERROR(err.what());
    }
}
void LuaGame::Update(double current_time, double dt)
{
    sol::protected_function func = (*mLuaState)["Update"];
    if (!func.valid())
        return;
    auto result = func(current_time, dt);
    if (!result.valid())
    {
        const sol::error err = result;
        ERROR(err.what());
    }
}
void LuaGame::BeginPlay(Scene* scene)
{
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
    auto table = L.create_named_table("glm");
    auto type   = table.new_usertype<glm::vec2>("vec2", ctors,
sol::meta_function::index, [](const glm::vec2& vec, int index) {
        if (index > 2)
            throw std::runtime_error("glm.vec2 access out of bounds");
        return vec[index];
            });
    type["x"]   = &glm::vec2::x;
    type["y"]   = &glm::vec2::y;
    type["length"] = [](const glm::vec2& vec) {
        return glm::length(vec);
    };
    // adding meta functions for operators
    // setting the index through set_function seems to be broken! (sol 3.2.3)
    //type.set_function(sol::meta_function::index, [](const glm::vec2& vec, int index) {
    //    return vec[index];
    //});
    type.set_function(sol::meta_function::addition, [](const glm::vec2& a, const glm::vec2& b) {
        return a + b;
    });
    type.set_function(sol::meta_function::subtraction, [](const glm::vec2& a, const glm::vec2& b) {
        return a - b;
    });
    type.set_function(sol::meta_function::multiplication, [](const glm::vec2& vector, float scalar) {
        return vector * scalar;
    });
    type.set_function(sol::meta_function::multiplication, [](float scalar, const glm::vec2& vector) {
        return vector * scalar;
    });
    type.set_function(sol::meta_function::division, [](const glm::vec2& vector, float scalar) {
        return vector / scalar;
    });
    type.set_function(sol::meta_function::to_string, [](const glm::vec2& vector) {
        return std::to_string(vector.x) + "," + std::to_string(vector.y);
    });

    table["dot"] = [](const glm::vec2 &a, const glm::vec2 &b) { return glm::dot(a, b); };
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

    auto classlib = table.new_usertype<ClassLibrary>("ClassLibrary");
    classlib["FindEntityClassByName"] = &ClassLibrary::FindEntityClassByName;
    classlib["FindEntityClassById"]   = &ClassLibrary::FindEntityClassById;
    classlib["FindSceneClassByName"]  = &ClassLibrary::FindSceneClassByName;
    classlib["FindSceneClassById"]    = &ClassLibrary::FindSceneClassById;

    auto entity_node = table.new_usertype<EntityNode>("EntityNode");
    entity_node["GetId"] = &EntityNode::GetId;

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
    entity["GetTranslation"]       = &Entity::GetTranslation;
    entity["GetName"]              = &Entity::GetName;
    entity["GetId"]                = &Entity::GetId;
    entity["GetClassId"]           = &Entity::GetClassId;
    entity["GetScale"]             = &Entity::GetScale;
    entity["GetRotation"]          = &Entity::GetRotation;
    entity["SetTranslation"]       = &Entity::SetTranslation;
    entity["SetScale"]             = &Entity::SetScale;
    entity["SetRotation"]          = &Entity::SetRotation;
    entity["GetNode"]              = (EntityNode&(Entity::*)(size_t))&Entity::GetNode;
    entity["FindNodeByClassName"]  = (EntityNode*(Entity::*)(const std::string&))&Entity::FindNodeByClassName;
    entity["FindNodeByClassId"]    = (EntityNode*(Entity::*)(const std::string&))&Entity::FindNodeByClassId;
    entity["FindNodeByInstanceId"] = (EntityNode*(Entity::*)(const std::string&))&Entity::FindNodeByInstanceId;

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
    scene["FindEntityByInstanceId"]   = (Entity*(Scene::*)(const std::string&))&Scene::FindEntityByInstanceId;
    scene["FindEntityByInstanceName"] = (Entity*(Scene::*)(const std::string&))&Scene::FindEntityByInstanceName;
    scene["GetEntity"]                = (Entity&(Scene::*)(size_t))&Scene::GetEntity;

    auto physics = table.new_usertype<PhysicsEngine>("Physics");
    physics["ApplyImpulseToCenter"] = (void(PhysicsEngine::*)(const std::string&, const glm::vec2&) const)&PhysicsEngine::ApplyImpulseToCenter;
    physics["ApplyImpulseToCenter"] = (void(PhysicsEngine::*)(const EntityNode&, const glm::vec2&) const)&PhysicsEngine::ApplyImpulseToCenter;

}


} // namespace
