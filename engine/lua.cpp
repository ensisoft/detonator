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
#include <bitset>

#include "base/assert.h"
#include "base/logging.h"
#include "base/types.h"
#include "base/color4f.h"
#include "base/format.h"
#include "base/trace.h"
#include "data/reader.h"
#include "data/writer.h"
#include "data/json.h"
#include "audio/graph.h"
#include "game/entity.h"
#include "game/util.h"
#include "game/scene.h"
#include "game/transform.h"
#include "engine/game.h"
#include "engine/audio.h"
#include "engine/classlib.h"
#include "engine/physics.h"
#include "engine/lua.h"
#include "engine/loader.h"
#include "engine/event.h"
#include "engine/state.h"
#include "uikit/window.h"
#include "uikit/widget.h"
#include "wdk/keys.h"
#include "wdk/system.h"

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

class GameError : public std::exception {
public:
    GameError(std::string&& message)
      : mMessage(std::move(message))
    {}
    virtual const char* what() noexcept
    { return mMessage.c_str(); }
private:
    std::string mMessage;
};

namespace {
template<typename... Args>
void CallLua(const sol::protected_function& func, const Args&... args)
{
    if (!func.valid())
        return;
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
        return;
    const sol::error err = result;

    // todo: Lua code has failed. This information should likely be
    // propagated in a logical Lua error object rather than by
    // throwing an exception.
    throw std::runtime_error(err.what());
}

template<typename LuaGame>
void BindEngine(sol::usertype<LuaGame>& engine, LuaGame& self)
{
    using namespace engine;
    engine["Play"] = sol::overload(
        [](LuaGame& self, ClassHandle<SceneClass> klass) {
            if (!klass)
                throw GameError("Nil scene class");
            PlayAction play;
            play.scene = game::CreateSceneInstance(klass);
            auto* ret = play.scene.get();
            self.PushAction(std::move(play));
            return ret;
        },
        [](LuaGame& self, std::string name) {
            auto klass = self.GetClassLib()->FindSceneClassByName(name);
            if (!klass)
                throw GameError("No such scene class: " + name);
            PlayAction play;
            play.scene = game::CreateSceneInstance(klass);
            auto* ret = play.scene.get();
            self.PushAction(std::move(play));
            return ret;
        });

    engine["Suspend"] = [](LuaGame& self) {
        SuspendAction suspend;
        self.PushAction(suspend);
    };
    engine["EndPlay"] = [](LuaGame& self) {
        EndPlayAction end;
        self.PushAction(end);
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
    engine["GrabMouse"] = [](LuaGame& self, bool grab) {
        GrabMouseAction mickey;
        mickey.grab = grab;
        self.PushAction(mickey);
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
    engine["DebugDrawLine"] = sol::overload(
        [](LuaGame& self, const glm::vec2&  a, const glm::vec2& b, const base::Color4f& color, float width) {
            DebugDrawLine draw;
            draw.a = FPoint(a.x, a.y);
            draw.b = FPoint(b.x, b.y);
            draw.color = color;
            draw.width = width;
            self.PushAction(std::move(draw));
        },
        [](LuaGame& self, const base::FPoint& a, const FPoint& b, const base::Color4f& color, float width) {
            DebugDrawLine draw;
            draw.a = a;
            draw.b = b;
            draw.color = color;
            draw.width = width;
            self.PushAction(std::move(draw));
        },
        [](LuaGame& self, float x0, float y0, float x1, float y1, const base::Color4f& color, float width) {
            DebugDrawLine draw;
            draw.a = base::FPoint(x0, y0);
            draw.b = base::FPoint(x1, y1);
            draw.color = color;
            draw.width = width;
            self.PushAction(std::move(draw));
        });
    engine["DebugClear"] = [](LuaGame& self) {
        DebugClearAction action;
        self.PushAction(action);
    };
    engine["OpenUI"] = sol::overload(
        [](LuaGame& self, ClassHandle<uik::Window> model) {
            if (!model)
                throw GameError("Nil UI window object.");
            // there's no "class" object for the UI system so we're just
            // going to create a mutable copy and put that on the UI stack.
            OpenUIAction action;
            action.ui = std::make_shared<uik::Window>(*model);
            self.PushAction(action);
            return action.ui.get();
        },
        [](LuaGame& self, std::string name) {
            auto handle = self.GetClassLib()->FindUIByName(name);
            if (!handle)
                throw GameError("No such UI: " + name);
            OpenUIAction action;
            action.ui = std::make_shared<uik::Window>(*handle);
            self.PushAction(action);
            return action.ui.get();
        });
    engine["CloseUI"] = [](LuaGame& self, int result) {
        CloseUIAction action;
        action.result = result;
        self.PushAction(std::move(action));
    };
    engine["PostEvent"] = [](LuaGame& self, const GameEvent& event) {
        PostEventAction action;
        action.event = event;
        self.PushAction(std::move(action));
    };
    engine["ShowDeveloperUI"] = [](LuaGame& self, bool show) {
        ShowDeveloperUIAction action;
        action.show = show;
        self.PushAction(action);
    };
}
template<typename Type>
bool TestFlag(const Type& object, const std::string& name)
{
    using Flags = typename Type::Flags;
    const auto enum_val = magic_enum::enum_cast<Flags>(name);
    if (!enum_val.has_value())
        throw GameError("No such flag: " + name);
    return object.TestFlag(enum_val.value());
}
template<typename Type>
void SetFlag(Type& object, const std::string& name, bool on_off)
{
    using Flags = typename Type::Flags;
    const auto enum_val = magic_enum::enum_cast<Flags>(name);
    if (!enum_val.has_value())
        throw GameError("No such flag: " + name);
    object.SetFlag(enum_val.value(), on_off);
}

sol::object ObjectFromScriptVarValue(const ScriptVar& var, sol::this_state state)
{
    sol::state_view lua(state);
    const auto type = var.GetType();
    if (type == ScriptVar::Type::Boolean)
        return sol::make_object(lua, var.GetValue<bool>());
    else if (type == ScriptVar::Type::Float)
        return sol::make_object(lua, var.GetValue<float>());
    else if (type == ScriptVar::Type::String)
        return sol::make_object(lua, var.GetValue<std::string>());
    else if (type == ScriptVar::Type::Integer)
        return sol::make_object(lua, var.GetValue<int>());
    else if (type == ScriptVar::Type::Vec2)
        return sol::make_object(lua, var.GetValue<glm::vec2>());
    else BUG("Unhandled ScriptVar type.");
}

template<typename Type>
sol::object GetScriptVar(const Type& object, const char* key, sol::this_state state)
{
    using namespace engine;
    sol::state_view lua(state);
    const ScriptVar* var = object.FindScriptVarByName(key);
    if (!var)
        throw GameError(base::FormatString("No such variable: '%1'", key));
    return ObjectFromScriptVarValue(*var, state);
}
template<typename Type>
void SetScriptVar(Type& object, const char* key, sol::object value)
{
    using namespace engine;
    const ScriptVar* var = object.FindScriptVarByName(key);
    if (var == nullptr)
        throw GameError(base::FormatString("No such variable: '%1'", key));
    else if (var->IsReadOnly())
        throw GameError(base::FormatString("Trying to write to a read only variable: '%1'", key));

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
    else throw GameError(base::FormatString("Variable type mismatch. '%1' expects: '%2'", key, var->GetType()));
}

void SetKvValue(engine::KeyValueStore& kv, const char* key, sol::object value)
{
    if (value.is<bool>())
        kv.SetValue(key, value.as<bool>());
    else if (value.is<int>())
        kv.SetValue(key, value.as<int>());
    else if (value.is<float>())
        kv.SetValue(key, value.as<float>());
    else if (value.is<std::string>())
        kv.SetValue(key, value.as<std::string>());
    else if (value.is<glm::vec2>())
        kv.SetValue(key, value.as<glm::vec2>());
    else if (value.is<glm::vec3>())
        kv.SetValue(key, value.as<glm::vec3>());
    else if (value.is<glm::vec4>())
        kv.SetValue(key, value.as<glm::vec4>());
    else if (value.is<base::Color4f>())
        kv.SetValue(key, value.as<base::Color4f>());
    else if (value.is<base::FSize>())
        kv.SetValue(key, value.as<base::FSize>());
    else if (value.is<base::FRect>())
        kv.SetValue(key, value.as<base::FRect>());
    else if (value.is<base::FPoint>())
        kv.SetValue(key, value.as<base::FPoint>());
    else throw GameError("Unsupported key value store type.");
}

// WAR. G++ 10.2.0 has internal segmentation fault when using the Get/SetScriptVar helpers
// directly in the call to create new usertype. adding these specializations as a workaround.
template sol::object GetScriptVar<game::Scene>(const game::Scene&, const char*, sol::this_state);
template sol::object GetScriptVar<game::Entity>(const game::Entity&, const char*, sol::this_state);
template void SetScriptVar<game::Scene>(game::Scene&, const char* key, sol::object);
template void SetScriptVar<game::Entity>(game::Entity&, const char* key, sol::object);

// shim to help with uik::WidgetCast overload resolution.
template<typename Widget> inline
Widget* WidgetCast(uik::Widget* widget)
{ return uik::WidgetCast<Widget>(widget); }

sol::object WidgetObjectCast(sol::this_state state, uik::Widget* widget, const std::string& type_string)
{
    sol::state_view lua(state);
    const auto type_value = magic_enum::enum_cast<uik::Widget::Type>(type_string);
    if (!type_value.has_value())
        throw GameError("No such widget type: " + type_string);

    const auto type = type_value.value();
    if (type == uik::Widget::Type::Form)
        return sol::make_object(lua, uik::WidgetCast<uik::Form>(widget));
    else if (type == uik::Widget::Type::Label)
        return sol::make_object(lua, uik::WidgetCast<uik::Label>(widget));
    else if (type == uik::Widget::Type::SpinBox)
        return sol::make_object(lua, uik::WidgetCast<uik::SpinBox>(widget));
    else if (type == uik::Widget::Type::ProgressBar)
        return sol::make_object(lua, uik::WidgetCast<uik::ProgressBar>(widget));
    else if (type == uik::Widget::Type::Slider)
        return sol::make_object(lua, uik::WidgetCast<uik::Slider>(widget));
    else if (type == uik::Widget::Type::GroupBox)
        return sol::make_object(lua, uik::WidgetCast<uik::GroupBox>(widget));
    else if (type == uik::Widget::Type::PushButton)
        return sol::make_object(lua, uik::WidgetCast<uik::PushButton>(widget));
    else if (type == uik::Widget::Type::CheckBox)
        return sol::make_object(lua, uik::WidgetCast<uik::CheckBox>(widget));
    else BUG("Unhandled widget type cast.");
    return sol::make_object(lua, sol::nil);
}

template<typename Widget>
void BindWidgetInterface(sol::usertype<Widget>& widget)
{
    widget["GetId"]          = &Widget::GetId;
    widget["GetName"]        = &Widget::GetName;
    widget["GetHash"]        = &Widget::GetHash;
    widget["GetSize"]        = &Widget::GetSize;
    widget["GetPosition"]    = &Widget::GetPosition;
    widget["GetType"]        = [](const Widget* widget) { return base::ToString(widget->GetType()); };
    widget["SetName"]        = &Widget::SetName;
    widget["TestFlag"]       = &TestFlag<Widget>;
    widget["SetFlag"]        = &SetFlag<Widget>;
    widget["IsEnabled"]      = &Widget::IsEnabled;
    widget["IsVisible"]      = &Widget::IsVisible;
    widget["Grow"]           = &Widget::Grow;
    widget["Translate"]      = &Widget::Translate;
    widget["SetVisible"]     = [](Widget& widget, bool on_off) {
        widget.SetFlag(Widget::Flags::VisibleInGame, on_off);
    };
    widget["Enable"] = [](Widget& widget, bool on_off) {
        widget.SetFlag(Widget::Flags::Enabled, on_off);
    };
    widget["SetSize"]        = sol::overload(
        [](Widget& widget, const uik::FSize& size)  {
            widget.SetSize(size);
        },
        [](Widget& widget, float width, float height) {
            widget.SetSize(width, height);
        });
    widget["SetPosition"] = sol::overload(
        [](Widget& widget, const uik::FPoint& point) {
            widget.SetPosition(point);
        },
        [](Widget& widget, float x, float y) {
            widget.SetPosition(x, y);
        });
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

template<typename Writer>
void BindDataWriterInterface(sol::usertype<Writer>& writer)
{
    writer["Write"] = sol::overload(
            (void(Writer::*)(const char*, int))&Writer::Write,
            (void(Writer::*)(const char*, float))&Writer::Write,
            (void(Writer::*)(const char*, bool))&Writer::Write,
            (void(Writer::*)(const char*, const std::string&))&Writer::Write,
            (void(Writer::*)(const char*, const glm::vec2&))&Writer::Write,
            (void(Writer::*)(const char*, const glm::vec3&))&Writer::Write,
            (void(Writer::*)(const char*, const glm::vec4&))&Writer::Write,
            (void(Writer::*)(const char*, const base::FRect&))&Writer::Write,
            (void(Writer::*)(const char*, const base::FPoint&))&Writer::Write,
            (void(Writer::*)(const char*, const base::FSize&))&Writer::Write,
            (void(Writer::*)(const char*, const base::Color4f&))&Writer::Write);
    writer["HasValue"]      = &Writer::HasValue;
    writer["NewWriteChunk"] = &Writer::NewWriteChunk;
    writer["AppendChunk"]   = (void(Writer::*)(const char*, const data::Writer&))&Writer::AppendChunk;
}
template<typename Reader>
void BindDataReaderInterface(sol::usertype<Reader>& reader)
{
    reader["ReadFloat"]   = (std::tuple<bool, float>(Reader::*)(const char*)const)&Reader::Read;
    reader["ReadInt"]     = (std::tuple<bool, int>(Reader::*)(const char*)const)&Reader::Read;
    reader["ReadBool"]    = (std::tuple<bool, bool>(Reader::*)(const char*)const)&Reader::Read;
    reader["ReadString"]  = (std::tuple<bool, std::string>(Reader::*)(const char*)const)&Reader::Read;
    reader["ReadVec2"]    = (std::tuple<bool, glm::vec2>(Reader::*)(const char*)const)&Reader::Read;
    reader["ReadVec3"]    = (std::tuple<bool, glm::vec3>(Reader::*)(const char*)const)&Reader::Read;
    reader["ReadVec4"]    = (std::tuple<bool, glm::vec4>(Reader::*)(const char*)const)&Reader::Read;
    reader["ReadFRect"]   = (std::tuple<bool, base::FRect>(Reader::*)(const char*)const)&Reader::Read;
    reader["ReadFPoint"]  = (std::tuple<bool, base::FPoint>(Reader::*)(const char*)const)&Reader::Read;
    reader["ReadFSize"]   = (std::tuple<bool, base::FSize>(Reader::*)(const char*)const)&Reader::Read;
    reader["ReadColor4f"] = (std::tuple<bool, base::Color4f>(Reader::*)(const char*)const)&Reader::Read;

    reader["Read"] = sol::overload(
            (std::tuple<bool, float>(Reader::*)(const char*, const float&)const)&Reader::Read,
            (std::tuple<bool, int>(Reader::*)(const char*, const int&)const)&Reader::Read,
            (std::tuple<bool, bool>(Reader::*)(const char*, const bool& )const)&Reader::Read,
            (std::tuple<bool, std::string>(Reader::*)(const char*, const std::string&)const)&Reader::Read,
            (std::tuple<bool, glm::vec2>(Reader::*)(const char*, const glm::vec2&)const)&Reader::Read,
            (std::tuple<bool, glm::vec3>(Reader::*)(const char*, const glm::vec3&)const)&Reader::Read,
            (std::tuple<bool, glm::vec4>(Reader::*)(const char*, const glm::vec4&)const)&Reader::Read,
            (std::tuple<bool, base::FRect>(Reader::*)(const char*, const base::FRect&)const)&Reader::Read,
            (std::tuple<bool, base::FPoint>(Reader::*)(const char*, const base::FPoint&)const)&Reader::Read,
            (std::tuple<bool, base::FPoint>(Reader::*)(const char*, const base::FPoint&)const)&Reader::Read,
            (std::tuple<bool, base::Color4f>(Reader::*)(const char*, const base::Color4f&)const)&Reader::Read);
    reader["HasValue"]     = &Reader::HasValue;
    reader["HasChunk"]     = &Reader::HasChunk;
    reader["IsEmpty"]      = &Reader::IsEmpty;
    reader["GetNumChunks"] = &Reader::GetNumChunks;
    reader["GetReadChunk"] = [](const Reader& reader, const char* name, unsigned index) {
        const auto chunks = reader.GetNumChunks(name);
        if (index >= chunks)
            throw GameError("data reader chunk index out of bounds.");
        return reader.GetReadChunk(name, index);
    };
}

template<typename Vector>
void BindGlmVectorOp(sol::usertype<Vector>& vec)
{
    vec.set_function(sol::meta_function::addition, [](const Vector& a, const Vector& b) {
        return a + b;
    });
    vec.set_function(sol::meta_function::subtraction, [](const Vector& a, const Vector& b) {
        return a - b;
    });
    vec.set_function(sol::meta_function::multiplication, sol::overload(
        [](float scalar, const Vector& vector) {
            return vector * scalar;
        },
        [](const Vector& vector, float scalar) {
            return vector * scalar;
        },
        [](const Vector& a, const Vector& b) {
            return a * b;
        }
    ));
    vec.set_function(sol::meta_function::division, sol::overload(
        [](float scalar, const Vector& vector) {
            return scalar / vector;
        },
        [](const Vector& vector, float scalar) {
            return vector / scalar;
        },
        [](const Vector& a, const Vector& b) {
            return a / b;
        }
    ));
    vec.set_function(sol::meta_function::to_string, [](const Vector& vector) {
        return base::ToString(vector);
    });
    vec["length"]    = [](const Vector& vec) { return glm::length(vec); };
    vec["normalize"] = [](const Vector& vec) { return glm::normalize(vec); };
}

template<typename T>
class ResultSet
{
public:
    using Set = std::set<T>;
    ResultSet(Set&& result)
      : mResult(std::move(result))
    { mBegin = mResult.begin(); }
    ResultSet(const Set& result)
      : mResult(result)
    { mBegin = mResult.begin(); }
    ResultSet()
    { mBegin = mResult.begin(); }
    T GetCurrent() const
    {
        if (mBegin == mResult.end())
            throw GameError("ResultSet iteration error.");
        return *mBegin;
    }
    void BeginIteration()
    { mBegin = mResult.begin(); }
    bool HasNext() const
    { return mBegin != mResult.end(); }
    bool IsEmpty() const
    { return mResult.empty(); }
    bool Next()
    {
        ++mBegin;
        return mBegin != mResult.end();
    }
private:
    Set mResult;
    typename Set::iterator mBegin;
};

template<typename T>
class ResultVector
{
public:
    using Vector = std::vector<T>;
    ResultVector(Vector&& result)
      : mResult(std::move(result))
    { mBegin = mResult.begin(); }
    ResultVector(const Vector& result)
      : mResult(result)
    { mBegin = mResult.begin(); }
    ResultVector()
    { mBegin = mResult.begin(); }
    void BeginIteration()
    { mBegin = mResult.begin(); }
    bool HasNext() const
    { return mBegin != mResult.end(); }
    bool IsEmpty() const
    { return mResult.empty(); }
    bool Next()
    {  return ++mBegin != mResult.end(); }
    const T& GetCurrent() const
    {
        if (mBegin == mResult.end())
            throw GameError("ResultSet iteration error.");
        return *mBegin;
    }
    const T& GetAt(size_t index) const
    {
        if (index >= mResult.size())
            throw GameError("ResultVector index out of bounds.");
        return mResult[index];
    }
    size_t GetSize() const
    { return mResult.size(); }
private:
    Vector mResult;
    typename Vector::iterator mBegin;
};

} // namespace

namespace engine
{

LuaGame::LuaGame(const std::string& lua_path,
                 const std::string& game_script,
                 const std::string& game_home,
                 const std::string& game_name)
    : mLuaPath(lua_path)
    , mGameScript(game_script)
{
    mLuaState = std::make_unique<sol::state>();
    // todo: should this specify which libraries to load?
    mLuaState->open_libraries();

    mLuaState->clear_package_loaders();

    mLuaState->add_package_loader([this](std::string module) {
        ASSERT(mLoader);
        if (!base::EndsWith(module, ".lua"))
            module += ".lua";

        DEBUG("Loading Lua module. [module=%1]", module);
        const auto& file = base::JoinPath(mLuaPath, module);
        const auto& buff = mLoader->LoadGameDataFromFile(file);
        if (!buff)
            throw std::runtime_error("can't find lua module: " + module);
        auto ret = mLuaState->load_buffer((const char*)buff->GetData(), buff->GetSize());
        if (!ret.valid())
        {
            sol::error err = ret;
            throw std::runtime_error(err.what());
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

    // bind engine interface.
    auto table  = (*mLuaState)["game"].get_or_create<sol::table>();
    table["home"] = game_home;
    table["name"] = game_name;
    table["OS"]   = OS_NAME;
    auto engine = table.new_usertype<LuaGame>("Engine");
    BindEngine(engine, *this);
    engine["SetViewport"] = sol::overload(
        [](LuaGame& self, const FRect& view) {
            self.mView = view;
        },
        [](LuaGame& self, float x, float y, float width, float height) {
            self.mView = base::FRect(x, y, width, height);
        },
        [](LuaGame& self, float width, float height) {
            self.mView = base::FRect(0.0f,  0.0f, width, height);
        });
    engine["GetTopUI"] = [](LuaGame& self, sol::this_state state) {
        sol::state_view lua(state);
        if (self.mWindowStack.empty())
            return sol::make_object(lua, sol::nil);
        return sol::make_object(lua, self.mWindowStack.top());
    };
}

LuaGame::~LuaGame() = default;

void LuaGame::SetStateStore(KeyValueStore* store)
{
    mStateStore = store;
}

void LuaGame::SetPhysicsEngine(const PhysicsEngine* engine)
{
    mPhysicsEngine = engine;
}
void LuaGame::SetAudioEngine(const AudioEngine* engine)
{
    mAudioEngine = engine;
}
void LuaGame::SetDataLoader(const Loader* loader)
{
    mLoader = loader;
}
void LuaGame::SetClassLibrary(const ClassLibrary* classlib)
{
    mClasslib = classlib;
}

bool LuaGame::LoadGame()
{
    const auto& main_game_script = base::JoinPath(mLuaPath, mGameScript);
    DEBUG("Loading main game script. [file='%1']", main_game_script);

    const auto buffer = mLoader->LoadGameDataFromFile(main_game_script);
    if (!buffer)
    {
        ERROR("Failed to load main game script data. [file='%1']", main_game_script);
        return false;
    }
    const auto& view = sol::string_view((const char*)buffer->GetData(), buffer->GetSize());
    auto result = mLuaState->script(view);
    if (!result.valid())
    {
        const sol::error err = result;
        ERROR("Lua script error. [file='%1', error='%2']", main_game_script, err.what());
        // throwing here is just too convenient way to propagate the Lua
        // specific error message up the stack without cluttering the interface,
        // and when running the engine inside the editor we really want to
        // have this lua error propagated all the way to the UI
        throw std::runtime_error(err.what());
    }

    (*mLuaState)["Audio"]    = mAudioEngine;
    (*mLuaState)["Physics"]  = mPhysicsEngine;
    (*mLuaState)["ClassLib"] = mClasslib;
    (*mLuaState)["State"]    = mStateStore;
    (*mLuaState)["Game"]     = this;
    CallLua((*mLuaState)["LoadGame"]);
    // tood: return value.
    return true;
}

void LuaGame::StartGame()
{
    CallLua((*mLuaState)["StartGame"]);
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

void LuaGame::StopGame()
{
    CallLua((*mLuaState)["StopGame"]);
}

void LuaGame::SaveGame()
{
    CallLua((*mLuaState)["SaveGame"]);
}

bool LuaGame::GetNextAction(Action* out)
{
    if (mActionQueue.empty())
        return false;
    *out = std::move(mActionQueue.front());
    mActionQueue.pop();
    return true;
}

FRect LuaGame::GetViewport() const
{ return mView; }

void LuaGame::OnUIOpen(uik::Window* ui)
{
    CallLua((*mLuaState)["OnUIOpen"], ui);
    mWindowStack.push(ui);
}
void LuaGame::OnUIClose(uik::Window* ui, int result)
{
    CallLua((*mLuaState)["OnUIClose"], ui, result);
    mWindowStack.pop();
}

void LuaGame::OnUIAction(uik::Window* ui, const uik::Window::WidgetAction& action)
{
    CallLua((*mLuaState)["OnUIAction"], ui, action);
}

void LuaGame::OnContactEvent(const ContactEvent& contact)
{
    auto* nodeA = contact.nodeA;
    auto* nodeB = contact.nodeB;
    auto* entityA = nodeA->GetEntity();
    auto* entityB = nodeB->GetEntity();

    if (contact.type == ContactEvent::Type::BeginContact)
    {
        CallLua((*mLuaState)["OnBeginContact"], entityA, entityB, nodeA, nodeB);
    }
    else if (contact.type == ContactEvent::Type::EndContact)
    {
        CallLua((*mLuaState)["OnEndContact"], entityA, entityB, nodeA, nodeB);
    }
}

void LuaGame::OnAudioEvent(const AudioEvent& event)
{
    CallLua((*mLuaState)["OnAudioEvent"], event);
}

void LuaGame::OnGameEvent(const GameEvent& event)
{
    CallLua((*mLuaState)["OnGameEvent"], event);
}

void LuaGame::OnKeyDown(const wdk::WindowEventKeyDown& key)
{
    CallLua((*mLuaState)["OnKeyDown"],
            static_cast<int>(key.symbol),
            static_cast<int>(key.modifiers.value()));
}
void LuaGame::OnKeyUp(const wdk::WindowEventKeyUp& key)
{
    CallLua((*mLuaState)["OnKeyUp"],
            static_cast<int>(key.symbol),
            static_cast<int>(key.modifiers.value()));
}
void LuaGame::OnChar(const wdk::WindowEventChar& text)
{

}
void LuaGame::OnMouseMove(const MouseEvent& mouse)
{
    CallLua((*mLuaState)["OnMouseMove"], mouse);
}
void LuaGame::OnMousePress(const MouseEvent& mouse)
{
    CallLua((*mLuaState)["OnMousePress"], mouse);
}
void LuaGame::OnMouseRelease(const MouseEvent& mouse)
{
    CallLua((*mLuaState)["OnMouseRelease"], mouse);
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

    state->clear_package_loaders();

    auto* lua_state = state.get();

    lua_state->add_package_loader([lua_state, this](std::string module) {
        ASSERT(mDataLoader);
        if (!base::EndsWith(module, ".lua"))
            module += ".lua";

        DEBUG("Loading Lua module. [module=%1]", module);
        const auto& file = base::JoinPath(mLuaPath, module);
        const auto& buff = mDataLoader->LoadGameDataFromFile(file);
        if (!buff)
            throw std::runtime_error("can't find lua module: " + module);
        auto ret = lua_state->load_buffer((const char*)buff->GetData(), buff->GetSize());
        if (!ret.valid())
        {
            sol::error err = ret;
            throw std::runtime_error(err.what());
        }
        return ret.get<sol::function>(); // hmm??
    });

    BindBase(*state);
    BindUtil(*state);
    BindData(*state);
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
    std::unordered_map<std::string, std::shared_ptr<sol::environment>> entity_env_map;
    std::unordered_map<std::string, std::shared_ptr<sol::environment>> script_env_map;

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
            const auto& script_file = base::JoinPath(mLuaPath, script + ".lua");
            const auto& script_buff = mDataLoader->LoadGameDataFromFile(script_file);
            if (!script_buff)
            {
                ERROR("Failed to load entity class script file. [class='%1', file='%2']", klass.GetName(), script_file);
                continue;
            }
            auto script_env = std::make_shared<sol::environment>(*state, sol::create, state->globals());
            const auto& script_view = sol::string_view((const char*)script_buff->GetData(),
                    script_buff->GetSize());
            const auto& result = state->script(script_view, *script_env);
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

    std::unique_ptr<sol::environment> scene_env;
    if ((*scene)->HasScriptFile())
    {
        const auto& klass = scene->GetClass();
        const auto& script = klass.GetScriptFileId();
        const auto& script_file = base::JoinPath(mLuaPath, script + ".lua");
        const auto& script_buff = mDataLoader->LoadGameDataFromFile(script_file);
        if (!script_buff)
        {
            ERROR("Failed to load scene class script file. [class='%1', file='%2']", klass.GetName(), script_file);
        }
        else
        {
            scene_env = std::make_unique<sol::environment>(*state, sol::create, state->globals());
            const auto& view = sol::string_view((const char*)script_buff->GetData(),
                                                script_buff->GetSize());
            const auto& result = state->script(view, *scene_env);
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

    // careful here, make sure to clean up the environment objects
    // first since they depend on lua state. changing the order
    // of these two lines will crash.
    mSceneEnv = std::move(scene_env);
    mTypeEnvs = std::move(entity_env_map);
    mLuaState = std::move(state);

    mScene    = scene;
    (*mLuaState)["Audio"]    = mAudioEngine;
    (*mLuaState)["Physics"]  = mPhysicsEngine;
    (*mLuaState)["ClassLib"] = mClassLib;
    (*mLuaState)["Scene"]    = mScene;
    (*mLuaState)["State"]    = mStateStore;
    (*mLuaState)["Game"]     = this;
    auto table = (*mLuaState)["game"].get_or_create<sol::table>();
    table["OS"] = OS_NAME;
    auto engine = table.new_usertype<ScriptEngine>("Engine");
    BindEngine(engine, *this);

    if (mSceneEnv)
        CallLua((*mSceneEnv)["BeginPlay"], scene);

    for (size_t i=0; i<scene->GetNumEntities(); ++i)
    {
        auto* entity = &mScene->GetEntity(i);
        if (mSceneEnv)
            CallLua((*mSceneEnv)["SpawnEntity"], scene, entity);

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
    if (mSceneEnv)
        CallLua((*mSceneEnv)["EndPlay"], scene);

    mSceneEnv.reset();
    mTypeEnvs.clear();
    mScene = nullptr;
    (*mLuaState)["Scene"] = nullptr;
}

void ScriptEngine::Tick(double game_time, double dt)
{
    if (mSceneEnv)
    {
        TRACE_CALL("Lua::Scene::Tick",
                   CallLua((*mSceneEnv)["Tick"], mScene, game_time, dt));
    }

    TRACE_SCOPE("Lua::Entity::Tick");
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
    if (mSceneEnv)
    {
        TRACE_CALL("Lua::Scene::Update",
                   CallLua((*mSceneEnv)["Update"], mScene, game_time, dt));
    }

    TRACE_SCOPE("Lua::Entity::Update");
    for (size_t i=0; i<mScene->GetNumEntities(); ++i)
    {
        auto* entity = &mScene->GetEntity(i);
        if (auto* env = GetTypeEnv(entity->GetClass()))
        {
            if (const auto* anim = entity->GetFinishedAnimation()) {
                CallLua((*env)["OnAnimationFinished"], entity, anim);
            }
            if (entity->TestFlag(Entity::Flags::UpdateEntity)) {
                CallLua((*env)["Update"], entity, game_time, dt);
            }
        }
    }
}
void ScriptEngine::PostUpdate(double game_time)
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
        if (mSceneEnv)
            CallLua((*mSceneEnv)["SpawnEntity"], mScene, entity);

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
         if (mSceneEnv)
             CallLua((*mSceneEnv)["KillEntity"], mScene, entity);

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
    *out = std::move(mActionQueue.front());
    mActionQueue.pop();
    return true;
}

void ScriptEngine::OnContactEvent(const ContactEvent& contact)
{
    const auto* function = contact.type == ContactEvent::Type::BeginContact
            ? "OnBeginContact" : "OnEndContact";

    auto* nodeA = contact.nodeA;
    auto* nodeB = contact.nodeB;
    auto* entityA = nodeA->GetEntity();
    auto* entityB = nodeB->GetEntity();

    const auto& klassA = entityA->GetClass();
    const auto& klassB = entityB->GetClass();

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
void ScriptEngine::OnGameEvent(const GameEvent& event)
{
    if (mSceneEnv)
        CallLua((*mSceneEnv)["OnGameEvent"], mScene, event);

    for (size_t i=0; i<mScene->GetNumEntities(); ++i)
    {
        auto* entity = &mScene->GetEntity(i);
        if (auto* env = GetTypeEnv(entity->GetClass()))
        {
            CallLua((*env)["OnGameEvent"], entity, event);
        }
    }
}

void ScriptEngine::OnKeyDown(const wdk::WindowEventKeyDown& key)
{
    DispatchKeyboardEvent("OnKeyDown", key);
}
void ScriptEngine::OnKeyUp(const wdk::WindowEventKeyUp& key)
{
    DispatchKeyboardEvent("OnKeyUp", key);
}
void ScriptEngine::OnChar(const wdk::WindowEventChar& text)
{

}
void ScriptEngine::OnMouseMove(const MouseEvent& mouse)
{
    DispatchMouseEvent("OnMouseMove", mouse);
}
void ScriptEngine::OnMousePress(const MouseEvent& mouse)
{
    DispatchMouseEvent("OnMousePress", mouse);
}
void ScriptEngine::OnMouseRelease(const MouseEvent& mouse)
{
    DispatchMouseEvent("OnMouseRelease", mouse);
}

template<typename KeyEvent>
void ScriptEngine::DispatchKeyboardEvent(const std::string& method, const KeyEvent& key)
{
    if (mSceneEnv)
        CallLua((*mSceneEnv)[method], mScene,
                static_cast<int>(key.symbol),
                static_cast<int>(key.modifiers.value()));

    for (size_t i=0; i<mScene->GetNumEntities(); ++i)
    {
        auto* entity = &mScene->GetEntity(i);
        if (!entity->TestFlag(Entity::Flags::WantsKeyEvents))
            continue;
        if (auto* env = GetTypeEnv(entity->GetClass()))
        {
            CallLua((*env)[method], entity,
                    static_cast<int>(key.symbol) ,
                    static_cast<int>(key.modifiers.value()));
        }
    }
}

void ScriptEngine::DispatchMouseEvent(const std::string& method, const MouseEvent& mouse)
{
    if (mSceneEnv)
        CallLua((*mSceneEnv)[method], mScene, mouse);

    for (size_t i=0; i<mScene->GetNumEntities(); ++i)
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

sol::environment* ScriptEngine::GetTypeEnv(const EntityClass& klass)
{
    if (!klass.HasScriptFile())
        return nullptr;
    const auto& klassId = klass.GetId();
    auto it = mTypeEnvs.find(klassId);
    if (it != mTypeEnvs.end())
        return it->second.get();

    const auto& script_id   = klass.GetScriptFileId();
    const auto& script_file = base::JoinPath(mLuaPath, script_id + ".lua");
    const auto& script_buff = mDataLoader->LoadGameDataFromFile(script_file);
    if (!script_buff)
    {
        ERROR("Failed to load entity class script file. [class='%1', file='%2']", klass.GetName(), script_file);
        return nullptr;
    }
    const auto& script_view = sol::string_view((const char*)script_buff->GetData(),
        script_buff->GetSize());
    auto env = std::make_unique<sol::environment>(*mLuaState, sol::create, mLuaState->globals());
    const auto& result = mLuaState->script(script_view, *env);
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
    util["ToVec2"] = [](const base::FPoint& point) {
        return glm::vec2(point.GetX(), point.GetY());
    };
    util["ToPoint"] = [](const glm::vec2& vec2) {
        return base::FPoint(vec2.x, vec2.y);
    };

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
    box["Reset"]       = sol::overload(
            [](FBox& box) {box.Reset(); },
            [](FBox& box, float width, float height) { box.Reset(width, height); }
            );

    util["JoinPath"]   = &base::JoinPath;
    util["FileExists"] = &base::FileExists;
    util["RandomString"] = &base::RandomString;

    util["FormatString"] = [](std::string str, sol::variadic_args args) {
        for (size_t i=0; i<args.size(); ++i) {
            const auto& arg = args[i];
            const auto index = i + 1;
            if (arg.is<std::string>())
                str = base::detail::ReplaceIndex(index, str, arg.get<std::string>());
            else if (arg.is<int>())
                str = base::detail::ReplaceIndex(index, str, arg.get<int>());
            else if (arg.is<float>())
                str = base::detail::ReplaceIndex(index, str, arg.get<float>());
            else if (arg.is<bool>())
                str = base::detail::ReplaceIndex(index, str, arg.get<bool>());
            else if (arg.is<base::FSize>())
                str = base::detail::ReplaceIndex(index, str, arg.get<base::FSize>());
            else if (arg.is<base::FPoint>())
                str = base::detail::ReplaceIndex(index, str, arg.get<base::FPoint>());
            else if (arg.is<base::FRect>())
                str = base::detail::ReplaceIndex(index, str, arg.get<base::FRect>());
            else if (arg.is<base::Color4f>())
                str = base::detail::ReplaceIndex(index, str, arg.get<base::Color4f>());
            else if (arg.is<glm::vec2>())
                str = base::detail::ReplaceIndex(index, str, arg.get<glm::vec2>());
            else if (arg.is<glm::vec3>())
                str = base::detail::ReplaceIndex(index, str, arg.get<glm::vec3>());
            else if (arg.is<glm::vec4>())
                str = base::detail::ReplaceIndex(index, str, arg.get<glm::vec4>());
            else throw GameError("Unsupported string format value type.");
        }
        return str;
    };
}

void BindBase(sol::state& L)
{
    auto base = L.create_named_table("base");
    base["debug"] = [](const std::string& str) { DEBUG(str); };
    base["warn"]  = [](const std::string& str) { WARN(str); };
    base["error"] = [](const std::string& str) { ERROR(str); };
    base["info"]  = [](const std::string& str) { INFO(str); };

    auto trace = L.create_named_table("trace");
    trace["marker"] = sol::overload(
            [](const std::string& str) { base::TraceMarker(str); },
            [](const std::string& str, unsigned index) { base::TraceMarker(str, index); }
    );
    trace["enter"]  = &base::TraceBeginScope;
    trace["leave"]  = &base::TraceEndScope;

    sol::constructors<base::FRect(), base::FRect(float, float, float, float)> rect_ctors;
    auto rect = base.new_usertype<base::FRect>("FRect", rect_ctors);
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
    rect["TestPoint"]      = sol::overload(
        [](const base::FRect& rect, float x, float y) {
            return rect.TestPoint(x, y);
        },
        [](const base::FRect& rect, const FPoint& point) {
            return rect.TestPoint(point);
        });
    rect["MapToGlobal"] = sol::overload(
        [](const base::FRect& rect, float x, float y) {
            return rect.MapToGlobal(x, y);
        },
        [](const base::FRect& rect, const FPoint& point) {
            return rect.MapToGlobal(point);
        });
    rect["MapToLocal"] = sol::overload(
        [](const base::FRect& rect, float x, float y) {
            return rect.MapToLocal(x, y);
        },
        [](const base::FRect& rect, const FPoint& point) {
            return rect.MapToLocal(point);
        });
    rect["GetQuadrants"]   = &base::FRect::GetQuadrants;
    rect["GetCorners"]     = &base::FRect::GetCorners;
    rect["GetCenter"]      = &base::FRect::GetCenter;
    rect["Combine"]        = &base::Union<float>;
    rect["Intersect"]      = &base::Intersect<float>;
    rect["TestIntersect"]  = &base::DoesIntersect<float>;
    rect.set_function(sol::meta_function::to_string, [](const FRect& rect) {
        return base::ToString(rect);
    });

    sol::constructors<base::FSize(), base::FSize(float, float)> size_ctors;
    auto size = base.new_usertype<base::FSize>("FSize", size_ctors);
    size["GetWidth"]  = &base::FSize::GetWidth;
    size["GetHeight"] = &base::FSize::GetHeight;
    size.set_function(sol::meta_function::multiplication, [](const base::FSize& size, float scalar) { return size * scalar; });
    size.set_function(sol::meta_function::addition, [](const base::FSize& lhs, const base::FSize& rhs) { return lhs + rhs; });
    size.set_function(sol::meta_function::subtraction, [](const base::FSize& lhs, const base::FSize& rhs) { return lhs - rhs; });
    size.set_function(sol::meta_function::to_string, [](const base::FSize& size) { return base::ToString(size); });

    sol::constructors<base::FPoint(), base::FPoint(float, float)> point_ctors;
    auto point = base.new_usertype<base::FPoint>("FPoint", point_ctors);
    point["GetX"] = &base::FPoint::GetX;
    point["GetY"] = &base::FPoint::GetY;
    point.set_function(sol::meta_function::addition, [](const base::FPoint& lhs, const base::FPoint& rhs) { return lhs + rhs; });
    point.set_function(sol::meta_function::subtraction, [](const base::FPoint& lhs, const base::FPoint& rhs) { return lhs - rhs; });
    point.set_function(sol::meta_function::to_string, [](const base::FPoint& point) { return base::ToString(point); });

    // build color name table
    for (const auto& color : magic_enum::enum_values<base::Color>())
    {
        const std::string name(magic_enum::enum_name(color));
        base[sol::create_if_nil]["Colors"][name] = magic_enum::enum_integer(color);
    }

    // todo: figure out a way to construct from color name, is that possible?
    sol::constructors<base::Color4f(),
            base::Color4f(float, float, float, float),
            base::Color4f(int, int, int, int)> color_ctors;
    auto color = base.new_usertype<base::Color4f>("Color4f", color_ctors);
    color["GetRed"]     = &base::Color4f::Red;
    color["GetGreen"]   = &base::Color4f::Green;
    color["GetBlue"]    = &base::Color4f::Blue;
    color["GetAlpha"]   = &base::Color4f::Alpha;
    color["SetRed"]     = (void(base::Color4f::*)(float))&base::Color4f::SetRed;
    color["SetGreen"]   = (void(base::Color4f::*)(float))&base::Color4f::SetGreen;
    color["SetBlue"]    = (void(base::Color4f::*)(float))&base::Color4f::SetBlue;
    color["SetAlpha"]   = (void(base::Color4f::*)(float))&base::Color4f::SetAlpha;
    color["SetColor"]   = sol::overload(
        [](base::Color4f& color, int value) {
            const auto color_value = magic_enum::enum_cast<base::Color>(value);
            if (!color_value.has_value())
                throw GameError("No such color value:" + std::to_string(value));
            color = base::Color4f(color_value.value());
        },
        [](base::Color4f& color, const std::string& name) {
            const auto color_value = magic_enum::enum_cast<base::Color>(name);
            if (!color_value.has_value())
                throw GameError("No such color name: " + name);
            color = base::Color4f(color_value.value());
        });
    color["FromEnum"] = sol::overload(
        [](int value) {
            const auto color_value = magic_enum::enum_cast<base::Color>(value);
            if (!color_value.has_value())
                throw GameError("No such color value:" + std::to_string(value));
            return base::Color4f(color_value.value());
        },
        [](const std::string& name) {
            const auto color_value = magic_enum::enum_cast<base::Color>(name);
            if (!color_value.has_value())
                throw GameError("No such color name: " + name);
            return base::Color4f(color_value.value());
        });
    color.set_function(sol::meta_function::to_string, [](const base::Color4f& color) { return base::ToString(color); });
}

void BindData(sol::state& L)
{
    auto data = L.create_named_table("data");
    auto reader = data.new_usertype<data::Reader>("Reader");
    BindDataReaderInterface<data::Reader>(reader);

    auto writer = data.new_usertype<data::Writer>("Writer");
    BindDataWriterInterface<data::Writer>(writer);

    // there's no automatic understanding that the JsonObject *is-a* reader/writer
    // and provides those methods. Thus the reader/writer methods must be bound on the
    // JsonObject's usertype as well.
    auto json = data.new_usertype<data::JsonObject>("JsonObject",sol::constructors<data::JsonObject()>());
    BindDataReaderInterface<data::JsonObject>(json);
    BindDataWriterInterface<data::JsonObject>(json);
    json["ParseString"] = sol::overload(
            (std::tuple<bool, std::string>(data::JsonObject::*)(const std::string&))&data::JsonObject::ParseString,
            (std::tuple<bool, std::string>(data::JsonObject::*)(const char*, size_t))&data::JsonObject::ParseString);
    json["ToString"] = &data::JsonObject::ToString;

    data["ParseJsonString"] = sol::overload(
            [](const std::string& json) {
                auto ret = std::make_unique<data::JsonObject>();
                auto [ok, error] = ret->ParseString(json);
                if (!ok)
                    ret.reset();
                return std::make_tuple(std::move(ret), std::move(error));
            },
            [](const char* json, size_t len) {
                auto ret = std::make_unique<data::JsonObject>();
                auto [ok, error] = ret->ParseString(json, len);
                if (!ok)
                    ret.reset();
                return std::make_tuple(std::move(ret), std::move(error));
            });
    data["WriteJsonFile"] = [](const data::JsonObject& json, const std::string& file) {
        return data::WriteJsonFile(json, file);
    };
    data["ReadJsonFile"] = [](const std::string& file) {
        return data::ReadJsonFile(file);
    };
    data["CreateWriter"] = [](const std::string& format) {
        std::unique_ptr<data::Writer> ret;
        if (format == "JSON")
            ret.reset(new data::JsonObject);
        return ret;
    };
    // overload this when/if there are different data formats
    data["WriteFile"] = [](const data::JsonObject& json, const std::string& file) {
        return data::WriteJsonFile(json, file);
    };
    data["ReadFile"] = [](const std::string& file) {
        const auto& upper = base::ToUpperUtf8(file);
        std::unique_ptr<data::Reader> ret;
        if (base::EndsWith(upper, ".JSON")) {
            auto [json, error] = data::ReadJsonFile(file);
            if (json) {
                ret = std::move(json);
                return std::make_tuple(std::move(ret), std::string(""));
            }
            return std::make_tuple(std::move(ret), std::move(error));
        }
        return std::make_tuple(std::move(ret),
                   std::string("unsupported file type"));
    };
}

void BindGLM(sol::state& L)
{
    auto glm  = L.create_named_table("glm");

    auto vec2 = glm.new_usertype<glm::vec2>("vec2",
        sol::constructors<glm::vec2(), glm::vec2(float, float)>(),
        sol::meta_function::index, [](const glm::vec2& vec, int index) {
            if (index >= 2)
                throw GameError("glm.vec2 access out of bounds");
            return vec[index];
            }
    );
    vec2["x"] = &glm::vec2::x;
    vec2["y"] = &glm::vec2::y;
    BindGlmVectorOp<glm::vec2>(vec2);

    auto vec3 = glm.new_usertype<glm::vec3>("vec3",
        sol::constructors<glm::vec3(), glm::vec3(float, float, float)>(),
        sol::meta_function::index, [](const glm::vec3& vec, int index) {
                if (index >= 3)
                    throw GameError("glm.vec3 access out of bounds");
                return vec[index];
            }
    );
    vec3["x"] = &glm::vec3::x;
    vec3["y"] = &glm::vec3::y;
    vec3["z"] = &glm::vec3::z;
    BindGlmVectorOp<glm::vec3>(vec3);

    auto vec4 = glm.new_usertype<glm::vec4>("vec4",
        sol::constructors<glm::vec4(), glm::vec4(float, float, float, float)>(),
        sol::meta_function::index, [](const glm::vec4& vec, int index) {
                if (index >= 4)
                    throw GameError("glm.vec4 access out of bounds");
                return vec[index];
            }
    );
    vec4["x"] = &glm::vec4::x;
    vec4["y"] = &glm::vec4::y;
    vec4["z"] = &glm::vec4::z;
    vec4["w"] = &glm::vec4::w;
    BindGlmVectorOp<glm::vec4>(vec4);

    glm["dot"] = sol::overload(
            [](const glm::vec2& a, const glm::vec2& b) { return glm::dot(a, b); },
            [](const glm::vec3& a, const glm::vec3& b) { return glm::dot(a, b); },
            [](const glm::vec4& a, const glm::vec4& b) { return glm::dot(a, b); });
    glm["length"] = sol::overload(
            [](const glm::vec2& vec) { return glm::length(vec); },
            [](const glm::vec3& vec) { return glm::length(vec); },
            [](const glm::vec4& vec) { return glm::length(vec); });
    glm["normalize"] = sol::overload(
            [](const glm::vec2& vec) { return glm::normalize(vec); },
            [](const glm::vec3& vec) { return glm::normalize(vec); },
            [](const glm::vec4& vec) { return glm::normalize(vec); });

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
            throw GameError("No such keysym value:" + std::to_string(value));
        return std::string(magic_enum::enum_name(key.value()));
    };
    table["BtnStr"] = [](int value) {
        const auto key = magic_enum::enum_cast<wdk::MouseButton>(value);
        if (!key.has_value())
            throw GameError("No such mouse button value: " + std::to_string(value));
        return std::string(magic_enum::enum_name(key.value()));
    };
    table["ModStr"] = [](int value) {
        const auto mod = magic_enum::enum_cast<wdk::Keymod>(value);
        if (!mod.has_value())
            throw GameError("No such keymod value: " + std::to_string(value));
        return std::string(magic_enum::enum_name(mod.value()));
    };
    table["ModBitStr"] = [](int bits) {
        std::string ret;
        wdk::bitflag<wdk::Keymod> mods;
        mods.set_from_value(bits);
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
            throw GameError("No such key symbol: " + std::to_string(value));
#if defined(WEBASSEMBLY)
        throw GameError("TestKeyDown is not available in WASM.");
#else
        return wdk::TestKeyDown(key.value());
#endif
    };
    table["TestMod"] = [](int bits, int value) {
        const auto mod = magic_enum::enum_cast<wdk::Keymod>(value);
        if (!mod.has_value())
            throw GameError("No such modifier: " + std::to_string(value));
        wdk::bitflag<wdk::Keymod> mods;
        mods.set_from_value(bits);
        return mods.test(mod.value());
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

    using KeyBitSet = std::bitset<96>;

    auto key_bit_string = table.new_usertype<KeyBitSet>("KeyBitSet",
        sol::constructors<KeyBitSet()>());

    key_bit_string["Set"] = [](KeyBitSet& bits, int key, bool on_off) {
        const auto sym = magic_enum::enum_cast<wdk::Keysym>(key);
        if (!sym.has_value())
            throw GameError("No such keysym: " + std::to_string(key));
        bits[static_cast<unsigned>(sym.value())] = on_off;
    };
    key_bit_string["Test"] = [](const KeyBitSet& bits, int key) {
        const auto sym = magic_enum::enum_cast<wdk::Keysym>(key);
        if (!sym.has_value())
            throw GameError("No such keysym: " + std::to_string(key));
        const bool ret = bits[static_cast<unsigned>(sym.value())];
        return ret;
    };
    key_bit_string["AnyBit"] = &KeyBitSet::any;
    key_bit_string["Clear"]  = [](KeyBitSet& bits) { bits.reset(); };
    // todo: fix the issues in the wdk bitset.
    // somehow the lack of explicit copy constructor seems to cause issues here?
    key_bit_string[sol::meta_function::bitwise_and] = sol::overload(
        [](const KeyBitSet& a, const KeyBitSet& b) {
            return a & b;
        },
        [](const KeyBitSet& bits, int key) {
            const auto sym = magic_enum::enum_cast<wdk::Keysym>(key);
            if (!sym.has_value())
                throw GameError("No such keysym: " + std::to_string(key));
            KeyBitSet tmp;
            tmp[static_cast<unsigned>(sym.value())] = true;
            return bits & tmp;
        });
    key_bit_string[sol::meta_function::bitwise_or] = sol::overload(
        [](const KeyBitSet& a, const KeyBitSet& b) {
            return a | b;
        },
        [](const KeyBitSet& bits, int key) {
            const auto sym = magic_enum::enum_cast<wdk::Keysym>(key);
            if (!sym.has_value())
                throw GameError("No such keysym: " + std::to_string(key));
            KeyBitSet tmp;
            tmp[static_cast<unsigned>(sym.value())] = true;
            return bits | tmp;
        });
}

void BindUIK(sol::state& L)
{
    auto table = L["uik"].get_or_create<sol::table>();
    table["WidgetCast"] = &WidgetObjectCast;

    auto widget = table.new_usertype<uik::Widget>("Widget");
    BindWidgetInterface(widget);
    widget["AsLabel"]        = &WidgetCast<uik::Label>;
    widget["AsPushButton"]   = &WidgetCast<uik::PushButton>;
    widget["AsCheckBox"]     = &WidgetCast<uik::CheckBox>;
    widget["AsGroupBox"]     = &WidgetCast<uik::GroupBox>;
    widget["AsSpinBox"]      = &WidgetCast<uik::SpinBox>;
    widget["AsProgressBar"]  = &WidgetCast<uik::ProgressBar>;
    widget["AsForm"]         = &WidgetCast<uik::Form>;
    widget["AsSlider"]       = &WidgetCast<uik::Slider>;

    auto form = table.new_usertype<uik::Form>("Form");
    BindWidgetInterface(form);

    auto label = table.new_usertype<uik::Label>("Label");
    BindWidgetInterface(label);
    label["GetText"] = &uik::Label::GetText;
    label["SetText"] = (void(uik::Label::*)(const std::string&))&uik::Label::SetText;

    auto checkbox = table.new_usertype<uik::CheckBox>("CheckBox");
    BindWidgetInterface(checkbox);
    checkbox["GetText"]    = &uik::CheckBox::GetText;
    checkbox["SetText"]    = (void(uik::CheckBox::*)(const std::string&))&uik::CheckBox::SetText;
    checkbox["IsChecked"]  = &uik::CheckBox::IsChecked;
    checkbox["SetChecked"] = &uik::CheckBox::SetChecked;

    auto groupbox = table.new_usertype<uik::GroupBox>("GroupBox");
    BindWidgetInterface(groupbox);
    groupbox["GetText"] = &uik::GroupBox::GetText;
    groupbox["SetText"] = (void(uik::GroupBox::*)(const std::string&))&uik::GroupBox::SetText;

    auto pushbtn = table.new_usertype<uik::PushButton>("PushButton");
    BindWidgetInterface(pushbtn);
    pushbtn["GetText"]  = &uik::PushButton::GetText;
    pushbtn["SetText"]  = (void(uik::PushButton::*)(const std::string&))&uik::PushButton::SetText;

    auto progressbar = table.new_usertype<uik::ProgressBar>("ProgressBar");
    BindWidgetInterface(progressbar);
    progressbar["SetText"]    = &uik::ProgressBar::SetText;
    progressbar["GetText"]    = &uik::ProgressBar::GetText;
    progressbar["ClearValue"] = &uik::ProgressBar::ClearValue;
    progressbar["SetValue"]   = &uik::ProgressBar::SetValue;
    progressbar["HasValue"]   = &uik::ProgressBar::HasValue;
    progressbar["GetValue"]   = [](uik::ProgressBar* progress, sol::this_state state) {
        sol::state_view view(state);
        if (progress->HasValue())
            return sol::make_object(view, progress->GetValue(0.0f));
        return sol::make_object(view, sol::nil);
    };

    auto spinbox = table.new_usertype<uik::SpinBox>("SpinBox");
    BindWidgetInterface(spinbox);
    spinbox["SetMin"]   = &uik::SpinBox::SetMin;
    spinbox["SetMax"]   = &uik::SpinBox::SetMax;
    spinbox["SetValue"] = &uik::SpinBox::SetValue;
    spinbox["GetMin"]   = &uik::SpinBox::GetMin;
    spinbox["GetMax"]   = &uik::SpinBox::GetMax;
    spinbox["GetValue"] = &uik::SpinBox::GetValue;

    auto slider = table.new_usertype<uik::Slider>("Slider");
    BindWidgetInterface(slider);
    slider["SetValue"] = &uik::Slider::SetValue;
    slider["GetValue"] = &uik::Slider::GetValue;

    auto window = table.new_usertype<uik::Window>("Window");
    window["GetId"]            = &uik::Window::GetId;
    window["GetName"]          = &uik::Window::GetName;
    window["GetNumWidgets"]    = &uik::Window::GetNumWidgets;
    window["FindWidgetById"]   = sol::overload(
        [](uik::Window* window, const std::string& id) {
            return window->FindWidgetById(id);
        },
        [](sol::this_state state, uik::Window* window, const std::string& id,  const std::string& type_string) {
            sol::state_view lua(state);
            auto* widget = window->FindWidgetById(id);
            if (!widget)
                return sol::make_object(lua, sol::nil);
            return WidgetObjectCast(state, widget, type_string);
        });
    window["FindWidgetByName"] = sol::overload(
        [](uik::Window* window, const std::string& name) {
            return window->FindWidgetByName(name);
        },
        [](sol::this_state state, uik::Window* window, const std::string& name, const std::string& type_string) {
            sol::state_view lua(state);
            auto* widget = window->FindWidgetByName(name);
            if (!widget)
                return sol::make_object(lua, sol::nil);
            return WidgetObjectCast(state, widget, type_string);
        });
    window["FindWidgetParent"] = [](uik::Window* window, uik::Widget* child) { return window->FindParent(child); };
    window["GetWidget"]        = [](uik::Window* window, unsigned index) {
        if (index >= window->GetNumWidgets())
            throw GameError(base::FormatString("Widget index %1 is out of bounds", index));
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
        throw GameError(base::FormatString("No such ui action index: %1", key));
    });
}

void BindGameLib(sol::state& L)
{
    auto table = L["game"].get_or_create<sol::table>();
    table["X"] = glm::vec2(1.0f, 0.0f);
    table["Y"] = glm::vec2(0.0f, 1.0f);

    auto classlib = table.new_usertype<ClassLibrary>("ClassLibrary");
    classlib["FindEntityClassByName"]     = &ClassLibrary::FindEntityClassByName;
    classlib["FindEntityClassById"]       = &ClassLibrary::FindEntityClassById;
    classlib["FindSceneClassByName"]      = &ClassLibrary::FindSceneClassByName;
    classlib["FindSceneClassById"]        = &ClassLibrary::FindSceneClassById;
    classlib["FindUIByName"]              = &ClassLibrary::FindUIByName;
    classlib["FindUIById"]                = &ClassLibrary::FindUIById;
    classlib["FindAudioGraphClassByName"] = &ClassLibrary::FindAudioGraphClassByName;
    classlib["FindAudioGraphClassById"]   = &ClassLibrary::FindAudioGraphClassById;

    auto drawable = table.new_usertype<DrawableItem>("Drawable");
    drawable["GetMaterialId"] = &DrawableItem::GetMaterialId;
    drawable["GetDrawableId"] = &DrawableItem::GetDrawableId;
    drawable["GetLayer"]      = &DrawableItem::GetLayer;
    drawable["GetLineWidth"]  = &DrawableItem::GetLineWidth;
    drawable["GetTimeScale"]  = &DrawableItem::GetTimeScale;
    drawable["SetTimeScale"]  = &DrawableItem::SetTimeScale;
    drawable["TestFlag"]      = &TestFlag<DrawableItem>;
    drawable["SetFlag"]       = &SetFlag<DrawableItem>;
    drawable["IsVisible"]     = &DrawableItem::IsVisible;
    drawable["SetVisible"]    = &DrawableItem::SetVisible;
    drawable["SetUniform"]    = [](DrawableItem& item, const char* name, sol::object value) {
        if (value.is<float>())
            item.SetMaterialParam(name, value.as<float>());
        else if (value.is<int>())
            item.SetMaterialParam(name, value.as<int>());
        else if (value.is<base::Color4f>())
            item.SetMaterialParam(name, value.as<base::Color4f>());
        else if (value.is<glm::vec2>())
            item.SetMaterialParam(name, value.as<glm::vec2>());
        else if (value.is<glm::vec3>())
            item.SetMaterialParam(name, value.as<glm::vec3>());
        else if (value.is<glm::vec4>())
            item.SetMaterialParam(name, value.as<glm::vec4>());
        else throw GameError("Unsupported material uniform type.");
    };
    drawable["FindUniform"] = [](const DrawableItem& item, const char* name, sol::this_state state) {
        sol::state_view L(state);
        if (const auto* value = item.FindMaterialParam(name))
            return sol::make_object(L, *value);
        return sol::make_object(L, sol::nil);
    };
    drawable["HasUniform"] = &DrawableItem::HasMaterialParam;
    drawable["DeleteUniform"] = &DrawableItem::DeleteMaterialParam;

    auto body = table.new_usertype<RigidBodyItem>("RigidBody");
    body["IsEnabled"]             = &RigidBodyItem::IsEnabled;
    body["IsSensor"]              = &RigidBodyItem::IsSensor;
    body["IsBullet"]              = &RigidBodyItem::IsBullet;
    body["CanSleep"]              = &RigidBodyItem::CanSleep;
    body["DiscardRotation"]       = &RigidBodyItem::DiscardRotation;
    body["GetFriction"]           = &RigidBodyItem::GetFriction;
    body["GetRestitution"]        = &RigidBodyItem::GetRestitution;
    body["GetAngularDamping"]     = &RigidBodyItem::GetAngularDamping;
    body["GetLinearDamping"]      = &RigidBodyItem::GetLinearDamping;
    body["GetDensity"]            = &RigidBodyItem::GetDensity;
    body["GetPolygonShapeId"]     = &RigidBodyItem::GetPolygonShapeId;
    body["GetLinearVelocity"]     = &RigidBodyItem::GetLinearVelocity;
    body["GetAngularVelocity"]    = &RigidBodyItem::GetAngularVelocity;
    body["Enable"]                = &RigidBodyItem::Enable;
    body["AdjustLinearVelocity"]  = sol::overload(
            [](RigidBodyItem& body, const glm::vec2& velocity) {
                body.AdjustLinearVelocity(velocity);
            },
            [](RigidBodyItem& body, float x, float y) {
                body.AdjustLinearVelocity(glm::vec2(x, y));
            });
    body["AdjustAngularVelocity"] = &RigidBodyItem::AdjustAngularVelocity;
    body["TestFlag"]              = &TestFlag<RigidBodyItem>;
    body["SetFlag"]               = &SetFlag<RigidBodyItem>;
    body["GetSimulationType"]     = [](const RigidBodyItem* item) {
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
    text["SetFlag"]       = &SetFlag<TextItem>;

    auto spn = table.new_usertype<SpatialNode>("SpatialNode");
    spn["GetShape"] = [](const SpatialNode* node) {
        return magic_enum::enum_name(node->GetShape());
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
    entity_node["HasTextItem"]    = &EntityNode::HasTextItem;
    entity_node["HasDrawable"]    = &EntityNode::HasDrawable;
    entity_node["HasSpatialNode"] = &EntityNode::HasSpatialNode;
    entity_node["GetDrawable"]    = (DrawableItem*(EntityNode::*)(void))&EntityNode::GetDrawable;
    entity_node["GetRigidBody"]   = (RigidBodyItem*(EntityNode::*)(void))&EntityNode::GetRigidBody;
    entity_node["GetTextItem"]    = (TextItem*(EntityNode::*)(void))&EntityNode::GetTextItem;
    entity_node["GetSpatialNode"] = (SpatialNode*(EntityNode::*)(void))&EntityNode::GetSpatialNode;
    entity_node["GetEntity"]      = (Entity*(EntityNode::*)(void))&EntityNode::GetEntity;
    entity_node["SetName"]        = &EntityNode::SetName;
    entity_node["SetScale"] = sol::overload(
        [](EntityNode& node, const glm::vec2& scale) { node.SetScale(scale); },
        [](EntityNode& node, float sx, float sy) { node.SetScale(sx, sy); });
    entity_node["SetSize"] = sol::overload(
        [](EntityNode& node, const glm::vec2& size) { node.SetSize(size); },
        [](EntityNode& node, float width, float height) { node.SetSize(width, height); });
    entity_node["SetTranslation"] = sol::overload(
        [](EntityNode& node, const glm::vec2& position) { node.SetTranslation(position); },
        [](EntityNode& node, float x, float y) { node.SetTranslation(x, y); });
    entity_node["Translate"] = sol::overload(
        [](EntityNode& node, const glm::vec2& delta) { node.Translate(delta); },
        [](EntityNode& node, float dx, float dy) { node.Translate(dx, dy); });
    entity_node["Rotate"] = &EntityNode::Rotate;

    auto entity_class = table.new_usertype<EntityClass>("EntityClass",
       sol::meta_function::index, &GetScriptVar<EntityClass>);
    entity_class["GetId"]   = &EntityClass::GetId;
    entity_class["GetName"] = &EntityClass::GetName;
    entity_class["GetLifetime"] = &EntityClass::GetLifetime;

    auto anim_class = table.new_usertype<AnimationClass>("AnimationClass");
    anim_class["GetName"]     = &AnimationClass::GetName;
    anim_class["GetId"]       = &AnimationClass::GetId;
    anim_class["GetDuration"] = &AnimationClass::GetDuration;
    anim_class["GetDelay"]    = &AnimationClass::GetDelay;
    anim_class["IsLooping"]   = &AnimationClass::IsLooping;

    auto anim = table.new_usertype<Animation>("Animation");
    anim["GetClassName"]   = &Animation::GetClassName;
    anim["GetClassId"]     = &Animation::GetClassId;
    anim["IsComplete"]     = &Animation::IsComplete;
    anim["IsLooping"]      = &Animation::IsLooping;
    anim["SetDelay"]       = &Animation::SetDelay;
    anim["GetDelay"]       = &Animation::GetDelay;
    anim["GetCurrentTime"] = &Animation::GetCurrentTime;
    anim["GetClass"]       = &Animation::GetClass;

    auto entity = table.new_usertype<Entity>("Entity",
        sol::meta_function::index,     &GetScriptVar<Entity>,
        sol::meta_function::new_index, &SetScriptVar<Entity>);
    entity["GetName"]              = &Entity::GetName;
    entity["GetId"]                = &Entity::GetId;
    entity["GetClassName"]         = &Entity::GetClassName;
    entity["GetClassId"]           = &Entity::GetClassId;
    entity["GetClass"]             = &Entity::GetClass;
    entity["GetNumNodes"]          = &Entity::GetNumNodes;
    entity["GetTime"]              = &Entity::GetTime;
    entity["GetLayer"]             = &Entity::GetLayer;
    entity["SetLayer"]             = &Entity::SetLayer;
    entity["IsAnimating"]          = &Entity::IsAnimating;
    entity["HasExpired"]           = &Entity::HasExpired;
    entity["HasBeenKilled"]        = &Entity::HasBeenKilled;
    entity["HasBeenSpawned"]       = &Entity::HasBeenSpawned;
    entity["GetScene"]             = (Scene*(Entity::*)(void))&Entity::GetScene;
    entity["GetNode"]              = (EntityNode&(Entity::*)(size_t))&Entity::GetNode;
    entity["FindNodeByClassName"]  = (EntityNode*(Entity::*)(const std::string&))&Entity::FindNodeByClassName;
    entity["FindNodeByClassId"]    = (EntityNode*(Entity::*)(const std::string&))&Entity::FindNodeByClassId;
    entity["FindNodeByInstanceId"] = (EntityNode*(Entity::*)(const std::string&))&Entity::FindNodeByInstanceId;
    entity["PlayIdle"]             = &Entity::PlayIdle;
    entity["PlayAnimationByName"]  = &Entity::PlayAnimationByName;
    entity["PlayAnimationById"]    = &Entity::PlayAnimationById;
    entity["Die"]                  = &Entity::Die;
    entity["TestFlag"]             = &TestFlag<Entity>;

    auto entity_args = table.new_usertype<EntityArgs>("EntityArgs", sol::constructors<EntityArgs()>());
    entity_args["id"]       = sol::property(&EntityArgs::id);
    entity_args["class"]    = sol::property(&EntityArgs::klass);
    entity_args["name"]     = sol::property(&EntityArgs::name);
    entity_args["scale"]    = sol::property(&EntityArgs::scale);
    entity_args["position"] = sol::property(&EntityArgs::position);
    entity_args["rotation"] = sol::property(&EntityArgs::rotation);
    entity_args["logging"]  = sol::property(&EntityArgs::enable_logging);

    using DynamicSpatialQueryResultSet = ResultSet<EntityNode*>;
    auto query_result_set = table.new_usertype<DynamicSpatialQueryResultSet>("SpatialQueryResultSet");
    query_result_set["IsEmpty"] = &DynamicSpatialQueryResultSet::IsEmpty;
    query_result_set["HasNext"] = &DynamicSpatialQueryResultSet::HasNext;
    query_result_set["Next"]    = &DynamicSpatialQueryResultSet::Next;
    query_result_set["Begin"]   = &DynamicSpatialQueryResultSet::BeginIteration;
    query_result_set["Get"]     = &DynamicSpatialQueryResultSet::GetCurrent;

    auto script_var = table.new_usertype<ScriptVar>("ScriptVar");
    script_var["GetValue"] = ObjectFromScriptVarValue;
    script_var["GetName"]  = &ScriptVar::GetName;
    script_var["GetId"]    = &ScriptVar::GetId;

    auto scene_class = table.new_usertype<SceneClass>("SceneClass",
        sol::meta_function::index,     &GetScriptVar<SceneClass>);
    scene_class["GetName"] = &SceneClass::GetName;
    scene_class["GetId"]   = &SceneClass::GetId;
    scene_class["GetNumScriptVars"] = &SceneClass::GetNumScriptVars;
    scene_class["GetScriptVar"]     = (const ScriptVar&(SceneClass::*)(size_t)const)&SceneClass::GetScriptVar;
    scene_class["FindScriptVarById"] = (const ScriptVar*(SceneClass::*)(const std::string&)const)&SceneClass::FindScriptVarById;
    scene_class["FindScriptVarByName"] = (const ScriptVar*(SceneClass::*)(const std::string&)const)&SceneClass::FindScriptVarByName;

    using EntityList = ResultVector<Entity*>;
    auto entity_list = table.new_usertype<EntityList>("EntityList");
    entity_list["IsEmpty"] = &EntityList::IsEmpty;
    entity_list["HasNext"] = &EntityList::HasNext;
    entity_list["Next"]    = &EntityList::Next;
    entity_list["Begin"]   = &EntityList::BeginIteration;
    entity_list["Get"]     = &EntityList::GetCurrent;
    entity_list["GetAt"]   = &EntityList::GetAt;
    entity_list["Size"]    = &EntityList::GetSize;

    auto scene = table.new_usertype<Scene>("Scene",
       sol::meta_function::index,     &GetScriptVar<Scene>,
       sol::meta_function::new_index, &SetScriptVar<Scene>);
    scene["ListEntitiesByClassName"]    = [](Scene& scene, const std::string& name) {
        return EntityList(scene.ListEntitiesByClassName(name));
    };
    scene["GetTime"]                    = &Scene::GetTime;
    scene["GetClassName"]               = &Scene::GetClassName;
    scene["GetClassId"]                 = &Scene::GetClassId;
    scene["GetClass"]                   = &Scene::GetClass;
    scene["GetNumEntities"]             = &Scene::GetNumEntities;
    scene["GetEntity"]                  = (Entity&(Scene::*)(size_t))&Scene::GetEntity;
    scene["FindEntityByInstanceId"]     = (Entity*(Scene::*)(const std::string&))&Scene::FindEntityByInstanceId;
    scene["FindEntityByInstanceName"]   = (Entity*(Scene::*)(const std::string&))&Scene::FindEntityByInstanceName;
    scene["KillEntity"]                 = &Scene::KillEntity;
    scene["FindEntityTransform"]        = &Scene::FindEntityTransform;
    scene["FindEntityNodeTransform"]    = &Scene::FindEntityNodeTransform;
    scene["FindEntityBoundingRect"]     = &Scene::FindEntityBoundingRect;
    scene["FindEntityNodeBoundingRect"] = &Scene::FindEntityNodeBoundingRect;
    scene["FindEntityNodeBoundingBox"]  = &Scene::FindEntityNodeBoundingBox;
    scene["MapVectorFromEntityNode"]    = &Scene::MapVectorFromEntityNode;
    scene["MapPointFromEntityNode"]     = sol::overload(
        [](Scene& scene, const Entity* entity, const EntityNode* node, const base::FPoint& point) {
            return scene.MapPointFromEntityNode(entity, node, point);
        },
        [](Scene& scene, const Entity* entity, const EntityNode* node, const glm::vec2& point) {
            return scene.MapPointFromEntityNode(entity, node, point);
        });
    scene["SpawnEntity"] = sol::overload(
        [](Scene& scene, const EntityArgs& args) { return scene.SpawnEntity(args, true); },
        [](Scene& scene, const EntityArgs& args, bool link) { return scene.SpawnEntity(args, link); });
    scene["QuerySpatialNodes"] = sol::overload(
        [](Scene& scene, const base::FPoint& point) {
            std::set<EntityNode*> result;
            scene.QuerySpatialNodes(point, &result);
            return std::make_unique<DynamicSpatialQueryResultSet>(std::move(result));
        },
        [](Scene& scene, const glm::vec2& point) {
            std::set<EntityNode*> result;
            scene.QuerySpatialNodes(base::FPoint(point.x, point.y), &result);
            return std::make_unique<DynamicSpatialQueryResultSet>(std::move(result));
        },
        [](Scene& scene, const base::FRect& rect) {
            std::set<EntityNode*> result;
            scene.QuerySpatialNodes(rect, &result);
            return std::make_unique<DynamicSpatialQueryResultSet>(std::move(result));
        }
    );

    auto physics = table.new_usertype<PhysicsEngine>("Physics");
    physics["ApplyImpulseToCenter"] = sol::overload(
        [](PhysicsEngine& self, const std::string& id, const glm::vec2& vec) {
            return self.ApplyImpulseToCenter(id, vec);
        },
        [](PhysicsEngine& self, const EntityNode& node, const glm::vec2& vec) {
            return self.ApplyImpulseToCenter(node, vec);
        });
    physics["ApplyForceToCenter"] = sol::overload(
        [](PhysicsEngine& self, const std::string& id, const glm::vec2& vec) {
            return self.ApplyForceToCenter(id, vec);
        },
        [](PhysicsEngine& self, const EntityNode& node, const glm::vec2& vec) {
            return self.ApplyForceToCenter(node, vec);
        });
    physics["SetLinearVelocity"] = sol::overload(
        [](PhysicsEngine& self, const std::string& id, const glm::vec2& vector) {
            return self.SetLinearVelocity(id, vector);
        },
        [](PhysicsEngine& self, const EntityNode& node, const glm::vec2& vector) {
            return self.SetLinearVelocity(node, vector);
        });
    physics["FindCurrentLinearVelocity"] = sol::overload(
        [](PhysicsEngine& self, const std::string& id) {
            return self.FindCurrentLinearVelocity(id);
        },
        [](PhysicsEngine& self, const EntityNode& node) {
            return self.FindCurrentLinearVelocity(node);
        }
    );
    physics["FindCurrentAngularVelocity"] = sol::overload(
        [](PhysicsEngine& self, const std::string& id) {
            return self.FindCurrentAngularVelocity(id);
        },
        [](PhysicsEngine& self, const EntityNode& node) {
            return self.FindCurrentAngularVelocity(node);
        }
    );
    physics["FindMass"] = sol::overload(
        [](PhysicsEngine& self, const std::string& id) {
            return self.FindMass(id);
        },
        [](PhysicsEngine& self, const EntityNode& node) {
            return self.FindMass(node);
        }
    );

    auto ray_cast_result = table.new_usertype<PhysicsEngine::RayCastResult>("RayCastResult");
    ray_cast_result["node"]     = &PhysicsEngine::RayCastResult::node;
    ray_cast_result["point"]    = &PhysicsEngine::RayCastResult::point;
    ray_cast_result["normal"]   = &PhysicsEngine::RayCastResult::normal;
    ray_cast_result["fraction"] = &PhysicsEngine::RayCastResult::fraction;

    using RayCastResultVector = ResultVector<PhysicsEngine::RayCastResult>;
    auto ray_cast_result_vector = table.new_usertype<RayCastResultVector>("RayCastResultVector");
    ray_cast_result_vector["IsEmpty"] = &RayCastResultVector::IsEmpty;
    ray_cast_result_vector["HasNext"] = &RayCastResultVector::HasNext;
    ray_cast_result_vector["Next"]    = &RayCastResultVector::Next;
    ray_cast_result_vector["Begin"]   = &RayCastResultVector::BeginIteration;
    ray_cast_result_vector["Get"]     = &RayCastResultVector::GetCurrent;
    ray_cast_result_vector["GetAt"]   = &RayCastResultVector::GetAt;
    ray_cast_result_vector["Size"]    = &RayCastResultVector::GetSize;

    physics["RayCast"] = sol::overload(
        [](PhysicsEngine& self, const glm::vec2& start, const glm::vec2& end, const std::string& mode) {
            const auto enum_val = magic_enum::enum_cast<PhysicsEngine::RayCastMode>(mode);
            if (!enum_val.has_value())
                throw GameError("No such ray cast mode: " + mode);
            std::vector<PhysicsEngine::RayCastResult> result;
            self.RayCast(start, end, &result, enum_val.value());
            return std::make_unique<RayCastResultVector>(std::move(result));
        },
        [](PhysicsEngine& self, const glm::vec2& start, const glm::vec2& end) {
            std::vector<PhysicsEngine::RayCastResult> result;
            self.RayCast(start, end, &result, PhysicsEngine::RayCastMode::All);
            return std::make_unique<RayCastResultVector>(std::move(result));
        });

    physics["GetScale"]   = &PhysicsEngine::GetScale;
    physics["GetGravity"] = &PhysicsEngine::GetGravity;
    physics["GetTimeStep"] = &PhysicsEngine::GetTimeStep;
    physics["GetNumPositionIterations"] = &PhysicsEngine::GetNumPositionIterations;
    physics["GetNumVelocityIterations"] = &PhysicsEngine::GetNumVelocityIterations;
    physics["MapVectorFromGame"] = &PhysicsEngine::MapVectorFromGame;
    physics["MapVectorToGame"]   = &PhysicsEngine::MapVectorToGame;
    physics["MapAngleFromGame"]  = &PhysicsEngine::MapAngleFromGame;
    physics["MapAngleToGame"]    = &PhysicsEngine::MapAngleToGame;
    physics["SetGravity"]        = &PhysicsEngine::SetGravity;
    physics["SetScale"]          = &PhysicsEngine::SetScale;

    auto audio = table.new_usertype<AudioEngine>("Audio");
    audio["PrepareMusicGraph"] = sol::overload(
            [](AudioEngine& engine, std::shared_ptr<const audio::GraphClass> klass) {
                if (!klass)
                    throw GameError("Nil audio graph class.");
                return engine.PrepareMusicGraph(klass);
            },
            [](AudioEngine& engine, const std::string& name) {
                const auto* lib = engine.GetClassLibrary();
                auto klass = lib->FindAudioGraphClassByName(name);
                if (!klass)
                    throw GameError("No such audio graph: " + name);
                return engine.PrepareMusicGraph(klass);
            });
    audio["PlayMusic"] = sol::overload(
            [](AudioEngine& engine, std::shared_ptr<const audio::GraphClass> klass) {
                if (!klass)
                    throw GameError("Nil audio graph class.");
                return engine.PlayMusic(klass);
            },
            [](AudioEngine& engine, std::shared_ptr<const audio::GraphClass> klass, unsigned when) {
                if (!klass)
                    throw GameError("Nil audio graph class.");
                return engine.PlayMusic(klass, when);
            },
            [](AudioEngine& engine, const std::string& name, unsigned when) {
                const auto* lib = engine.GetClassLibrary();
                auto klass = lib->FindAudioGraphClassByName(name);
                if (!klass)
                    throw GameError("No such audio graph: " + name);
                return engine.PlayMusic(klass, when);
            },
            [](AudioEngine& engine, const std::string& name) {
                const auto* lib = engine.GetClassLibrary();
                auto klass = lib->FindAudioGraphClassByName(name);
                if (!klass)
                    throw GameError("No such audio graph: " + name);
                return engine.PlayMusic(klass);
            });

    audio["ResumeMusic"] = sol::overload(
            [](AudioEngine& engine, const std::string& track, unsigned when) {
                engine.ResumeMusic(track, when);
            },
            [](AudioEngine& engine, const std::string& track) {
                engine.ResumeMusic(track, 0);
            });
    audio["PauseMusic"] = sol::overload(
            [](AudioEngine& engine, const std::string& track, unsigned when) {
                engine.PauseMusic(track, when);
            },
            [](AudioEngine& engine, const std::string& track) {
                engine.PauseMusic(track, 0);
            });
    audio["KillMusic"] = sol::overload(
            [](AudioEngine& engine, const std::string& track, unsigned when) {
                engine.KillMusic(track, when);
            },
            [](AudioEngine& engine, const std::string& track) {
                engine.KillMusic(track, 0);
            });

    audio["CancelMusicCmds"] = &AudioEngine::CancelMusicCmds;
    audio["SetMusicGain"]   = &AudioEngine::SetMusicGain;
    audio["SetMusicEffect"] = [](AudioEngine& engine,
                                 const std::string& track,
                                 const std::string& effect,
                                 unsigned duration) {
        const auto effect_value = magic_enum::enum_cast<AudioEngine::Effect>(effect);
        if (!effect_value.has_value())
            throw GameError("No such audio effect:" + effect);
        engine.SetMusicEffect(track, duration, effect_value.value());
    };
    audio["PlaySoundEffect"] = sol::overload(
            [](AudioEngine& engine, std::shared_ptr<const audio::GraphClass> klass, unsigned when) {
                if (!klass)
                    throw GameError("Nil audio effect graph class.");
                return engine.PlaySoundEffect(klass, when);
            },
            [](AudioEngine& engine, std::shared_ptr<const audio::GraphClass> klass) {
                if (!klass)
                    throw GameError("Nil audio effect graph class.");
                return engine.PlaySoundEffect(klass, 0);
            },
            [](AudioEngine& engine, const std::string& name, unsigned when) {
                const auto* lib = engine.GetClassLibrary();
                auto klass = lib->FindAudioGraphClassByName(name);
                if (!klass)
                    throw GameError("No such audio effect graph:" + name);
                return engine.PlaySoundEffect(klass, when);
            },
            [](AudioEngine& engine, const std::string& name) {
                const auto* lib = engine.GetClassLibrary();
                auto klass = lib->FindAudioGraphClassByName(name);
                if (!klass)
                    throw GameError("No such audio effect graph:" + name);
                return engine.PlaySoundEffect(klass, 0);
            });
    audio["SetSoundEffectGain"] = &AudioEngine::SetSoundEffectGain;

    auto audio_event = table.new_usertype<AudioEvent>("AudioEvent",
        sol::meta_function::index, [&L](const AudioEvent& event, const char* key) {
                sol::state_view lua(L);
                if (!std::strcmp(key, "type"))
                    return sol::make_object(lua, base::ToString(event.type));
                else if (!std::strcmp(key, "track"))
                    return sol::make_object(lua, event.track);
                else if (!std::strcmp(key, "source"))
                    return sol::make_object(lua, event.source);
                throw GameError(base::FormatString("No such audio event index: %1", key));
            }
    );

    auto mouse_event = table.new_usertype<MouseEvent>("MouseEvent",
        sol::meta_function::index, [&L](const MouseEvent& event, const char* key) {
                sol::state_view lua(L);
                if (!std::strcmp(key, "window_coord"))
                    return sol::make_object(lua, event.window_coord);
                else if (!std::strcmp(key, "scene_coord"))
                    return sol::make_object(lua, event.scene_coord);
                else if (!std::strcmp(key, "button"))
                    return sol::make_object(lua, event.btn);
                else if (!std::strcmp(key, "modifiers"))
                    return sol::make_object(lua, event.mods.value());
                else if (!std::strcmp(key, "over_scene"))
                    return sol::make_object(lua, event.over_scene);
                throw GameError(base::FormatString("No such mouse event index: %1", key));
            }
    );

    auto game_event = table.new_usertype<GameEvent>("GameEvent", sol::constructors<GameEvent()>(),
        sol::meta_function::index, [&L](const GameEvent& event, const char* key) {
            sol::state_view lua(L);
            if (!std::strcmp(key, "from"))
                return sol::make_object(lua, event.from);
            else if (!std::strcmp(key, "to"))
                return sol::make_object(lua, event.to);
            else if (!std::strcmp(key ,"message"))
                return sol::make_object(lua, event.message);
            else if (!std::strcmp(key, "value"))
                return sol::make_object(lua, event.value);
            throw GameError(base::FormatString("No such game event index: %1", key));
        },
        sol::meta_function::new_index, [&L](GameEvent& event, const char* key, sol::object value) {
            if (!std::strcmp(key, "from"))
                event.from = value.as<std::string>();
            else if (!std::strcmp(key, "to"))
                event.to = value.as<std::string>();
            else if (!std::strcmp(key, "message"))
                event.message = value.as<std::string>();
            else if (!std::strcmp(key, "value")) {
                if (value.is<bool>())
                    event.value = value.as<bool>();
                else if (value.is<int>())
                    event.value = value.as<int>();
                else if (value.is<float>())
                    event.value = value.as<float>();
                else if (value.is<std::string>())
                    event.value = value.as<std::string>();
                else if(value.is<glm::vec2>())
                    event.value = value.as<glm::vec2>();
                else if (value.is<glm::vec3>())
                    event.value = value.as<glm::vec3>();
                else if (value.is<glm::vec4>())
                    event.value = value.as<glm::vec4>();
                else if (value.is<base::Color4f>())
                    event.value = value.as<base::Color4f>();
                else if (value.is<base::FSize>())
                    event.value = value.as<base::FSize>();
                else if (value.is<base::FRect>())
                    event.value = key, value.as<base::FRect>();
                else if (value.is<base::FPoint>())
                    event.value = value.as<base::FPoint>();
                else throw GameError("Unsupported game event value type.");
            } else throw GameError(base::FormatString("No such game event index: %1", key));
        });

    auto kvstore = table.new_usertype<KeyValueStore>("KeyValueStore", sol::constructors<KeyValueStore()>(),
        sol::meta_function::index, [](const KeyValueStore& kv, const char* key, sol::this_state state) {
            sol::state_view lua(state);
            KeyValueStore::Value value;
            if (!kv.GetValue(key, &value))
                throw GameError("No such key value store index: " + std::string(key));
            return sol::make_object(lua, value);
        },
        sol::meta_function::new_index, [&L](KeyValueStore& kv, const char* key, sol::object value) {
            SetKvValue(kv, key, value);
        }
    );
    kvstore["SetValue"] = SetKvValue;
    kvstore["HasValue"] = &KeyValueStore::HasValue;
    kvstore["Clear"]    = &KeyValueStore::Clear;
    kvstore["Persist"]  = sol::overload(
        [](const KeyValueStore& kv, data::JsonObject& json) {
            kv.Persist(json);
        },
        [](const KeyValueStore& kv, data::Writer& writer) {
            kv.Persist(writer);
        });

    kvstore["Restore"]  = sol::overload(
            [](KeyValueStore& kv, const data::JsonObject& json) {
                return kv.Restore(json);
            },
            [](KeyValueStore& kv, const data::Reader& reader) {
                return kv.Restore(reader);
            });
    kvstore["GetValue"] = sol::overload(
        [](const KeyValueStore& kv, const char* key, sol::this_state state) {
            sol::state_view lua(state);
            KeyValueStore::Value value;
            if (!kv.GetValue(key, &value))
                throw GameError("No such key value key: " + std::string(key));
            return sol::make_object(lua, value);
        },
        [](KeyValueStore& kv, const char* key, sol::this_state state, sol::object value) {
            sol::state_view lua(state);
            KeyValueStore::Value val;
            if (kv.GetValue(key, &val))
                return sol::make_object(lua, val);
            SetKvValue(kv, key, value);
            return value;
        });
    kvstore["InitValue"] = [](KeyValueStore& kv, const char* key, sol::object value) {
        if (kv.HasValue(key))
            return;
        SetKvValue(kv, key, value);
    };
}

} // namespace
