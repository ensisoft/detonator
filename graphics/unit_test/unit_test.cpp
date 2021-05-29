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

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "graphics/types.h"
#include "graphics/color4f.h"

bool operator==(const gfx::Color4f& lhs, const gfx::Color4f& rhs)
{
    return real::equals(lhs.Red(), rhs.Red()) &&
           real::equals(lhs.Green(), rhs.Green()) &&
           real::equals(lhs.Blue(), rhs.Blue()) &&
           real::equals(lhs.Alpha(), rhs.Alpha());
}

bool operator==(const gfx::Rect<float>& lhs,
                const gfx::Rect<float>& rhs)
{
    return real::equals(lhs.GetX(), rhs.GetX()) &&
            real::equals(lhs.GetY(), rhs.GetY()) &&
            real::equals(lhs.GetWidth(), rhs.GetWidth()) &&
            real::equals(lhs.GetHeight(), rhs.GetHeight());
}

template<typename T>
bool operator==(const gfx::Rect<T>& lhs,
                const gfx::Rect<T>& rhs)
{
    return lhs.GetX() == rhs.GetX() &&
        lhs.GetY() == rhs.GetY() &&
        lhs.GetWidth() == rhs.GetWidth() &&
        lhs.GetHeight() == rhs.GetHeight();
}

template<typename T>
void unit_test_rect()
{
    using R = gfx::Rect<T>;
    R r;
    TEST_REQUIRE(r.IsEmpty());
    TEST_REQUIRE(r.GetHeight() == T(0));
    TEST_REQUIRE(r.GetWidth() == T(0));
    TEST_REQUIRE(r.GetX() == T(0));
    TEST_REQUIRE(r.GetY() == T(0));

    r.Resize(100, 150);
    TEST_REQUIRE(r.IsEmpty() == false);
    TEST_REQUIRE(r.GetHeight() == T(150));
    TEST_REQUIRE(r.GetWidth() == T(100));
    TEST_REQUIRE(r.GetX() == T(0));
    TEST_REQUIRE(r.GetY() == T(0));

    r.Move(10, 20);
    TEST_REQUIRE(r.IsEmpty() == false);
    TEST_REQUIRE(r.GetHeight() == T(150));
    TEST_REQUIRE(r.GetWidth() == T(100));
    TEST_REQUIRE(r.GetX() == T(10));
    TEST_REQUIRE(r.GetY() == T(20));

    r.Translate(90, 80);
    TEST_REQUIRE(r.GetX() == T(100));
    TEST_REQUIRE(r.GetY() == T(100));
}

template<typename T>
void unit_test_rect_intersect()
{
    using R = gfx::Rect<T>;

    struct TestCase {
        R lhs;
        R rhs;
        R expected_result;
    } cases[] = {
        // empty rect no overlap
        {
            R(0, 0, 0, 0),
            R(0, 0, 1, 1),
            R()
        },
        // empty rect, no overlap
        {
            R(0, 0, 1, 1),
            R(0, 0, 0, 0),
            R()
        },

        // no overlap on x axis
        { R(0, 0, 10, 10),
          R(10, 0, 10, 10),
          R()
        },

        // no overlap on x axis
        {
            R(0, 0, 10, 10),
            R(-10, 0, 10, 10),
            R()
        },

        // no overlap on y axis
        {
            R(0, 0, 10, 10),
            R(0, 10, 10, 10),
            R()
        },

        // no overlap on y axis
        {
            R(0, 0, 10, 10),
            R(0, -10, 10, 10),
            R()
        },

        // overlaps itself
        {
            R(0, 0, 10, 10),
            R(0, 0, 10, 10),
            R(0, 0, 10, 10)
        },

        // sub rectangle within one overlaps
        {
            R(0, 0, 10, 10),
            R(2, 2, 5, 5),
            R(2, 2, 5, 5)
        },

        // overlap in bottom right corner
        {
            R(0, 0, 10, 10),
            R(5, 5, 10, 10),
            R(5, 5, 5, 5)
        },

        // overlap in top left corner
        {
            R(0, 0, 10, 10),
            R(-5, -5, 10, 10),
            R(0, 0, 5, 5)
        }

    };

    for (const auto& test : cases)
    {
        const auto& ret = Intersect(test.lhs, test.rhs);
        TEST_REQUIRE(ret == test.expected_result);
    }
}

template<typename T>
void unit_test_rect_union()
{
    using R = gfx::Rect<T>;

    struct TestCase {
        R lhs;
        R rhs;
        R expected_result;
    } cases[] = {
        // empty rectangle
        {
            R(0, 0, 0, 0),
            R(0, 0, 10, 10),
            R(0, 0, 10, 10)
        },
        // empty rectangle
        {
            R(0, 0, 10, 10),
            R(0, 0, 0, 0),
            R(0, 0, 10, 10)
        },

        // disjoint rectangles
        {
            R(0, 0, 5, 5),
            R(5, 5, 5, 5),
            R(0, 0, 10, 10)
        },

        // disjoint rectangles, negative values.
        {
            R(-5, -5, 5, 5),
            R(-10, -10, 5, 5),
            R(-10, -10, 10, 10)
        },

        // overlapping rectangles
        {
            R(20, 20, 10, 10),
            R(25, 25, 5, 5),
            R(20, 20, 10, 10)
        }
    };

    for (const auto& test : cases)
    {
        const auto& ret = Union(test.lhs, test.rhs);
        TEST_REQUIRE(ret == test.expected_result);
    }
}

template<typename T>
void unit_test_rect_serialize()
{
    const T test_values[] = {
        T(0), T(1.5), T(100), T(-40), T(125.0)
    };
    for (const auto& val : test_values)
    {
        const gfx::Rect<T> src(val, val, val, val);
        const auto& value = gfx::Rect<T>::FromJson(src.ToJson());
        TEST_REQUIRE(value.has_value());
        TEST_REQUIRE(value.value() == src);
    }
}

template<typename T>
void unit_test_rect_test_point()
{
    using R = gfx::Rect<T>;
    using P = gfx::Point<T>;

    const R rect(10, 10, 15, 7);
    TEST_REQUIRE(!rect.TestPoint(0, 0));
    TEST_REQUIRE(!rect.TestPoint(10, 10));
    TEST_REQUIRE(!rect.TestPoint(11, 8));
    TEST_REQUIRE(!rect.TestPoint(11, 30));
    TEST_REQUIRE(rect.TestPoint(11, 11));
}

void unit_test_color_serialize()
{
    const float test_values[] = {
        0.0f, 0.2f, 0.5f, 1.0f
    };
    for (const float val : test_values)
    {
        {
            const gfx::Color4f src(val, val, val, val);
            const auto& value = gfx::Color4f::FromJson(src.ToJson());
            TEST_REQUIRE(value.has_value());
            TEST_REQUIRE(value.value() == src);
        }
        {
            bool success = true;
            const auto& json = nlohmann::json::parse(
                R"({"r":0.0, "g":"basa", "b":0.0, "a":0.0})");

            const auto& value = gfx::Color4f::FromJson(json);
            TEST_REQUIRE(value.has_value() == false);
        }
    }
}


int test_main(int argc, char* argv[])
{
    unit_test_rect<int>();
    unit_test_rect_intersect<float>();
    unit_test_rect_intersect<int>();
    unit_test_rect_union<float>();
    unit_test_rect_union<int>();
    unit_test_rect_serialize<int>();
    unit_test_rect_serialize<float>();
    unit_test_rect_test_point<int>();
    unit_test_rect_test_point<float>();
    unit_test_color_serialize();

    return 0;
}
