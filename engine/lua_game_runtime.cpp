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
#  include <sol/sol.hpp>
#  include <glm/glm.hpp>
#  include <neargye/magic_enum.hpp>
#include "warnpop.h"

#include <unordered_set>
#include <random>
#include <bitset>

#include "base/assert.h"
#include "base/logging.h"
#include "base/types.h"
#include "base/color4f.h"
#include "base/format.h"
#include "base/trace.h"
#include "data/writer.h"
#include "data/io.h"
#include "audio/graph.h"
#include "game/entity.h"
#include "game/scene.h"
#include "graphics/material.h"
#include "game/tilemap.h"
#include "engine/lua.h"
#include "engine/lua_game_runtime.h"
#include "engine/game.h"
#include "engine/audio.h"
#include "engine/classlib.h"
#include "engine/physics.h"
#include "engine/loader.h"
#include "engine/event.h"
#include "engine/state.h"
#include "uikit/window.h"
#include "uikit/widget.h"

#include "base/snafu.h"

#if defined(WINDOWS_OS)
#  define OS_NAME "WIN32"
#elif defined(LINUX_OS)
# define OS_NAME "LINUX"
#elif defined(WEBASSEMBLY)
# define OS_NAME "WASM"
#else
#  error Unknown platform
#endif

using namespace game;
using namespace engine::lua;

// About engine and Lua game error handling.

// Normally in the engine there are 3 types of possible "error"
// conditions all of which use different strategy to deal with:
// a) Engine bugs created by the engine programmer. these are dealt
//    with the BUG and ASSERT macros which when triggered dump core
//    and abort the program.
// b) Logical "error" conditions that the engine must be prepared
//    to deal with such as junk data, or not being able to open a
//    a file/resource etc. these are best deal with error codes/flags
//    error strings and messages. the important thing to note here is
//    that from the engine perspective these are not errors at all.
//    they're only errors from the *user* perspective.
// c) Unexpected failures such as OS resource allocation failures,
//    create socket, create mutex allocate memory etc. these are
//    handled by throwing exceptions for convenient propagation of
//    this failure information up the stack without having to muddle
//    the rest of the engine code with this type of information and
//    (irrelevant) failure propagation.
//
// When dealing with arbitrary Lua code the engine must be ready to
// handle failures in Lua in some way. That means that *BUGS* in the
// Lua game code are logical error conditions from the engine perspective
// and the engine must be ready to deal with those. So essentially what
// is a type (a) BUG condition in Lua game code is a type (b) logical
// error condition in the engine.

// When dealing with the Lua game code errors we can expect the following:
//
// 1. Syntax errors. In C++ these would be build-time (compiler) errors and
//    the program would never even be built. These could simply not happen
//    in a running program. However, since Lua is a dynamic language these
//    will happen at runtime instead.
//
//    Some more examples of these types of errors.
//    - trying to call a function which doesn't exist
//    - trying to access a property which doesn't exist
//    - trying to access a variable which doesn't exist
//    - calling a function wrong
//     - incorrect number of arguments
//     - incorrect argument types
//     - incorrect arguments for operators such as trying to sum a string
//       and an int
//
// 2. Logical game bugs. These happen when the game code is correct in terms
//    of its syntax but is wrong in terms of its semantics. For example, it
//    might be calling  a function with arguments that are not  part of the
//    function's domain.
//
//
// So what to do about these?
//
// For type (1) errors, i.e. syntax errors the game tries to do something
// that makes no sense. The best strategy that I can think of right now is
// to produce an error with a stack trace (if possible) and stop executing
// any Lua code from that point on. The error message should at minimum show
// the offending Lua code line. Most of these should already be handled by
// the Lua interpreter itself. The only case that we might have to consider
// here is maybe the Lua index and new_index meta methods.
//
// Type (2) errors raises the question whether the engine should be validating
// the inputs coming from the game or not. In other words whether to check that
// the arguments coming to a function are part the function's domain etc.
// If no validation is done then any bug such as OOB access on some underlying
// data structure can silently create corruption or (most likely) hit a BUG in
// the engine thus taking the whole game process down. For the Lua game developer
// this strategy might be a little difficult for understanding that a bug in the
// Lua game code caused an abort and a stack trace inside the engine. Especially
// if the stack trace is the C++ stack trace (with all the native Lua binding stuff)
// instead of the *Lua* stack trace. Seems that a better strategy would be to
// take down the Lua game only and show produce a Lua only error message + stack trace.
//
// Therefore, right now I'm inclined to think that the Lua engine API binding
// should perform input validation and make sure that the called functionality
// is called right. That opens the next question what to do when bugs are detected?
// Some possible API semantical alternatives (any logging is an addition to any of these
// and is not a API strategy or semantical solution by itself).
// a) Simply ignore the buggy/incorrect calls, return "default" or nil values and objects.
// b) Change API semantics so that each engine Lua API would return some "status"
//   value to indicate success or error + any actual return value.
// c) Raise a Lua error
//
//
// I really don't like the option (a). It's far too easy to simply ignore
// (either accidentally  or on purpose) the issue and continue with incorrect state.
// Option (b) has the same problem  as (a). Adding status error checking is an
// improvement but would make for some really tedious client side (game) programming.
// Who wants to check explicitly check every function call for success or for failure?
// This isn't Go after all! Therefore, it seems to me that the option (c) is the most
// reasonable of these. I.e. in case of a buggy Lua game call a Lua error is raised.
// Then the question of "return values" also disappear. We simply don't need to write
// any code to deal with bugs.
//
// The important thing to note here is that the C++ mechanism most readily available
// by Sol3 for creating a Lua error is to throw a C++ exception. Unless the game Lua
// code used a pcall the top level Sol3 protected_function will propagate the Lua
// error back to the C++ code which can then propagate it further up the stack and
// eventually show it to the user.
//
// One thing to be careful about though is that calling Sol3 wrong will also throw
// exceptions. So unless we're very careful we end up having BUGS in the Lua binding
// code turned into "Lua game errors" which is not what we should want to do!!

