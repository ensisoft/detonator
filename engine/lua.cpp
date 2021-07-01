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
#  include <boost/random/mersenne_twister.hpp>
#  include <boost/random/uniform_int_distribution.hpp>
#  include <boost/random/uniform_real_distribution.hpp>
#include "warnpop.h"

#include <unordered_set>
#include <random>

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
#include "uikit/window.h"
#include "uikit/widget.h"
#include "wdk/keys.h"
#include "wdk/system.h"

// About Lua error handling. The binding code here must be careful
// to understand what is a BUG, a logical error condition and an
// exceptional condition. Normally in the engine code BUG is an
// error made by the programmer of the engine and results in a
// stack strace and a core dump. Logical error conditions are
// conditions that the code needs to be prepared to deal with, e.g
// failed/mangled data in various content files, missing data files
// etc. Finally exceptional conditions are conditions that happen
// as some unexpected failure (most typically an underlying OS
// resource allocation has failed). Exceptions are normally not
// used for logical error conditions (oops, this texture file could
// not be read etc.)
//
// However here when dealing with calls coming from the running game
// what could normally be considered a BUG in other parts of the
// engine code may not be so here since the code here needs to be
// prepared to deal with mistakes in the Lua code. (That being said
// it's still possible that *this* code contains BUGS too)
// For example: If an OOB array access is attempted it's normally a bug
// in the calling code and triggers an ASSERT. However when coming from
// a Lua it must be an expected condition. I.e we must expect that the
// Lua code will call us wrong and be prepared to deal with such situations.
//
// So what strategies are there for dealing with this?
// 1. simply ignore incorrect/buggy calls
//    - if return value is needed return some "default" value.
//    - possibly log a warning/error
// 2. device API semantics that return some "status" OK (boolean)
//    value to indicate that the call was OK.
// 3. raise a Lua error and let the caller either fail or use pcall
//
// It seems that the option number 3 is the most reasonable of these
// i.e in case of any buggy calls coming from lua a lua error is raised
// and then it's the callers responsibility to deal with that somehow
// by for example wrapping the call inside pcall.
// And for better or for worse an std exception can be used to indicate
// to sol that Lua error should be raised. (Quickly looking didn't see
// another way of raising Lua errors, maybe it does exist but sol2 docs
// are sucn an awful mess..)
// The problem with the exceptions though is that when this code has
// BUGS i.e. calls the sol2 API incorrectly for example sol2 will then
// throw an exception. So that muddles the waters a bit unfortunately..

