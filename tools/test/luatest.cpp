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
#include "warnpop.h"

#include <string>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <variant>

// type provided by the engine
class Entity
{
public:
    Entity() = default;

    Entity(std::string type, std::string name)
      : mType(type)
      , mName(name)
    {}

    glm::vec2 GetPosition() const
    { return mPos; }
    void SetPosition(const glm::vec2& pos)
    { mPos = pos; }
    std::string GetName() const
    { return mName; }

    using Value = std::variant<int, float, std::string>;

    Value GetVariantValue() const
    { return mValue; }

    void SetVariantValue(Value val)
    { mValue = std::move(val); }

    Value variant_property;

private:
    std::string mType;
    std::string mName;
    glm::vec2 mPos;
    Value mValue;
};

class Scene
{
public:
    Scene()
    {
        mEntities.emplace_back("tank", "tank 1");
        mEntities.emplace_back("tank", "tank 2");
        mEntities.emplace_back("tank", "tank 3");
        mMap["foo"] = &mEntities[0];
        mMap["bar"] = &mEntities[1];
        mMap["meh"] = &mEntities[2];
    }
    Entity* GetEntity(const std::string& id)
    {
        return mMap[id];
    }
private:
    std::vector<Entity> mEntities;
    std::unordered_map<std::string, Entity*> mMap;
};


class Foobar
{
public:
    int GetValue(int index) const
    {
        std::cout << "Foobar::GetValue " << index << std::endl;
        return 1234;
    }
};

class Doodah
{
public:
    int GetValue(int index) const
    {
        std::cout << "Doodah::GetValue " << index << std::endl;
        return 1234;
    }
};

void vector_test()
{
    sol::state L;
    L.open_libraries();

    std::vector<int> foo;
    foo.resize(2);
    foo[0] = 123;
    foo[1] = 333;

    L["vec"] = sol::make_object(L, (const std::vector<int>*)&foo);
    L.script("print(vec[2])\n"
             "vec[1] = 666");
    std::printf("out = %d\n", foo[0]);
}

void env_test()
{
    sol::state L;
    L.open_libraries();
    L["keke"] = [](int x) { return 123*x; };
    sol::environment a(L, sol::create, L.globals());
    sol::environment b(L, sol::create, L.globals());

    // two functions by the same name but in different environments.
    L.script("function jallu()\n"
             "print(keke(2))\n"
             "end\n", a);
    L.script("function jallu()\n"
             "print(keke(3))\n"
             "end\n", b);
    a["jallu"]();
    b["jallu"]();
}

void meta_method_index_test()
{
    sol::state L;
    L.open_libraries();
    // the index only works when set in the constructor?
    auto foo = L.new_usertype<Foobar>("foo",
		sol::meta_function::index, &Foobar::GetValue);
    auto bar = L.new_usertype<Doodah>("bar");
    bar.set_function(sol::meta_function::index, &Doodah::GetValue);

	L.script(
  "f = foo.new()\n"
        "b = bar.new()\n"
		"print(f[1])\n"
		"print(b[1])\n"
	);
}

template<typename T>
class Array {
public:
    Array(std::vector<T>* vector)
      : mArray(vector)
    {}
    using value_type = typename std::vector<T>::value_type;
    using iterator   = typename std::vector<T>::const_iterator;
    using size_type  = typename std::vector<T>::size_type;

    iterator begin() noexcept
    { return mArray->begin(); }
    iterator end() noexcept
    { return mArray->end(); }
    size_type size() const noexcept
    { return mArray->size(); }
    void push_back(T value)
    { mArray->push_back(value); }
    bool empty() const noexcept
    { return mArray->empty(); }
    const T& GetItem(unsigned index) const
    { return (*mArray)[index]; }

private:
    std::vector<T>* mArray = nullptr;
};

