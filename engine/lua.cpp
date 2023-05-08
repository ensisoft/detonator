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
#include "data/io.h"
#include "audio/graph.h"
#include "game/entity.h"
#include "game/util.h"
#include "game/scene.h"
#include "game/transform.h"
#include "graphics/material.h"
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
using GameError = std::runtime_error;

// data policy class for ArrayInterface below
// uses a non-owning pointer to data that comes from
// somewhere else such as ScriptVar
template<typename T>
class ArrayDataPointer
{
public:
    ArrayDataPointer(std::vector<T>* array)
      : mArray(array)
    {}
protected:
    ArrayDataPointer() = default;
    void CommitChanges()
    {
        // do nothing is intentional.
    }
    std::vector<T>& GetArray()
    { return *mArray; }
    const std::vector<T>& GetArray() const
    { return *mArray; }
private:
    std::vector<T>* mArray = nullptr;
};

// data policy class for ArrayInterface below.
// has a copy of the data and then exposes a pointer to it.
template<typename T>
class ArrayDataObject
{
public:
    ArrayDataObject(const std::vector<T>& array)
      : mArrayData(array)
    {}
    ArrayDataObject(std::vector<T>&& array)
      : mArrayData(array)
    {}
protected:
    ArrayDataObject() = default;
    void CommitChanges()
    {
        // do nothing intentional.
    }
    std::vector<T>& GetArray()
    { return mArrayData; }
    const std::vector<T>& GetArray() const
    { return mArrayData; }
private:
    std::vector<T> mArrayData;
};

template<typename T>
class ArrayObjectReference;

template<>
class ArrayObjectReference<Entity*>
{
public:
    ArrayObjectReference(const ScriptVar* var, Scene* scene)
      : mVar(var)
      , mScene(scene)
    {
        // resolve object IDs to the actual objects
        const auto& refs = mVar->GetArray<ScriptVar::EntityReference>();
        for (const auto& ref : refs)
            mEntities.push_back(mScene->FindEntityByInstanceId(ref.id));
    }
protected:
    ArrayObjectReference() = default;
    void CommitChanges()
    {
        // resolve entity objects back into their IDs
        // and store the changes in the script variable.
        std::vector<ScriptVar::EntityReference> refs;
        for (auto* entity : mEntities)
        {
            ScriptVar::EntityReference ref;
            ref.id = entity ? entity->GetId() : "";
        }
        mVar->SetArray(std::move(refs));
    }
    std::vector<Entity*>& GetArray()
    { return mEntities; }
    const std::vector<Entity*>& GetArray() const
    { return mEntities; }
private:
    const ScriptVar* mVar = nullptr;
    Scene* mScene = nullptr;
    std::vector<Entity*> mEntities;
};

template<>
class ArrayObjectReference<EntityNode*>
{
public:
    ArrayObjectReference(const ScriptVar* var, Entity* entity)
      : mVar(var)
      , mEntity(entity)
    {
        const auto& refs = mVar->GetArray<ScriptVar::EntityNodeReference>();
        // The entity node instance IDs are dynamic. since the
        // reference is placed during design time it's based on the class IDs
        for (const auto& ref : refs)
            mNodes.push_back(mEntity->FindNodeByClassId(ref.id));
    }
protected:
    ArrayObjectReference() = default;
    void CommitChanges()
    {
        std::vector<ScriptVar::EntityNodeReference> refs;
        for (auto* node : mNodes)
        {
            ScriptVar::EntityNodeReference ref;
            ref.id = node ? node->GetClassId() : "";
        }
        mVar->SetArray(std::move(refs));
    }
    std::vector<EntityNode*>& GetArray()
    { return mNodes; }
    const std::vector<EntityNode*>& GetArray() const
    { return mNodes; }
private:
    const ScriptVar* mVar = nullptr;
    Entity* mEntity = nullptr;
    std::vector<EntityNode*> mNodes;
};

// template to adapt an underlying std::vector
// to sol2's Lua container interface. the vector
// is not owned by the array interface object and
// may come for example from a scene/entity scripting
// variable when the variable is an array.
template<typename T, template<typename> class DataPolicy = ArrayDataPointer>
class ArrayInterface : public DataPolicy<T> {
public:
    using value_type = typename std::vector<T>::value_type;
    // this was using const_iterator and i'm not sure where that
    // came from (perhaps from the sol3 "documentation"). Anyway,
    // using const_iterator makes emscripten build shit itself.
    using iterator   = typename std::vector<T>::iterator;
    using size_type  = typename std::vector<T>::size_type;

    using BaseClass = DataPolicy<T>;

    template<typename Arg0>
    ArrayInterface(bool read_only, Arg0&& arg0)
      : BaseClass(std::forward<Arg0>(arg0))
      , mReadOnly(read_only)
    {}
    template<typename Arg0, typename Arg1>
    ArrayInterface(bool read_only, Arg0&& arg0, Arg1&& arg1)
      : BaseClass(std::forward<Arg0>(arg0),
                  std::forward<Arg1>(arg1))
      , mReadOnly(read_only)
    {}

    iterator begin() noexcept
    { return BaseClass::GetArray().begin(); }
    iterator end() noexcept
    { return BaseClass::GetArray().end(); }
    size_type size() const noexcept
    { return BaseClass::GetArray().size(); }
    bool empty() const noexcept
    { return BaseClass::GetArray().empty(); }
    void push_back(const T& value)
    {
        BaseClass::GetArray().push_back(value);
        BaseClass::CommitChanges();
    }
    T GetItemFromLua(unsigned index) const
    {
        // lua uses 1 based indexing.
        index = index - 1;
        if (index >= size())
            throw GameError("ArrayInterface access out of bounds.");
        return BaseClass::GetArray()[index];
    }
    void SetItemFromLua(unsigned index, const T& item)
    {
        // Lua uses 1 based indexing
        index = index - 1;
        if (index >= size())
            throw GameError("ArrayInterface access out of bounds.");
        if (IsReadOnly())
            throw GameError("Trying to write to read only array.");
        BaseClass::GetArray()[index] = item;
        BaseClass::CommitChanges();
    }
    void PopBack()
    {
        if (IsReadOnly())
            throw GameError("Trying to modify read only array.");
        auto& arr = BaseClass::GetArray();
        if (arr.empty())
            return;
        arr.pop_back();
        BaseClass::CommitChanges();
    }
    void PopFront()
    {
        if (IsReadOnly())
            throw GameError("Trying to modify read only array.");
        auto& arr = BaseClass::GetArray();
        if (arr.empty())
            return;
        arr.erase(arr.begin());
        BaseClass::CommitChanges();
    }
    void Clear()
    {
        if (IsReadOnly())
            throw GameError("Trying to modify read only array.");
        BaseClass::GetArray().clear();
        BaseClass::CommitChanges();
    }

    T GetFirst() const
    {
        return GetItemFromLua(1);
    }
    T GetLast() const
    {
        return GetItemFromLua(size());
    }

    T GetItem(unsigned index) const
    { return base::SafeIndex(BaseClass::GetArray(), index); }

    bool IsReadOnly() const
    { return mReadOnly; }

private:
    bool mReadOnly = false;
};

