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
#include "base/test_help.h"
#include "base/color4f.h"
#include "base/types.h"
#include "base/trace.h"
#include "base/utility.h"
#include "base/allocator.h"

#if !defined(UNIT_TEST_BUNDLE)
#  include "base/json.cpp"
#  include "base/utility.cpp"
#  include "base/trace.cpp"
#  include "base/assert.cpp"
#endif

template<typename T>
void unit_test_rect()
{
    TEST_CASE(test::Type::Feature)

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
    TEST_CASE(test::Type::Feature)

    {
        base::FRect rect(0.0f, 0.0f, 100.0f, 50.0f);
        const auto [q0, q1, q2, q3] = rect.GetQuadrants();
        TEST_REQUIRE(q0 == base::FRect(0.0f, 0.0f, 50.0f, 25.0f));
        TEST_REQUIRE(q1 == base::FRect(0.0f, 25.0f, 50.0f, 25.0f));
        TEST_REQUIRE(q2 == base::FRect(50.0f, 0.0f, 50.0f, 25.0f));
        TEST_REQUIRE(q3 == base::FRect(50.0f, 25.0f, 50.0f, 25.0f));
    }

    {
        base::FRect rect(-100.0f, -100.0f, 200.0f, 200.0f);
        const auto [q0, q1, q2, q3] = rect.GetQuadrants();
        TEST_REQUIRE(q0 == base::FRect(-100.0f, -100.0f, 100.0f, 100.0f));
        TEST_REQUIRE(q1 == base::FRect(-100.0f, 0.0f, 100.0f, 100.0f));
        TEST_REQUIRE(q2 == base::FRect(0.0f, -100.0f, 100.0f, 100.0f));
        TEST_REQUIRE(q3 == base::FRect(0.0f, 0.0f, 100.0f, 100.0f));
    }
}


template<typename T>
void unit_test_rect_intersect()
{
    TEST_CASE(test::Type::Feature)

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
            R(10, 0, 0, 10)
        },
        // no overlap on x axis
        {
            R(0, 0, 10, 10),
            R(-10, 0, 10, 10),
            R(0, 0, 0, 10)
        },
        // no overlap on y axis
        {
            R(0, 0, 10, 10),
            R(0, 10, 10, 10),
            R(0, 10, 10, 0)
        },
        // no overlap on y axis
        {
            R(0, 0, 10, 10),
            R(0, -10, 10, 10),
            R(0, 0, 10, 0)
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
    TEST_CASE(test::Type::Feature)

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
    TEST_CASE(test::Type::Feature)

    using R = base::Rect<T>;
    using P = base::Point<T>;

    const R rect(10, 10, 15, 7);
    TEST_REQUIRE(!rect.TestPoint(0, 0));
    TEST_REQUIRE(!rect.TestPoint(11, 8));
    TEST_REQUIRE(!rect.TestPoint(11, 30));

    TEST_REQUIRE(rect.TestPoint(11, 11));

    // border values.
    TEST_REQUIRE(rect.TestPoint(10, 10));
    TEST_REQUIRE(rect.TestPoint(25, 17));
}

void unit_test_rect_mapping()
{
    base::FRect rect(0.0f, 0.0f, 100.0f, 200.0f);

    {
        base::FRect ret;
        ret = MapToLocalNormalize(rect, base::FRect(0.0f, 0.0f, 10.0f, 10.0f));
        TEST_REQUIRE(ret == base::FRect(0.0f,  0.0f, 0.1f, 0.05f));

        ret = MapToLocalNormalize(rect, base::FRect(10.0f, 0.0f, 10.0f, 10.0f));
        TEST_REQUIRE(ret == base::FRect(0.1f, 0.0f, 0.1f, 0.05f));

        ret = MapToLocalNormalize(rect, base::FRect(0.0f, -10.0f, 10.0f, 10.0f));
        TEST_REQUIRE(ret == base::FRect(0.0f, -0.05f, 0.1f, 0.05f));
    }

    {
        base::FRect ret;
        ret = MapToGlobalExpand(rect, base::FRect(0.0f, 0.0f, 0.1f, 0.1f));
        TEST_REQUIRE(ret == base::FRect(0.0f, 0.0f, 10.0f, 20.0f));
        ret = MapToGlobalExpand(rect, base::FRect(0.1f, 0.1f, 0.1f, 0.1f));
        TEST_REQUIRE(ret == base::FRect(10.0f, 20.0f, 10.0f, 20.0f));

        rect.Translate(150.0f, 50.0f);
        ret = MapToGlobalExpand(rect, base::FRect(0.1f, 0.1f, 0.1f, 0.1f));
        TEST_REQUIRE(ret == base::FRect(160.0f, 70.0f, 10.0f, 20.0f));
    }
}

