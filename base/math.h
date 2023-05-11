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

#pragma once

#include "config.h"

#include "warnpush.h"
#include "warnpop.h"

#include <algorithm>
#include <type_traits>
#include <random>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <cstring>

#include "base/assert.h"

namespace math
{
    const auto Pi = 3.14159265358979323846;

    namespace detail {
        template <typename T> inline constexpr
        int signum(T x, std::false_type is_signed) noexcept {
            return T(0) < x;
        }
        template <typename T> inline constexpr
        int signum(T x, std::true_type is_signed) noexcept {
            return (T(0) < x) - (x < T(0));
        }
    } // namespace

    template <typename T> inline constexpr
    int signum(T x) noexcept {
        return detail::signum(x, std::is_signed<T>());
    }

    template<typename T> inline
    T wrap(T min, T max, T val) noexcept
    {
        if (val > max)
            return min;
        if (val < min)
            return max;
        return val;
    }

    template<typename T> inline
    T clamp(T min, T max, T val) noexcept
    {
        if (val < min)
            return min;
        if (val > max)
            return max;
        return val;
    }

    template<typename T> inline
    T lerp(const T& y0, const T& y1, float t) noexcept
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

    template<typename T>
    T interpolate(const T& y0, const T& y1, float t, Interpolation method) noexcept
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
    bool equals(RealT goal, RealT value, RealT epsilon = 0.0001) noexcept
    {
        // maybe the default epsilon needs to be reconsidered ?
        // FYI: powers of 2 can be presented exactly, i.e. 2.0, 4.0, 8.0
        // and also 1.0, 0.5, 0.25 etc.
        return std::abs(goal - value) <= epsilon;
    }

    // Generate pseudo random numbers based on the given seed.
    template<unsigned Seed, typename T>
    T rand(T min, T max) noexcept
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

    // generate a random number in the range of min max (inclusive)
    // the random number generator is automatically seeded.
    template<typename T>
    T rand(T min, T max) noexcept
    {
        // if we enable this flag we always give out a deterministic
        // sequence by initializing the engine with a predetermined seed.
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

    template<typename T, size_t Seed>
    struct RandomGenerator {
        inline T operator()() const noexcept {
            return math::rand<Seed, T>(min_, max_);
        }
        inline T operator()(T min, T max) const noexcept {
            return math::rand<Seed, T>(min, max);
        }
        inline static T rand(T min, T max) noexcept {
            return math::rand<Seed, T>(min, max);
        }
        T min_;
        T max_;
     };


    class NoiseGenerator
    {
    public:
        NoiseGenerator(float frequency,
              uint32_t prime1, uint32_t prime2, uint32_t prime3)
        {
            mPrime1 = prime1;
            mPrime2 = prime2;
            mPrime3 = prime3;
            mFrequency = frequency;
        }

        // 1 dimensional noise,
        // returns noise value in the range [0, 1.0f]
        float GetSample(float x) const
        {
            const float period = 1.0f / mFrequency;
            const float x0 = int(x / period) * period;
            const float x1 = x0 + period;
            const auto y0 = Random(x0);
            const auto y1 = Random(x1);
            const float t = (x - x0) / period;
            return interpolate(y0, y1, t, Interpolation::Cosine);
        }

        // 2 dimensional noise
        // returns a noise value in the range [0, 1.0f]
        float GetSample(float x, float y) const
        {
            const float period = 1.0f / mFrequency;
            const float x0 = int(x / period) * period;
            const float x1 = x0 + period;
            const float y0 = int(y / period) * period;
            const float y1 = y0 + period;
            const float samples[4] = {
                Random(x0, y0),
                Random(x1, y0),
                Random(x0, y1),
                Random(x1, y1)
            };
            const auto t = (x - x0) / period;
            const float xbot = interpolate(samples[0], samples[1], t,
                Interpolation::Cosine);
            const float xtop = interpolate(samples[2], samples[3], t,
                Interpolation::Cosine);
            return interpolate(xbot, xtop, (y-y0) / period, Interpolation::Cosine);
        }
    private:
        float Random(float x) const
        {
            uint32_t bits = 0;  // std::bit_cast in cpp20
            std::memcpy(&bits, &x, sizeof(x));
            const uint32_t mask = (~(uint32_t)0) >> 1;
            const uint32_t val  = (bits << 13) ^ bits;
            return ((val * (val * val * mPrime1 + mPrime2) + mPrime3) & mask) / (float)mask;
        }
        float Random(float x, float y) const
        {
            return Random(x + y * 57);
        }

    private:
        uint32_t mPrime1 = 0;
        uint32_t mPrime2 = 0;
        uint32_t mPrime3 = 0;
        float mFrequency = 0;
    };

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
    std::vector<Vertex> FindConvexHull(const Vertex* verts, size_t num_verts)
    {
        // this is the so-called "Jarvis March."

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

    // Check whether the given point is inside the given rectangle or not.
    static bool CheckRectPointIntersection(float minX, float maxX, float minY, float maxY, // rectangle
                                           float x, float y) // point
    {
        if (x < minX || x > maxX)
            return false;
        else if (y < minY || y > maxY)
            return false;
        return true;
    }


    static bool CheckRectCircleIntersection(float minX, float maxX, float minY, float maxY, // rectangle
                                            float x, float y, float radius)// circle

    {
        ASSERT(maxX >= minX);
        ASSERT(maxY >= minY);

        // find the point closest to the center of the circle inside the rectangle
        const auto pointX = clamp(minX, maxX, x);
        const auto pointY = clamp(minY, maxY, y);

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

    static bool CheckRectLineIntersection(float minX, float maxX, float minY, float maxY, // rectangle
                                          float x1, float y1, float x2, float y2) // line
    {
        ASSERT(maxX >= minX);
        ASSERT(maxY >= minY);
        //ASSERT(x2 >= x1);
        if (x2 < x1) {
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
        else if (line_minX > maxX) // right check
            return false;
        else if (line_maxY < minY) // above check
            return false;
        else if (line_minY > maxY) // below check
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
        if (CheckRectPointIntersection(minX, maxX, minY, maxY, x1, y1) ||
            CheckRectPointIntersection(minX, maxX, minY, maxY, x2, y2))
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
