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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <iostream>

#include "base/test_minimal.h"
#include "base/test_help.h"
#include "base/bitflag.h"
#include "data/reader.h"
#include "data/writer.h"
#include "data/json.h"

template<typename T>
void TestValue(const char* key, const data::Reader& reader, const T& expected)
{
    T value;
    TEST_REQUIRE(reader.HasValue(key));
    TEST_REQUIRE(reader.Read(key, &value));
    TEST_REQUIRE(value == expected);
}

void unit_test_basic()
{
    data::JsonObject json;
    TEST_REQUIRE(json.IsEmpty());

    json.Write("double", 2.0);
    json.Write("float", 1.0f);
    json.Write("int", 123);
    json.Write("unsigned", 333u);
    json.Write("boolean", false);
    json.Write("string", "foobar string");
    json.Write("vec2", glm::vec2(1.0f, 2.0f));
    json.Write("vec3", glm::vec3(1.0f, 2.0f, 3.0f));
    json.Write("vec4", glm::vec4(1.0f, 2.0f, 3.0f, 4.0f));
    json.Write("rect", base::FRect(1.0f, 2.0f, 10.0f, 20.0f));
    json.Write("point", base::FPoint(-50.0f, -50.0f));
    json.Write("size", base::FSize(50.0f, 50.0f));
    json.Write("color", base::Color4f(base::Color::HotPink));

    TEST_REQUIRE(json.HasValue("double"));
    TEST_REQUIRE(json.HasValue("float"));
    TEST_REQUIRE(json.HasValue("float"));
    TEST_REQUIRE(json.HasValue("int"));
    TEST_REQUIRE(json.HasValue("unsigned"));
    TEST_REQUIRE(json.HasValue("boolean"));
    TEST_REQUIRE(json.HasValue("string"));
    TEST_REQUIRE(json.HasValue("vec2"));
    TEST_REQUIRE(json.HasValue("vec3"));
    TEST_REQUIRE(json.HasValue("vec4"));
    TEST_REQUIRE(json.HasValue("rect"));
    TEST_REQUIRE(json.HasValue("point"));
    TEST_REQUIRE(json.HasValue("size"));
    TEST_REQUIRE(json.HasValue("color"));
    TEST_REQUIRE(json.HasValue("huhu") == false);
    TEST_REQUIRE(json.IsEmpty() == false);

    TestValue("double", json, 2.0);
    TestValue("float", json, 1.0f);
    TestValue("int", json, 123);
    TestValue("unsigned", json, 333u);
    TestValue("boolean", json, false);
    TestValue("string", json, std::string("foobar string"));
    TestValue("vec2", json, glm::vec2(1.0f, 2.0f));
    TestValue("vec3", json, glm::vec3(1.0f, 2.0f, 3.0f));
    TestValue("vec4", json, glm::vec4(1.0f, 2.0f, 3.0f, 4.0f));
    TestValue("rect", json, base::FRect(1.0f, 2.0f, 10.0f, 20.0f));
    TestValue("point", json, base::FPoint(-50.0f, -50.0f));
    TestValue("size", json, base::FSize(50.0f, 50.0f));
    TestValue("color", json, base::Color4f(base::Color::HotPink));
}

void unit_test_bitflag()
{
    enum class Fruits {
        Apple, Banana, Kiwi, Quava
    };
    base::bitflag<Fruits> f;
    f.set(Fruits::Apple, true);
    f.set(Fruits::Kiwi, true);

    data::JsonObject json;
    json.Write("fruits", f);

    f.clear();
    json.Read("fruits", &f);
    TEST_REQUIRE(f.test(Fruits::Apple));
    TEST_REQUIRE(f.test(Fruits::Kiwi));
    TEST_REQUIRE(f.test(Fruits::Quava) == false);
}

void unit_test_variant()
{
    using Variant = std::variant<float, glm::vec2, glm::vec3, std::string>;
    Variant f = 123.0f;
    Variant x = glm::vec2(1.0f, 2.0f);
    Variant y = glm::vec3(2.0f, 3.0f, 4.0f);
    Variant z = std::string("joo joo");

    data::JsonObject json;
    json.Write("f", f);
    json.Write("x", x);
    json.Write("y", y);
    json.Write("z", z);

    Variant out;
    json.Read("f", &out);
    TEST_REQUIRE(std::holds_alternative<float>(out));
    TEST_REQUIRE(std::get<float>(out) == real::float32(123.0f));

    json.Read("x", &out);
    TEST_REQUIRE(std::holds_alternative<glm::vec2>(out));
    TEST_REQUIRE(std::get<glm::vec2>(out) == glm::vec2(1.0f, 2.0f));

    json.Read("y", &out);
    TEST_REQUIRE(std::holds_alternative<glm::vec3>(out));
    TEST_REQUIRE(std::get<glm::vec3>(out) == glm::vec3(2.0f, 3.0f, 4.0f));
}

void unit_test_optional()
{
    {
        std::optional<float> opt_f = 123.0f;
        std::optional<std::string> opt_s = "keke";
        std::optional<glm::vec3> opt_v = glm::vec3(1.0f, 2.0f, 3.0f);

        data::JsonObject json;
        json.Write("float", opt_f);
        json.Write("string", opt_s);
        json.Write("vec3", opt_v);

        opt_f.reset();
        opt_s.reset();
        opt_v.reset();
        TEST_REQUIRE(json.Read("float", &opt_f));
        TEST_REQUIRE(json.Read("string", &opt_s));
        TEST_REQUIRE(json.Read("vec3", &opt_v));

        TEST_REQUIRE(opt_f.has_value());
        TEST_REQUIRE(opt_s.has_value());
        TEST_REQUIRE(opt_v.has_value());
        TEST_REQUIRE(opt_f == real::float32(123.0f));
        TEST_REQUIRE(opt_s == "keke");
        TEST_REQUIRE(opt_v == glm::vec3(1.0f, 2.0f, 3.0f));
    }

    {
        std::optional<float> opt_f;

        data::JsonObject json;
        json.Write("float", opt_f);

        opt_f.reset();

        TEST_REQUIRE(json.Read("float", &opt_f));
        TEST_REQUIRE(!opt_f.has_value());
    }
}

template<typename T>
void unit_test_array(const T* array, size_t size)
{
    std::string str;
    {
        data::JsonObject json;
        json.Write("foobar", array, size);
        str = json.ToString();
        std::cout << str << std::endl;
    }
    {
        data::JsonObject json;
        const auto [ok, err] = json.ParseString(str);
        TEST_REQUIRE(ok);
        TEST_REQUIRE(json.HasArray("foobar"));
        TEST_REQUIRE(json.GetNumItems("foobar") == size);

        for (size_t i=0; i<size; ++i)
        {
            T item;
            TEST_REQUIRE(json.Read("foobar", i, &item));
            TEST_REQUIRE(item == array[i]);
        }
    }
}

int test_main(int argc, char* argv[])
{
    unit_test_basic();
    unit_test_bitflag();
    unit_test_variant();
    unit_test_optional();
    {
        const std::string strs[] = {
          "jeesus", "ajaa", "mopolla"
        };
        unit_test_array(strs, 3);
    }
    {
        const int values[5] = {
            1, -1, 68, 800, 43
        };
        unit_test_array(values, 5);
    }
    return 0;
}