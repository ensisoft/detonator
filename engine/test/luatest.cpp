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


// type provided by the engine
class Entity
{
public:
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
private:
    std::string mType;
    std::string mName;
    glm::vec2 mPos;
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
    bar.set_function(sol::meta_function::index, &Foobar::GetValue);

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

int main(int argc, char* argv[])
{
    //vector_test();
    array_type_test();

    return 0;
}