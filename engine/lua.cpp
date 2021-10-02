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
#include "engine/event.h"
#include "engine/state.h"
#include "uikit/window.h"
#include "uikit/widget.h"
#include "wdk/keys.h"
#include "wdk/system.h"

using namespace game;

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
    using namespace engine;
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
    engine["DebugClear"] = [](LuaGame& self) {
        DebugClearAction action;
        self.PushAction(action);
    };
    engine["OpenUI"] = sol::overload(
        [](LuaGame& self, ClassHandle<uik::Window> model) {
            if (!model)
                throw std::runtime_error("Nil UI window object.");
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
                throw std::runtime_error("No such UI: " + name);
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
    using namespace engine;
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
    using namespace engine;
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
    else throw std::runtime_error("Unsupported key value store type.");
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
        throw std::runtime_error("No such widget type: " + type_string);

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
    widget["GetType"]        = [](const uik::Widget* widget) { return base::ToString(widget->GetType()); };
    widget["SetName"]        = &uik::Widget::SetName;
    widget["SetSize"]        = (void(Widget::*)(const uik::FSize&))&Widget::SetSize;
    widget["SetPosition"]    = (void(Widget::*)(const uik::FPoint&))&Widget::SetPosition;
    widget["TestFlag"]       = &TestFlag<uik::Widget>;
    widget["SetFlag"]        = &SetFlag<uik::Widget>;
    widget["IsEnabled"]      = &Widget::IsEnabled;
    widget["IsVisible"]      = &Widget::IsVisible;
    widget["Grow"]           = &Widget::Grow;
    widget["Translate"]      = &Widget::Translate;
    widget["SetVisible"]     = [](uik::Widget& widget, bool on_off) {
        widget.SetFlag(uik::Widget::Flags::VisibleInGame, on_off);
    };
    widget["Enable"] = [](uik::Widget& widget, bool on_off) {
        widget.SetFlag(uik::Widget::Flags::Enabled, on_off);
    };
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
            throw std::runtime_error("data reader chunk index out of bounds.");
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
    vec.set_function(sol::meta_function::multiplication, [](const Vector& vector, float scalar) {
        return vector * scalar;
    });
    vec.set_function(sol::meta_function::multiplication, [](float scalar, const Vector& vector) {
        return vector * scalar;
    });
    vec.set_function(sol::meta_function::division, [](const Vector& vector, float scalar) {
        return vector / scalar;
    });
    vec.set_function(sol::meta_function::to_string, [](const Vector& vector) {
        return base::ToString(vector);
    });
    vec["length"]    = [](const Vector& vec) { return glm::length(vec); };
    vec["normalize"] = [](const Vector& vec) { return glm::normalize(vec); };
}

} // namespace

