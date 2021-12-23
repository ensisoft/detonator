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

#include <chrono>
#include <thread>
#include <iostream>

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "base/color4f.h"
#include "base/types.h"
#include "base/trace.h"

bool operator==(const base::Color4f& lhs, const base::Color4f& rhs)
{
    return real::equals(lhs.Red(), rhs.Red()) &&
           real::equals(lhs.Green(), rhs.Green()) &&
           real::equals(lhs.Blue(), rhs.Blue()) &&
           real::equals(lhs.Alpha(), rhs.Alpha());
}

bool operator==(const base::Rect<float>& lhs,
                const base::Rect<float>& rhs)
{
    return real::equals(lhs.GetX(), rhs.GetX()) &&
           real::equals(lhs.GetY(), rhs.GetY()) &&
           real::equals(lhs.GetWidth(), rhs.GetWidth()) &&
           real::equals(lhs.GetHeight(), rhs.GetHeight());
}

template<typename T>
bool operator==(const base::Rect<T>& lhs,
                const base::Rect<T>& rhs)
{
    return lhs.GetX() == rhs.GetX() &&
           lhs.GetY() == rhs.GetY() &&
           lhs.GetWidth() == rhs.GetWidth() &&
           lhs.GetHeight() == rhs.GetHeight();
}

template<typename T>
void unit_test_rect()
{
    using R = base::Rect<T>;
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

void unit_test_rect_quadrants()
{
    {
        base::FRect rect(0.0f, 0.0f, 100.0f, 50.0f);
        const auto [q0, q1, q2, q3] = rect.Quadrants();
        TEST_REQUIRE(q0 == base::FRect(0.0f, 0.0f, 50.0f, 25.0f));
        TEST_REQUIRE(q1 == base::FRect(0.0f, 25.0f, 50.0f, 25.0f));
        TEST_REQUIRE(q2 == base::FRect(50.0f, 0.0f, 50.0f, 25.0f));
        TEST_REQUIRE(q3 == base::FRect(50.0f, 25.0f, 50.0f, 25.0f));
    }

    {
        base::FRect rect(-100.0f, -100.0f, 200.0f, 200.0f);
        const auto [q0, q1, q2, q3] = rect.Quadrants();
        TEST_REQUIRE(q0 == base::FRect(-100.0f, -100.0f, 100.0f, 100.0f));
        TEST_REQUIRE(q1 == base::FRect(-100.0f, 0.0f, 100.0f, 100.0f));
        TEST_REQUIRE(q2 == base::FRect(0.0f, -100.0f, 100.0f, 100.0f));
        TEST_REQUIRE(q3 == base::FRect(0.0f, 0.0f, 100.0f, 100.0f));
    }
}


template<typename T>
void unit_test_rect_intersect()
{
    using R = base::Rect<T>;

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
        {
            R(0, 0, 10, 10),
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
    using R = base::Rect<T>;

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
void unit_test_rect_test_point()
{
    using R = base::Rect<T>;
    using P = base::Point<T>;

    const R rect(10, 10, 15, 7);
    TEST_REQUIRE(!rect.TestPoint(0, 0));
    TEST_REQUIRE(!rect.TestPoint(10, 10));
    TEST_REQUIRE(!rect.TestPoint(11, 8));
    TEST_REQUIRE(!rect.TestPoint(11, 30));
    TEST_REQUIRE(rect.TestPoint(11, 11));
}

void bar()
{
    TRACE_SCOPE("bar");
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
}

void foo()
{
    TRACE_SCOPE("foo");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    bar();
}
void meh()
{
    TRACE_SCOPE("meh", "foo=%u", 123);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

void unit_test_trace()
{
    base::TraceLog trace(10);

    base::SetThreadTrace(&trace);
    base::TraceStart();
    {
        TRACE_SCOPE("unit_test");
        foo();
        meh();
    }
    TEST_REQUIRE(trace.GetNumEntries() == 4);
    TEST_REQUIRE(trace.GetEntry(0).level == 0);
    TEST_REQUIRE(trace.GetEntry(0).name == "unit_test");
    TEST_REQUIRE(trace.GetEntry(1).level == 1);
    TEST_REQUIRE(trace.GetEntry(1).name == "foo");
    TEST_REQUIRE(trace.GetEntry(2).level == 2);
    TEST_REQUIRE(trace.GetEntry(2).name == "bar");
    TEST_REQUIRE(trace.GetEntry(3).level == 1);
    TEST_REQUIRE(trace.GetEntry(3).name == "meh");
    TEST_REQUIRE(trace.GetEntry(3).comment == "foo=123");

    for (size_t i=0; i<trace.GetNumEntries(); ++i)
    {
        const auto& entry = trace.GetEntry(i);
        for (unsigned i=0; i<entry.level; ++i)
            std::cout << "  ";
        std::cout << entry.name <<  " " << entry.finish_time - entry.start_time << entry.comment << std::endl;
    }
}

int test_main(int argc, char* argv[])
{
    unit_test_rect<int>();
    unit_test_rect_quadrants();
    unit_test_rect_intersect<float>();
    unit_test_rect_intersect<int>();
    unit_test_rect_union<float>();
    unit_test_rect_union<int>();
    unit_test_rect_test_point<int>();
    unit_test_rect_test_point<float>();
    unit_test_trace();
    return 0;
}