namespace {
template<typename... Args>
void CallLua(const sol::protected_function& func, const Args&... args)
{
    if (!func.valid())
        return;
    const auto& result = func(args...);
    // All the calls into Lua begin by the engine calling into Lua.
    // That means that if any error is raised by:
    // - Lua code calls error(....) to raise an error
    // - Lua code calls into the binding code below buggily and and exception
    //   gets thrown which sol2 will catch and convert into Lua error
    // - The binding code itself is buggy and calls into sol2 wrong which throws an
    //   exception...
    // - There's some other c++ exception 
    // regardless of the source of the error/exception will then obtain
    // an invalid object here.
    if (result.valid())
        return;
    const sol::error err = result;
    ERROR(err.what());
}

template<typename LuaGame>
void BindEngine(sol::usertype<LuaGame>& engine, LuaGame& self)
{
    using namespace game;
    engine["Play"] = sol::overload(
        [](LuaGame& self, ClassHandle<SceneClass> klass) {
            if (!klass)
                throw std::runtime_error("Nil scene class");
            PlayAction play;
            play.klass = klass;
            self.PushAction(play);
        },
        [](LuaGame& self, std::string name) {
            auto handle = self.GetClassLib()->FindSceneClassByName(name);
            if (!handle)
                throw std::runtime_error("No such scene class: " + name);
            PlayAction play;
            play.klass = handle;
            self.PushAction(play);
        });

    engine["Suspend"] = [](LuaGame& self) {
        SuspendAction suspend;
        self.PushAction(suspend);
    };
    engine["Stop"] = [](LuaGame& self) {
        StopAction stop;
        self.PushAction(stop);
    };
    engine["Resume"] = [](LuaGame& self) {
        ResumeAction resume;
        self.PushAction(resume);
    };
    engine["Quit"] = [](LuaGame& self, int exit_code) {
        QuitAction quit;
        quit.exit_code = exit_code;
        self.PushAction(quit);
    };
    engine["Delay"] = [](LuaGame& self, float value) {
        DelayAction delay;
        delay.seconds = value;
        self.PushAction(delay);
    };
    engine["ShowMouse"] = [](LuaGame& self, bool show) {
        ShowMouseAction mickey;
        mickey.show = show;
        self.PushAction(mickey);
    };
    engine["ShowDebug"] = [](LuaGame& self, bool show) {
        ShowDebugAction act;
        act.show = show;
        self.PushAction(act);
    };
    engine["SetFullScreen"] = [](LuaGame& self, bool full_screen) {
        RequestFullScreenAction fs;
        fs.full_screen = full_screen;
        self.PushAction(fs);
    };
    engine["BlockKeyboard"] = [](LuaGame& self, bool yes_no) {
        BlockKeyboardAction block;
        block.block = yes_no;
        self.PushAction(block);
    };
    engine["BlockMouse"] = [](LuaGame& self, bool yes_no) {
        BlockMouseAction block;
        block.block = yes_no;
        self.PushAction(block);
    };
    engine["DebugPrint"] = [](LuaGame& self, std::string message) {
        DebugPrintAction action;
        action.message = std::move(message);
        self.PushAction(std::move(action));
    };
    engine["DebugClear"] = [](LuaGame& self) {
        DebugClearAction action;
        self.PushAction(action);
    };
    engine["OpenUI"] = sol::overload(
        [](LuaGame& self, ClassHandle<uik::Window> model) {
            if (!model)
                throw std::runtime_error("Nil UI window object.");
            OpenUIAction action;
            action.ui = model;
            self.PushAction(std::move(action));
        },
        [](LuaGame& self, std::string name) {
            auto handle = self.GetClassLib()->FindUIByName(name);
            if (!handle)
                throw std::runtime_error("No such UI: " + name);
            OpenUIAction action;
            action.ui = handle;
            self.PushAction(action);
        });
    engine["CloseUI"] = [](LuaGame& self, int result) {
        CloseUIAction action;
        action.result = result;
        self.PushAction(std::move(action));
    };
}
template<typename Type>
bool TestFlag(const Type& object, const std::string& name)
{
    using Flags = typename Type::Flags;
    const auto enum_val = magic_enum::enum_cast<Flags>(name);
    if (!enum_val.has_value())
        throw std::runtime_error("No such flag: " + name);
    return object.TestFlag(enum_val.value());
}
template<typename Type>
void SetFlag(Type& object, const std::string& name, bool on_off)
{
    using Flags = typename Type::Flags;
    const auto enum_val = magic_enum::enum_cast<Flags>(name);
    if (!enum_val.has_value())
        throw std::runtime_error("No such flag: " + name);
    object.SetFlag(enum_val.value(), on_off);
}

template<typename Type>
sol::object GetScriptVar(const Type& object, const char* key, sol::this_state state)
{
    using namespace game;
    sol::state_view lua(state);
    const ScriptVar* var = object.FindScriptVar(key);
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
    throw std::runtime_error(base::FormatString("No such variable: '%1'", key));
}
template<typename Type>
void SetScriptVar(Type& object, const char* key, sol::object value)
{
    using namespace game;
    const ScriptVar* var = object.FindScriptVar(key);
    if (var == nullptr)
        throw std::runtime_error(base::FormatString("No such variable: '%1'", key));
    else if (var->IsReadOnly())
        throw std::runtime_error(base::FormatString("Trying to write to a read only variable: '%1'", key));

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
    else throw std::runtime_error(base::FormatString("Variable type mismatch. '%1' expects: '%2'", key, var->GetType()));
}

// WAR. G++ 10.2.0 has internal segmentation fault when using the Get/SetScriptVar helpers
// directly in the call to create new usertype. adding these specializations as a workaround.
template sol::object GetScriptVar<game::Scene>(const game::Scene&, const char*, sol::this_state);
template sol::object GetScriptVar<game::Entity>(const game::Entity&, const char*, sol::this_state);
template void SetScriptVar<game::Scene>(game::Scene&, const char* key, sol::object);
template void SetScriptVar<game::Entity>(game::Entity&, const char* key, sol::object);

template<typename Widget, uik::Widget::Type CastType>
Widget* WidgetCast(uik::Widget* widget)
{
    if (widget->GetType() != CastType)
        throw std::runtime_error(base::FormatString("Widget '%1' is not a %2", widget->GetName(), CastType));
    return static_cast<Widget*>(widget);
}

// the problem with using a std random number generation is that
// the results may not be portable across implementations and it seems
// that the standard Lua math random stuff has this problem.
// http://lua-users.org/wiki/MathLibraryTutorial
// "... math.randomseed will call the underlying C function srand ..."
class RandomEngine {
public:
    static void Seed(int seed)
    { mTwister = boost::random::mt19937(seed); }
    static int NextInt()
    { return NextInt(INT_MIN, INT_MAX); }
    static int NextInt(int min, int max)
    {
        boost::random::uniform_int_distribution<int> dist(min, max);
        return dist(mTwister);
    }
    static float NextFloat(float min, float max)
    {
        boost::random::uniform_real_distribution<float> dist(min, max);
        return dist(mTwister);
    }
private:
    static boost::random::mt19937 mTwister;
};
boost::random::mt19937 RandomEngine::mTwister;

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
    BindUIK(*mLuaState);
    BindGameLib(*mLuaState);