namespace tracing_test {
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
void keke()
{
    TRACE_SCOPE("keke");
    base::TraceComment("keke");
    base::TraceMarker("keke");
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

void meh()
{
    TRACE_SCOPE("meh", "foo=%u", 123);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    keke();
}
} // tracing test

void unit_test_trace()
{
    base::TraceLog trace(10);

    base::SetThreadTrace(&trace);
    base::EnableTracing(true);
    base::TraceStart();
    {
        using namespace tracing_test;

        TRACE_SCOPE("unit_test");
        foo();
        meh();
    }
    TEST_REQUIRE(trace.GetNumEntries() == 5);
    TEST_REQUIRE(trace.GetEntry(0).level == 0);
    TEST_REQUIRE(trace.GetEntry(0).name == std::string("unit_test"));
    TEST_REQUIRE(trace.GetEntry(1).level == 1);
    TEST_REQUIRE(trace.GetEntry(1).name == std::string("foo"));
    TEST_REQUIRE(trace.GetEntry(2).level == 2);
    TEST_REQUIRE(trace.GetEntry(2).name == std::string("bar"));
    TEST_REQUIRE(trace.GetEntry(3).level == 1);
    TEST_REQUIRE(trace.GetEntry(3).name == std::string("meh"));
    TEST_REQUIRE(trace.GetEntry(3).comment == "foo=123");
    TEST_REQUIRE(trace.GetEntry(4).name == std::string("keke"));
    TEST_REQUIRE(trace.GetEntry(4).comment == "keke");
    TEST_REQUIRE(trace.GetEntry(4).markers.size() ==  1);
    TEST_REQUIRE(trace.GetEntry(4).markers[0] == "keke");

    for (size_t i=0; i<trace.GetNumEntries(); ++i)
    {
        const auto& entry = trace.GetEntry(i);
        for (unsigned i=0; i<entry.level; ++i)
            std::cout << "  ";
        std::cout << entry.name <<  " " << entry.finish_time - entry.start_time << entry.comment << std::endl;
    }
}


void unit_test_util()
{
    TEST_CASE(test::Type::Feature)

    {
        std::vector<int> foo = {1, 2, 3};
        std::vector<int> bar = {4, 5, 6};

        const auto& ret = base::CombineVectors(std::move(foo), std::move(bar));
        TEST_REQUIRE(ret.size() == 6);
        TEST_REQUIRE(ret[0] == 1);
        TEST_REQUIRE(ret[1] == 2);
        TEST_REQUIRE(ret[2] == 3);
        TEST_REQUIRE(ret[3] == 4);
        TEST_REQUIRE(ret[4] == 5);
        TEST_REQUIRE(ret[5] == 6);
    }
}

void unit_test_allocator()
{
    TEST_CASE(test::Type::Feature)

    struct Kiwi {
        std::string foo;
    };
    struct Banana {
        std::string foo;
        double value;
    };

    using Allocator = base::Allocator<Kiwi, Banana>;

    {
        Allocator allocator;

        auto* kiwi0 = allocator.CreateObject<Kiwi>(0);
        kiwi0->foo = "kiwi0";

        auto* banana0 = allocator.CreateObject<Banana>(0);
        banana0->foo = "banana0";
        banana0->value = 123;

        {
            TEST_REQUIRE(allocator.GetObject<Kiwi>(0)->foo == "kiwi0");
            TEST_REQUIRE(allocator.GetObject<Banana>(0)->foo == "banana0");
            TEST_REQUIRE(allocator.GetObject<Banana>(0)->value == 123);
        }

        allocator.DestroyObject(0, kiwi0);
        TEST_REQUIRE(allocator.GetObject<Kiwi>(0) == nullptr);
        TEST_REQUIRE(allocator.GetObject<Banana>(0) != nullptr);
        TEST_REQUIRE(allocator.GetObject<Banana>(0)->foo == "banana0");
        TEST_REQUIRE(allocator.GetObject<Banana>(0)->value == 123);

        allocator.DestroyObject(0, banana0);
        TEST_REQUIRE(allocator.GetObject<Banana>(0) == nullptr);
    }

    {
        Allocator allocator;
        auto index0 = allocator.GetNextIndex();
        TEST_REQUIRE(index0 == 0);
        TEST_REQUIRE(allocator.GetCount() == 1);

        auto* kiwi0 = allocator.CreateObject<Kiwi>(index0);
        kiwi0->foo = "kiwi0";

        auto index1 = allocator.GetNextIndex();
        TEST_REQUIRE(index1 == 1);
        TEST_REQUIRE(allocator.GetCount() == 2);
        auto* kiwi1 = allocator.CreateObject<Kiwi>(index1);
        kiwi1->foo = "kiwi1";


        allocator.DestroyAll(index0);
        allocator.FreeIndex(index0);
        TEST_REQUIRE(allocator.GetObject<Kiwi>(0) == nullptr);
        TEST_REQUIRE(allocator.GetCount() == 1);


        auto index2 = allocator.GetNextIndex();
        TEST_REQUIRE(index2 == 0);
        TEST_REQUIRE(allocator.GetCount() == 2);
        TEST_REQUIRE(allocator.GetObject<Kiwi>(0) == nullptr);
        auto kiwi2 = allocator.CreateObject<Kiwi>(index2);
        kiwi2->foo = "kiwi2";

        allocator.DestroyAll(index2);
        allocator.FreeIndex(index2);

        allocator.DestroyAll(index1);
        allocator.FreeIndex(index1);

        TEST_REQUIRE(allocator.GetCount() == 0);
        TEST_REQUIRE(allocator.GetNextIndex() == 1);
        TEST_REQUIRE(allocator.GetNextIndex() == 0);

        allocator.FreeIndex(0);
        allocator.FreeIndex(1);

    }

    {
        Allocator allocator;
        auto index0 = allocator.GetNextIndex();
        auto index1 = allocator.GetNextIndex();
        auto index2 = allocator.GetNextIndex();
        allocator.CreateObject<Kiwi>(0);
        allocator.CreateObject<Kiwi>(2);

        TEST_REQUIRE(allocator.GetObject<Kiwi>(0) != nullptr);
        TEST_REQUIRE(allocator.GetObject<Kiwi>(1) == nullptr);
        TEST_REQUIRE(allocator.GetObject<Kiwi>(2) != nullptr);
        allocator.GetObject<Kiwi>(0)->foo = "kiwi0";
        allocator.GetObject<Kiwi>(2)->foo = "kiwi2";

        using Sequence = base::AllocatorSequence<Kiwi, Kiwi, Banana>;
        Sequence sequence(&allocator);

        auto beg = sequence.begin();
        auto end = sequence.end();
        TEST_REQUIRE(beg != end);
        TEST_REQUIRE(beg->foo == "kiwi0");
        ++beg;
        TEST_REQUIRE(beg->foo == "kiwi2");
        ++beg;
        TEST_REQUIRE(beg == end);

        beg = sequence.begin();
        end = sequence.end();
        TEST_REQUIRE(beg->foo == "kiwi0");
        beg++;
        TEST_REQUIRE(beg->foo == "kiwi2");
        beg++;
        TEST_REQUIRE(beg == end);

        allocator.Cleanup();
    }

    {

        auto ret = test::TimedTest(1000, []() {
            std::vector<Kiwi> vector;
            for (size_t i=0; i<1000; ++i)
            {
                vector.emplace_back(Kiwi{});
                auto& back = vector.back();
                back.foo = "kiwi";
            }
            for (size_t i=0; i<1000; ++i)
            {
                vector[i].foo = "iwik";
            }
            vector.clear();
        });
        test::PrintTestTimes("Vector push_back (Kiwi)", ret);
    }
    {
        auto ret = test::TimedTest(1000, []() {
            base::Allocator<Kiwi> allocator;
            for (size_t i=0; i<1000; ++i)
            {
                auto index = allocator.GetNextIndex();
                auto* kiwi = allocator.CreateObject<Kiwi>(index);
                kiwi->foo = "kiwi";
            }
            for (size_t i=0; i<1000; ++i)
            {
                auto* kiwi = allocator.GetObject<Kiwi>(i);
                kiwi->foo = "iwik";
            }
            allocator.Cleanup();
        });
        test::PrintTestTimes("Allocator CreateObject (Kiwi)", ret);
    }

}

void unit_test_string()
{
    TEST_CASE(test::Type::Feature)

    // split string on space
    {
        std::vector<std::string> ret;
        ret = base::SplitString("foobar");
        TEST_REQUIRE(ret.size() == 1);
        TEST_REQUIRE(ret[0] == "foobar");

        ret = base::SplitString("foo bar");
        TEST_REQUIRE(ret.size() == 2);
        TEST_REQUIRE(ret[0] == "foo");
        TEST_REQUIRE(ret[1] == "bar");

        ret = base::SplitString("foo      bar");
        TEST_REQUIRE(ret.size() == 2);
        TEST_REQUIRE(ret[0] == "foo");
        TEST_REQUIRE(ret[1] == "bar");
    }

    // split string on new line
    {
        std::vector<std::string> ret;
        ret = base::SplitString("foobar\n", '\n');
        TEST_REQUIRE(ret.size() == 1);
        TEST_REQUIRE(ret[0] == "foobar");

        ret = base::SplitString("foo\nbar", '\n');
        TEST_REQUIRE(ret.size() == 2);
        TEST_REQUIRE(ret[0] == "foo");
        TEST_REQUIRE(ret[1] == "bar");

        ret = base::SplitString("foo\n bar", '\n');
        TEST_REQUIRE(ret.size() == 2);
        TEST_REQUIRE(ret[0] == "foo");
        TEST_REQUIRE(ret[1] == " bar");
    }
}

EXPORT_TEST_MAIN(
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
    unit_test_rect_mapping();
    unit_test_trace();

    unit_test_util();

    unit_test_allocator();

    unit_test_string();
    return 0;
}
) // TEST_MAIN