namespace {
// Call into Lua, i.e. invoke a function in some Lua script.
// Returns true if the call was executed, or false to indicate that
// there's no such function to call. Throws an exception on script error.
template<typename... Args>
bool CallLua(const sol::protected_function& func, const Args&... args)
{
    if (!func.valid())
        return false;
    const auto& result = func(args...);
    // All the calls into Lua begin by the engine calling into Lua.
    // The protected_function will create a new protected scope and
    //   a) realize Lua errors raised by error(...)
    //   b) catch c++ exceptions
    // then return validity status and any error message through the
    // result object.
    // However, we must take care inside the Binding code since any
    // *BUGS* there i.e. calling sol3 wrong will also have sol3 throw
    // an exception. This would turn an engine (binding code) BUG
    // into a Lua game bug which is not what we want!
    if (result.valid())
        return true;
    const sol::error err = result;

    // todo: Lua code has failed. This information should likely be
    // propagated in a logical Lua error object rather than by
    // throwing an exception.
    throw GameError(err.what());
}

template<typename Ret, typename... Args>
bool CallLua(Ret* retval, const sol::protected_function& func, const Args&... args)
{
    if (!func.valid())
        return false;
    const auto& result = func(args...);
    // All the calls into Lua begin by the engine calling into Lua.
    // The protected_function will create a new protected scope and
    //   a) realize Lua errors raised by error(...)
    //   b) catch c++ exceptions
    // then return validity status and any error message through the
    // result object.
    // However, we must take care inside the Binding code since any
    // *BUGS* there i.e. calling sol3 wrong will also have sol3 throw
    // an exception. This would turn an engine (binding code) BUG
    // into a Lua game bug which is not what we want!
    if (result.valid())
    {
        if (result.return_count() != 1)
            throw GameError("No return value from Lua.");
        *retval = result;
        return true;
    }
    const sol::error err = result;

    // todo: Lua code has failed. This information should likely be
    // propagated in a logical Lua error object rather than by
    // throwing an exception.
    throw GameError(err.what());
}

} // namespace