namespace engine
{

LuaGame::LuaGame(const std::string& lua_path,
                 const std::string& game_script,
                 const std::string& game_home,
                 const std::string& game_name)
{
    mLuaState = std::make_unique<sol::state>();
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
    auto engine = table.new_usertype<LuaGame>("Engine");
    BindEngine(engine, *this);
    engine["SetViewport"] = [](LuaGame& self, const FRect& view) {
        self.mView = view;
    };
    mLuaState->script_file(base::JoinPath(lua_path, game_script));
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
bool LuaGame::LoadGame(const ClassLibrary* loader)
{
    mClasslib = loader;
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

void LuaGame::OnUIAction(uik::Window* ui, const uik::Window::WidgetAction& action)
{
    CallLua((*mLuaState)["OnUIAction"], ui, action);
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
    // ? is a wildcard (usually denoted by kleene star *)
    // todo: setup a package loader instead of messing with the path?
    // https://github.com/ThePhD/sol2/issues/90
    std::string path = (*state)["package"]["path"];
    path = path + ";" + mLuaPath + "/?.lua";
    path = path + ";" + mLuaPath + "/?/?.lua";
    (*state)["package"]["path"] = path;

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
            const auto& file = base::JoinPath(mLuaPath, script + ".lua");
            if (!base::FileExists(file))
            {
                ERROR("Entity '%1' Lua file '%2' was not found.", klass.GetName(), file);
                continue;
            }
            auto env = std::make_shared<sol::environment>(*state, sol::create, state->globals());
            state->script_file(file, *env);
            it = script_env_map.insert({script, env}).first;
            //DEBUG("Loaded Lua script file '%1'.", file);
        }
        entity_env_map[klass.GetId()] = it->second;
        DEBUG("Entity class '%1' script loaded.", klass.GetName());
    }

    std::unique_ptr<sol::environment> scene_env;
    if ((*scene)->HasScriptFile())
    {
        const auto& klass = scene->GetClass();
        const auto& script = klass.GetScriptFileId();
        const auto& file   = base::JoinPath(mLuaPath, script + ".lua");
        if (!base::FileExists(file)) {
            ERROR("Scene '%1' Lua file '%2' was not found.", klass.GetName(), file);
        } else {
            scene_env = std::make_unique<sol::environment>(*state, sol::create, state->globals());
            state->script_file(file, *scene_env);
            DEBUG("Scene class '%1' script loaded.", klass.GetName());
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
        CallLua((*mSceneEnv)["Tick"], mScene, game_time, dt);

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
        CallLua((*mSceneEnv)["Update"], mScene, game_time, dt);

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
            else throw std::runtime_error("Unsupported string format value type.");
        }
        return str;
    };
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
    rect.set_function(sol::meta_function::to_string, [](const FRect& rect) {
        return base::ToString(rect);
    });

    sol::constructors<base::FSize(), base::FSize(float, float)> size_ctors;
    auto size = table.new_usertype<base::FSize>("FSize", size_ctors);
    size["GetWidth"]  = &base::FSize::GetWidth;
    size["GetHeight"] = &base::FSize::GetHeight;
    size.set_function(sol::meta_function::multiplication, [](const base::FSize& size, float scalar) { return size * scalar; });
    size.set_function(sol::meta_function::addition, [](const base::FSize& lhs, const base::FSize& rhs) { return lhs + rhs; });
    size.set_function(sol::meta_function::subtraction, [](const base::FSize& lhs, const base::FSize& rhs) { return lhs - rhs; });
    size.set_function(sol::meta_function::to_string, [](const base::FSize& size) { return base::ToString(size); });

    sol::constructors<base::FPoint(), base::FPoint(float, float)> point_ctors;
    auto point = table.new_usertype<base::FPoint>("FPoint", point_ctors);
    point["GetX"] = &base::FPoint::GetX;
    point["GetY"] = &base::FPoint::GetY;
    point.set_function(sol::meta_function::addition, [](const base::FPoint& lhs, const base::FPoint& rhs) { return lhs + rhs; });
    point.set_function(sol::meta_function::subtraction, [](const base::FPoint& lhs, const base::FPoint& rhs) { return lhs - rhs; });
    point.set_function(sol::meta_function::to_string, [](const base::FPoint& point) { return base::ToString(point); });

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
                throw std::runtime_error("glm.vec2 access out of bounds");
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
                    throw std::runtime_error("glm.vec3 access out of bounds");
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
                    throw std::runtime_error("glm.vec4 access out of bounds");
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
        const auto mod = magic_enum::enum_cast<wdk::Keymod>(value);
        if (!mod.has_value())
            throw std::runtime_error("No such keymod value: " + std::to_string(value));
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
            throw std::runtime_error("No such key symbol: " + std::to_string(value));
        return wdk::TestKeyDown(key.value());
    };
    table["TestMod"] = [](int bits, int value) {
        const auto mod = magic_enum::enum_cast<wdk::Keymod>(value);
        if (!mod.has_value())
            throw std::runtime_error("No such modifier: " + std::to_string(value));
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
        else throw std::runtime_error("Unsupported material uniform type.");
    };
    drawable["GetUniform"] = [](const DrawableItem& item, const char* name, sol::this_state state) {
        sol::state_view L(state);
        if (const auto* value = item.FindMaterialParam(name))
            return sol::make_object(L, *value);
        throw std::runtime_error("No such material uniform: " + std::string(name));
    };
    drawable["HasUniform"] = &DrawableItem::HasMaterialParam;
    drawable["DeleteUniform"] = &DrawableItem::DeleteMaterialParam;

    auto body = table.new_usertype<RigidBodyItem>("RigidBody");
    body["GetFriction"]           = &RigidBodyItem::GetFriction;
    body["GetRestitution"]        = &RigidBodyItem::GetRestitution;
    body["GetAngularDamping"]     = &RigidBodyItem::GetAngularDamping;
    body["GetLinearDamping"]      = &RigidBodyItem::GetLinearDamping;
    body["GetDensity"]            = &RigidBodyItem::GetDensity;
    body["GetPolygonShapeId"]     = &RigidBodyItem::GetPolygonShapeId;
    body["GetLinearVelocity"]     = &RigidBodyItem::GetLinearVelocity;
    body["GetAngularVelocity"]    = &RigidBodyItem::GetAngularVelocity;
    body["AdjustLinearVelocity"]  = &RigidBodyItem::AdjustLinearVelocity;
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
    entity["HasBeenKilled"]        = &Entity::HasBeenKilled;
    entity["HasBeenSpawned"]       = &Entity::HasBeenSpawned;
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
    entity_args["logging"]  = sol::property(&EntityArgs::enable_logging);

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
    physics["SetLinearVelocity"]    = (void(PhysicsEngine::*)(const std::string&, const glm::vec2&) const)&PhysicsEngine::SetLinearVelocity;
    physics["SetLinearVelocity"]    = (void(PhysicsEngine::*)(const EntityNode&, const glm::vec2&) const)&PhysicsEngine::SetLinearVelocity;

