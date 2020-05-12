// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#include "config.h"

#include <algorithm>
#include <iostream>

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "base/math.h"

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

int test_main(int argc, char* argv[])
{
    {
        const Point2D a = {1.0f,  0.0f};
        const Point2D b = {2.0f,  1.0f};
        const Point2D c = {2.0f, -1.0f};

        TEST_REQUIRE(math::FindTriangleWindingOrder(a, b, c) == math::TriangleWindingOrder::Clockwise);
        TEST_REQUIRE(math::FindTriangleWindingOrder(c, b, a) == math::TriangleWindingOrder::CounterClockwise);
        TEST_REQUIRE(math::FindTriangleWindingOrder(a, a, a) == math::TriangleWindingOrder::Undetermined);
    }

    // convex hull
    {
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

    return 0;
}