template<typename T, template<typename> class DataPolicy = ArrayDataPointer>
void BindArrayInterface(sol::table& table, const char* name)
{
    using Type = ArrayInterface<T, DataPolicy>;

    // regarding the array indexing for the subscript operator.
    // Lua uses 1 based indexing and allows (with built-in arrays)
    // access to indices that don't exist. For example, you can do
    //   local foo = {'foo', 'bar'}
    //   print(foo[0])
    //   print(foo[3])
    //
    // this will print nil twice.
    // Lua also allows for holes to be had in the array. for example
    //    foo[4] = 'keke'
    //    print(foo[4])
    //    print(foo[3])
    //
    // will print keke followed by nil
    //
    // Going to stick to more C++ like semantics here and say that
    // trying to access an index that doesn't exist is Lua application error.
    auto arr = table.new_usertype<Type>(name,
        sol::meta_function::index, &Type::GetItemFromLua,
        sol::meta_function::new_index, &Type::SetItemFromLua);
    arr["IsEmpty"]    = &Type::empty;
    arr["Size"]       = &Type::size;
    arr["IsReadOnly"] = &Type::IsReadOnly;
    arr["GetItem"]    = &Type::GetItemFromLua;
    arr["SetItem"]    = &Type::SetItemFromLua;
    arr["PopBack"]    = &Type::PopBack;
    arr["PopFront"]   = &Type::PopFront;
    arr["First"]      = &Type::GetFirst;
    arr["Last"]       = &Type::GetLast;
    arr["PushBack"]   = &Type::push_back;
    arr["Clear"]      = &Type::Clear;
}

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
    const auto read_only = var.IsReadOnly();
    if (type == ScriptVar::Type::Boolean)
    {
        using ArrayType = ArrayInterface<bool, ArrayDataPointer>;
        if (var.IsArray())
            return sol::make_object(lua, ArrayType(read_only, &var.GetArray<bool>()));
        return sol::make_object(lua, var.GetValue<bool>());
    }
    else if (type == ScriptVar::Type::Float)
    {
        using ArrayType = ArrayInterface<float, ArrayDataPointer>;
        if (var.IsArray())
            return sol::make_object(lua, ArrayType(read_only, &var.GetArray<float>()));
        return sol::make_object(lua, var.GetValue<float>());
    }
    else if (type == ScriptVar::Type::String)
    {
        using ArrayType = ArrayInterface<std::string, ArrayDataPointer>;
        if (var.IsArray())
            return sol::make_object(lua,  ArrayType(read_only, &var.GetArray<std::string>()));
        return sol::make_object(lua, var.GetValue<std::string>());
    }
    else if (type == ScriptVar::Type::Integer)
    {
        using ArrayType = ArrayInterface<int, ArrayDataPointer>;
        if (var.IsArray())
            return sol::make_object(lua, ArrayType(read_only, &var.GetArray<int>()));
        return sol::make_object(lua, var.GetValue<int>());
    }
    else if (type == ScriptVar::Type::Vec2)
    {
        using ArrayType = ArrayInterface<glm::vec2, ArrayDataPointer>;
        if (var.IsArray())
            return sol::make_object(lua, ArrayType(read_only, &var.GetArray<glm::vec2>()));
        return sol::make_object(lua, var.GetValue<glm::vec2>());
    } else BUG("Unhandled ScriptVar type.");
}

template<typename T>
sol::object ResolveEntityReferences(const T& whatever, const ScriptVar& var, sol::state_view& state)
{
    // this is a generic resolver for cases when a reference cannot
    // be resolved. for example if entity contains entity references
    // we can't resolve those. only scene can resolve entity references.
    // this generic resolver will simply just return the string IDs.
    if (var.IsArray())
    {
        using ArrayType = ArrayInterface<std::string, ArrayDataObject>;

        const auto& refs = var.GetArray<ScriptVar::EntityReference>();

        std::vector<std::string> strs;
        for (const auto& ref : refs)
            strs.push_back(ref.id);

        return sol::make_object(state, ArrayType(true, std::move(strs)));
    }

    const auto& ref = var.GetValue<ScriptVar::EntityReference>();

    return sol::make_object(state, ref.id);
}

sol::object ResolveEntityReferences(game::Scene& scene, const ScriptVar& var, sol::state_view& state)
{
    // the reference to the entity is placed during design time
    // and the scene placement node has an ID which becomes the
    // entity instance ID when the scene is instantiated.
    if (var.IsArray())
    {
        using ArrayType = ArrayInterface<Entity*, ArrayObjectReference>;

        return sol::make_object(state, ArrayType(var.IsReadOnly(), &var, &scene));
    }
    const auto& ref = var.GetValue<ScriptVar::EntityReference>();

    return sol::make_object(state, scene.FindEntityByInstanceId(ref.id));
}

template<typename T>
sol::object ResolveNodeReferences(const T& whatever, const ScriptVar& var, sol::state_view& state)
{
    // this is a generic resolver for cases when a reference cannot be resolved.
    // for example if a scene contains entity node references we can't resolve those,
    // but only entity can resolve entity node references.
    // this generic resolver will simply just return the string IDs
    if (var.IsArray())
    {
        using ArrayType = ArrayInterface<std::string, ArrayDataObject>;

        const auto& refs = var.GetArray<ScriptVar::EntityNodeReference>();

        std::vector<std::string> strs;
        for (const auto& ref : refs)
            strs.push_back(ref.id);

        return sol::make_object(state, ArrayType(true, std::move(strs)));
    }
    const auto& ref = var.GetValue<ScriptVar::EntityNodeReference>();

    return sol::make_object(state, ref.id);

}

sol::object ResolveNodeReferences(game::Entity& entity, const ScriptVar& var, sol::state_view& state)
{
    // The entity node instance IDs are dynamic. since the
    // reference is placed during design time it's based on the class IDs
    if (var.IsArray())
    {
        using ArrayType = ArrayInterface<EntityNode*, ArrayObjectReference>;

        return sol::make_object(state, ArrayType(var.IsReadOnly(), &var, &entity));
    }

    const auto& ref = var.GetValue<ScriptVar::EntityNodeReference>();

    return sol::make_object(state, entity.FindNodeByClassId(ref.id));
}

template<typename Type>
sol::object ResolveMaterialReferences(Type& object, const ScriptVar& var, sol::state_view& state)
{
    engine::ClassLibrary* lib = state["ClassLib"];

    if (var.IsArray())
    {
        using MaterialClassHandle = engine::ClassLibrary::ClassHandle<const gfx::MaterialClass>;

        using ArrayType = ArrayInterface<MaterialClassHandle, ArrayDataObject>;

        const auto& refs = var.GetArray<ScriptVar::MaterialReference>();

        std::vector<MaterialClassHandle> objects;
        for (const auto& ref : refs)
        {
            objects.push_back(lib->FindMaterialClassById(ref.id));
        }

        return sol::make_object(state, ArrayType(true, std::move(objects)));
    }

    const auto& ref = var.GetValue<ScriptVar::MaterialReference>();

    return sol::make_object(state, lib->FindMaterialClassById(ref.id));
}

