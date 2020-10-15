// Copyright (c) 2010-2018 Sami Väisänen, Ensisoft
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

#pragma once

#include "config.h"

#include "warnpush.h"
#include "warnpop.h"

#include <type_traits>
#include <random>
#include <cmath>
#include <ctime>
#include <cstdlib>

namespace math
{
    const auto Pi = 3.14159265358979323846;

    template<typename T> inline
    T wrap(T min, T max, T val)
    {
        if (val > max)
            return min;
        if (val < min)
            return max;
        return val;
    }

    template<typename T> inline
    T clamp(T min, T max, T val)
    {
        if (val < min)
            return min;
        if (val > max)
            return max;
        return val;
    }

    template<typename T> inline
    T lerp(const T& y0, const T& y1, float t)
    {
        return (1.0f - t) * y0 + t * y1;
    }

    // Interpolation defines the function used to determine  the intermediate
    // values between y0 and y1 are achieved when T varies between 0.0f and 1.0f.
    // I.e. when T is 0.0 interpolation returns y0 and when T is 1.0 interpolation
    // returns y1, For other values a mix of y0 and y1 is returned.
    // https://codeplea.com/simple-interpolation
    enum class Interpolation {
        // No interpolation, a discrete jump from y0 to y1 when t is > 0.5.
        Step,
        // Linear interpolation also known as "lerp". Take a linear mix
        // of y0 and y1 in exact proportions per t.
        Linear,
        // Use cosine function to smooth t before doing linear interpolation.
        Cosine,
        // Use a polynomial function to smooth t before doing linear interpolation.
        SmoothStep,
        // Accelerate increase in y1 value when t approaches 1.0f
        Acceleration,
        // Decelerate increase in y1 value when t approaches 1.0f
        Deceleration
    };

    template<typename T> inline
    T interpolate(const T& y0, const T& y1, float t, Interpolation method)
    {
        if (method == Interpolation::Step)
            return (t < 0.5f) ? y0 : y1;
        else if (method == Interpolation::Cosine)
            t = -std::cos(Pi * t) * 0.5 + 0.5;
        else if (method == Interpolation::SmoothStep)
            t = 3*t*t - 2*t*t*t;
        else if (method == Interpolation::Acceleration)
            t = t*t;
        else if (method == Interpolation::Deceleration)
            t = 1.0f - ((1-t)*(1-t));
        return math::lerp(y0, y1, t);
    }


    // epsilon based check for float equality
    template<typename RealT> inline
    bool equals(RealT goal, RealT value, RealT epsilon = 0.0001)
    {
        // maybe the default epsilon needs to be reconsidered ?
        // FYI: powers of 2 can be presented exactly, i.e. 2.0, 4.0, 8.0
        // and also 1.0, 0.5, 0.25 etc.
        return std::abs(goal - value) <= epsilon;
    }

    // generate a random number in the range of min max (inclusive)
    // the random number generator is automatically seeded.
    template<typename T>
    T rand(T min, T max)
    {
        // if we enable this flag we always give out a deterministic
        // sequence by initializing the engine with a predeterminted seed.
        // this is convenient for example testing purposes, enable this
        // flag and always get the same sequence without having to change
        // the calling code that doesn't really care.
    #if defined(MATH_FORCE_DETERMINISTIC_RANDOM)
        static std::default_random_engine engine(0xdeadbeef);
    #else
        static std::default_random_engine engine(std::time(nullptr));
    #endif
        if constexpr (std::is_floating_point<T>::value) {
            std::uniform_real_distribution<T> dist(min, max);
            return dist(engine);
        } else {
            std::uniform_int_distribution<T> dist(min, max);
            return dist(engine);
        }
    }

    // Generate pseudo random numbers based on the given seed.
    template<typename T, size_t Seed>
    T rand(T min, T max)
    {
        static std::default_random_engine engine(Seed);
        if constexpr (std::is_floating_point<T>::value) {
            std::uniform_real_distribution<T> dist(min, max);
            return dist(engine);
        } else {
            std::uniform_int_distribution<T> dist(min, max);
            return dist(engine);
        }
    }

    enum class TriangleWindingOrder {
        Undetermined,
        Clockwise,
        CounterClockwise,

    };

    // Given 3 vertices find the polygon winding order.
    template<typename Vec2> inline
    TriangleWindingOrder FindTriangleWindingOrder(const Vec2& a, const Vec2& b, const Vec2& c)
    {
        // https://stackoverflow.com/questions/9120032/determine-winding-of-a-2d-triangles-after-triangulation
        // https://www.element84.com/blog/determining-the-winding-of-a-polygon-given-as-a-set-of-ordered-points

        // positive area indicates clockwise winding.
        // negative area indicates counter clockwise winding.n
        // zero area is degerate case and the vertices are collinear (not linearly independent)
        const float ret = (b.y - a.y) * (c.x - b.x) - (b.x - a.x) * (c.y - b.y);
        if (ret > 0.0f)
            return TriangleWindingOrder::Clockwise;
        else if (ret < 0.0f)
            return TriangleWindingOrder::CounterClockwise;
        return TriangleWindingOrder::Undetermined;
    }

    template<typename Vertex>
    std::vector<Vertex> FindConvexHull(const Vertex* verts, size_t num_verts)
    {
        // this is the so called "Jarvis March."

        std::vector<Vertex> hull;

        if (num_verts < 3)
            return hull;

        // find the leftmost point index.
        auto leftmost = 0;
        for (size_t i=1; i<num_verts; ++i)
        {
            const auto& a = GetPosition(verts[i]);
            const auto& b = GetPosition(verts[leftmost]);
            if (a.x < b.x)
                leftmost = i;
        }

        auto current = leftmost;
        do
        {
            // add the most recently found point to the hull
            hull.push_back(verts[current]);

            // take a guess at choosing the next vertex.
            auto next = (current + 1) % num_verts;

            // we can imagine there to be a line from the current vertex to the
            // next vertex. then we look for vertices that are to the left of this line.
            // any such vertex will become the next new guess and then we repeat.
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
            // triangle (a, b, x) will have counter clockwise winding order while
            // triangle (a, b, y) will have clockwise winding order.
            //
            // note that for this algorithm it doesn't matter if we choose left or right since
            // either selection will result in the same convex hull except that in different
            // order of vertices.

            // look for a vertex that is to the left of the current line segment between a and b.
            // i.e. current and next.
            for (size_t i=0; i<num_verts; ++i)
            {
                if (i == next)
                    continue;

                const auto& a = GetPosition(verts[current]);
                const auto& b = GetPosition(verts[next]);
                const auto& c = GetPosition(verts[i]);
                if (FindTriangleWindingOrder(a, b, c) == TriangleWindingOrder::CounterClockwise)
                    next = i;
            }
            current = next;
        }
        while (current != leftmost);

        return hull;
    }

    template<typename Vertex> inline
    std::vector<Vertex> FindConvexHull(const std::vector<Vertex>& verts)
    {
        if (verts.empty())
            return {};
        return FindConvexHull(&verts[0], verts.size());
    }

} // namespace
