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

#include <type_traits>
#include <random>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <cstring>

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