template<typename Type>
sol::object GetScriptVar(Type& object, const char* key, sol::this_state state, sol::this_environment this_env)
{
    using namespace engine;
    sol::state_view lua(state);
    const ScriptVar* var = object.FindScriptVarByName(key);
    if (!var)
        throw GameError(base::FormatString("No such variable: '%1'", key));
    if (var->IsPrivate())
    {
        // looks like a sol2 bug. this_env is sometimes nullopt!
        // https://github.com/ThePhD/sol2/issues/1464
        if (this_env)
        {
            sol::environment& environment(this_env);
            const std::string script_id = environment["__script_id__"];
            if (object.GetScriptFileId() != script_id)
                throw GameError(base::FormatString("Trying to access private variable: '%1'", key));
        }
    }

    if (var->GetType() == game::ScriptVar::Type::EntityReference)
    {
        return ResolveEntityReferences(object, *var, lua);
    }
    else if (var->GetType() == game::ScriptVar::Type::EntityNodeReference)
    {
        return ResolveNodeReferences(object, *var, lua);
    }
    else if (var->GetType() == game::ScriptVar::Type::MaterialReference)
    {
        return ResolveMaterialReferences(object, *var, lua);
    }

    else return ObjectFromScriptVarValue(*var, state);
}
template<typename Type>
void SetScriptVar(Type& object, const char* key, sol::object value, sol::this_state state, sol::this_environment this_env)
{
    using namespace engine;
    const ScriptVar* var = object.FindScriptVarByName(key);
    if (var == nullptr)
        throw GameError(base::FormatString("No such variable: '%1'", key));
    else if (var->IsReadOnly())
        throw GameError(base::FormatString("Trying to write to a read only variable: '%1'", key));
    else if (var->IsPrivate())
    {
        // looks like a sol2 bug. this_env is sometimes nullopt!
        // https://github.com/ThePhD/sol2/issues/1464
        if (this_env)
        {
            sol::environment& environment(this_env);
            const std::string script_id = environment["__script_id__"];
            if (object.GetScriptFileId() != script_id)
                throw GameError(base::FormatString("Trying to access private variable: '%1'", key));
        }
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
    else if (value.is<game::EntityNode*>() && var->HasType<game::ScriptVar::EntityNodeReference>())
        var->SetValue(game::ScriptVar::EntityNodeReference{value.as<game::EntityNode*>()->GetClassId()});
    else if (value.is<game::Entity*>() && var->HasType<game::ScriptVar::EntityReference>())
        var->SetValue(game::ScriptVar::EntityReference{value.as<game::Entity*>()->GetId()});
    else if (!value.valid()) {
        if (var->HasType<game::ScriptVar::EntityNodeReference>())
            var->SetValue(game::ScriptVar::EntityNodeReference{""});
        else if (var->HasType<game::ScriptVar::EntityReference>())
            var->SetValue(game::ScriptVar::EntityReference{""});
    }
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
    else throw GameError("Unsupported key-value store value type.");
}

// WAR. G++ 10.2.0 has internal segmentation fault when using the Get/SetScriptVar helpers
// directly in the call to create new usertype. adding these specializations as a workaround.
template sol::object GetScriptVar<game::Scene>(game::Scene&, const char*, sol::this_state, sol::this_environment);
template sol::object GetScriptVar<game::Entity>(game::Entity&, const char*, sol::this_state, sol::this_environment);
template void SetScriptVar<game::Scene>(game::Scene&, const char* key, sol::object, sol::this_state, sol::this_environment);
template void SetScriptVar<game::Entity>(game::Entity&, const char* key, sol::object, sol::this_state, sol::this_environment);

sol::object GetAnimatorVar(const game::Animator& animator, const char* key, sol::this_state this_state)
{
    sol::state_view state(this_state);
    if (const auto* ptr = animator.FindValue(key))
    {
        if (const auto* p = std::get_if<bool>(ptr))
            return sol::make_object(state, *p);
        else if (const auto* p = std::get_if<int>(ptr))
            return sol::make_object(state, *p);
        else if (const auto* p = std::get_if<float>(ptr))
            return sol::make_object(state, *p);
        else if (const auto* p = std::get_if<std::string>(ptr))
            return sol::make_object(state, *p);
        else if (const auto* p = std::get_if<glm::vec2>(ptr))
            return sol::make_object(state, *p);
        else BUG("Missing animator value type.");
    }
    return sol::nil;
}
void SetAnimatorVar(game::Animator& animator, const char* key, sol::object value, sol::this_state this_state)
{
    if (value.is<bool>())
        animator.SetValue(key, value.as<bool>());
    else if (value.is<int>())
        animator.SetValue(key, value.as<int>());
    else if (value.is<float>())
        animator.SetValue(key, value.as<float>());
    else if (value.is<std::string>())
        animator.SetValue(key, value.as<std::string>());
    else if (value.is<glm::vec2>())
        animator.SetValue(key, value.as<glm::vec2>());
    else throw GameError("Unsupported animator value type.");
}

// shim to help with uik::WidgetCast overload resolution.
template<typename Widget> inline
Widget* WidgetCast(uik::Widget* widget)
{ return uik::WidgetCast<Widget>(widget); }

template<typename Actuator> inline
Actuator* ActuatorCast(game::Actuator* actuator)
{ return dynamic_cast<Actuator*>(actuator); }

sol::object WidgetObjectCast(sol::this_state state, uik::Widget* widget)
{
    sol::state_view lua(state);
    const auto type = widget->GetType();
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
    widget["GetId"]               = &Widget::GetId;
    widget["GetName"]             = &Widget::GetName;
    widget["GetHash"]             = &Widget::GetHash;
    widget["GetSize"]             = &Widget::GetSize;
    widget["GetPosition"]         = &Widget::GetPosition;
    widget["GetType"]             = [](const Widget* widget) { return base::ToString(widget->GetType()); };
    widget["SetName"]             = &Widget::SetName;
    widget["TestFlag"]            = &TestFlag<Widget>;
    widget["SetFlag"]             = &SetFlag<Widget>;
    widget["IsEnabled"]           = &Widget::IsEnabled;
    widget["IsVisible"]           = &Widget::IsVisible;
    widget["Grow"]                = &Widget::Grow;
    widget["Translate"]           = &Widget::Translate;
    widget["SetStyleProperty"]    = &Widget::SetStyleProperty;
    widget["DeleteStyleProperty"] = &Widget::DeleteStyleProperty;
    widget["GetStyleProperty"]    = [](Widget& widget, const std::string& key, sol::this_state state) {
        sol::state_view lua(state);
        if (const auto* prop = widget.GetStyleProperty(key))
            return sol::make_object(lua, *prop);
        return sol::make_object(lua, sol::nil);
    };
    widget["SetStyleMaterial"]    = &Widget::SetStyleMaterial;
    widget["DeleteStyleMaterial"] = &Widget::DeleteStyleMaterial;
    widget["GetStyleMaterial"]    = [](Widget& widget, const std::string& key, sol::this_state state) {
        sol::state_view lua(state);
        if (const auto* str = widget.GetStyleMaterial(key))
            return sol::make_object(lua, *str);
        return sol::make_object(lua, sol::nil);
    };
    widget["SetColor"]    = &Widget::SetColor;
    widget["SetMaterial"] = &Widget::SetMaterial;
    widget["SetGradient"] = &Widget::SetGradient;

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

template<typename Actuator>
void BindActuatorInterface(sol::usertype<Actuator>& actuator)
{
    actuator["GetClassId"]   = &Actuator::GetClassId;
    actuator["GetClassName"] = &Actuator::GetClassName;
    actuator["GetNodeId"]    = &Actuator::GetNodeId;
    actuator["GetStartTime"] = &Actuator::GetStartTime;
    actuator["GetDuration"]  = &Actuator::GetDuration;
}

// the problem with using a std random number generation is that
// the results may not be portable across implementations and it seems
// that the standard Lua math random stuff has this problem.
// http://lua-users.org/wiki/MathLibraryTutorial
// "... math.randomseed will call the underlying C function srand ..."
class RandomEngine {
public:
    void Seed(int seed)
    { mTwister = boost::random::mt19937(seed); }
    int NextInt()
    { return NextInt(INT_MIN, INT_MAX); }
    int NextInt(int min, int max)
    {
        boost::random::uniform_int_distribution<int> dist(min, max);
        return dist(mTwister);
    }
    float NextFloat(float min, float max)
    {
        boost::random::uniform_real_distribution<float> dist(min, max);
        return dist(mTwister);
    }
    static RandomEngine& Get()
    {
        static RandomEngine instance;
        return instance;
    }
    static void SeedGlobal(int seed)
    {
        Get().Seed(seed);
    }
    static int NextIntGlobal()
    {
        return Get().NextInt();
    }
    static int NextIntGlobal(int min, int max)
    {
        return Get().NextInt(min, max);
    }
    static float NextFloatGlobal(float min, float max)
    {
        return Get().NextFloat(min, max);
    }
private:
    boost::random::mt19937 mTwister;
};

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
    T GetNext()
    {
        if (mBegin == mResult.end())
            throw GameError("ResultSet iteration error.");
        return *mBegin++;
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
    void EraseCurrent()
    {
        ASSERT(mBegin != mResult.end());
        mBegin = mResult.erase(mBegin);
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
            throw GameError("ResultVector iteration error.");
        return *mBegin;
    }
    const T& GetNext()
    {
        if (mBegin == mResult.end())
            throw GameError("ResultVector iteration error.");
        return *mBegin++;
    }
    const T& GetAt(size_t index) const
    {
        if (index >= mResult.size())
            throw GameError("ResultVector index out of bounds.");
        return mResult[index];
    }
    size_t GetSize() const
    { return mResult.size(); }

    static ResultVector Join(const ResultVector& lhs, const ResultVector& rhs)
    {
        Vector vec;
        base::AppendVector(vec, lhs.mResult);
        base::AppendVector(vec, rhs.mResult);
        return  ResultVector(std::move(vec));
    }
private:
    Vector mResult;
    typename Vector::iterator mBegin;
};

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

    // todo: improve the package loading so that we can realize
    // other locations for the Lua scripts as well. This might
    // happen for instance when an exported Lua script is imported
    // into another project under some folder under the workspace.
    // In this case the imported Lua script file would not be in
    // workspace/lua but something like workspace/blabla/lua/
    mLuaState->add_package_loader([this](std::string module) {
        ASSERT(mDataLoader);
        if (!base::EndsWith(module, ".lua"))
            module += ".lua";

        DEBUG("Loading Lua module. [module=%1]", module);
        const auto& file = base::JoinPath(mLuaPath, module);
        const auto& buff = mDataLoader->LoadEngineDataFile(file);
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
                throw GameError("Nil scene class");
            PlayAction play;
            play.scene = game::CreateSceneInstance(klass);
            auto* ret = play.scene.get();
            self.mActionQueue.push(std::move(play));
            return ret;
        },
        [](LuaRuntime& self, std::string name) {
            auto klass = self.mClassLib->FindSceneClassByName(name);
            if (!klass)
                throw GameError("No such scene class: " + name);
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
                throw GameError("Nil UI window object.");
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
                throw GameError("No such UI: " + name);
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

        const auto& script_uri  = mGameScript;
        const auto& script_buff = mDataLoader->LoadEngineDataUri(script_uri);
        DEBUG("Loading main game script. [uri='%1']", script_uri);

        if (!script_buff)
        {
            ERROR("Failed to load main game script file. [uri='%1']", script_uri);
            throw std::runtime_error("failed to load main game script.");
        }
        const auto& script_file = script_buff->GetName();
        const auto& script_view = script_buff->GetStringView();
        auto result = mLuaState->script(script_view, *mGameEnv);
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

void LuaRuntime::BeginPlay(Scene* scene)
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

            const auto& script_file = script_buff->GetName();
            const auto& script_view = script_buff->GetStringView();
            const auto& result = mLuaState->script(script_view, *script_env);
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

            const auto& script_file = script_buff->GetName();
            const auto& script_view = script_buff->GetStringView();
            const auto& result = mLuaState->script(script_view, *script_env);
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

            const auto& script_file = script_buff->GetName();
            const auto& script_view = script_buff->GetStringView();
            const auto& result = mLuaState->script(script_view, *scene_env);
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
    (*mLuaState)["Scene"] = mScene;

    if (mGameEnv)
        CallLua((*mGameEnv)["BeginPlay"], scene);

    if (mSceneEnv)
        CallLua((*mSceneEnv)["BeginPlay"], scene);

    for (size_t i=0; i<scene->GetNumEntities(); ++i)
    {
        auto* entity = &mScene->GetEntity(i);
        if (mSceneEnv)
            CallLua((*mSceneEnv)["SpawnEntity"], scene, entity);

        if (auto* env = GetTypeEnv(entity->GetClass()))
        {
            CallLua((*env)["BeginPlay"], entity, scene);
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

void LuaRuntime::EndPlay(Scene* scene)
{
    if (mGameEnv)
        CallLua((*mGameEnv)["EndPlay"], scene);

    if (mSceneEnv)
        CallLua((*mSceneEnv)["EndPlay"], scene);

    mSceneEnv.reset();
    mEntityEnvs.clear();
    mScene = nullptr;
    (*mLuaState)["Scene"] = nullptr;
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
            CallLua((*mSceneEnv)["SpawnEntity"], mScene, entity);

        if (auto* env = GetTypeEnv(entity->GetClass()))
        {
            CallLua((*env)["BeginPlay"], entity, mScene);
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
             CallLua((*mSceneEnv)["KillEntity"], mScene, entity);

         if (auto* env = GetTypeEnv(entity->GetClass()))
         {
             CallLua((*env)["EndPlay"], entity, mScene);
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
        throw GameError(base::FormatString("CallMethod method '%1' failed. %2", method, err.what()));
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

    const auto& script_file = script_buff->GetName();
    const auto& script_view = script_buff->GetStringView();
    const auto& result = mLuaState->script(script_view, *script_env);
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

    const auto& script_file = script_buff->GetName();
    const auto& script_view = script_buff->GetStringView();
    const auto& result = mLuaState->script(script_view, *script_env);
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
    const auto& script_file = script_buff->GetName();
    const auto& script_view = script_buff->GetStringView();
    auto script_env = std::make_unique<sol::environment>(*mLuaState, sol::create, mLuaState->globals());
    (*script_env)["__script_id__"] = script;
    const auto& result = mLuaState->script(script_view, *script_env);
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
    util["RandomSeed"] = &RandomEngine::SeedGlobal;
    util["Random"] = sol::overload(
        [](sol::this_state state) {
            sol::state_view view(state);
            return sol::make_object(view, RandomEngine::NextIntGlobal());
        },
        [](sol::this_state state, int min, int max) {
            sol::state_view view(state);
            return sol::make_object(view, RandomEngine::NextIntGlobal(min, max));
        },
        [](sol::this_state state, float min, float max) {
            sol::state_view view(state);
            return sol::make_object(view, RandomEngine::NextFloatGlobal(min, max));
        });

    sol::constructors<RandomEngine()> random_engine_ctor;
    auto random_engine = util.new_usertype<RandomEngine>("RandomEngine", random_engine_ctor);
    random_engine["Seed"] = &RandomEngine::Seed;
    random_engine["Random"] = sol::overload(
        [](RandomEngine& engine, sol::this_state state) {
            sol::state_view view(state);
            return sol::make_object(view, engine.NextInt());
        },
        [](RandomEngine& engine, sol::this_state state, int min, int max) {
            sol::state_view view(state);
            return sol::make_object(view, engine.NextInt(min, max));
        },
        [](RandomEngine& engine, sol::this_state state, float min, float max) {
            sol::state_view view(state);
            return sol::make_object(view, engine.NextFloat(min, max));
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

    BindArrayInterface<int,         ArrayDataPointer>(util, "IntArrayInterface");
    BindArrayInterface<float,       ArrayDataPointer>(util, "FloatArrayInterface");
    BindArrayInterface<bool,        ArrayDataPointer>(util, "BoolArrayInterface");
    BindArrayInterface<std::string, ArrayDataPointer>(util, "StringArrayInterface");
    BindArrayInterface<glm::vec2,   ArrayDataPointer>(util, "Vec2ArrayInterface");

    BindArrayInterface<std::string, ArrayDataObject>(util, "StringArray");

    BindArrayInterface<Entity*,     ArrayObjectReference>(util, "EntityRefArray");
    BindArrayInterface<EntityNode*, ArrayObjectReference>(util, "EntityNodeRefArray");

    using MaterialClassHandle = engine::ClassLibrary::ClassHandle<const gfx::MaterialClass>;
    BindArrayInterface<MaterialClassHandle, ArrayDataObject>(util, "MaterialRefArray");

    util["Join"] = [](const ArrayInterface<std::string>& array, const std::string& separator) {
        std::string ret;
        for (unsigned i=0; i<array.size(); ++i) {
            ret += array.GetItem(i);
            ret += separator;
        }
        return ret;
    };
}

void BindBase(sol::state& L)
{
    auto base = L.create_named_table("base");
    base["debug"] = [](const std::string& str) { DEBUG(str); };
    base["warn"]  = [](const std::string& str) { WARN(str); };
    base["error"] = [](const std::string& str) { ERROR(str); };
    base["info"]  = [](const std::string& str) { INFO(str); };

    //auto math  = L.create_named_table("math");
    auto& math = base;
    math["wrap"] = sol::overload(
        [](float min, float max, float value) {
            return math::wrap(min, max, value);
        },
        [](int min, int max, int value) {
            return math::wrap(min, max, value);
        });
    math["clamp"] = sol::overload(
        [](float min, float max, float value) {
            return math::clamp(min, max, value);
        },
        [](int min, int max, int value) {
            return math::clamp(min, max, value);
        });

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
    rect["IsEmpty"]        = &base::FRect::IsEmpty;
    rect["Resize"]         = sol::overload(
        [](base::FRect& rect, float x, float y) {
            rect.Resize(x, y);
        },
        [](base::FRect& rect, const FSize& point) {
            rect.Resize(point);
        },
        [](base::FRect& rect, const glm::vec2& point) {
            rect.Resize(point.x, point.y);
        });
    rect["Grow"] =  sol::overload(
        [](base::FRect& rect, float x, float y) {
            rect.Grow(x, y);
        },
        [](base::FRect& rect, const FSize& point) {
            rect.Grow(point);
        },
        [](base::FRect& rect, const glm::vec2& point) {
            rect.Grow(point.x, point.y);
        });
    rect["Move"] =  sol::overload(
        [](base::FRect& rect, float x, float y) {
            rect.Move(x, y);
        },
        [](base::FRect& rect, const FPoint& point) {
            rect.Move(point);
        },
        [](base::FRect& rect, const glm::vec2& point) {
            rect.Move(point.x, point.y);
        });

    rect["Translate"]  =  sol::overload(
        [](base::FRect& rect, float x, float y) {
            rect.Translate(x, y);
        },
        [](base::FRect& rect, const FPoint& point) {
            rect.Translate(point);
        },
        [](base::FRect& rect, const glm::vec2& point) {
            rect.Translate(point.x, point.y);
        });

    rect["TestPoint"] = sol::overload(
        [](const base::FRect& rect, float x, float y) {
            return rect.TestPoint(x, y);
        },
        [](const base::FRect& rect, const FPoint& point) {
            return rect.TestPoint(point);
        },
        [](const base::FRect& rect, const glm::vec2& point) {
            return rect.TestPoint(point.x, point.y);
        });

    rect["MapToGlobal"] = sol::overload(
        [](const base::FRect& rect, float x, float y) {
            return rect.MapToGlobal(x, y);
        },
        [](const base::FRect& rect, const FPoint& point) {
            return rect.MapToGlobal(point);
        },
        [](const base::FRect& rect, const glm::vec2& point) {
           return rect.MapToGlobal(point.x, point.y);
        });
    rect["MapToLocal"] = sol::overload(
        [](const base::FRect& rect, float x, float y) {
            return rect.MapToLocal(x, y);
        },
        [](const base::FRect& rect, const FPoint& point) {
            return rect.MapToLocal(point);
        },
        [](const base::FRect& rect, const glm::vec2& point) {
            return rect.MapToLocal(point.x, point.y);
        });
    rect["GetQuadrants"]   = &base::FRect::GetQuadrants;
    rect["GetCorners"]     = &base::FRect::GetCorners;
    rect["GetCenter"]      = &base::FRect::GetCenter;
    rect["Combine"]        = &base::Union<float>;
    rect["Intersect"]      = &base::Intersect<float>;
    rect["TestIntersect"]  = [](const base::FRect& lhs, const base::FRect& rhs) { return base::DoesIntersect(lhs, rhs); };
    rect.set_function(sol::meta_function::to_string, [](const FRect& rect) {
        return base::ToString(rect);
    });

    sol::constructors<base::FSize(), base::FSize(float, float)> size_ctors;
    auto size = base.new_usertype<base::FSize>("FSize", size_ctors);
    size["GetWidth"]  = &base::FSize::GetWidth;
    size["GetHeight"] = &base::FSize::GetHeight;
    size["IsZero"]    = &base::FSize::IsZero;
    size.set_function(sol::meta_function::multiplication, [](const base::FSize& size, float scalar) { return size * scalar; });
    size.set_function(sol::meta_function::addition, [](const base::FSize& lhs, const base::FSize& rhs) { return lhs + rhs; });
    size.set_function(sol::meta_function::subtraction, [](const base::FSize& lhs, const base::FSize& rhs) { return lhs - rhs; });
    size.set_function(sol::meta_function::to_string, [](const base::FSize& size) { return base::ToString(size); });

    sol::constructors<base::FPoint(), base::FPoint(float, float)> point_ctors;
    auto point = base.new_usertype<base::FPoint>("FPoint", point_ctors);
    point["GetX"] = &base::FPoint::GetX;
    point["GetY"] = &base::FPoint::GetY;
    point["SetX"] = &base::FPoint::SetX;
    point["SetY"] = &base::FPoint::SetY;
    point["Distance"] = &base::Distance;
    point["SquareDistance"] = &base::Distance;

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
    data["WriteFile"] = data::WriteFile;
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
        return std::make_tuple(std::move(ret), std::string("unsupported file type"));
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
    // todo: add more stuff here if/when needed.

    auto table = L["gfx"].get_or_create<sol::table>();

    auto material_class = table.new_usertype<gfx::MaterialClass>("MaterialClass");
    material_class["GetName"] = &gfx::MaterialClass::GetName;
    material_class["GetId"]   = &gfx::MaterialClass::GetId;

    auto material = table.new_usertype<gfx::Material>("Material");
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
    widget["AsRadioButton"]  = &WidgetCast<uik::RadioButton>;

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

    auto radio = table.new_usertype<uik::RadioButton>("RadioButton");
    BindWidgetInterface(radio);
    radio["Select"]     = &uik::RadioButton::Select;
    radio["IsSelected"] = &uik::RadioButton::IsSelected;
    radio["GetText"]    = &uik::RadioButton::GetText;
    radio["SetText"]    = &uik::RadioButton::SetText;

    auto window = table.new_usertype<uik::Window>("Window",
        sol::meta_function::index, [](sol::this_state state, uik::Window* window, const char* key) {
                sol::state_view lua(state);
                auto* widget = window->FindWidgetByName(key);
                if (!widget)
                    return sol::make_object(lua, sol::nil);
                return WidgetObjectCast(state, widget);
            });
    window["GetId"]            = &uik::Window::GetId;
    window["GetName"]          = &uik::Window::GetName;
    window["GetNumWidgets"]    = &uik::Window::GetNumWidgets;
    window["FindWidgetById"]   = [](sol::this_state state, uik::Window* window, const std::string& id) {
        sol::state_view lua(state);
        auto* widget = window->FindWidgetById(id);
        if (!widget)
            return sol::make_object(lua, sol::nil);
        return WidgetObjectCast(state, widget);
    };
    window["FindWidgetByName"] =  [](sol::this_state state, uik::Window* window, const std::string& name) {
        sol::state_view lua(state);
        auto* widget = window->FindWidgetByName(name);
        if (!widget)
            return sol::make_object(lua, sol::nil);
        return WidgetObjectCast(state, widget);
    };
    window["FindWidgetParent"] = [](sol::this_state state, uik::Window* window, uik::Widget* child) {
        return WidgetObjectCast(state, window->FindParent(child));
    };
    window["GetWidget"]        = [](sol::this_state state, uik::Window* window, unsigned index) {
        if (index >= window->GetNumWidgets())
            throw GameError(base::FormatString("Widget index %1 is out of bounds", index));
        return WidgetObjectCast(state, &window->GetWidget(index));
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
    drawable["SetMaterial"] = sol::overload(
        [](DrawableItem& item, const std::string& name, sol::this_state this_state) {
            sol::state_view L(this_state);
            ClassLibrary* lib = L["ClassLib"];
            const auto ret = lib->FindMaterialClassByName(name);
            if (ret)
                item.SetMaterialId(ret->GetId());
            else WARN("No such material class. [name='%1']", name);
        },
        [](DrawableItem& item, const std::shared_ptr<const gfx::MaterialClass>& klass) {
            if (!klass)
                throw GameError("Nil material class in SetMaterial.");
            item.SetMaterialId(klass->GetId());
        });

    drawable["SetActiveTextureMap"] = [](DrawableItem& item, const std::string& name, sol::this_state this_state) {
        sol::state_view L(this_state);
        ClassLibrary* lib = L["ClassLib"];
        const auto ret = lib->FindMaterialClassById(item.GetMaterialId());
        if (!ret)
            return;

        for (unsigned i=0; i<ret->GetNumTextureMaps(); ++i) {
            const auto* map = ret->GetTextureMap(i);
            if (map->GetName() == name)
            {
                item.SetActiveTextureMap(map->GetId());
                return;
            }
        }
        WARN("No such texture map. [name='%1']", name);
    };
    drawable["SetMaterialId"] = &DrawableItem::SetMaterialId;
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
    drawable["HasUniform"]    = &DrawableItem::HasMaterialParam;
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
    body["ApplyImpulse"]          = sol::overload(
        [](RigidBodyItem& body, const glm::vec2& impulse) {
            body.ApplyLinearImpulseToCenter(impulse);
        },
        [](RigidBodyItem& body, float x, float y) {
            body.ApplyLinearImpulseToCenter(glm::vec2(x, y));
        });

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
    spn["IsEnabled"] = &SpatialNode::IsEnabled;
    spn["Enable"]    = &SpatialNode::Enable;

    auto entity_node = table.new_usertype<EntityNode>("EntityNode");
    entity_node["GetId"]          = &EntityNode::GetId;
    entity_node["GetName"]        = &EntityNode::GetName;
    entity_node["GetClassId"]     = &EntityNode::GetClassId;
    entity_node["GetClassName"]   = &EntityNode::GetClassName;
    entity_node["GetTranslation"] = &EntityNode::GetTranslation;
    entity_node["GetSize"]        = &EntityNode::GetSize;
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
    entity_node["SetRotation"]    = &EntityNode::SetRotation;
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
    entity_node["Grow"] = sol::overload(
        [](EntityNode& node, const glm::vec2& size) { node.Grow(size); },
        [](EntityNode& node, float dx, float dy) { node.Grow(dx, dy); } );

    auto entity_class = table.new_usertype<EntityClass>("EntityClass",
       sol::meta_function::index, &GetScriptVar<EntityClass>);
    entity_class["GetId"]       = &EntityClass::GetId;
    entity_class["GetName"]     = &EntityClass::GetName;
    entity_class["GetLifetime"] = &EntityClass::GetLifetime;
    entity_class["GetTag"]      = &EntityClass::GetTag;

    auto actuator_class = table.new_usertype<ActuatorClass>("ActuatorClass");
    actuator_class["GetName"]       = &ActuatorClass::GetName;
    actuator_class["GetId"]         = &ActuatorClass::GetId;
    actuator_class["GetNodeId"]     = &ActuatorClass::GetNodeId;
    actuator_class["GetStartTime"]  = &ActuatorClass::GetStartTime;
    actuator_class["GetDuration"]   = &ActuatorClass::GetDuration;
    actuator_class["GetType"]       = [](const ActuatorClass* klass) {
        return magic_enum::enum_name(klass->GetType());
    };

    auto actuator = table.new_usertype<Actuator>("Actuator");
    BindActuatorInterface<Actuator>(actuator);
    actuator["AsKinematicActuator"] = &ActuatorCast<KinematicActuator>;
    actuator["AsFlagActuator"]      = &ActuatorCast<SetFlagActuator>;
    actuator["AsValueActuator"]     = &ActuatorCast<SetValueActuator>;
    actuator["AsTransformActuator"] = &ActuatorCast<TransformActuator>;
    actuator["AsMaterialActuator"]  = &ActuatorCast<MaterialActuator>;

    auto kinematic_actuator = table.new_usertype<KinematicActuator>("KinematicActuator");
    BindActuatorInterface<KinematicActuator>(kinematic_actuator);

    auto setflag_actuator = table.new_usertype<SetFlagActuator>("SetFlagActuator");
    BindActuatorInterface<SetFlagActuator>(setflag_actuator);

    auto setvalue_actuator = table.new_usertype<SetValueActuator>("SetValueActuator");
    BindActuatorInterface<SetValueActuator>(setvalue_actuator);

    auto transform_actuator = table.new_usertype<TransformActuator>("TransformActuator");
    BindActuatorInterface<TransformActuator>(transform_actuator);
    transform_actuator["SetEndPosition"] = sol::overload(
        [](TransformActuator* actuator, const glm::vec2& position) {
            actuator->SetEndPosition(position);
        },
        [](TransformActuator* actuator, float x, float y) {
            actuator->SetEndPosition(x, y);
        });
    transform_actuator["SetEndSize"] = sol::overload(
        [](TransformActuator* actuator, const glm::vec2& size) {
            actuator->SetEndSize(size);
        },
        [](TransformActuator* actuator, float x, float y) {
            actuator->SetEndSize(x, y);
        });
    transform_actuator["SetEndScale"] = sol::overload(
        [](TransformActuator* actuator, const glm::vec2& scale) {
            actuator->SetEndScale(scale);
        },
        [](TransformActuator* actuator, float x, float y) {
            actuator->SetEndScale(x, y);
        });
    transform_actuator["SetEndRotation"] = &TransformActuator::SetEndRotation;

    auto material_actuator = table.new_usertype<MaterialActuator>("MaterialActuator");
    BindActuatorInterface<MaterialActuator>(material_actuator);

    auto animator = table.new_usertype<Animator>("Animator",
        sol::meta_function::index,     &GetAnimatorVar,
        sol::meta_function::new_index, &SetAnimatorVar);
    animator["GetName"]   = &Animator::GetName;
    animator["GetTime"]   = &Animator::GetTime;
    animator["HasValue"]  = &Animator::HasValue;
    animator["SetValue"]  = &SetAnimatorVar;
    animator["FindValue"] = &GetAnimatorVar;

    auto anim_class = table.new_usertype<AnimationClass>("AnimationClass");
    anim_class["GetName"]     = &AnimationClass::GetName;
    anim_class["GetId"]       = &AnimationClass::GetId;
    anim_class["GetDuration"] = &AnimationClass::GetDuration;
    anim_class["GetDelay"]    = &AnimationClass::GetDelay;
    anim_class["IsLooping"]   = &AnimationClass::IsLooping;

    auto anim = table.new_usertype<Animation>("Animation");
    anim["GetClassName"]       =  &Animation::GetClassName;
    anim["GetClassId"]         =  &Animation::GetClassId;
    anim["IsComplete"]         =  &Animation::IsComplete;
    anim["IsLooping"]          =  &Animation::IsLooping;
    anim["SetDelay"]           =  &Animation::SetDelay;
    anim["GetDelay"]           =  &Animation::GetDelay;
    anim["GetCurrentTime"]     =  &Animation::GetCurrentTime;
    anim["GetDuration"]        =  &Animation::GetDuration;
    anim["GetClass"]           =  &Animation::GetClass;
    anim["FindActuatorById"]   = sol::overload(
        [](game::Animation* animation, const std::string& name) {
            return animation->FindActuatorById(name);
        },
        [](game::Animation* animation, const std::string& name, const std::string& type, sol::this_state state) {
            sol::state_view lua(state);
            auto* actuator = animation->FindActuatorById(name);
            if (!actuator)
                return sol::make_object(lua, sol::nil);
            const auto enum_val = magic_enum::enum_cast<game::ActuatorClass::Type>(type);
            if (!enum_val.has_value())
                throw GameError("No such Actuator type: " + type);
            const auto value = enum_val.value();
            if (value == game::ActuatorClass::Type::Transform)
                return sol::make_object(lua, actuator->AsTransformActuator());
            else if (value == game::ActuatorClass::Type::Material)
                return sol::make_object(lua, actuator->AsMaterialActuator());
            else if (value== game::ActuatorClass::Type::Kinematic)
                return sol::make_object(lua, actuator->AsKinematicActuator());
            else if (value == game::ActuatorClass::Type::SetFlag)
                return sol::make_object(lua, actuator->AsFlagActuator());
            else if (value == game::ActuatorClass::Type::SetValue)
                return sol::make_object(lua, actuator->AsValueActuator());
            else BUG("Unhandled actuator type.");
        });
    anim["FindActuatorByName"] = sol::overload(
        [](game::Animation* animation, const std::string& name) {
           return animation->FindActuatorByName(name);
        },
        [](game::Animation* animation, const std::string& name, const std::string& type, sol::this_state state) {
            sol::state_view lua(state);
            auto* actuator = animation->FindActuatorByName(name);
            if (!actuator)
                return sol::make_object(lua, sol::nil);
            const auto enum_val = magic_enum::enum_cast<game::ActuatorClass::Type>(type);
            if (!enum_val.has_value())
                throw GameError("No such Actuator type: " + type);
            const auto value = enum_val.value();
            if (value == game::ActuatorClass::Type::Transform)
                return sol::make_object(lua, actuator->AsTransformActuator());
            else if (value == game::ActuatorClass::Type::Material)
                return sol::make_object(lua, actuator->AsMaterialActuator());
            else if (value== game::ActuatorClass::Type::Kinematic)
                return sol::make_object(lua, actuator->AsKinematicActuator());
            else if (value == game::ActuatorClass::Type::SetFlag)
                return sol::make_object(lua, actuator->AsFlagActuator());
            else if (value == game::ActuatorClass::Type::SetValue)
                return sol::make_object(lua, actuator->AsValueActuator());
            else BUG("Unhandled actuator type.");
        });

    auto entity = table.new_usertype<Entity>("Entity",
        sol::meta_function::index,     &GetScriptVar<Entity>,
        sol::meta_function::new_index, &SetScriptVar<Entity>);
    entity["GetName"]              = &Entity::GetName;
    entity["GetId"]                = &Entity::GetId;
    entity["GetTag"]               = &Entity::GetTag;
    entity["GetClassName"]         = &Entity::GetClassName;
    entity["GetClassId"]           = &Entity::GetClassId;
    entity["GetClass"]             = &Entity::GetClass;
    entity["GetNumNodes"]          = &Entity::GetNumNodes;
    entity["GetTime"]              = &Entity::GetTime;
    entity["GetLayer"]             = &Entity::GetLayer;
    entity["SetLayer"]             = &Entity::SetLayer;
    entity["IsDying"]              = &Entity::IsDying;
    entity["IsVisible"]            = &Entity::IsVisible;
    entity["IsAnimating"]          = &Entity::IsAnimating;
    entity["HasExpired"]           = &Entity::HasExpired;
    entity["HasBeenKilled"]        = &Entity::HasBeenKilled;
    entity["HasBeenSpawned"]       = &Entity::HasBeenSpawned;
    entity["HasAnimator"]          = &Entity::HasAnimator;
    entity["GetAnimator"]          = (Animator*(Entity::*)(void))&Entity::GetAnimator;
    entity["GetScene"]             = (Scene*(Entity::*)(void))&Entity::GetScene;
    entity["GetNode"]              = (EntityNode&(Entity::*)(size_t))&Entity::GetNode;
    entity["FindScriptVarById"]    = (ScriptVar*(Entity::*)(const std::string&))&Entity::FindScriptVarById;
    entity["FindScriptVarByName"]  = (ScriptVar*(Entity::*)(const std::string&))&Entity::FindScriptVarByName;
    entity["FindNodeByClassName"]  = (EntityNode*(Entity::*)(const std::string&))&Entity::FindNodeByClassName;
    entity["FindNodeByClassId"]    = (EntityNode*(Entity::*)(const std::string&))&Entity::FindNodeByClassId;
    entity["FindNodeByInstanceId"] = (EntityNode*(Entity::*)(const std::string&))&Entity::FindNodeByInstanceId;
    entity["PlayIdle"]             = &Entity::PlayIdle;
    entity["PlayAnimationByName"]  = &Entity::PlayAnimationByName;
    entity["PlayAnimationById"]    = &Entity::PlayAnimationById;
    entity["Die"]                  = &Entity::Die;
    entity["SetTag"]               = &Entity::SetTag;
    entity["TestFlag"]             = &TestFlag<Entity>;
    entity["SetFlag"]              = &SetFlag<Entity>;
    entity["SetVisible"]           = &Entity::SetVisible;
    entity["SetTimer"]             = &Entity::SetTimer;
    entity["PostEvent"]            = sol::overload(
        [](Entity* entity, const std::string& message, const std::string& sender, sol::object value) {
            Entity::PostedEvent event;
            event.message = message;
            event.sender  = sender;
            if (value.is<bool>())
                event.value = value.as<bool>();
            else if (value.is<int>())
                event.value = value.as<int>();
            else if (value.is<float>())
                event.value = value.as<float>();
            else if (value.is<std::string>())
                event.value = value.as<std::string>();
            else if (value.is<glm::vec2>())
                event.value = value.as<glm::vec2>();
            else if (value.is<glm::vec3>())
                event.value = value.as<glm::vec3>();
            else if (value.is<glm::vec4>())
                event.value = value.as<glm::vec4>();
            else throw GameError("Unsupported event event value type.");
            entity->PostEvent(std::move(event));
        },
        [](Entity* entity, const Entity::PostedEvent& event) {
            entity->PostEvent(event);
        });

    auto entity_posted_event = table.new_usertype<Entity::PostedEvent>("EntityEvent", sol::constructors<Entity::PostedEvent()>());
    entity_posted_event["message"] = &Entity::PostedEvent::message;
    entity_posted_event["sender"]  = &Entity::PostedEvent::sender;
    entity_posted_event["value"]   = &Entity::PostedEvent::value;

    auto entity_args = table.new_usertype<EntityArgs>("EntityArgs", sol::constructors<EntityArgs()>());
    entity_args["id"]       = sol::property(&EntityArgs::id);
    entity_args["class"]    = sol::property(&EntityArgs::klass);
    entity_args["name"]     = sol::property(&EntityArgs::name);
    entity_args["scale"]    = sol::property(&EntityArgs::scale);
    entity_args["position"] = sol::property(&EntityArgs::position);
    entity_args["rotation"] = sol::property(&EntityArgs::rotation);
    entity_args["logging"]  = sol::property(&EntityArgs::enable_logging);
    entity_args["layer"]    = sol::property(&EntityArgs::layer);

    using DynamicSpatialQueryResultSet = ResultSet<EntityNode*>;
    auto query_result_set = table.new_usertype<DynamicSpatialQueryResultSet>("SpatialQueryResultSet");
    query_result_set["IsEmpty"] = &DynamicSpatialQueryResultSet::IsEmpty;
    query_result_set["HasNext"] = &DynamicSpatialQueryResultSet::HasNext;
    query_result_set["Next"]    = &DynamicSpatialQueryResultSet::Next;
    query_result_set["Begin"]   = &DynamicSpatialQueryResultSet::BeginIteration;
    query_result_set["Get"]     = &DynamicSpatialQueryResultSet::GetCurrent;
    query_result_set["GetNext"] = &DynamicSpatialQueryResultSet::GetNext;
    query_result_set["Find"]    = [](DynamicSpatialQueryResultSet& results, const sol::function& predicate) {
        while (results.HasNext()) {
            EntityNode* node = results.GetNext();
            const bool this_is_it = predicate(node);
            if (this_is_it)
                return node;
        }
        return (EntityNode*)nullptr;
    };
    query_result_set["Filter"] = [](DynamicSpatialQueryResultSet& results, const sol::function& predicate) {
        results.BeginIteration();
        while (results.HasNext()) {
            EntityNode* node = results.GetCurrent();
            const bool keep_this = predicate(node);
            if (keep_this) {
                results.Next();
            } else  results.EraseCurrent();
        }
        results.BeginIteration();
    };

    auto script_var = table.new_usertype<ScriptVar>("ScriptVar");
    script_var["GetValue"]   = ObjectFromScriptVarValue;
    script_var["GetName"]    = &ScriptVar::GetName;
    script_var["GetId"]      = &ScriptVar::GetId;
    script_var["IsReadOnly"] = &ScriptVar::IsReadOnly;
    script_var["IsArray"]    = &ScriptVar::IsArray;
    script_var["IsPrivate"]  = &ScriptVar::IsPrivate;

    auto scene_class = table.new_usertype<SceneClass>("SceneClass",
        sol::meta_function::index,     &GetScriptVar<SceneClass>);
    scene_class["GetName"] = &SceneClass::GetName;
    scene_class["GetId"]   = &SceneClass::GetId;
    scene_class["GetNumScriptVars"] = &SceneClass::GetNumScriptVars;
    scene_class["GetScriptVar"]     = (const ScriptVar&(SceneClass::*)(size_t)const)&SceneClass::GetScriptVar;
    scene_class["FindScriptVarById"] = (const ScriptVar*(SceneClass::*)(const std::string&)const)&SceneClass::FindScriptVarById;
    scene_class["FindScriptVarByName"] = (const ScriptVar*(SceneClass::*)(const std::string&)const)&SceneClass::FindScriptVarByName;
    scene_class["GetLeftBoundary"] = [](const SceneClass& klass, sol::this_state state) {
        const float* val = klass.GetLeftBoundary();
        sol::state_view lua(state);
        return val ? sol::make_object(lua, *val)
                   : sol::make_object(lua, sol::nil);
    };
    scene_class["GetRightBoundary"] = [](const SceneClass& klass, sol::this_state state) {
        const float* val = klass.GetRightBoundary();
        sol::state_view lua(state);
        return val ? sol::make_object(lua, *val)
                   : sol::make_object(lua, sol::nil);
    };
    scene_class["GetTopBoundary"]  = [](const SceneClass& klass, sol::this_state state) {
        const float* val = klass.GetTopBoundary();
        sol::state_view lua(state);
        return val ? sol::make_object(lua, *val)
                   : sol::make_object(lua, sol::nil);
    };
    scene_class["GetBottomBoundary"] = [](const SceneClass& klass, sol::this_state state) {
        const float* val = klass.GetBottomBoundary();
        sol::state_view lua(state);
        return val ? sol::make_object(lua, *val)
                   : sol::make_object(lua, sol::nil);
    };

    using EntityList = ResultVector<Entity*>;
    auto entity_list = table.new_usertype<EntityList>("EntityList");
    entity_list["IsEmpty"] = &EntityList::IsEmpty;
    entity_list["HasNext"] = &EntityList::HasNext;
    entity_list["Next"]    = &EntityList::Next;
    entity_list["Begin"]   = &EntityList::BeginIteration;
    entity_list["Get"]     = &EntityList::GetCurrent;
    entity_list["GetAt"]   = &EntityList::GetAt;
    entity_list["Size"]    = &EntityList::GetSize;
    entity_list["GetNext"] = &EntityList::GetNext;
    entity_list["Join"]    = &EntityList::Join;
    entity_list["ForEach"] = [](EntityList& list, const sol::function& callback, sol::variadic_args args) {
        list.BeginIteration();
        while (list.HasNext()) {
            Entity* entity = list.GetNext();
            callback(entity, args);
        }
    };

    auto scene = table.new_usertype<Scene>("Scene",
       sol::meta_function::index,     &GetScriptVar<Scene>,
       sol::meta_function::new_index, &SetScriptVar<Scene>);
    scene["ListEntitiesByClassName"]    = [](Scene& scene, const std::string& name) {
        return EntityList(scene.ListEntitiesByClassName(name));
    };
    scene["ListEntitiesByTag"] = [](Scene& scene, const std::string& tag) {
        return EntityList(scene.ListEntitiesByTag(tag));
    };
    scene["GetTime"]                    = &Scene::GetTime;
    scene["GetClassName"]               = &Scene::GetClassName;
    scene["GetClassId"]                 = &Scene::GetClassId;
    scene["GetClass"]                   = &Scene::GetClass;
    scene["GetNumEntities"]             = &Scene::GetNumEntities;
    scene["GetEntity"]                  = (Entity&(Scene::*)(size_t))&Scene::GetEntity;
    scene["FindEntityByInstanceId"]     = (Entity*(Scene::*)(const std::string&))&Scene::FindEntityByInstanceId;
    scene["FindEntityByInstanceName"]   = (Entity*(Scene::*)(const std::string&))&Scene::FindEntityByInstanceName;
    scene["FindScriptVarById"]          = (ScriptVar*(Scene::*)(const std::string&))&Scene::FindScriptVarById;
    scene["FindScriptVarByName"]        = (ScriptVar*(Scene::*)(const std::string&))&Scene::FindScriptVarByName;
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
        [](Scene& scene, const EntityArgs& args) {
            return scene.SpawnEntity(args, true);
        },
        [](Scene& scene, const EntityArgs& args, bool link) {
            return scene.SpawnEntity(args, link);
        },
        [](Scene& scene, const std::string& klass, sol::this_state this_state) {
            sol::state_view L(this_state);
            ClassLibrary* classlib = L["ClassLib"];

            game::EntityArgs args;
            args.klass = classlib->FindEntityClassByName(klass);
            if (!args.klass)
                throw GameError("No such entity class could be found: " + klass);

            return scene.SpawnEntity(args);
        },
        [](Scene& scene, const std::string& klass, const sol::table& args_table, sol::this_state this_state) {
            sol::state_view L(this_state);
            ClassLibrary* classlib = L["ClassLib"];

            game::EntityArgs args;
            args.klass = classlib->FindEntityClassByName(klass);
            if (!args.klass)
                throw GameError("No such entity class could be found: " + klass);

            args.id             = args_table.get_or("id", std::string(""));
            args.name           = args_table.get_or("name", std::string(""));
            args.layer          = args_table.get_or("layer", 0);
            args.scale.x        = args_table.get_or("sx", 1.0f);
            args.scale.y        = args_table.get_or("sy", 1.0f);
            args.position.x     = args_table.get_or("x", 0.0f);
            args.position.y     = args_table.get_or("y", 0.0f);
            args.rotation       = args_table.get_or("r", 0.0f);
            args.enable_logging = args_table.get_or("logging", false);

            const bool link_to_root = args_table.get_or("link", true);
            const glm::vec2* pos = nullptr;
            const glm::vec2* scale = nullptr;
            if (const auto* ptr = args_table.get_or("pos", pos))
                args.position = *ptr;
            if (const auto* ptr = args_table.get_or("scale", scale))
                args.scale = *ptr;

            return scene.SpawnEntity(args, link_to_root);
        });

    scene["QuerySpatialNodes"] = sol::overload(
        [](Scene& scene, const base::FPoint& point, const std::string& mode) {
            const auto enum_val = magic_enum::enum_cast<game::Scene::SpatialQueryMode>(mode);
            if (!enum_val.has_value())
                throw GameError("No such spatial query mode: " + mode);

            std::set<EntityNode*> result;
            scene.QuerySpatialNodes(point, &result, enum_val.value());
            return std::make_unique<DynamicSpatialQueryResultSet>(std::move(result));
        },
        [](Scene& scene, const base::FPoint& point, float radius, const std::string& mode) {
            const auto enum_val = magic_enum::enum_cast<game::Scene::SpatialQueryMode>(mode);
            if (!enum_val.has_value())
                throw GameError("No such spatial query mode: " + mode);

            std::set<EntityNode*> result;
            scene.QuerySpatialNodes(point, radius, &result, enum_val.value());
            return std::make_unique<DynamicSpatialQueryResultSet>(std::move(result));
        },
        [](Scene& scene, const glm::vec2& point, const std::string& mode) {
            const auto enum_val = magic_enum::enum_cast<game::Scene::SpatialQueryMode>(mode);
            if (!enum_val.has_value())
                throw GameError("No such spatial query mode: " + mode);

            std::set<EntityNode*> result;
            scene.QuerySpatialNodes(base::FPoint(point.x, point.y), &result, enum_val.value());
            return std::make_unique<DynamicSpatialQueryResultSet>(std::move(result));
        },
        [](Scene& scene, const glm::vec2& point, float radius, const std::string& mode) {
            const auto enum_val = magic_enum::enum_cast<game::Scene::SpatialQueryMode>(mode);
            if (!enum_val.has_value())
                throw GameError("No such spatial query mode: " + mode);

            std::set<EntityNode*> result;
            scene.QuerySpatialNodes(base::FPoint(point.x, point.y), radius, &result, enum_val.value());
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
    ray_cast_result_vector["GetNext"] = &RayCastResultVector::GetNext;

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
    audio["EnableEffects"]      = &AudioEngine::EnableEffects;

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

            const auto* val = base::SafeFind(event.values, std::string(key));
            return val ? sol::make_object(lua, *val)
                       : sol::make_object(lua, sol::nil);
        },
        sol::meta_function::new_index, [&L](GameEvent& event, const char* key, sol::object value) {
            if (!std::strcmp(key, "from")) {
                if (value.is<std::string>())
                    event.from = value.as<std::string>();
                else if (value.is<game::Scene*>())
                    event.from = value.as<game::Scene*>();
                else if (value.is<game::Entity*>())
                    event.from = value.as<game::Entity*>();
                else throw GameError("Unsupported game event from field type.");
            } else if (!std::strcmp(key, "to")) {
                if (value.is<std::string>())
                    event.to = value.as<std::string>();
                else if (value.is<game::Scene*>())
                    event.to = value.as<game::Scene*>();
                else if (value.is<game::Entity*>())
                    event.to = value.as<game::Entity*>();
                else throw GameError("Unsupported game event to field type.");
            } else if (!std::strcmp(key, "message")) {
                if (value.is<std::string>())
                    event.message = value.as<std::string>();
                else throw GameError("Unsupported game event message field type.");
            } else {
                const auto& k = std::string(key);
                if (value.is<bool>())
                    event.values[key] = value.as<bool>();
                else if (value.is<int>())
                    event.values[key] = value.as<int>();
                else if (value.is<float>())
                    event.values[key] = value.as<float>();
                else if (value.is<std::string>())
                    event.values[key] = value.as<std::string>();
                else if(value.is<glm::vec2>())
                    event.values[key] = value.as<glm::vec2>();
                else if (value.is<glm::vec3>())
                    event.values[key] = value.as<glm::vec3>();
                else if (value.is<glm::vec4>())
                    event.values[key] = value.as<glm::vec4>();
                else if (value.is<base::Color4f>())
                    event.values[key] = value.as<base::Color4f>();
                else if (value.is<base::FSize>())
                    event.values[key] = value.as<base::FSize>();
                else if (value.is<base::FRect>())
                    event.values[key] = key, value.as<base::FRect>();
                else if (value.is<base::FPoint>())
                    event.values[key] = value.as<base::FPoint>();
                else if (value.is<game::Entity*>())
                    event.values[key] = value.as<game::Entity*>();
                else if (value.is<game::Scene*>())
                    event.values[key] = value.as<game::Scene*>();
                else throw GameError("Unsupported game event value type.");
            }
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
