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
#  include <glm/vec2.hpp>
#include "warnpop.h"

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "engine/settings.h"

int test_main(int argc, char* argv[])
{
    // test has value and separte object "name spaces".
    {
        engine::Settings settings;
        TEST_REQUIRE(settings.HasValue("foo", "key") == false);
        TEST_REQUIRE(settings.HasValue("bar", "key") == false);

        settings.SetValue("foo", "key", "assa sassa mandelmassa");
        TEST_REQUIRE(settings.HasValue("foo", "key") == true);
        TEST_REQUIRE(settings.HasValue("bar", "key") == false);
        settings.SetValue("bar", "key", 1234);
        TEST_REQUIRE(settings.HasValue("foo", "key") == true);
        TEST_REQUIRE(settings.HasValue("bar", "key") == true);
    }

    // test different value types when they exist
    {
        enum class Fruits {
            Banana, Apple, Kiwi, Guava
        };

        engine::Settings settings;
        settings.SetValue("foo", "key_string", "foobar");
        settings.SetValue("foo", "key_int", 1234);
        settings.SetValue("foo", "key_unsigned_int", 12345u);
        settings.SetValue("foo", "key_float", 45.0f);
        settings.SetValue("foo", "key_double", 45.0);
        settings.SetValue("foo", "key_enum", Fruits::Kiwi);
        settings.SetValue("foo", "key_vec", glm::vec2(1.0f, 2.0f));

        TEST_REQUIRE(settings.GetValue("foo", "key_string", "") == "foobar");
        TEST_REQUIRE(settings.GetValue("foo", "key_int", 0) == 1234);
        TEST_REQUIRE(settings.GetValue("foo", "key_unsigned_int", 0u) == 12345u);
        TEST_REQUIRE(settings.GetValue("foo", "key_float", 0.0f) == real::float32(45.0f));
        TEST_REQUIRE(settings.GetValue("foo", "key_enum", Fruits::Banana) == Fruits::Kiwi);
        TEST_REQUIRE(settings.GetValue("foo", "key_vec", glm::vec2(0.0f)) == glm::vec2(1.0f, 2.0f));
    }

    // test different value types when they don't exist
    {
        enum class Fruits {
            Banana, Apple, Kiwi, Quava
        };

        engine::Settings settings;
        TEST_REQUIRE(settings.GetValue("foo", "key_string", "foobar") == "foobar");
        TEST_REQUIRE(settings.GetValue("foo", "key_int", 1234) == 1234);
        TEST_REQUIRE(settings.GetValue("foo", "key_unsigned_int", 12345u) == 12345u);
        TEST_REQUIRE(settings.GetValue("foo", "key_float", 45.0f) == real::float32(45.0f));
        TEST_REQUIRE(settings.GetValue("foo", "key_enum", Fruits::Kiwi) == Fruits::Kiwi);
        TEST_REQUIRE(settings.GetValue("foo", "key_vec", glm::vec2(1.0f, 2.0f)) == glm::vec2(1.0f, 2.0f));
    }

    {
        enum class Fruits {
            Banana, Apple, Kiwi, Quava
        };

        engine::Settings settings;
        settings.SetValue("foo", "key_string", "foobar");
        settings.SetValue("foo", "key_int", 1234);
        settings.SetValue("foo", "key_unsigned_int", 12345u);
        settings.SetValue("foo", "key_float", 45.0f);
        settings.SetValue("foo", "key_double", 45.0);
        settings.SetValue("foo", "key_enum", Fruits::Kiwi);
        settings.SetValue("foo", "key_vec", glm::vec2(1.0f, 2.0f));
        settings.SaveToFile("settings_test.json");
        settings.Clear();
        TEST_REQUIRE(!settings.HasValue("foo", "key_string"));
        TEST_REQUIRE(!settings.HasValue("foo", "key_int"));
        TEST_REQUIRE(!settings.HasValue("foo", "key_unsigned_int"));
        TEST_REQUIRE(!settings.HasValue("foo", "key_float"));
        TEST_REQUIRE(!settings.HasValue("foo", "key_double"));
        TEST_REQUIRE(!settings.HasValue("foo", "key_enum"));
        TEST_REQUIRE(!settings.HasValue("foo", "key_vec"));

        settings.LoadFromFile("settings_test.json");
        TEST_REQUIRE(settings.GetValue("foo", "key_string", "") == "foobar");
        TEST_REQUIRE(settings.GetValue("foo", "key_int", 0) == 1234);
        TEST_REQUIRE(settings.GetValue("foo", "key_unsigned_int", 0u) == 12345u);
        TEST_REQUIRE(settings.GetValue("foo", "key_float", 0.0f) == real::float32(45.0f));
        TEST_REQUIRE(settings.GetValue("foo", "key_enum", Fruits::Banana) == Fruits::Kiwi);
        TEST_REQUIRE(settings.GetValue("foo", "key_vec", glm::vec2(0.0f)) == glm::vec2(1.0f, 2.0f));
    }

    // test exceptional conditions.
    {
        engine::Settings settings;
        TEST_EXCEPTION(settings.SaveToFile("this/path/is/junk/blah.json"));
        TEST_EXCEPTION(settings.LoadFromFile("this/path/is/junk/blah.json"));

        std::ofstream out("test.json", std::ios::out | std::ios::trunc);
        out << "{ \"foo\": { adsgassa } }" << std::endl;
        TEST_EXCEPTION(settings.LoadFromFile("test.json"));
    }

    // test array
    {
        const std::vector<unsigned> ints = {1, 4, 55, 12345};
        const std::vector<std::string> strs = {"jeesus", "ajaa", "mopolla"};

        engine::Settings settings;
        settings.SetValue("foo", "ints", ints);
        settings.SetValue("foo", "strs", strs);

        std::vector<unsigned> int_ret;
        int_ret = settings.GetValue("foo", "ints", int_ret);
        TEST_REQUIRE(int_ret[0] == 1);
        TEST_REQUIRE(int_ret[1] == 4);
        TEST_REQUIRE(int_ret[2] == 55);
        TEST_REQUIRE(int_ret[3] == 12345);

        std::vector<std::string> str_ret;
        str_ret = settings.GetValue("foo", "strs", str_ret);
        TEST_REQUIRE(str_ret[0] == "jeesus");
        TEST_REQUIRE(str_ret[1] == "ajaa");
        TEST_REQUIRE(str_ret[2] == "mopolla");
    }

    return 0;
}