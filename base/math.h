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

#include "base/warnpush.h"

namespace math
{
    constexpr auto Pi = 3.14159265358979323846;
    constexpr auto Circle = Pi*2.0;
    constexpr auto SemiCircle = Pi;

    namespace detail {
        template <typename T> constexpr
        int signum(T x, std::false_type /* is_signed */) noexcept {
            return T(0) < x;
        }
        template <typename T> constexpr
        int signum(T x, std::true_type /* is_signed */) noexcept {
            return (T(0) < x) - (x < T(0));
        }
    } // namespace

    inline float RunningAvg(const float current_average, const unsigned count, const float next_value) noexcept
    {
        return current_average + (next_value - current_average) / static_cast<float>(count);
    }
    inline double RunningAvg(const double current_average, const unsigned count, const double next_value) noexcept
    {
        return current_average + (next_value - current_average) / static_cast<double>(count);
    }

    template<typename Float> constexpr
    Float DegreesToRadians(Float degrees) noexcept
    {
        return degrees * (Pi / 180.0);
    }
    template<typename Float> constexpr
    Float RadiansToDegrees(Float radians) noexcept
    {
        return radians * (180.0 / Pi);
    }

    template <typename T> constexpr
    int signum(T x) noexcept {
        return detail::signum(x, std::is_signed<T>());
    }

    template<typename T>
    T wrap(T min, T max, T val) noexcept
    {
        if (val > max)
            return min;
        if (val < min)
            return max;
        return val;
    }

    template<typename T>
    T clamp(T min, T max, T val) noexcept
    {
        if (val < min)
            return min;
        if (val > max)
            return max;
        return val;
    }

    template<typename T>
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
            t = clamp(0.0f, 1.0f, t);
            if (t == 0.0f)
                return 0.0f;
            if (t == 1.0f)
                return 1.0f;

            const auto c4 = (2.0f * Pi) / 3.0f;