void array_type_test()
{
    sol::state L;
    L.open_libraries();
    L.script(R"(
function print_size(arr)
   print(tostring(#arr))
   print('')
end
function print_iterate_pairs(arr)
   for k, v in pairs(arr) do
       print(tostring(k) .. '=>' .. tostring(v))
   end
   print('')
end
function print_iterate_index(arr)
   for i=1, #arr do
      print(tostring(arr[i]))
   end
   print('')
end
    )");

    using IntArray = Array<int>;
    auto foo = L.new_usertype<IntArray>("IntArray",
            sol::meta_function::index, [](const IntArray& array, unsigned index) {
                // lua uses 1 based indexing.
                return array.GetItem(index-1);
            }
    );

    std::vector<int> vec;
    vec.push_back(111);
    vec.push_back(45);
    vec.push_back(-1);
    Array<int> array(&vec);

    L["print_size"](&array);
    L["print_iterate_pairs"](&array);
    L["print_iterate_index"](&array);

}

void environment_variable_test()
{
    sol::state L;
    L.open_libraries();

    sol::environment env(L, sol::create, L.globals());

    // set a string on this environment
    env["my_var"] = "foobar";

    // sanity checking here. these work as expected.
    {
        // ok, works great.
        std::string value = env["my_var"];
        if (value != "foobar")
            std::printf("bonkers!\n");

        // ok, works, great
        L.script(R"(
            print(my_var)
        )", env);
    }

    L["test"] = [](sol::this_state state, sol::this_environment this_env) {
        //std::printf("test_func\n");
        //std::printf("has state = %s\n", state ? "yes" : "no");
        std::printf("has environment = %s\n", this_env ? "yes" : "no");

        // the problem here is that we can't get the my_var string since
        // the environment is not set. in other words the implicit conversion
        // from this_env to sol::environment tries to access std::optional
        // that doesn't have a value.
        if (this_env)
        {
            sol::environment& env = this_env;
            std::string str = env["my_var"];
            std::printf("my_var '%s'\n", str.c_str());
        }
    };
    // env is empty. ok
    L["test"]();

    // call the same function using a different environment
    // result -> no environment.
    env["test"]();

    // ok, try using this type of invocation
    // still the same result -> no environment
    {
        sol::protected_function test_func = L["test"];
        test_func(env);
    }

    // ok, try using this type of invocation
    // still the same result -> no environment
    {
        sol::protected_function test_func = env["test"];
        test_func();
    }


    // calling a script that calls the test function.
    //  even when passing the env parameter this_env is still nullopt!
    L.script(R"(
function some_func()
   test()
end
    )", env);

    env["some_func"]();

}

struct MyEntity {};

std::string GetSomething(MyEntity& entity, const char* key, sol::this_state this_state, sol::this_environment this_env)
{
    std::printf("GetSomething, key='%s', has environment = %s\n", key, this_env ? "yes" : "no");

    return "dummy";
}

void environment_variable_test_entity()
{
    sol::state L;
    L.open_libraries();

    auto entity = L.new_usertype<MyEntity>("MyEntity",
      sol::meta_function::index, &GetSomething);

    sol::environment env(L, sol::create, L.globals());

    L.script(R"(
function Tick(entity)
   --print(entity.test_value)
   print('hello')
   local var = entity.test_value
end
    )", env);

    MyEntity e;

    env["Tick"](&e);

}

void function_from_lua()
{
    sol::state L;
    L.open_libraries();

    L["keke"] = [](sol::function f) {
        f();
    };

    L.script(R"(
print('hello')

function my_function()
  print('my function says hi!')
end

keke(my_function)

    )");
}


int my_exception_handler(lua_State* L, sol::optional<const std::exception&> maybe_exception, sol::string_view description) {
    // L is the lua state, which you can wrap in a state_view if necessary
    // maybe_exception will contain exception, if it exists
    // description will either be the what() of the exception or a description saying that we hit the general-case catch(...)

    if (maybe_exception) {
        const std::exception& ex = *maybe_exception;
        std::cout << "exception: " << ex.what() << std::endl;
        std::cout << "description: " << description << std::endl;
    }
    else {
        std::cout << "(from the description parameter): ";
        std::cout.write(description.data(), static_cast<std::streamsize>(description.size()));
        std::cout << std::endl;
    }

    // you must push 1 element onto the stack to be
    // transported through as the error object in Lua
    // note that Lua -- and 99.5% of all Lua users and libraries -- expects a string
    // so we push a single string (in our case, the description of the error)
    return sol::stack::push(L, description);
}
void will_throw() {
	throw std::runtime_error("oh no not an exception!!!");
}


void exception_handler_test()
{
    sol::state lua;
    lua.open_libraries(sol::lib::base);
    //lua.set_exception_handler(&my_exception_handler);
    lua.set_function("will_throw", &will_throw);
    lua.script(R"(
function invalid_lua_code()
    --local foo = nil
    --foo:rectangulate()
    will_throw()
end
)");

    sol::protected_function func = lua["invalid_lua_code"];

    sol::protected_function_result ret = func();
    sol::error err = ret;

    //sol::protected_function_result pfr = lua.safe_script("will_throw()", &sol::script_pass_on_error);
    //c_assert(!pfr.valid());
    //sol::error err = pfr;
    std::cout << "err what: " << err.what() << std::endl;
    //std::cout << std::endl;

}

void variant_test()
{
    sol::state lua;
    lua.open_libraries();

    auto entity = lua.new_usertype<Entity>("Entity",
        sol::meta_function::index, [](const Entity& entity, const std::string& key) {
            std::cout << "index " << key << std::endl;
            return entity.variant_property;
        },
        sol::meta_function::new_index, [](Entity& entity, const std::string& key, Entity::Value value) {
            std::cout << "new_index " << key << std::endl;
            entity.variant_property = value;
        });

    entity["GetVariantValue"] = &Entity::GetVariantValue;
    entity["SetVariantValue"] = &Entity::SetVariantValue;

    // sol property is a wrapper for creating either read, write or read/write "properties"
    // so using this raw member pointer instead.
    entity["variant_property"] = &Entity::variant_property;

    // create the entity instance
    Entity the_entity;
    lua["the_entity"] = &the_entity;

    the_entity.variant_property = 123;
    the_entity.SetVariantValue(321);

    // read from lua
    lua.script(R"(
print(the_entity.variant_property)
print(the_entity:GetVariantValue())
print(the_entity.using_index_meta_method)
)");

    // write from lua
    lua.script(R"(
the_entity.using_index_meta_method = 123
the_entity.variant_property = 'lalal'
the_entity:SetVariantValue('foobar')

)");

    if (const auto* str = std::get_if<std::string>(&the_entity.variant_property))
        std::cout << *str << std::endl;

    auto val = the_entity.GetVariantValue();
    if (const auto* str = std::get_if<std::string>(&val))
        std::cout << *str << std::endl;

    try
    {
        // wrong type frm lua
        lua.script(R"(
the_entity.variant_property = true
)");
    }
    catch (const std::exception& lua_err)
    {
        //std::cout << lua_err.what();
        //std::cout << std::endl;
    }

    try
    {
        // wrong type frm lua
        lua.script(R"(
the_entity:SetVariantValue(true)
)");
    }
    catch (const std::exception& lua_err)
    {
        //std::cout << lua_err.what();
        //std::cout << std::endl;
    }


    lua.script(R"(
the_entity.variant_property = 123.0

print(the_entity.variant_property)
    )");
}

void variant_bug_test()
{
    // testing for what seems to be a sol2 bug.
    // https://github.com/ThePhD/sol2/issues/1524
    //
    // using sol::overload to differentiate between int and other types.

    sol::state lua;
    lua.open_libraries();

    using Variant = std::variant<float, int, std::string>;

    Variant the_value;
    lua["set_the_variant"] = sol::overload(
        [&the_value](int value) {
            the_value = value;
        },
        [&the_value](Variant val) {
            the_value = val;
        });

    lua["get_the_variant"] = [&the_value]() {
        return the_value;
    };

    lua.script(R"(
set_the_variant(123)
print(get_the_variant())
set_the_variant(123.0)
print(get_the_variant())
set_the_variant('keke')
print(get_the_variant())
)");

}

int main(int argc, char* argv[])
{
    //vector_test();
    //array_type_test();
    //environment_variable_test();
    //environment_variable_test_entity();
    //function_from_lua();
    //exception_handler_test();

    //variant_test();

    variant_bug_test();

    return 0;
}