    auto audio = table.new_usertype<AudioEngine>("Audio");
    audio["PrepareMusicGraph"] = sol::overload(
            [](AudioEngine& engine, std::shared_ptr<const audio::GraphClass> klass) {
                if (!klass)
                    throw std::runtime_error("Nil audio graph class.");
                return engine.PrepareMusicGraph(klass);
            },
            [](AudioEngine& engine, const std::string& name) {
                const auto* lib = engine.GetClassLibrary();
                auto klass = lib->FindAudioGraphClassByName(name);
                if (!klass)
                    throw std::runtime_error("No such audio graph: " + name);
                return engine.PrepareMusicGraph(klass);
            });
    audio["PlayMusic"] = sol::overload(
            [](AudioEngine& engine, std::shared_ptr<const audio::GraphClass> klass) {
                if (!klass)
                    throw std::runtime_error("Nil audio graph class.");
                return engine.PlayMusic(klass);
            },
            [](AudioEngine& engine, std::shared_ptr<const audio::GraphClass> klass, unsigned when) {
                if (!klass)
                    throw std::runtime_error("Nil audio graph class.");
                return engine.PlayMusic(klass, when);
            },
            [](AudioEngine& engine, const std::string& name, unsigned when) {
                const auto* lib = engine.GetClassLibrary();
                auto klass = lib->FindAudioGraphClassByName(name);
                if (!klass)
                    throw std::runtime_error("No such audio graph: " + name);
                return engine.PlayMusic(klass, when);
            },
            [](AudioEngine& engine, const std::string& name) {
                const auto* lib = engine.GetClassLibrary();
                auto klass = lib->FindAudioGraphClassByName(name);
                if (!klass)
                    throw std::runtime_error("No such audio graph: " + name);
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
            throw std::runtime_error("No such audio effect:" + effect);
        engine.SetMusicEffect(track, duration, effect_value.value());
    };
    audio["PlaySoundEffect"] = sol::overload(
            [](AudioEngine& engine, std::shared_ptr<const audio::GraphClass> klass, unsigned when) {
                if (!klass)
                    throw std::runtime_error("Nil audio effect graph class.");
                return engine.PlaySoundEffect(klass, when);
            },
            [](AudioEngine& engine, std::shared_ptr<const audio::GraphClass> klass) {
                if (!klass)
                    throw std::runtime_error("Nil audio effect graph class.");
                return engine.PlaySoundEffect(klass, 0);
            },
            [](AudioEngine& engine, const std::string& name, unsigned when) {
                const auto* lib = engine.GetClassLibrary();
                auto klass = lib->FindAudioGraphClassByName(name);
                if (!klass)
                    throw std::runtime_error("No such audio effect graph:" + name);
                return engine.PlaySoundEffect(klass, when);
            },
            [](AudioEngine& engine, const std::string& name) {
                const auto* lib = engine.GetClassLibrary();
                auto klass = lib->FindAudioGraphClassByName(name);
                if (!klass)
                    throw std::runtime_error("No such audio effect graph:" + name);
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
                throw std::runtime_error(base::FormatString("No such audio event index: %1", key));
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
                throw std::runtime_error(base::FormatString("No such mouse event index: %1", key));
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
            throw std::runtime_error(base::FormatString("No such game event index: %1", key));
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
                else throw std::runtime_error("Unsupported game event value type.");
            }
        });

    auto kvstore = table.new_usertype<KeyValueStore>("KeyValueStore", sol::constructors<KeyValueStore()>(),
        sol::meta_function::index, [](const KeyValueStore& kv, const char* key, sol::this_state state) {
            sol::state_view lua(state);
            KeyValueStore::Value value;
            if (!kv.GetValue(key, &value))
                throw std::runtime_error("No such key value store index: " + std::string(key));
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
                throw std::runtime_error("No such key value key: " + std::string(key));
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