namespace engine
{

LuaRuntime::LuaRuntime(const std::string& lua_path,
                       const std::string& game_script,
                       const std::string& game_home,
                       const std::string& game_name)
  : mLuaPath(lua_path)
  , mGameScript(game_script)
  , mGameHome(game_home)
  , mGameName(game_name)
{}

LuaRuntime::~LuaRuntime()
{
    // careful here, make sure to clean up the environment objects
    // first since they depend on lua state.
    mAnimatorEnvs.clear();
    mEntityEnvs.clear();
    mWindowEnvs.clear();
    mSceneEnv.reset();
    mGameEnv.reset();
    mLuaState.reset();
}
void LuaRuntime::SetFrameNumber(unsigned int frame)
{
    if (mLuaState)
    {
        (*mLuaState)["Frame"] = frame;
    }
}

void LuaRuntime::SetSurfaceSize(unsigned int width, unsigned int height)
{
    if (mLuaState)
    {
        (*mLuaState)["SurfaceWidth"]  = width;
        (*mLuaState)["SurfaceHeight"] = height;
        if (mPreviewMode || mEditingMode && mGameEnv)
            CallLua((*mGameEnv)["OnRenderingSurfaceResized"], width, height);
    }
}

void LuaRuntime::SetEditingMode(bool editing)
{ mEditingMode = editing; }
void LuaRuntime::SetPreviewMode(bool preview)
{ mPreviewMode = preview; }
void LuaRuntime::SetClassLibrary(const ClassLibrary* classlib)
{ mClassLib = classlib; }
void LuaRuntime::SetPhysicsEngine(const PhysicsEngine* engine)
{ mPhysicsEngine = engine; }
void LuaRuntime::SetAudioEngine(const AudioEngine* engine)
{ mAudioEngine = engine; }
void LuaRuntime::SetDataLoader(const Loader* loader)
{ mDataLoader = loader; }
void LuaRuntime::SetStateStore(KeyValueStore* store)
{ mStateStore = store; }
void LuaRuntime::SetCurrentUI(uik::Window* window)
{ mWindow = window; }

void LuaRuntime::Init()
{
    mLuaState = std::make_unique<sol::state>();
    mLuaState->open_libraries();
    mLuaState->clear_package_loaders();

    mLuaState->add_package_loader([this](std::string module) {
        ASSERT(mDataLoader);
        if (!base::EndsWith(module, ".lua"))
            module += ".lua";

        DEBUG("Loading Lua module. [module=%1]", module);

        engine::Loader::EngineDataHandle buffer;

        if (base::StartsWith(module, "app://") ||
            base::StartsWith(module, "pck://") ||
            base::StartsWith(module, "ws://") ||
            base::StartsWith(module, "fs://"))
            buffer = mDataLoader->LoadEngineDataUri(module);
        else buffer = mDataLoader->LoadEngineDataFile(base::JoinPath(mLuaPath, module));

        if (!buffer)
            throw GameError("Can't find lua module: " + module);

        auto ret = mLuaState->load_buffer((const char*)buffer->GetData(), buffer->GetByteSize());
        if (!ret.valid())
        {
            sol::error err = ret;
            throw GameError(base::FormatString("Lua error in '%1'\n%2", module, err.what()));
        }
        return ret.get<sol::function>(); // hmm??
    });

    BindBase(*mLuaState);
    BindUtil(*mLuaState);
    BindData(*mLuaState);
    BindGLM(*mLuaState);
    BindGFX(*mLuaState);
    BindWDK(*mLuaState);
    BindUIK(*mLuaState);
    BindGameLib(*mLuaState);

    (*mLuaState)["PreviewMode"] = mPreviewMode;
    (*mLuaState)["EditingMode"] = mEditingMode;
    (*mLuaState)["Audio"]    = mAudioEngine;
    (*mLuaState)["Physics"]  = mPhysicsEngine;
    (*mLuaState)["ClassLib"] = mClassLib;
    (*mLuaState)["State"]    = mStateStore;
    (*mLuaState)["Game"]     = this;
    (*mLuaState)["CallMethod"] = [this](sol::object object, const std::string& method,
                                        sol::variadic_args va) {
        return this->CallCrossEnvMethod(object, method, va);
    };

    auto table = (*mLuaState)["game"].get_or_create<sol::table>();
    table["OS"]   = OS_NAME;
    table["home"] = mGameHome;
    table["name"] = mGameName;

    auto engine = table.new_usertype<LuaRuntime>("Engine");

    engine["Play"] = sol::overload(
        [](LuaRuntime& self, ClassHandle<SceneClass> klass) {
            if (!klass)
            {
                ERROR("Failed to play scene. Scene class is nil.");
                return (game::Scene*)nullptr;
            }
            PlayAction play;
            play.scene = game::CreateSceneInstance(klass);
            auto* ret = play.scene.get();
            self.mActionQueue.push(std::move(play));
            return ret;
        },
        [](LuaRuntime& self, std::string name) {
            auto klass = self.mClassLib->FindSceneClassByName(name);
            if (!klass)
            {
                ERROR("Failed to play scene. No such scene class. [klass='%1']", name);
                return (game::Scene*)nullptr;
            }
            PlayAction play;
            play.scene = game::CreateSceneInstance(klass);
            auto* ret = play.scene.get();
            self.mActionQueue.push(std::move(play));
            return ret;
        });
    engine["Suspend"] = [](LuaRuntime& self) {
        SuspendAction suspend;
        self.mActionQueue.push(suspend);
    };
    engine["EndPlay"] = [](LuaRuntime& self) {
        EndPlayAction end;
        self.mActionQueue.push(end);
    };
    engine["Resume"] = [](LuaRuntime& self) {
        ResumeAction resume;
        self.mActionQueue.push(resume);
    };
    engine["Quit"] = [](LuaRuntime& self, int exit_code) {
        QuitAction quit;
        quit.exit_code = exit_code;
        self.mActionQueue.push(quit);
    };
    engine["Delay"] = [](LuaRuntime& self, float value) {
        DelayAction delay;
        delay.seconds = value;
        self.mActionQueue.push(delay);
    };
    engine["GrabMouse"] = [](LuaRuntime& self, bool grab) {
        GrabMouseAction mickey;
        mickey.grab = grab;
        self.mActionQueue.push(mickey);
    };
    engine["ShowMouse"] = [](LuaRuntime& self, bool show) {
        ShowMouseAction mickey;
        mickey.show = show;
        self.mActionQueue.push(mickey);
    };
    engine["ShowDebug"] = [](LuaRuntime& self, bool show) {
        ShowDebugAction act;
        act.show = show;
        self.mActionQueue.push(act);
    };
    engine["SetFullScreen"] = [](LuaRuntime& self, bool full_screen) {
        RequestFullScreenAction fs;
        fs.full_screen = full_screen;
        self.mActionQueue.push(fs);
    };
    engine["BlockKeyboard"] = [](LuaRuntime& self, bool yes_no) {
        BlockKeyboardAction block;
        block.block = yes_no;
        self.mActionQueue.push(block);
    };
    engine["BlockMouse"] = [](LuaRuntime& self, bool yes_no) {
        BlockMouseAction block;
        block.block = yes_no;
        self.mActionQueue.push(block);
    };
    engine["DebugPrint"] = [](LuaRuntime& self, std::string message) {
        DebugPrintAction action;
        action.message = std::move(message);
        self.mActionQueue.push(std::move(action));
    };
    engine["DebugDrawCircle"] = sol::overload(
        [](LuaRuntime& self, const glm::vec2& center, float radius, const base::Color4f& color, float width) {
            DebugDrawCircle draw;
            draw.center = FPoint(center.x, center.y);
            draw.radius = radius;
            draw.color  = color;
            draw.width   = width;
            self.mActionQueue.push(draw);
        },
        [](LuaRuntime& self, const base::FPoint& center, float radius, const base::Color4f& color, float width) {
            DebugDrawCircle draw;
            draw.center = center;
            draw.radius = radius;
            draw.color  = color;
            draw.width  = width;
            self.mActionQueue.push(draw);
        });
    engine["DebugDrawLine"] = sol::overload(
        [](LuaRuntime& self, const glm::vec2&  a, const glm::vec2& b, const base::Color4f& color, float width) {
            DebugDrawLine draw;
            draw.a = FPoint(a.x, a.y);
            draw.b = FPoint(b.x, b.y);
            draw.color = color;
            draw.width = width;
            self.mActionQueue.push(draw);
        },
        [](LuaRuntime& self, const base::FPoint& a, const FPoint& b, const base::Color4f& color, float width) {
            DebugDrawLine draw;
            draw.a = a;
            draw.b = b;
            draw.color = color;
            draw.width = width;
            self.mActionQueue.push(draw);
        },
        [](LuaRuntime& self, float x0, float y0, float x1, float y1, const base::Color4f& color, float width) {
            DebugDrawLine draw;
            draw.a = base::FPoint(x0, y0);
            draw.b = base::FPoint(x1, y1);
            draw.color = color;
            draw.width = width;
            self.mActionQueue.push(draw);
        });
    engine["DebugDrawRect"] = sol::overload(
        [](LuaRuntime& self, const glm::vec2& top_left, const glm::vec2& bottom_right, base::Color4f& color, float width) {
            DebugDrawRect draw;
            draw.top_left     = FPoint(top_left.x, top_left.y);
            draw.bottom_right = FPoint(bottom_right.x, bottom_right.y);
            draw.color = color;
            draw.width = width;
            self.mActionQueue.push(draw);
        },
        [](LuaRuntime& self, const base::FPoint& top_left, const base::FPoint& bottom_right, base::Color4f& color, float width) {
            DebugDrawRect draw;
            draw.top_left = top_left;
            draw.bottom_right = bottom_right;
            draw.color = color;
            draw.width = width;
            self.mActionQueue.push(draw);
        });

    engine["DebugClear"] = [](LuaRuntime& self) {
        DebugClearAction action;
        self.mActionQueue.push(action);
    };
    engine["DebugPause"] = [](LuaRuntime& self, bool pause) {
        DebugPauseAction action;
        action.pause = pause;
        self.mActionQueue.push(action);
    };

    engine["SetViewport"] = sol::overload(
        [](LuaRuntime& self, const FRect& view) {
            self.mView = view;
        },
        [](LuaRuntime& self, float x, float y, float width, float height) {
            self.mView = base::FRect(x, y, width, height);
        },
        [](LuaRuntime& self, float width, float height) {
            self.mView = base::FRect(0.0f,  0.0f, width, height);
        });
    engine["GetTopUI"] = [](LuaRuntime& self, sol::this_state state) {
        sol::state_view lua(state);
        if (self.mWindow == nullptr)
            return sol::make_object(lua, sol::nil);
        return sol::make_object(lua, self.mWindow);
    };
    engine["OpenUI"] = sol::overload(
        [](LuaRuntime& self, ClassHandle<uik::Window> model) {
            if (!model)
            {
                ERROR("Failed to open game UI. Window object is nil.");
                return (uik::Window*)nullptr;
            }
            // there's no "class" object for the UI system so we're just
            // going to create a mutable copy and put that on the UI stack.
            OpenUIAction action;
            action.ui = std::make_shared<uik::Window>(*model);
            self.mActionQueue.push(action);
            return action.ui.get();
        },
        [](LuaRuntime& self, std::string name) {
            auto handle = self.mClassLib->FindUIByName(name);
            if (!handle)
            {
                ERROR("Failed to open game UI. No such window class. [name='%1']", name);
                return (uik::Window*)nullptr;
            }
            OpenUIAction action;
            action.ui = std::make_shared<uik::Window>(*handle);
            self.mActionQueue.push(action);
            return action.ui.get();
        });
    engine["CloseUI"] = [](LuaRuntime& self, int result) {
        CloseUIAction action;
        action.result = result;
        self.mActionQueue.push(std::move(action));
    };
    engine["PostEvent"] =  [](LuaRuntime& self, const GameEvent& event) {
        PostEventAction action;
        action.event = event;
        self.mActionQueue.push(std::move(action));
    };
    engine["ShowDeveloperUI"] = [](LuaRuntime& self, bool show) {
        ShowDeveloperUIAction action;
        action.show = show;
        self.mActionQueue.push(action);
    };
    engine["EnableEffect"] = [](LuaRuntime& self, std::string effect, bool on_off) {
        EnableEffectAction action;
        action.name  = std::move(effect);
        action.value = on_off;
        self.mActionQueue.push(action);
    };

    if (!mGameScript.empty())
    {
        mGameEnv = std::make_unique<sol::environment>(*mLuaState, sol::create, mLuaState->globals());
        (*mGameEnv)["__script_id__"] = "__main__";

        // todo: use the LoadEngineDataId here. (Requires changing the main script from URI to an ID)
        const auto& script_uri  = mGameScript;
        const auto& script_buff = mDataLoader->LoadEngineDataUri(script_uri);
        const auto& chunk_name  = mGameScript;
        DEBUG("Loading main game script. [uri='%1']", script_uri);

        if (!script_buff)
        {
            ERROR("Failed to load main game script file. [uri='%1']", script_uri);
            throw std::runtime_error("failed to load main game script.");
        }
        const auto& script_file = script_buff->GetSourceName();
        const auto& script_view = script_buff->GetStringView();
        auto result = mLuaState->script(script_view, *mGameEnv, chunk_name);
        if (!result.valid())
        {
            const sol::error err = result;
            ERROR("Lua script error. [file='%1', error='%2']", script_file, err.what());
            // throwing here is just too convenient way to propagate the Lua
            // specific error message up the stack without cluttering the interface,
            // and when running the engine inside the editor we really want to
            // have this lua error propagated all the way to the UI
            throw std::runtime_error(err.what());
        }
    }
}

bool LuaRuntime::LoadGame()
{
    if (mGameEnv)
        CallLua((*mGameEnv)["LoadGame"]);
    // tood: return value.
    return true;
}

void LuaRuntime::StartGame()
{
    if (mGameEnv)
        CallLua((*mGameEnv)["StartGame"]);
}

void LuaRuntime::SaveGame()
{
    if (mGameEnv)
        CallLua((*mGameEnv)["SaveGame"]);
}
void LuaRuntime::StopGame()
{
    if (mGameEnv)
        CallLua((*mGameEnv)["StopGame"]);
}

void LuaRuntime::BeginPlay(Scene* scene, Tilemap* map)
{
    // table that maps entity types to their scripting
    // environments. then we later invoke the script per
    // each instance's type on each instance of that type.
    // In other words if there's an EntityClass 'foobar'
    // it has a "foobar.lua" script and there are 2 entities
    // a and b, the same script foobar.lua will be invoked
    // for a total of two times (per script function), once
    // per each instance.
    std::unordered_map<std::string, std::shared_ptr<sol::environment>> entity_env_map;
    std::unordered_map<std::string, std::shared_ptr<sol::environment>> script_env_map;
    std::unordered_map<std::string, std::shared_ptr<sol::environment>> animator_env_map;

    for (size_t i=0; i<scene->GetNumEntities(); ++i)
    {
        const auto& entity = scene->GetEntity(i);
        const auto& klass  = entity.GetClass();
        // have we already seen this entity class id?
        if (entity_env_map.find(klass.GetId()) != entity_env_map.end())
            continue;

        if (!klass.HasScriptFile())
            continue;

        const auto& script = klass.GetScriptFileId();
        auto it = script_env_map.find(script);
        if (it == script_env_map.end())
        {
            const auto& script_buff = mDataLoader->LoadEngineDataId(script);
            if (!script_buff)
            {
                ERROR("Failed to load entity class script file. [class='%1', script='%2']", klass.GetName(), script);
                continue;
            }
            auto script_env = std::make_shared<sol::environment>(*mLuaState, sol::create, mLuaState->globals());
            // store the script ID with the script object/environment
            // this is used when for example checking access to a scripting variable.
            // i.e. we check that the entity's script ID is the same as the script ID
            // stored the script environment.
            (*script_env)["__script_id__"] = script;

            const auto& script_file = script_buff->GetSourceName();
            const auto& script_view = script_buff->GetStringView();
            const auto& chunk_name  = script_buff->GetName();
            const auto& result = mLuaState->script(script_view, *script_env, chunk_name);
            if (!result.valid())
            {
                const sol::error err = result;
                ERROR("Lua script error. [file='%1', error='%2']", script_file, err.what());
                // throwing here is just too convenient way to propagate the Lua
                // specific error message up the stack without cluttering the interface,
                // and when running the engine inside the editor we really want to
                // have this lua error propagated all the way to the UI
                throw std::runtime_error(err.what());
            }
            it = script_env_map.insert({script, script_env}).first;
            DEBUG("Entity class script loaded. [class='%1', file='%2']", klass.GetName(), script_file);
        }
        entity_env_map[klass.GetId()] = it->second;
    }

    for (size_t i=0; i<scene->GetNumEntities(); ++i)
    {
        const auto& entity = scene->GetEntity(i);
        const auto& klass  = entity.GetClass();
        if (klass.GetNumAnimators() == 0)
            continue;
        const auto& animator = klass.GetAnimator(0);
        if (!animator.HasScriptId())
            continue;

        if (animator_env_map.find(animator.GetId()) != animator_env_map.end())
            continue;

        const auto& scriptId = animator.GetScriptId();
        auto it = script_env_map.find(scriptId);
        if (it == script_env_map.end())
        {
            const auto& script_buff = mDataLoader->LoadEngineDataId(scriptId);
            if (!script_buff)
            {
                ERROR("Failed to load entity animator script file. [class='%1', script='%2']", klass.GetName(), scriptId);
                continue;
            }
            auto script_env = std::make_shared<sol::environment>(*mLuaState, sol::create, mLuaState->globals());
            // store the script ID with the script object/environment
            // this is used when for example checking access to a scripting variable.
            // i.e. we check that the entity's script ID is the same as the script ID
            // stored the script environment.
            (*script_env)["__script_id__"] = scriptId;

            const auto& script_file = script_buff->GetSourceName();
            const auto& script_view = script_buff->GetStringView();
            const auto& chunk_name  = script_buff->GetName();
            const auto& result = mLuaState->script(script_view, *script_env, chunk_name);
            if (!result.valid())
            {
                const sol::error err = result;
                ERROR("Lua script error. [file='%1', error='%2']", script_file, err.what());
                // throwing here is just too convenient way to propagate the Lua
                // specific error message up the stack without cluttering the interface,
                // and when running the engine inside the editor we really want to
                // have this lua error propagated all the way to the UI
                throw std::runtime_error(err.what());
            }
            it = script_env_map.insert({scriptId, script_env}).first;
            DEBUG("Entity animator script loaded. [class='%1', file='%2']", klass.GetName(), script_file);
        }
        animator_env_map[animator.GetId()] = it->second;
    }

    std::unique_ptr<sol::environment> scene_env;
    if ((*scene)->HasScriptFile())
    {
        const auto& klass = scene->GetClass();
        const auto& script = klass.GetScriptFileId();
        const auto& script_buff = mDataLoader->LoadEngineDataId(script);
        if (!script_buff)
        {
            ERROR("Failed to load scene class script file. [class='%1', script='%2']", klass.GetName(), script);
        }
        else
        {
            scene_env = std::make_unique<sol::environment>(*mLuaState, sol::create, mLuaState->globals());
            (*scene_env)["__script_id__"] = script;

            const auto& script_file = script_buff->GetSourceName();
            const auto& script_view = script_buff->GetStringView();
            const auto& chunk_name  = script_buff->GetName();
            const auto& result = mLuaState->script(script_view, *scene_env, chunk_name);
            if (!result.valid())
            {
                const sol::error err = result;
                ERROR("Lua script error. [file='%1', error='%2']", script_file, err.what());
                // throwing here is just too convenient way to propagate the Lua
                // specific error message up the stack without cluttering the interface,
                // and when running the engine inside the editor we really want to
                // have this lua error propagated all the way to the UI
                throw std::runtime_error(err.what());
            }
            DEBUG("Scene class script loaded. [class='%1', file='%2']", klass.GetName(), script_file);
        }
    }

    mSceneEnv   = std::move(scene_env);
    mEntityEnvs = std::move(entity_env_map);
    mAnimatorEnvs = std::move(animator_env_map);

    mScene = scene;
    mTilemap = map;
    (*mLuaState)["Scene"] = mScene;
    (*mLuaState)["Map"]   = mTilemap;

    if (mGameEnv)
        CallLua((*mGameEnv)["BeginPlay"], scene, map);

    if (mSceneEnv)
        CallLua((*mSceneEnv)["BeginPlay"], scene, map);

    for (size_t i=0; i<scene->GetNumEntities(); ++i)
    {
        auto* entity = &mScene->GetEntity(i);
        if (mSceneEnv)
            CallLua((*mSceneEnv)["SpawnEntity"], scene, map, entity);

        if (auto* env = GetTypeEnv(entity->GetClass()))
        {
            CallLua((*env)["BeginPlay"], entity, scene, map);
        }

        if (auto* animator = entity->GetAnimator())
        {
            if (auto* env = GetTypeEnv(animator->GetClass()))
            {
                CallLua((*env)["Init"], animator, entity);
            }
        }
    }
}

void LuaRuntime::EndPlay(Scene* scene, Tilemap* map)
{
    if (mGameEnv)
        CallLua((*mGameEnv)["EndPlay"], scene, map);

    if (mSceneEnv)
        CallLua((*mSceneEnv)["EndPlay"], scene, map);

    mSceneEnv.reset();
    mEntityEnvs.clear();
    mScene = nullptr;
    mTilemap = nullptr;
    (*mLuaState)["Scene"] = nullptr;
    (*mLuaState)["Map"]   = nullptr;
}

void LuaRuntime::Tick(double game_time, double dt)
{
    if (mGameEnv)
    {
        TRACE_CALL("Lua::Game::Tick",
                   CallLua((*mGameEnv)["Tick"], game_time, dt));
    }

    if (mScene)
    {
        if (mSceneEnv)
        {
            TRACE_CALL("Lua::Scene::Tick",
                       CallLua((*mSceneEnv)["Tick"], mScene, game_time, dt));
        }

        TRACE_SCOPE("Lua::Entity::Tick");
        for (size_t i = 0; i < mScene->GetNumEntities(); ++i)
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
}
void LuaRuntime::Update(double game_time, double dt)
{
    if (mGameEnv)
    {
        TRACE_CALL("Lua::Game::Update",
                   CallLua((*mGameEnv)["Update"], game_time, dt));
    }

    if (!mScene)
        return;

    if (mSceneEnv)
    {
        TRACE_CALL("Lua::Scene::Update",
                   CallLua((*mSceneEnv)["Update"], mScene, game_time, dt));
    }

    TRACE_SCOPE("Lua::Entity::Update");
    for (size_t i = 0; i < mScene->GetNumEntities(); ++i)
    {
        auto* entity = &mScene->GetEntity(i);
        if (auto* env = GetTypeEnv(entity->GetClass()))
        {
            if (const auto* anim = entity->GetFinishedAnimation())
            {
                CallLua((*env)["OnAnimationFinished"], entity, anim);
            }
            if (entity->TestFlag(Entity::Flags::UpdateEntity))
            {
                CallLua((*env)["Update"], entity, game_time, dt);
            }
        }

        // The animator code is here simply because it's just too convenient to have the
        // animator a) be scriptable b) have access to the same Lua APIs that exist everywhere
        // else too.. This should be pretty flexible in terms of what can be done to the
        // entity when changing animation states. Also possible to play audio effects etc.
        if (!entity->HasAnimator())
            continue;

        const auto& entity_klass = entity->GetClass();
        const auto& animator_klass = entity_klass.GetAnimator(0);

        std::vector<game::Entity::AnimatorAction> actions;
        entity->UpdateAnimator(dt, &actions);
        if (auto* env = GetTypeEnv(animator_klass))
        {
            auto& e = *env;
            auto* animator = entity->GetAnimator();
            for (auto& action : actions)
            {
                if (auto* ptr = std::get_if<Animator::EnterState>(&action))
                    CallLua(e["EnterState"], animator, ptr->state->GetName(), entity);
                else if (auto* ptr = std::get_if<Animator::LeaveState>(&action))
                    CallLua(e["LeaveState"], animator, ptr->state->GetName(), entity);
                else if (auto* ptr = std::get_if<Animator::UpdateState>(&action))
                    CallLua(e["UpdateState"], animator, ptr->state->GetName(), ptr->time, ptr->dt, entity);
                else if (auto* ptr = std::get_if<Animator::EvalTransition>(&action))
                {
                    // if the call to Lua succeeds and the return value is true then update the animator to
                    // take a transition from the current state to the next state.
                    bool return_value_from_lua = false;
                    if (CallLua(&return_value_from_lua, e["EvalTransition"], animator, ptr->from->GetName(), ptr->to->GetName(), entity) && return_value_from_lua)
                        entity->UpdateAnimator(ptr->transition, ptr->to);
                }
                else if (auto* ptr = std::get_if<Animator::StartTransition>(&action))
                    CallLua(e["StartTransition"], animator, ptr->from->GetName(), ptr->to->GetName(), ptr->transition->GetDuration(), entity);
                else if (auto* ptr = std::get_if<Animator::FinishTransition>(&action))
                    CallLua(e["FinishTransition"], animator, ptr->from->GetName(), ptr->to->GetName(),entity);
                else if (auto* ptr = std::get_if<Animator::UpdateTransition>(&action))
                    CallLua(e["UpdateTransition"], animator, ptr->from->GetName(), ptr->to->GetName(), ptr->transition->GetDuration(), ptr->time, ptr->dt, entity);
            }
        }
    }
}

void LuaRuntime::PostUpdate(double game_time)
{
    TRACE_SCOPE("Lua::Entity::PostUpdate");
    for (size_t i=0; i<mScene->GetNumEntities(); ++i)
    {
        auto* entity = &mScene->GetEntity(i);
        if (!entity->TestFlag(Entity::Flags::UpdateEntity))
            continue;
        if (auto* env = GetTypeEnv(entity->GetClass()))
        {
            CallLua((*env)["PostUpdate"], entity, game_time);
        }
    }
}

void LuaRuntime::BeginLoop()
{
    // entities spawned in the scene during calls to update/tick
    // have the spawned flag on. Invoke the BeginPlay callbacks
    // for those entities.
    for (size_t i=0; i<mScene->GetNumEntities(); ++i)
    {
        auto* entity = &mScene->GetEntity(i);
        if (!entity->TestFlag(game::Entity::ControlFlags::Spawned))
            continue;

        if (mSceneEnv)
            CallLua((*mSceneEnv)["SpawnEntity"], mScene, mTilemap, entity);

        if (auto* env = GetTypeEnv(entity->GetClass()))
        {
            CallLua((*env)["BeginPlay"], entity, mScene, mTilemap);
        }

        if (auto* animator = entity->GetAnimator())
        {
            if (auto* env = GetTypeEnv(animator->GetClass()))
            {
                CallLua((*env)["Init"], animator, entity);
            }
        }
    }
}

void LuaRuntime::EndLoop()
{
     // entities killed in the scene during calls to update/tick
     // have the kill flag on. Invoke the EndPlay callbacks for
     // those entities.
     for (size_t i=0; i<mScene->GetNumEntities(); ++i)
     {
         auto* entity = &mScene->GetEntity(i);
         if (!entity->TestFlag(game::Entity::ControlFlags::Killed))
             continue;
         if (mSceneEnv)
             CallLua((*mSceneEnv)["KillEntity"], mScene, mTilemap, entity);

         if (auto* env = GetTypeEnv(entity->GetClass()))
         {
             CallLua((*env)["EndPlay"], entity, mScene, mTilemap);
         }
     }
}

bool LuaRuntime::GetNextAction(Action* out)
{
    if (mActionQueue.empty())
        return false;
    *out = std::move(mActionQueue.front());
    mActionQueue.pop();
    return true;
}

void LuaRuntime::OnContactEvent(const ContactEvent& contact)
{
    const auto* function = contact.type == ContactEvent::Type::BeginContact
            ? "OnBeginContact"
            : "OnEndContact";

    auto* nodeA = contact.nodeA;
    auto* nodeB = contact.nodeB;
    auto* entityA = nodeA->GetEntity();
    auto* entityB = nodeB->GetEntity();

    const auto& klassA = entityA->GetClass();
    const auto& klassB = entityB->GetClass();

    if (mGameEnv)
        CallLua((*mGameEnv)[function], entityA, entityB, nodeA, nodeB);

    if (mSceneEnv)
        CallLua((*mSceneEnv)[function](mScene, entityA, nodeA, entityB, nodeB));

    if (auto* env = GetTypeEnv(klassA))
    {
        CallLua((*env)[function], entityA, nodeA, entityB, nodeB);
    }
    if (auto* env = GetTypeEnv(klassB))
    {
        CallLua((*env)[function], entityB, nodeB, entityA, nodeA);
    }
}
void LuaRuntime::OnGameEvent(const GameEvent& event)
{
    if (mGameEnv)
        CallLua((*mGameEnv)["OnGameEvent"], event);

    if (mScene)
    {
        if (mSceneEnv)
            CallLua((*mSceneEnv)["OnGameEvent"], mScene, event);

        for (size_t i = 0; i < mScene->GetNumEntities(); ++i)
        {
            auto* entity = &mScene->GetEntity(i);
            if (auto* env = GetTypeEnv(entity->GetClass()))
            {
                CallLua((*env)["OnGameEvent"], entity, event);
            }
        }
    }
}

void LuaRuntime::OnAudioEvent(const AudioEvent& event)
{
    if (mGameEnv)
        CallLua((*mGameEnv)["OnAudioEvent"], event);
}

void LuaRuntime::OnSceneEvent(const game::Scene::Event& event)
{
    if (const auto* ptr = std::get_if<game::Scene::EntityTimerEvent>(&event))
    {
        auto* entity = ptr->entity;
        if (mSceneEnv)
            CallLua((*mSceneEnv)["OnEntityTimer"], mScene, entity, ptr->event.name, ptr->event.jitter);

        if (auto* env = GetTypeEnv(entity->GetClass()))
        {
            CallLua((*env)["OnTimer"], entity, ptr->event.name, ptr->event.jitter);
        }
    }
    else if (const auto* ptr = std::get_if<game::Scene::EntityEventPostedEvent>(&event))
    {
        auto* entity = ptr->entity;
        if (mSceneEnv)
            CallLua((*mSceneEnv)["OnEntityEvent"], mScene, entity, ptr->event);

        if (auto* env = GetTypeEnv(entity->GetClass()))
        {
            CallLua((*env)["OnEvent"], entity, ptr->event);
        }
    }
}

void LuaRuntime::OnKeyDown(const wdk::WindowEventKeyDown& key)
{
    DispatchKeyboardEvent("OnKeyDown", key);
}
void LuaRuntime::OnKeyUp(const wdk::WindowEventKeyUp& key)
{
    DispatchKeyboardEvent("OnKeyUp", key);
}
void LuaRuntime::OnChar(const wdk::WindowEventChar& text)
{

}
void LuaRuntime::OnMouseMove(const MouseEvent& mouse)
{
    DispatchMouseEvent("OnMouseMove", mouse);
}
void LuaRuntime::OnMousePress(const MouseEvent& mouse)
{
    DispatchMouseEvent("OnMousePress", mouse);
}
void LuaRuntime::OnMouseRelease(const MouseEvent& mouse)
{
    DispatchMouseEvent("OnMouseRelease", mouse);
}

void LuaRuntime::OnUIOpen(uik::Window* ui)
{
    if (mGameEnv)
        CallLua((*mGameEnv)["OnUIOpen"], ui);

    if (mScene &&  mSceneEnv)
        CallLua((*mSceneEnv)["OnUIOpen"], mScene, ui);

    if (auto* env = GetTypeEnv(*ui))
    {
        CallLua((*env)["OnUIOpen"], ui);
    }
}
void LuaRuntime::OnUIClose(uik::Window* ui, int result)
{
    if (mGameEnv)
        CallLua((*mGameEnv)["OnUIClose"], ui, result);

    if (mScene && mSceneEnv)
        CallLua((*mSceneEnv)["OnUIClose"], mScene, ui, result);

    if (auto* env = GetTypeEnv(*ui))
    {
        CallLua((*env)["OnUIClose"], ui, result);
    }
}
void LuaRuntime::OnUIAction(uik::Window* ui, const uik::Window::WidgetAction& action)
{
    if (mGameEnv)
        CallLua((*mGameEnv)["OnUIAction"], ui, action);

    if (mScene && mSceneEnv)
        CallLua((*mSceneEnv)["OnUIAction"], mScene, ui, action);

    if (auto* env = GetTypeEnv(*ui))
    {
        CallLua((*env)["OnUIAction"], ui, action);
    }
}

template<typename KeyEvent>
void LuaRuntime::DispatchKeyboardEvent(const std::string& method, const KeyEvent& key)
{
    if (mGameEnv)
        CallLua((*mGameEnv)[method],
                static_cast<int>(key.symbol),
                static_cast<int>(key.modifiers.value()));

    if (mWindow && mWindow->TestFlag(uik::Window::Flags::WantsKeyEvents))
    {
        if (auto* env = GetTypeEnv(*mWindow))
        {
            CallLua((*env)[method], mWindow,
                    static_cast<int>(key.symbol),
                    static_cast<int>(key.modifiers.value()));
        }
    }

    if (mScene)
    {
        if (mSceneEnv)
            CallLua((*mSceneEnv)[method], mScene,
                    static_cast<int>(key.symbol),
                    static_cast<int>(key.modifiers.value()));

        for (size_t i = 0; i < mScene->GetNumEntities(); ++i)
        {
            auto* entity = &mScene->GetEntity(i);
            if (!entity->TestFlag(Entity::Flags::WantsKeyEvents))
                continue;
            if (auto* env = GetTypeEnv(entity->GetClass()))
            {
                CallLua((*env)[method], entity,
                        static_cast<int>(key.symbol),
                        static_cast<int>(key.modifiers.value()));
            }
        }
    }
}

void LuaRuntime::DispatchMouseEvent(const std::string& method, const MouseEvent& mouse)
{
    if (mGameEnv)
        CallLua((*mGameEnv)[method], mouse);

    if (mWindow && mWindow->TestFlag(uik::Window::Flags::WantsMouseEvents))
    {
        if (auto* env = GetTypeEnv(*mWindow))
        {
            CallLua((*env)[method], mWindow, mouse);
        }
    }

    if (mScene)
    {
        if (mSceneEnv)
            CallLua((*mSceneEnv)[method], mScene, mouse);

        for (size_t i = 0; i < mScene->GetNumEntities(); ++i)
        {
            auto* entity = &mScene->GetEntity(i);
            if (!entity->TestFlag(Entity::Flags::WantsMouseEvents))
                continue;
            if (auto* env = GetTypeEnv(entity->GetClass()))
            {
                CallLua((*env)[method], entity, mouse);
            }
        }
    }
}

sol::object LuaRuntime::CallCrossEnvMethod(sol::object object, const std::string& method, sol::variadic_args args)
{
    sol::environment* env = nullptr;
    std::string target_name;
    std::string target_type;
    if (object.is<game::Scene*>())
    {
        auto* scene = object.as<game::Scene*>();
        env = mSceneEnv.get();
        target_name = scene->GetClassName();
        target_type = "Scene";
    }
    else if (object.is<game::Entity*>())
    {
        auto* entity = object.as<game::Entity*>();
        env = GetTypeEnv(entity->GetClass());
        target_name = entity->GetClassName();
        target_type = "Entity";
    }
    else if (object.is<uik::Window*>())
    {
        auto* window = object.as<uik::Window*>();
        env = GetTypeEnv(*window);
        target_name = window->GetName();
        target_type = "Window";
    }
    else throw GameError("Unsupported object type CallMethod method call. Only entity, scene or window object is supported.");

    if (env == nullptr)
        throw GameError(base::FormatString("CallMethod method call target '%1/%2' object doesn't have a Lua environment.", target_type, target_name));

    sol::protected_function func = (*env)[method];
    if (!func.valid())
        throw GameError(base::FormatString("No such CallMethod method '%1' was found. ", method));

    const auto& result = func(object, args);
    if (!result.valid())
    {
        const sol::error err = result;
        throw GameError(base::FormatString("CallMethod '%1' failed. %2", method, err.what()));
    }
    // todo: how to return any number of return values ?
    if (result.return_count() == 1)
    {
        sol::object ret = result;
        return ret;
    }
    return sol::make_object(*mLuaState, sol::nil);
}

sol::environment* LuaRuntime::GetTypeEnv(const AnimatorClass& klass)
{
    if (!klass.HasScriptId())
        return nullptr;
    const auto& klassId = klass.GetId();
    auto it = mAnimatorEnvs.find(klassId);
    if (it != mAnimatorEnvs.end())
        return it->second.get();

    const auto& script = klass.GetScriptId();
    const auto& script_buff = mDataLoader->LoadEngineDataId(script);
    if (!script_buff)
    {
        ERROR("Failed to load entity animator script file. [class='%1', script='%2']", klass.GetName(), script);
        return nullptr;
    }
    auto script_env = std::make_shared<sol::environment>(*mLuaState, sol::create, mLuaState->globals());
    // store the script ID with the script object/environment
    // this is used when for example checking access to a scripting variable.
    // i.e. we check that the entity's script ID is the same as the script ID
    // stored the script environment.
    (*script_env)["__script_id__"] = script;

    const auto& script_file = script_buff->GetSourceName();
    const auto& script_view = script_buff->GetStringView();
    const auto& chunk_name  = script_buff->GetName();
    const auto& result = mLuaState->script(script_view, *script_env, chunk_name);
    if (!result.valid())
    {
        const sol::error err = result;
        ERROR("Lua script error. [file='%1', error='%2']", script_file, err.what());
        // throwing here is just too convenient way to propagate the Lua
        // specific error message up the stack without cluttering the interface,
        // and when running the engine inside the editor we really want to
        // have this lua error propagated all the way to the UI
        throw std::runtime_error(err.what());
    }
    DEBUG("Entity animator script loaded. [class='%1', file='%2']", klass.GetName(), script_file);
    it = mAnimatorEnvs.insert({klassId, script_env}).first;
    return it->second.get();
}

sol::environment* LuaRuntime::GetTypeEnv(const EntityClass& klass)
{
    if (!klass.HasScriptFile())
        return nullptr;
    const auto& klassId = klass.GetId();
    auto it = mEntityEnvs.find(klassId);
    if (it != mEntityEnvs.end())
        return it->second.get();

    const auto& script = klass.GetScriptFileId();
    const auto& script_buff = mDataLoader->LoadEngineDataId(script);
    if (!script_buff)
    {
        ERROR("Failed to load entity class script file. [class='%1', script='%2']", klass.GetName(), script);
        return nullptr;
    }
    auto script_env = std::make_shared<sol::environment>(*mLuaState, sol::create, mLuaState->globals());
    // store the script ID with the script object/environment
    // this is used when for example checking access to a scripting variable.
    // i.e. we check that the entity's script ID is the same as the script ID
    // stored the script environment.
    (*script_env)["__script_id__"] = script;

    const auto& script_file = script_buff->GetSourceName();
    const auto& script_view = script_buff->GetStringView();
    const auto& chunk_name  = script_buff->GetName();
    const auto& result = mLuaState->script(script_view, *script_env, chunk_name);
    if (!result.valid())
    {
        const sol::error err = result;
        ERROR("Lua script error. [file='%1', error='%2']", script_file, err.what());
        // throwing here is just too convenient way to propagate the Lua
        // specific error message up the stack without cluttering the interface,
        // and when running the engine inside the editor we really want to
        // have this lua error propagated all the way to the UI
        throw std::runtime_error(err.what());
    }
    DEBUG("Entity class script loaded. [class='%1', file='%2']", klass.GetName(), script_file);
    it = mEntityEnvs.insert({klassId, script_env}).first;
    return it->second.get();
}

sol::environment* LuaRuntime::GetTypeEnv(const uik::Window& window)
{
    if (!window.HasScriptFile())
        return nullptr;
    const auto& id = window.GetId();
    auto it = mWindowEnvs.find(id);
    if (it != mWindowEnvs.end())
        return it->second.get();

    const auto& script = window.GetScriptFile();
    const auto& script_buff = mDataLoader->LoadEngineDataId(script);
    if (!script_buff)
    {
        ERROR("Failed to load UiKit window script file. [class='%1', script='%2']", window.GetName(), script);
        return nullptr;
    }
    const auto& script_file = script_buff->GetSourceName();
    const auto& script_view = script_buff->GetStringView();
    const auto& chunk_name  = script_buff->GetName();
    auto script_env = std::make_unique<sol::environment>(*mLuaState, sol::create, mLuaState->globals());
    (*script_env)["__script_id__"] = script;
    const auto& result = mLuaState->script(script_view, *script_env, chunk_name);
    if (!result.valid())
    {
        const sol::error err = result;
        ERROR("Lua script error. [file='%1', error='%2']", script_file, err.what());
        // throwing here is just too convenient way to propagate the Lua
        // specific error message up the stack without cluttering the interface,
        // and when running the engine inside the editor we really want to
        // have this lua error propagated all the way to the UI
        throw std::runtime_error(err.what());
    }
    DEBUG("UiKit window script loaded. [window='%1', file='%2']", window.GetName(), script_file);
    it = mWindowEnvs.insert({id, std::move(script_env)}).first;
    return it->second.get();
}

} // namespace
