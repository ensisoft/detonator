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
#  if defined(MATH_SUPPORT_GLM)
#    include <glm/glm.hpp>
#    include <glm/gtx/matrix_decompose.hpp>
#    include <glm/gtx/euler_angles.hpp>
#    include <glm/gtx/fast_square_root.hpp>
#  endif
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
    constexpr auto Pi = 3.14159265358979323846;
    constexpr auto Circle = Pi*2.0;
    constexpr auto SemiCircle = Pi;

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

    template<typename Float> inline constexpr
    Float DegreesToRadians(Float degrees) noexcept
    {
        return degrees * (Pi / 180.0);
    }
    template<typename Float> inline constexpr
    Float RadiansToDegrees(Float radians) noexcept
    {
        return radians * (180.0 / Pi);
    }

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

    namespace easing {
        inline float ease_in_sine(float t) noexcept
        { return 1.0f - std::cos((t * Pi) / 2.0); }
        inline float ease_out_sine(float t) noexcept
        { return std::sin((t * Pi) / 2.0); }
        inline float ease_in_out_sine(float t) noexcept
        { return -(std::cos(Pi * t) - 1.0) / 2.0;}
        inline float ease_in_quadratic(float t) noexcept
        { return t*t; }
        inline float ease_out_quadratic(float t) noexcept
        { return 1.0f - (1.0f - t) * (1.0f - t);}
        inline float ease_in_out_quadratic(float t) noexcept
        {
            return t < 0.5f
                   ? 2.0f * t * t
                   : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
        }
        inline float ease_in_cubic(float t) noexcept
        { return t * t * t; }
        inline float ease_out_cubic(float t) noexcept
        { return 1.0f - std::pow(1.0f - t, 3.0f); }
        inline float ease_in_out_cubic(float t) noexcept
        {
            return t < 0.5f
                 ? 4.0f  * t * t * t
                 : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
        }
        inline float ease_in_back(float t) noexcept
        {
            const auto c1 = 1.70158f;
            const auto c3 = c1 + 1.0f;
            return c3 * t * t * t - c1 * t * t;
        }
        inline float ease_out_back(float t) noexcept
        {
            const auto c1 = 1.70158f;
            const auto c3 = c1 + 1.0f;
            return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
        }
        inline float ease_in_out_back(float t) noexcept
        {
            const auto c1 = 1.70158f;
            const auto c2 = c1 * 1.525f;

            return t < 0.5f
                   ? (std::pow(2.0f * t, 2.0f) * ((c2 + 1.0f) * 2.0f * t - c2)) / 2.0f
                   : (std::pow(2.0f * t - 2.0f, 2.0f) * ((c2 + 1.0f) * (t * 2.0f - 2.0f) + c2) + 2.0f) / 2.0f;
        }

        inline float ease_in_elastic(float t) noexcept
        {
            t = math::clamp(0.0f, 1.0f, t);
            if (t == 0.0f)
                return 0.0f;
            else if (t == 1.0f)
                return 1.0f;

            const auto c4 = (2.0f * Pi) / 3.0f;

            return  -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * c4);
        }

        inline float ease_out_elastic(float t) noexcept
        {
            t = math::clamp(0.0f, 1.0f, t);
            if (t == 0.0f)
                return 0.0f;
            else if (t == 1.0f)
                return 1.0f;

            const auto c4 = (2.0f * Pi) / 3.0f;

            return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
        }
        inline float ease_in_out_elastic(float t) noexcept
        {
            t = math::clamp(0.0f, 1.0f, t);
            if (t == 0.0f)
                return 0.0f;
            else if (t == 1.0f)
                return 1.0f;

            const auto c5 = (2.0f * Pi) / 4.5f;

            return  t < 0.5f
                    ? -(std::pow(2.0f,  20.0f * t - 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) / 2.0f
                    :  (std::pow(2.0f, -20.0f * t + 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) / 2.0f + 1.0f;
        }

        inline float ease_out_bounce(float t) noexcept
        {
            const auto n1 = 7.5625f;
            const auto d1 = 2.75f;
            if (t < 1.0f / d1)
                return n1 * t * t;
            else if (t < 2.0f / d1)
            {
                const auto x = t - 1.5f/d1;
                return n1 * x * x + 0.75f;
            }
            else if (t < 2.5f / d1)
            {
                const auto x = t - 2.25f/d1;
                return n1 * x * x + 0.9375f;
            }
            const auto x = t - 2.625f/d1;
            return n1 * x * x + 0.984375f;
        }
        inline float ease_in_bounce(float t) noexcept
        {
            return 1.0f - ease_out_bounce(1.0f - t);
        }
        inline float ease_in_out_bounce(float t) noexcept
        {
            return t < 0.5f
                   ? (1.0f - ease_out_bounce(1.0f - 2.0f * t)) / 2.0f
                   : (1.0f + ease_out_bounce(2.0f * t - 1.0f)) / 2.0f;
        }
    }

    namespace interp {
        inline float step_start(float t) noexcept
        { return t > 0.0f ? 1.0f : 0.0f; }
        inline float step_end(float t) noexcept
        { return t >= 1.0f ? 1.0f : 0.0f; }
        inline float step(float t) noexcept
        { return (t < 0.5f) ? 0.0f : 1.0f; }
        inline float cosine(float t) noexcept
        { return -std::cos(Pi * t) * 0.5f + 0.5f; }
        inline float smooth_step(float t) noexcept
        { return 3.0f*t*t - 2.0f*t*t*t; }
        inline float acceleration(float t) noexcept
        { return t*t; }
        inline float deceleration(float t) noexcept
        { return 1.0f - ((1.0f-t)*(1.0f-t)); }
    }

    // Interpolation defines the function used to determine  the intermediate
    // values between y0 and y1 are achieved when T varies between 0.0f and 1.0f.
    // I.e. when T is 0.0 interpolation returns y0 and when T is 1.0 interpolation
    // returns y1, For other values a mix of y0 and y1 is returned.
    // https://codeplea.com/simple-interpolation
    enum class Interpolation {
        // No interpolation, a discrete jump fro y0 to y1 then t > 0.0
        StepStart,
        // No interpolation, a discrete jump from y0 to y1 when t is >= 0.5.
        Step,
        // No interpolation, a discrete jump from y0 to y1 when t is >= 1.0
        StepEnd,
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
        Deceleration,

        // Easing curves, CSS inspired.
        // https://easings.net/
        EaseInSine,
        EaseOutSine,
        EaseInOutSine,
        EaseInQuadratic,
        EaseOutQuadratic,
        EaseInOutQuadratic,
        EaseInCubic,
        EaseOutCubic,
        EaseInOutCubic,
        EaseInBack,
        EaseOutBack,
        EaseInOutBack,
        EaseInElastic,
        EaseOutElastic,
        EaseInOutElastic,
        EaseInBounce,
        EaseOutBounce,
        EaseInOutBounce
    };

    inline float interpolate(float t, Interpolation method) noexcept
    {
        if (method == Interpolation::StepStart)
            return interp::step_start(t);
        else if (method == Interpolation::Step)
            return interp::step(t);
        else if (method == Interpolation::StepEnd)
            return interp::step_end(t);
        else if (method == Interpolation::Linear)
            return math::clamp(0.0f, 1.0f, t);
        else if (method == Interpolation::Cosine)
            return interp::cosine(t);
        else if (method == Interpolation::SmoothStep)
            return interp::smooth_step(t);
        else if (method == Interpolation::Acceleration)
            return interp::acceleration(t);
        else if (method == Interpolation::Deceleration)
            return interp::deceleration(t);
        else if (method == Interpolation::EaseInSine)
            return easing::ease_in_sine(t);
        else if (method == Interpolation::EaseOutSine)
            return easing::ease_out_sine(t);
        else if (method == Interpolation::EaseInOutSine)
            return easing::ease_in_out_sine(t);
        else if (method == Interpolation::EaseInQuadratic)
            return easing::ease_in_quadratic(t);
        else if (method == Interpolation::EaseOutQuadratic)
            return easing::ease_out_quadratic(t);
        else if (method == Interpolation::EaseInOutQuadratic)
            return easing::ease_in_out_quadratic(t);
        else if (method == Interpolation::EaseInCubic)
            return easing::ease_in_cubic(t);
        else if (method == Interpolation::EaseOutCubic)
            return easing::ease_out_cubic(t);
        else if (method == Interpolation::EaseInOutCubic)
            return easing::ease_in_out_cubic(t);
        else if (method == Interpolation::EaseInBack)
            return easing::ease_in_back(t);
        else if (method == Interpolation::EaseOutBack)
            return easing::ease_out_back(t);
        else if (method == Interpolation::EaseInOutBack)
            return easing::ease_in_out_back(t);
        else if (method == Interpolation::EaseInElastic)
            return easing::ease_in_elastic(t);
        else if (method == Interpolation::EaseOutElastic)
            return easing::ease_out_elastic(t);
        else if (method == Interpolation::EaseInOutElastic)
            return easing::ease_in_out_elastic(t);
        else if (method == Interpolation::EaseInBounce)
            return easing::ease_in_bounce(t);
        else if (method == Interpolation::EaseOutBounce)
            return easing::ease_out_bounce(t);
        else if (method == Interpolation::EaseInOutBounce)
            return easing::ease_in_out_bounce(t);
        else BUG("Missing interpolation method.");
        return t;
    }

    template<typename T>
    T interpolate(const T& y0, const T& y1, float t, Interpolation method) noexcept
    {
        return math::lerp(y0, y1, math::interpolate(t, method));
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

#if defined(MATH_SUPPORT_GLM)
    inline bool equals(const glm::vec2& lhs, const glm::vec2& rhs, float epsilon = 0.0001f) noexcept
    {
        return equals(lhs.x, rhs.x, epsilon) &&
               equals(lhs.y, rhs.y, epsilon);
    }
    inline bool equals(const glm::vec3& lhs, const glm::vec3& rhs, float epsilon = 0.0001f) noexcept
    {
        return equals(lhs.x, rhs.x, epsilon) &&
               equals(lhs.y, rhs.y, epsilon) &&
               equals(lhs.z, rhs.z, epsilon);
    }
    inline bool equals(const glm::vec4& lhs, const glm::vec4& rhs, float epsilon = 0.0001f) noexcept
    {
        return equals(lhs.x, rhs.x, epsilon) &&
               equals(lhs.y, rhs.y, epsilon) &&
               equals(lhs.z, rhs.z, epsilon) &&
               equals(lhs.w, rhs.w, epsilon);
    }

    // Rotate a vector on the xy plane around the Z axis.
    inline glm::vec2 RotateVectorAroundZ(const glm::vec2& vec, float angle) noexcept
    {
        return glm::eulerAngleZ(angle) * glm::vec4(vec.x, vec.y, 0.0f, 0.0f);
    }
    // transform a direction vector (such as a normal) safely even if the
    // transformation matrix contains a non-uniform scale.
    inline glm::vec4 TransformNormalVector(const glm::mat4& matrix, const glm::vec4& vector) noexcept
    {
        return glm::transpose(glm::inverse(matrix)) * vector;
    }
    // transform a direction vector (such as a normal) safely even if the
    // transformation matrix contains a non-uniform scale.
    inline glm::vec2 TransformNormalVector(const glm::mat4& matrix, const glm::vec2& vector) noexcept
    {
        return TransformNormalVector(matrix, glm::vec4(vector, 0.0f, 0.0f));
    }

    inline glm::vec4 TransformVector(const glm::mat4& matrix, const glm::vec4& vector) noexcept
    {
        return glm::normalize(matrix * glm::vec4(vector.x, vector.y, vector.z, 0.0f)); // disregard translation
    }
    inline glm::vec4 TransformVector(const glm::mat4& matrix, const glm::vec2& vector) noexcept
    {
        return glm::normalize(matrix * glm::vec4(vector.x, vector.y, 0.0f, 0.0f));
    }

    inline glm::vec4 TransformPoint(const glm::mat4& matrix, const glm::vec4& point) noexcept
    {
        return matrix * point;
    }

    inline glm::vec4 TransformPoint(const glm::mat4& matrix, const glm::vec2& point) noexcept
    {
        return matrix * glm::vec4(point.x, point.y, 0.0f, 1.0f);
    }

    // Find the angle that rotates the basis vector X such that
    // it's collinear with the parameter vector.
    // returns the angle in radians.
    inline float FindVectorRotationAroundZ(const glm::vec2& vec) noexcept
    {
        const auto cosine = glm::dot(glm::normalize(vec), glm::vec2(1.0f, 0.0f));
        if (vec.y > 0.0f)
            return std::acos(cosine);
        return -std::acos(cosine);
    }

    inline float GetRotationFromMatrix(const glm::mat4& mat) noexcept
    {
        glm::vec3 scale;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::quat orientation;
        glm::decompose(mat, scale, orientation, translation, skew, perspective);
        return glm::angle(orientation);
    }

    inline glm::vec2 GetScaleFromMatrix(const glm::mat4& mat) noexcept
    {
        glm::vec3 scale;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::quat orientation;
        glm::decompose(mat, scale, orientation, translation, skew, perspective);
        return scale;
    }

    inline glm::vec2 GetTranslationFromMatrix(const glm::mat4& mat) noexcept
    {
        glm::vec3 scale;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::quat orientation;
        glm::decompose(mat, scale, orientation, translation, skew, perspective);
        return translation;
    }

    inline glm::vec3 ComputeNormal(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) noexcept
    {
        // https://github.com/g-truc/glm/blob/master/manual.md#section1
        // code samples
        return glm::normalize(glm::cross(c - a, b - a));
    }
    inline glm::vec2 ComputeNormal(const glm::vec2& a, const glm::vec2& b) noexcept
    {
        // https://github.com/g-truc/glm/blob/master/manual.md#section1
        // code samples
        //return glm::normalize(glm::cross(glm::vec3(c, 0.0f) - glm::vec3(a, 0.0f),
        //                                 glm::vec3(b, 0.0f) - glm::vec3(a, 0.0f)));
        const auto direction = b - a;
        const auto angle = FindVectorRotationAroundZ(direction);
        const auto perpendicular_angle = angle - DegreesToRadians(90.0f);
        return RotateVectorAroundZ(glm::vec2(1.0f, 0.0f), perpendicular_angle);
    }

    inline glm::vec3 ComputeNormalFast(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) noexcept
    {
        return glm::fastNormalize(glm::cross(c - a, b - a));
    }
    inline glm::vec2 ComputeNormalFast(const glm::vec2& a, const glm::vec2& b) noexcept
    {
        return ComputeNormal(a, b);
    }

#endif

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