    // bind engine interface.
    auto table  = (*mLuaState)["game"].get_or_create<sol::table>();
    auto engine = table.new_usertype<LuaGame>("Engine");
    BindEngine(engine, *this);
    engine["SetViewport"] = [](LuaGame& self, const FRect& view) {
        self.mView = view;
    };

    // todo: maybe this needs some configuring or whatever?
    mLuaState->script_file(lua_path + "/game.lua");
}

LuaGame::~LuaGame() = default;

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
void LuaGame::Tick(double game_time, double dt)
{
    CallLua((*mLuaState)["Tick"], game_time, dt);
}
void LuaGame::Update(double game_time, double dt)
{
    CallLua((*mLuaState)["Update"], game_time, dt);
}
void LuaGame::BeginPlay(Scene* scene)
{
    mScene = scene;
    (*mLuaState)["Scene"] = mScene;

    CallLua((*mLuaState)["BeginPlay"], scene);
}
void LuaGame::EndPlay(Scene* scene)
{
    CallLua((*mLuaState)["EndPlay"], scene);
    (*mLuaState)["Scene"] = nullptr;
    mScene = nullptr;
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

void LuaGame::OnUIOpen(uik::Window* ui)
{
    CallLua((*mLuaState)["OnUIOpen"], ui);
}
void LuaGame::OnUIClose(uik::Window* ui, int result)
{
    CallLua((*mLuaState)["OnUIClose"], ui, result);
}

void LuaGame::OnUIAction(const uik::Window::WidgetAction& action)
{
    CallLua((*mLuaState)["OnUIAction"], action);
}

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

ScriptEngine::~ScriptEngine() = default;

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
    BindEngine(engine, *this);

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

void ScriptEngine::EndPlay(Scene* scene)
{
    mTypeEnvs.clear();
    mScene = nullptr;
    (*mLuaState)["Scene"] = nullptr;
}

void ScriptEngine::Tick(double game_time, double dt)
{
    for (size_t i=0; i<mScene->GetNumEntities(); ++i)
    {
        auto* entity = &mScene->GetEntity(i);
        if (!entity->TestFlag(Entity::Flags::TickEntity))
            continue;
        if (auto* env = GetTypeEnv(entity->GetClass()))
        {
            CallLua((*env)["Tick"], entity, game_time, dt);
        }
    }
}
void ScriptEngine::Update(double game_time, double dt)
{
    for (size_t i=0; i<mScene->GetNumEntities(); ++i)
    {
        auto* entity = &mScene->GetEntity(i);
        if (!entity->TestFlag(Entity::Flags::UpdateEntity))
            continue;
        if (auto* env = GetTypeEnv(entity->GetClass()))
        {
            CallLua((*env)["Update"], entity, game_time, dt);
        }
    }
}
void ScriptEngine::BeginLoop()
{
    // entities spawned in the scene during calls to update/tick
    // have the spawned flag on. Invoke the BeginPlay callbacks
    // for those entities.
    for (size_t i=0; i<mScene->GetNumEntities(); ++i)
    {
        auto* entity = &mScene->GetEntity(i);
        if (!entity->TestFlag(game::Entity::ControlFlags::Spawned))
            continue;
        if (auto* env = GetTypeEnv(entity->GetClass()))
        {
            CallLua((*env)["BeginPlay"], entity, mScene);
        }
    }
}

void ScriptEngine::EndLoop()
{
     // entities killed in the scene during calls to update/tick
     // have the kill flag on. Invoke the EndPlay callbacks for
     // those entities.
     for (size_t i=0; i<mScene->GetNumEntities(); ++i)
     {
         auto* entity = &mScene->GetEntity(i);
         if (!entity->TestFlag(game::Entity::ControlFlags::Killed))
             continue;
         if (auto* env = GetTypeEnv(entity->GetClass()))
         {
             CallLua((*env)["EndPlay"], entity, mScene);
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
        if (!entity->TestFlag(Entity::Flags::WantsKeyEvents))
            continue;
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
        if (!entity->TestFlag(Entity::Flags::WantsKeyEvents))
            continue;
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
    util["GetRotationFromMatrix"]    = &GetRotationFromMatrix;
    util["GetScaleFromMatrix"]       = &GetScaleFromMatrix;
    util["GetTranslationFromMatrix"] = &GetTranslationFromMatrix;
    util["RotateVector"]             = &RotateVector;

    // see comments at RandomEngine about why this is done.
    util["RandomSeed"] = &RandomEngine::Seed;
    util["Random"] = sol::overload(
        [](sol::this_state state) {
            sol::state_view view(state);
            return sol::make_object(view, RandomEngine::NextInt());
        },
        [](sol::this_state state, int min, int max) {
            sol::state_view view(state);
            return sol::make_object(view, RandomEngine::NextInt(min, max));
        },
        [](sol::this_state state, float min, float max) {
            sol::state_view view(state);
            return sol::make_object(view, RandomEngine::NextFloat(min, max));
        });

    sol::constructors<FBox(),
            FBox(float, float),
            FBox(const glm::mat4& mat, float, float),
            FBox(const glm::mat4& mat)> ctors;
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

void BindBase(sol::state& L)
{
    auto table = L.create_named_table("base");
    table["debug"] = [](const std::string& str) { DEBUG(str); };
    table["warn"]  = [](const std::string& str) { WARN(str); };
    table["error"] = [](const std::string& str) { ERROR(str); };
    table["info"]  = [](const std::string& str) { INFO(str); };

    sol::constructors<base::FRect(), base::FRect(float, float, float, float)> rect_ctors;
    auto rect = table.new_usertype<base::FRect>("FRect", rect_ctors);
    rect["GetHeight"]      = &base::FRect::GetHeight;
    rect["GetWidth"]       = &base::FRect::GetWidth;
    rect["GetX"]           = &base::FRect::GetX;
    rect["GetY"]           = &base::FRect::GetY;
    rect["SetX"]           = &base::FRect::SetX;
    rect["SetY"]           = &base::FRect::SetY;
    rect["SetWidth"]       = &base::FRect::SetWidth;
    rect["SetHeight"]      = &base::FRect::SetHeight;
    rect["Resize"]         = (void(base::FRect::*)(float, float))&base::FRect::Resize;
    rect["Grow"]           = (void(base::FRect::*)(float, float))&base::FRect::Grow;
    rect["Move"]           = (void(base::FRect::*)(float, float))&base::FRect::Move;
    rect["Translate"]      = (void(base::FRect::*)(float, float))&base::FRect::Translate;
    rect["IsEmpty"]        = &base::FRect::IsEmpty;
    rect["Combine"]        = &base::Union<float>;
    rect["Intersect"]      = &base::Intersect<float>;
    rect["TestIntersect"]  = &base::DoesIntersect<float>;

    sol::constructors<base::FSize(), base::FSize(float, float)> size_ctors;
    auto size = table.new_usertype<base::FSize>("FSize", size_ctors);
    size["GetWidth"]  = &base::FSize::GetWidth;
    size["GetHeight"] = &base::FSize::GetHeight;
    size.set_function(sol::meta_function::multiplication, [](const base::FSize& size, float scalar) { return size * scalar; });
    size.set_function(sol::meta_function::addition, [](const base::FSize& lhs, const base::FSize& rhs) { return lhs + rhs; });
    size.set_function(sol::meta_function::subtraction, [](const base::FSize& lhs, const base::FSize& rhs) { return lhs - rhs; });

    sol::constructors<base::FPoint(), base::FPoint(float, float)> point_ctors;
    auto point = table.new_usertype<base::FPoint>("FPoint", point_ctors);
    point["GetX"] = &base::FPoint::GetX;
    point["GetY"] = &base::FPoint::GetY;
    point.set_function(sol::meta_function::addition, [](const base::FPoint& lhs, const base::FPoint& rhs) { return lhs + rhs; });
    point.set_function(sol::meta_function::subtraction, [](const base::FPoint& lhs, const base::FPoint& rhs) { return lhs - rhs; });

    // build color name table
    for (const auto& color : magic_enum::enum_values<base::Color>())
    {
        const std::string name(magic_enum::enum_name(color));
        table[sol::create_if_nil]["Colors"][name] = magic_enum::enum_integer(color);
    }

    // todo: figure out a way to construct from color name, is that possible?
    sol::constructors<base::Color4f(),
            base::Color4f(float, float, float, float),
            base::Color4f(int, int, int, int)> color_ctors;
    auto color = table.new_usertype<base::Color4f>("Color4f", color_ctors);
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
            throw std::runtime_error("No such color value:" + std::to_string(value));
        color = base::Color4f(color_value.value());
    };
    color["FromEnum"]   = [](int value) {
        const auto color_value = magic_enum::enum_cast<base::Color>(value);
        if (!color_value.has_value())
            throw std::runtime_error("No such color value:" + std::to_string(value));
        return base::Color4f(color_value.value());
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
    vec2["x"]         = &glm::vec2::x;
    vec2["y"]         = &glm::vec2::y;
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
        if (!key.has_value())
            throw std::runtime_error("No such keysym value:" + std::to_string(value));
        return std::string(magic_enum::enum_name(key.value()));
    };
    table["BtnStr"] = [](int value) {
        const auto key = magic_enum::enum_cast<wdk::MouseButton>(value);
        if (!key.has_value())
            throw std::runtime_error("No such mouse button value: " + std::to_string(value));
        return std::string(magic_enum::enum_name(key.value()));
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
            throw std::runtime_error("No such key symbol: " + std::to_string(value));
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

void BindUIK(sol::state& L)
{
    auto table = L["uik"].get_or_create<sol::table>();

    auto widget = table.new_usertype<uik::Widget>("Widget");
    widget["GetId"]          = &uik::Widget::GetId;
    widget["GetName"]        = &uik::Widget::GetName;
    widget["GetHash"]        = &uik::Widget::GetHash;
    widget["GetSize"]        = &uik::Widget::GetSize;
    widget["GetPosition"]    = &uik::Widget::GetPosition;
    widget["GetType"]        = [](const uik::Widget* widget) { return base::ToString(widget->GetType()); };
    widget["SetName"]        = &uik::Widget::SetName;
    widget["SetSize"]        = (void(uik::Widget::*)(const uik::FSize&))&uik::Widget::SetSize;
    widget["SetPosition"]    = (void(uik::Widget::*)(const uik::FPoint&))&uik::Widget::SetPosition;
    widget["TestFlag"]       = &TestFlag<uik::Widget>;
    widget["SetFlag"]        = &SetFlag<uik::Widget>;
    widget["IsEnabled"]      = &uik::Widget::IsEnabled;
    widget["IsVisible"]      = &uik::Widget::IsVisible;
    widget["IsContainer"]    = &uik::Widget::IsContainer;
    widget["GetNumChildren"] = &uik::Widget::GetNumChildren;
    widget["Grow"]           = &uik::Widget::Grow;
    widget["Translate"]      = &uik::Widget::Translate;
    widget["AsLabel"]        = &WidgetCast<uik::Label,      uik::Widget::Type::Label>;
    widget["AsPushButton"]   = &WidgetCast<uik::PushButton, uik::Widget::Type::PushButton>;
    widget["AsCheckBox"]     = &WidgetCast<uik::CheckBox,   uik::Widget::Type::CheckBox>;
    widget["AsGroupBox"]     = &WidgetCast<uik::GroupBox,   uik::Widget::Type::GroupBox>;
    widget["GetChild"]       = [](uik::Widget* widget, unsigned index) {
        if (!widget->IsContainer())
            throw std::runtime_error(base::FormatString("Widget '%1' is not a container.", widget->GetName()));
        if (index >= widget->GetNumChildren())
            throw std::runtime_error(base::FormatString("Widget '%1' index (%2) is out of bounds.", widget->GetName(), index));
        return &widget->GetChild(index);
    };
    auto label          = table.new_usertype<uik::Label>("Label");
    auto check          = table.new_usertype<uik::CheckBox>("CheckBox");
    auto group          = table.new_usertype<uik::GroupBox>("GroupBox");
    auto pushbtn        = table.new_usertype<uik::PushButton>("PushButton");
    label["GetText"]    = &uik::Label::GetText;
    label["SetText"]    = (void(uik::Label::*)(const std::string&))&uik::Label::SetText;
    check["GetText"]    = &uik::CheckBox::GetText;
    check["SetText"]    = (void(uik::CheckBox::*)(const std::string&))&uik::CheckBox::SetText;
    check["IsChecked"]  = &uik::CheckBox::IsChecked;
    check["SetChecked"] = &uik::CheckBox::SetChecked;
    group["GetText"]    = &uik::GroupBox::GetText;
    group["SetText"]    = (void(uik::GroupBox::*)(const std::string&))&uik::GroupBox::SetText;
    pushbtn["GetText"]  = &uik::PushButton::GetText;
    pushbtn["SetText"]  = (void(uik::PushButton::*)(const std::string&))&uik::PushButton::SetText;

    auto window = table.new_usertype<uik::Window>("Window");
    window["GetId"]            = &uik::Window::GetId;
    window["GetSize"]          = &uik::Window::GetSize;
    window["GetName"]          = &uik::Window::GetName;
    window["GetWidth"]         = &uik::Window::GetWidth;
    window["GetHeight"]        = &uik::Window::GetHeight;
    window["GetNumWidgets"]    = &uik::Window::GetNumWidgets;
    window["GetRootWidget"]    = [](uik::Window* window) { return &window->GetRootWidget(); };
    window["FindWidgetById"]   = [](uik::Window* window, const std::string& id) { return window->FindWidgetById(id); };
    window["FindWidgetByName"] = [](uik::Window* window, const std::string& name) { return window->FindWidgetByName(name); };
    window["FindWidgetParent"] = [](uik::Window* window, uik::Widget* child) { return window->FindParent(child); };
    window["Resize"]           = (void(uik::Window::*)(const uik::FSize&))&uik::Window::Resize;
    window["GetWidget"]        = [](uik::Window* window, unsigned index) {
        if (index >= window->GetNumWidgets())
            throw std::runtime_error(base::FormatString("Widget index %1 is out of bounds", index));
        return &window->GetWidget(index);
    };

    auto action = table.new_usertype<uik::Window::WidgetAction>("Action",
    sol::meta_function::index, [&L](const uik::Window::WidgetAction* action, const char* key) {
        sol::state_view lua(L);
        if (!std::strcmp(key, "name"))
            return sol::make_object(lua, action->name);
        else if (!std::strcmp(key, "id"))
            return sol::make_object(lua, action->id);
        else if (!std::strcmp(key, "type"))
            return sol::make_object(lua, base::ToString(action->type));
        else if (!std::strcmp(key, "value")) {
            if (std::holds_alternative<int>(action->value))
                return sol::make_object(lua, std::get<int>(action->value));
            else if (std::holds_alternative<float>(action->value))
                return sol::make_object(lua, std::get<float>(action->value));
            else if (std::holds_alternative<bool>(action->value))
                return sol::make_object(lua, std::get<bool>(action->value));
            else if (std::holds_alternative<std::string>(action->value))
                return sol::make_object(lua, std::get<std::string>(action->value));
            else BUG("???");
        }
        throw std::runtime_error(base::FormatString("No such ui action index: %1", key));
    });
}

void BindGameLib(sol::state& L)
{
    auto table = L["game"].get_or_create<sol::table>();

    auto classlib = table.new_usertype<ClassLibrary>("ClassLibrary");
    classlib["FindEntityClassByName"] = &ClassLibrary::FindEntityClassByName;
    classlib["FindEntityClassById"]   = &ClassLibrary::FindEntityClassById;
    classlib["FindSceneClassByName"]  = &ClassLibrary::FindSceneClassByName;
    classlib["FindSceneClassById"]    = &ClassLibrary::FindSceneClassById;
    classlib["FindUIByName"]          = &ClassLibrary::FindUIByName;
    classlib["FindUIById"]            = &ClassLibrary::FindUIById;

    auto drawable = table.new_usertype<DrawableItem>("Drawable");
    drawable["GetMaterialId"] = &DrawableItem::GetMaterialId;
    drawable["GetDrawableId"] = &DrawableItem::GetDrawableId;
    drawable["GetLayer"]      = &DrawableItem::GetLayer;
    drawable["GetLineWidth"]  = &DrawableItem::GetLineWidth;
    drawable["GetTimeScale"]  = &DrawableItem::GetTimeScale;
    drawable["SetTimeScale"]  = &DrawableItem::SetTimeScale;
    drawable["TestFlag"]      = &TestFlag<DrawableItem>;
    drawable["SetFlag"]       = &SetFlag<DrawableItem>;

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
    body["TestFlag"]           = &TestFlag<RigidBodyItem>;
    body["SetFlag"]            = &SetFlag<RigidBodyItem>;
    body["GetSimulationType"]  = [](const RigidBodyItem* item) {
        return magic_enum::enum_name(item->GetSimulation());
    };
    body["GetCollisionShapeType"] = [](const RigidBodyItem* item) {
        return magic_enum::enum_name(item->GetCollisionShape());
    };

    auto text = table.new_usertype<TextItem>("TextItem");
    text["GetText"]       = &TextItem::GetText;
    text["GetColor"]      = &TextItem::GetTextColor;
    text["GetLayer"]      = &TextItem::GetLayer;
    text["GetFontName"]   = &TextItem::GetFontName;
    text["GetFontSize"]   = &TextItem::GetFontSize;
    text["GetLineHeight"] = &TextItem::GetLineHeight;
    text["SetText"]       = (void(TextItem::*)(const std::string&))&TextItem::SetText;
    text["SetColor"]      = &TextItem::SetTextColor;
    text["TestFlag"]      = &TestFlag<TextItem>;

    auto entity_node = table.new_usertype<EntityNode>("EntityNode");
    entity_node["GetId"]          = &EntityNode::GetId;
    entity_node["GetName"]        = &EntityNode::GetName;
    entity_node["GetClassId"]     = &EntityNode::GetClassId;
    entity_node["GetClassName"]   = &EntityNode::GetClassName;
    entity_node["GetTranslation"] = &EntityNode::GetTranslation;
    entity_node["GetScale"]       = &EntityNode::GetScale;
    entity_node["GetRotation"]    = &EntityNode::GetRotation;
    entity_node["HasRigidBody"]   = &EntityNode::HasRigidBody;
    entity_node["HasTextItem"]    = &EntityNode::HasTextItem;
    entity_node["HasDrawable"]    = &EntityNode::HasDrawable;
    entity_node["GetDrawable"]    = (DrawableItem*(EntityNode::*)(void))&EntityNode::GetDrawable;
    entity_node["GetRigidBody"]   = (RigidBodyItem*(EntityNode::*)(void))&EntityNode::GetRigidBody;
    entity_node["GetTextItem"]    = (TextItem*(EntityNode::*)(void))&EntityNode::GetTextItem;
    entity_node["SetScale"]       = &EntityNode::SetScale;
    entity_node["SetSize"]        = &EntityNode::SetSize;
    entity_node["SetTranslation"] = &EntityNode::SetTranslation;
    entity_node["SetName"]        = &EntityNode::SetName;
    entity_node["Translate"]      = (void(EntityNode::*)(const glm::vec2&))&EntityNode::Translate;
    entity_node["Rotate"]         = (void(EntityNode::*)(float))&EntityNode::Rotate;

    auto entity = table.new_usertype<Entity>("Entity",
        sol::meta_function::index,     &GetScriptVar<Entity>,
        sol::meta_function::new_index, &SetScriptVar<Entity>);
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
    entity["TestFlag"]             = &TestFlag<Entity>;

    auto entity_args = table.new_usertype<EntityArgs>("EntityArgs", sol::constructors<EntityArgs()>());
    entity_args["class"]    = sol::property(&EntityArgs::klass);
    entity_args["name"]     = sol::property(&EntityArgs::name);
    entity_args["scale"]    = sol::property(&EntityArgs::scale);
    entity_args["position"] = sol::property(&EntityArgs::position);
    entity_args["rotation"] = sol::property(&EntityArgs::rotation);

    auto scene = table.new_usertype<Scene>("Scene",
       sol::meta_function::index,     &GetScriptVar<Scene>,
       sol::meta_function::new_index, &SetScriptVar<Scene>);
    scene["GetNumEntities"]           = &Scene::GetNumEntities;
    scene["FindEntityByInstanceId"]   = (Entity*(Scene::*)(const std::string&))&Scene::FindEntityByInstanceId;
    scene["FindEntityByInstanceName"] = (Entity*(Scene::*)(const std::string&))&Scene::FindEntityByInstanceName;
    scene["GetEntity"]                = (Entity&(Scene::*)(size_t))&Scene::GetEntity;
    scene["KillEntity"]               = &Scene::KillEntity;
    scene["SpawnEntity"]              = &Scene::SpawnEntity;
    scene["FindEntityTransform"]      = &Scene::FindEntityTransform;
    scene["FindEntityNodeTransform"]  = &Scene::FindEntityNodeTransform;
    scene["GetTime"]                  = &Scene::GetTime;
    scene["GetClassName"]             = &Scene::GetClassName;
    scene["GetClassId"]               = &Scene::GetClassId;

    auto physics = table.new_usertype<PhysicsEngine>("Physics");
    physics["ApplyImpulseToCenter"] = (void(PhysicsEngine::*)(const std::string&, const glm::vec2&) const)&PhysicsEngine::ApplyImpulseToCenter;
    physics["ApplyImpulseToCenter"] = (void(PhysicsEngine::*)(const EntityNode&, const glm::vec2&) const)&PhysicsEngine::ApplyImpulseToCenter;

}


} // namespace
