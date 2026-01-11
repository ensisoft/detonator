// Copyright (C) 2020-2026 Sami Väisänen
// Copyright (C) 2020-2026 Ensisoft http://www.ensisoft.com
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

#pragma once

#include "config.h"

#include <vector>
#include <algorithm> // for swap, min, max
#include <cmath> // for fabs

#include "base/assert.h"
#include "base/math.h"

namespace base
{
    enum class TriangleWindingOrder {
        Undetermined,
        Clockwise,
        CounterClockwise,
    };

    // Given 3 vertices find the polygon winding order.
    template<typename Vec2>
    TriangleWindingOrder FindTriangleWindingOrder(const Vec2& a, const Vec2& b, const Vec2& c) noexcept
    {
        // https://stackoverflow.com/questions/9120032/determine-winding-of-a-2d-triangles-after-triangulation
        // https://www.element84.com/blog/determining-the-winding-of-a-polygon-given-as-a-set-of-ordered-points

        // positive area indicates clockwise winding.
        // negative area indicates counter-clockwise winding.
        // zero area is degenerate case and the vertices are collinear (not linearly independent)
        const float ret = (b.y - a.y) * (c.x - b.x) - (b.x - a.x) * (c.y - b.y);
        if (ret > 0.0f)
            return TriangleWindingOrder::Clockwise;
        else if (ret < 0.0f)
            return TriangleWindingOrder::CounterClockwise;
        return TriangleWindingOrder::Undetermined;
    }

    template<typename Vertex>
    std::vector<Vertex> FindConvexHull(const Vertex* vertices, size_t vertex_count)
    {
        // this is the so-called "Jarvis March."

        std::vector<Vertex> hull;

        if (vertex_count < 3)
            return hull;

        // find the leftmost point index.
        auto leftmost = 0;
        for (size_t i=1; i<vertex_count; ++i)
        {
            const auto& a = GetPosition(vertices[i]);
            const auto& b = GetPosition(vertices[leftmost]);
            if (a.x < b.x)
                leftmost = i;
        }

        auto current = leftmost;
        do
        {
            // add the most recently found point to the hull
            hull.push_back(vertices[current]);

            // take a guess at choosing the next vertex.
            auto next = (current + 1) % vertex_count;

            // we can imagine there to be a line from the current vertex to the
            // next vertex. then we look for vertices that are to the left of this line.
            // any such vertex will become the next new guess, and then we repeat.
            // so this will always choose the "leftmost" vertex with respect to the
            // current line segment between current and next.
            //
            //
            //         x
            //
            //   a---------->b
            //
            //         y
            //
            //
            // when looking from A to B X is to the left and Y is to the right.
            // we can test for this by checking the polygon winding order.
            // triangle (a, b, x) will have counter-clockwise winding order while
            // triangle (a, b, y) will have clockwise winding order.
            //
            // note that for this algorithm it doesn't matter if we choose left or right since
            // either selection will result in the same convex hull except that in different
            // order of vertices.

            // look for a vertex that is to the left of the current line segment between a and b.
            // i.e. current and next.
            for (size_t i=0; i<vertex_count; ++i)
            {
                if (i == next)
                    continue;

                const auto& a = GetPosition(vertices[current]);
                const auto& b = GetPosition(vertices[next]);
                const auto& c = GetPosition(vertices[i]);
                if (FindTriangleWindingOrder(a, b, c) == TriangleWindingOrder::CounterClockwise)
                    next = i;
            }
            current = next;
        }
        while (current != leftmost);

        return hull;
    }
    template<typename Vertex>
    std::vector<Vertex> FindConvexHull(const std::vector<Vertex>& vertices)
    {
        if (vertices.empty())
            return {};
        return FindConvexHull(&vertices[0], vertices.size());
    }

    // Check whether the given point is inside the given rectangle or not.
    static bool TestPointRectIntersection(const float minX, const float maxX, const float minY, const float maxY, // rectangle
                                          const float x, const float y) // point
    {
        if (x < minX || x > maxX)
            return false;
        if (y < minY || y > maxY)
            return false;
        return true;
    }

