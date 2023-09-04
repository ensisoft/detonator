// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#include "base/test_minimal.h"
#include "base/test_help.h"
#include "base/scanf.h"

void unit_test_scanf_success()
{
    TEST_CASE(test::Type::Feature)

    // literal string
    {
        TEST_REQUIRE(base::Scanf("keke", "keke"));
    }


    // float
    {
        float value;
        TEST_REQUIRE(base::Scanf("1.0", &value));
        TEST_REQUIRE(value == 1.0f);
    }
    {
        float value;
        TEST_REQUIRE(base::Scanf("   1.0", &value));
        TEST_REQUIRE(value == 1.0f);
    }
    {
        float value;
        TEST_REQUIRE(base::Scanf("   1.0   ", &value));
        TEST_REQUIRE(value == 1.0f);
    }
    {
        float value;
        TEST_REQUIRE(base::Scanf("-1.0", &value));
        TEST_REQUIRE(value == -1.0f);
    }
    {
        float value;
        TEST_REQUIRE(base::Scanf("-1.5", &value));
        TEST_REQUIRE(value == -1.5f);
    }
    {
        float one, two;
        TEST_REQUIRE(base::Scanf("123.0 321.0", &one, &two));
        TEST_REQUIRE(one == 123.0f);
        TEST_REQUIRE(two == 321.0f);
    }
    {
        float one, two;
        TEST_REQUIRE(base::Scanf("123.0, 321.0", &one, ",", &two));
        TEST_REQUIRE(one == 123.0f);
        TEST_REQUIRE(two == 321.0f);
    }

    // int
    {
        int value;
        TEST_REQUIRE(base::Scanf("123", &value));
        TEST_REQUIRE(value == 123);
    }
    {
        int value;
        TEST_REQUIRE(base::Scanf("   123", &value));
        TEST_REQUIRE(value == 123);
    }
    {
        int value;
        TEST_REQUIRE(base::Scanf("   123   ", &value));
        TEST_REQUIRE(value == 123);
    }
    {
        int value;
        TEST_REQUIRE(base::Scanf("-123", &value));
        TEST_REQUIRE(value == -123);
    }
    {
        int one, two;
        TEST_REQUIRE(base::Scanf("123 321", &one, &two));
        TEST_REQUIRE(one == 123);
        TEST_REQUIRE(two == 321);
    }

    // string
    {
        std::string value;
        TEST_REQUIRE(base::Scanf("'keke'", &value));
        TEST_REQUIRE(value == "keke");
    }
    {
        std::string value;
        TEST_REQUIRE(base::Scanf("'keke kuku'", &value));
        TEST_REQUIRE(value == "keke kuku");
    }

    {
        std::string value;
        TEST_REQUIRE(base::Scanf("  'keke kuku'", &value));
        TEST_REQUIRE(value == "keke kuku");
    }

    {
        std::string value;
        TEST_REQUIRE(base::Scanf("'keke kuku'   ", &value));
        TEST_REQUIRE(value == "keke kuku");
    }
    {
        std::string one, two;
        TEST_REQUIRE(base::Scanf("'foo' 'bar'", &one, &two));
        TEST_REQUIRE(one == "foo");
        TEST_REQUIRE(two == "bar");
    }

    {
        std::string value;
        TEST_REQUIRE(base::Scanf("'don\\'t know \\ anything'", &value));
        TEST_REQUIRE(value == "don't know \\ anything");
    }


    {
        base::FPoint point;
        TEST_REQUIRE(base::Scanf("1.0,2.0", &point));
        TEST_REQUIRE(point.GetX() == 1.0f);
        TEST_REQUIRE(point.GetY() == 2.0f);
    }
    {
        base::FPoint point;
        TEST_REQUIRE(base::Scanf("foobar 1.0,2.0", "foobar", &point));
        TEST_REQUIRE(point.GetX() == 1.0f);
        TEST_REQUIRE(point.GetY() == 2.0f);
    }

    {
        base::FSize size;
        TEST_REQUIRE(base::Scanf("foobar 1.0,2.0", "foobar", &size));
        TEST_REQUIRE(size.GetWidth() == 1.0f);
        TEST_REQUIRE(size.GetHeight() == 2.0f);
    }

    {
        bool val = false;
        TEST_REQUIRE(base::Scanf("value is true", "value", "is", &val));
        TEST_REQUIRE(val == true);
    }
    {
        bool val = true;
        TEST_REQUIRE(base::Scanf("value is false", "value", "is", &val));
        TEST_REQUIRE(val == false);
    }

    {
        bool val = false;
        TEST_REQUIRE(base::Scanf("value is 1", "value", "is", &val));
        TEST_REQUIRE(val == true);
    }
    {
        bool val = true;
        TEST_REQUIRE(base::Scanf("value is 0", "value", "is", &val));
        TEST_REQUIRE(val == false);
    }

    {
        bool val = true;
        TEST_REQUIRE(base::Scanf("set property foobar false", "set", "property", "foobar", &val));
        TEST_REQUIRE(val == false);
    }
}

void unit_test_scanf_failure()
{
    TEST_CASE(test::Type::Feature)
    TEST_CASE_TODO
}

EXPORT_TEST_MAIN(
int test_main(int argc, char* argv[])
{
    unit_test_scanf_success();
    unit_test_scanf_failure();
    return 0;
}
) // TEST_MAIN