            return  -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * c4);
        }

        inline float ease_out_elastic(float t) noexcept
        {
            t = clamp(0.0f, 1.0f, t);
            if (t == 0.0f)
                return 0.0f;
            if (t == 1.0f)
                return 1.0f;

            const auto c4 = (2.0f * Pi) / 3.0f;

            return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
        }
        inline float ease_in_out_elastic(float t) noexcept
        {
            t = clamp(0.0f, 1.0f, t);
            if (t == 0.0f)
                return 0.0f;
            if (t == 1.0f)
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
            if (t < 2.0f / d1)
            {
                const auto x = t - 1.5f/d1;
                return n1 * x * x + 0.75f;
            }
            if (t < 2.5f / d1)
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

    inline float interpolate(const float t, const Interpolation method) noexcept
    {
        if (method == Interpolation::StepStart)
            return interp::step_start(t);
        if (method == Interpolation::Step)
            return interp::step(t);
        if (method == Interpolation::StepEnd)
            return interp::step_end(t);
        if (method == Interpolation::Linear)
            return clamp(0.0f, 1.0f, t);
        if (method == Interpolation::Cosine)
            return interp::cosine(t);
        if (method == Interpolation::SmoothStep)
            return interp::smooth_step(t);
        if (method == Interpolation::Acceleration)
            return interp::acceleration(t);
        if (method == Interpolation::Deceleration)
            return interp::deceleration(t);
        if (method == Interpolation::EaseInSine)
            return easing::ease_in_sine(t);
        if (method == Interpolation::EaseOutSine)
            return easing::ease_out_sine(t);
        if (method == Interpolation::EaseInOutSine)
            return easing::ease_in_out_sine(t);
        if (method == Interpolation::EaseInQuadratic)
            return easing::ease_in_quadratic(t);
        if (method == Interpolation::EaseOutQuadratic)
            return easing::ease_out_quadratic(t);
        if (method == Interpolation::EaseInOutQuadratic)
            return easing::ease_in_out_quadratic(t);
        if (method == Interpolation::EaseInCubic)
            return easing::ease_in_cubic(t);
        if (method == Interpolation::EaseOutCubic)
            return easing::ease_out_cubic(t);
        if (method == Interpolation::EaseInOutCubic)
            return easing::ease_in_out_cubic(t);
        if (method == Interpolation::EaseInBack)
            return easing::ease_in_back(t);
        if (method == Interpolation::EaseOutBack)
            return easing::ease_out_back(t);
        if (method == Interpolation::EaseInOutBack)
            return easing::ease_in_out_back(t);
        if (method == Interpolation::EaseInElastic)
            return easing::ease_in_elastic(t);
        if (method == Interpolation::EaseOutElastic)
            return easing::ease_out_elastic(t);
        if (method == Interpolation::EaseInOutElastic)
            return easing::ease_in_out_elastic(t);
        if (method == Interpolation::EaseInBounce)
            return easing::ease_in_bounce(t);
        if (method == Interpolation::EaseOutBounce)
            return easing::ease_out_bounce(t);
        if (method == Interpolation::EaseInOutBounce)
            return easing::ease_in_out_bounce(t);

        BUG("Missing interpolation method.");
        return t;
    }

    template<typename T>
    T interpolate(const T& y0, const T& y1, const float t, const Interpolation method) noexcept
    {
        return math::lerp(y0, y1, interpolate(t, method));
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
    inline glm::vec2 RunningAvg(const glm::vec2& current_value, const unsigned count, const glm::vec2& next_value) noexcept
    {
        glm::vec2 ret;
        ret.x = RunningAvg(current_value.x, count, next_value.x);
        ret.y = RunningAvg(current_value.y, count, next_value.y);
        return ret;
    }

    inline glm::vec3 RunningAvg(const glm::vec3& current_value, const unsigned count, const glm::vec3& next_value) noexcept
    {
        glm::vec3 ret;
        ret.x = RunningAvg(current_value.x, count, next_value.x);
        ret.y = RunningAvg(current_value.y, count, next_value.y);
        ret.z = RunningAvg(current_value.z, count, next_value.z);
        return ret;
    }

    inline glm::vec4 RunningAvg(const glm::vec4& current_value, const unsigned count, const glm::vec4& next_value) noexcept
    {
        glm::vec4 ret;
        ret.x = RunningAvg(current_value.x, count, next_value.x);
        ret.y = RunningAvg(current_value.y, count, next_value.y);
        ret.z = RunningAvg(current_value.z, count, next_value.z);
        ret.w = RunningAvg(current_value.w, count, next_value.w);
        return ret;
    }

    inline bool DistanceIsLess(const glm::vec2& target, const glm::vec2& current, const float maximum) noexcept
    {
        const auto& diff = target - current;
        return diff.x*diff.x + diff.y*diff.y < maximum*maximum;
    }
    inline bool DistanceIsLessOrEqual(const glm::vec2& target, const glm::vec2& current, const float maximum) noexcept
    {
        const auto& diff = target - current;
        return diff.x*diff.x + diff.y*diff.y <= maximum*maximum;
    }
    inline bool DistanceIsMore(const glm::vec2& target, const glm::vec2& current, const float minimum) noexcept
    {
        const auto& diff = target - current;
        return diff.x*diff.x + diff.y*diff.y > minimum*minimum;
    }
    inline bool DistanceIsMoreOrEqual(const glm::vec2& target, const glm::vec2& current, const float minimum) noexcept
    {
        const auto& diff = target - current;
        return diff.x*diff.x + diff.y*diff.y >= minimum*minimum;
    }

    inline bool equals(const glm::vec2& lhs, const glm::vec2& rhs, const float epsilon = 0.0001f) noexcept
    {
        return equals(lhs.x, rhs.x, epsilon) &&
               equals(lhs.y, rhs.y, epsilon);
    }
    inline bool equals(const glm::vec3& lhs, const glm::vec3& rhs, const float epsilon = 0.0001f) noexcept
    {
        return equals(lhs.x, rhs.x, epsilon) &&
               equals(lhs.y, rhs.y, epsilon) &&
               equals(lhs.z, rhs.z, epsilon);
    }
    inline bool equals(const glm::vec4& lhs, const glm::vec4& rhs, const float epsilon = 0.0001f) noexcept
    {
        return equals(lhs.x, rhs.x, epsilon) &&
               equals(lhs.y, rhs.y, epsilon) &&
               equals(lhs.z, rhs.z, epsilon) &&
               equals(lhs.w, rhs.w, epsilon);
    }

    // Rotate a vector on the xy plane around the Z axis.
    inline glm::vec2 RotateVectorAroundZ(const glm::vec2& vec, const float angle) noexcept
    {
        return glm::eulerAngleZ(angle) * glm::vec4(vec.x, vec.y, 0.0f, 0.0f);
    }

    // transform a vector (such as a normal, or any direction vector) safely even if the
    // transformation matrix contains a non-uniform scale.
    inline glm::vec3 TransformDirection(const glm::mat4& matrix, const glm::vec3& vector) noexcept
    {
        const auto& normal_transform = glm::transpose(glm::inverse(glm::mat3(matrix)));
        return glm::normalize(normal_transform * vector);
    }
    inline glm::vec4 TransformDirection(const glm::mat4& matrix, const glm::vec4& vector) noexcept
    {
        const auto& ret = TransformDirection(matrix, glm::vec3{vector.x, vector.y, vector.z});
        return {ret.x, ret.y, ret.z, 0.0f};
    }
    inline glm::vec2 TransformDirection(const glm::mat4& matrix, const glm::vec2& vector) noexcept
    {
        return TransformDirection(matrix, glm::vec3(vector, 0.0f));
    }

    inline glm::vec4 TransformPosition(const glm::mat4& matrix, const glm::vec4& point) noexcept
    {
        return matrix * point;
    }
    inline glm::vec3 TransformPosition(const glm::mat4& matrix, const glm::vec3& point) noexcept
    {
        return matrix * glm::vec4(point.x, point.y, point.z, 1.0);
    }
    inline glm::vec4 TransformPosition(const glm::mat4& matrix, const glm::vec2& point) noexcept
    {
        return matrix * glm::vec4(point.x, point.y, 0.0f, 1.0f);
    }

    // Find the angle that rotates the basis vector X such that
    // it's collinear with the parameter vector.
    // returns the angle in radians.
    inline float FindVectorRotationAroundZ(const glm::vec2& vec) noexcept
    {
        return std::atan2f(vec.y, vec.x);
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

    // Compute a vector that is perpendicular to the given vector counter clock-wise.
    inline glm::vec2 ComputePerpendicularVector(const glm::vec2& vector) noexcept
    {
        return { -vector.y, vector.x };
    }

    // Compute a vector that is perpendicular to the given vector clock-wise
    inline glm::vec2 ComputePerpendicularVectorCW(const glm::vec2& vector) noexcept
    {
        return { vector.y, -vector.x };
    }

#endif

} // namespace

#include "base/warnpop.h"