    static bool TestRectCircleIntersection(const float minX, const float maxX, const float minY, const float maxY, // rectangle
                                           const float x, const float y, const float radius)// circle

    {
        ASSERT(maxX >= minX);
        ASSERT(maxY >= minY);

        // find the point closest to the center of the circle inside the rectangle
        const auto pointX = math::clamp(minX, maxX, x);
        const auto pointY = math::clamp(minY, maxY, y);

        // if the distance from the center of the circle to the closest
        // point inside the rectangle is less than the radius of the circle
        // then the shapes are in collision.
        const auto distX = x - pointX;
        const auto distY = y - pointY;

        // Pythagoras theorem applies, a² + b² = c²
        // using a squared radius allows us to avoid computing the actual distance
        // using a square root which is expensive. the results are the same.
        const auto distance_squared = distX*distX + distY*distY;
        const auto radius_squared = radius*radius;
        const auto collision = distance_squared < radius_squared;
        return collision;
    }

    static bool TestRectLineIntersection(const float minX, const float maxX, const float minY, const float maxY, // rectangle
                                         float x1, float y1, float x2, float y2) // line
    {
        ASSERT(maxX >= minX);
        ASSERT(maxY >= minY);
        //ASSERT(x2 >= x1);
        if (x2 < x1)
        {
            std::swap(x1, x2);
            std::swap(y1, y2);
        }

        const auto line_maxY = std::max(y1, y2);
        const auto line_minY = std::min(y1, y2);
        const auto line_minX = std::min(x1, x2);
        const auto line_maxX = std::max(x1, x2);

        // simple rejection criteria. when the line is completely in
        // some region such as left, right, above or above the
        // rectangle (i.e. both end points) then it cannot intersect
        // with the rectangle.

        if (line_maxX < minX) // left check
            return false;
        if (line_minX > maxX) // right check
            return false;
        if (line_maxY < minY) // above check
            return false;
        if (line_minY > maxY) // below check
            return false;

        const auto dx = x2 - x1;
        const auto dy = y2 - y1;

        // simple cases for nearly vertical or horizontal lines.  lines
        // that are nearly vertical or horizontal are quickly discarded
        // in the above check. if they aren't then they must intersect
        // with the rect. also keep in mind that when the line is vertical
        // the slope cannot be computed, so this special condition must be
        // avoided.
        if (fabs(dx <= 0.001) || fabs(dy) <= 0.001)
            return true;

        // the rest of the code here deals with sloping lines. a sloping
        // line must then cut through at least one of the edges of the rect.


        // simple case if either end point is inside the rectangle then
        // the line and the rectangle intersect.
        if (TestPointRectIntersection(minX, maxX, minY, maxY, x1, y1) ||
            TestPointRectIntersection(minX, maxX, minY, maxY, x2, y2))
            return true;

        // difficult case remains, both line end points are outside the rectangle.
        // if the intersects with either the left or right edge of the rect then
        // intersection takes place.

        // compute line slope
        const auto m = dy / dx;
        // compute line Y-intercept
        const auto b = y1 - m * x1;

        // check the Y intercept at minX rect boundary
        {
            const auto y_intercept = y1 + m*(minX-x1);
            if (y_intercept >= minY && y_intercept <= maxY)
                return true;
        }
        // check the Y intercept at maxX rect boundary
        {
            const auto y_intercept = y1 + m*(maxX-x1);
            if (y_intercept >= minY && y_intercept <= maxY)
                return true;
        }

        // remember the line equation with slope-intercept form
        // y = mx + b, where m is the slope and b is the y-intercept.
        // re-arranging this can be solved for x, x = (y - b)/m

        // check the X intercept on minY rect boundary
        {
            const auto x_intercept = (minY - b)/m;
            if (x_intercept >= minX && x_intercept <= maxX)
                return true;
        }
        // check the X intercept on maxY rect boundary
        {
            const auto x_intercept = (maxY - b)/m;
            if (x_intercept >= minX && x_intercept <= maxX)
                return true;
        }

        return false;
    }

} // namespace