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

#include <algorithm>
#include <iostream>

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "base/math.h"

#if !defined(UNIT_TEST_BUNDLE)
#  include "base/assert.cpp"
#endif

struct Point2D {
    float x, y;
};

bool operator==(const Point2D& lhs, const Point2D& rhs)
{
    return real::equals(lhs.x, rhs.x) &&
           real::equals(lhs.y, rhs.y);
}

inline Point2D GetPosition(const Point2D& p)
{ return p; }

void unit_test_triangle_winding_order()
{
    TEST_CASE(test::Type::Feature)

    const Point2D a = {1.0f,  0.0f};
    const Point2D b = {2.0f,  1.0f};
    const Point2D c = {2.0f, -1.0f};

    TEST_REQUIRE(math::FindTriangleWindingOrder(a, b, c) == math::TriangleWindingOrder::Clockwise);
    TEST_REQUIRE(math::FindTriangleWindingOrder(c, b, a) == math::TriangleWindingOrder::CounterClockwise);
    TEST_REQUIRE(math::FindTriangleWindingOrder(a, a, a) == math::TriangleWindingOrder::Undetermined);
}

void unit_test_convex_hull()
{
    TEST_CASE(test::Type::Feature)

    std::vector<Point2D> points;
    points.push_back({2.0f, 2.0f});
    points.push_back({4.0f, 4.0f});
    points.push_back({4.0f, 1.0f}); // inside
    points.push_back({5.0f, 2.0f}); // inside
    points.push_back({6.0f, 1.0f}); // inside
    points.push_back({4.0f, -1.0f});
    points.push_back({8.0f, 2.0f});

    const std::vector<Point2D> expected = {
        {2.0f, 2.0f},
        {4.0f, 4.0f},
        {8.0f, 2.0f},
        {4.0f, -1.0f}
    };

    for (size_t i=0; i<points.size(); ++i)
    {
        auto ret = math::FindConvexHull(points);
        for (const auto& p : ret)
            std::cout << "(" << p.x << "," << p.y << ") ";
        std::cout << std::endl;

        TEST_REQUIRE(ret.size() == expected.size());
        for (size_t i=0; i<ret.size(); ++i)
            TEST_REQUIRE(ret[i] == expected[i]);

        std::rotate(points.begin(), points.begin() +1 , points.end());
    }
}

void unit_test_rect_circle_intersection()
{
    TEST_CASE(test::Type::Feature)

    // outside any edge
    TEST_REQUIRE(!math::CheckRectCircleIntersection(0.0f, 100.0f, 0.0f, 100.0f, 110.0f, 50.0f, 10.0f));
    TEST_REQUIRE(!math::CheckRectCircleIntersection(0.0f, 100.0f, 0.0f, 100.0f, -10.0f, 50.0f, 10.0f));
    TEST_REQUIRE(!math::CheckRectCircleIntersection(0.0f, 100.0f, 0.0f, 100.0f,  50.0f, 110.0f, 10.0f));
    TEST_REQUIRE(!math::CheckRectCircleIntersection(0.0f, 100.0f, 0.0f, 100.0f,  50.0f, -10.0f, 10.0f));

    // intersecting any edge
    TEST_REQUIRE(math::CheckRectCircleIntersection(0.0f, 100.0f, 0.0f, 100.0f, 110.0f, 50.0f, 15.0f));
    TEST_REQUIRE(math::CheckRectCircleIntersection(0.0f, 100.0f, 0.0f, 100.0f, -10.0f, 50.0f, 15.0f));
    TEST_REQUIRE(math::CheckRectCircleIntersection(0.0f, 100.0f, 0.0f, 100.0f,  50.0f, 110.0f, 15.0f));
    TEST_REQUIRE(math::CheckRectCircleIntersection(0.0f, 100.0f, 0.0f, 100.0f,  50.0f, -10.0f, 15.0f));

    // inside the rect
    TEST_REQUIRE(math::CheckRectCircleIntersection(0.0f, 100.0f, 0.0f, 100.0f, 50.0f, 50.0f, 10.0f));
}

EXPORT_TEST_MAIN(
int test_main(int argc, char* argv[])
{
    unit_test_triangle_winding_order();
    unit_test_convex_hull();
    unit_test_rect_circle_intersection();
    return 0;
}
) // TEST_MAIN