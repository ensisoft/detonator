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

    // epsilon based check for float equality
    template<typename RealT> inline
    bool equals(RealT goal, RealT value, RealT epsilon = 0.0001) noexcept
    {
        // maybe the default epsilon needs to be reconsidered ?
        // FYI: powers of 2 can be presented exactly, i.e. 2.0, 4.0, 8.0
        // and also 1.0, 0.5, 0.25 etc.
        return std::abs(goal - value) <= epsilon;
    }

    template<typename PointType>
    float Distance(const PointType& a, const PointType& b) noexcept
    {
        const auto delta = a - b;
        const auto x = delta.x();
        const auto y = delta.y();
        return std::sqrt(x*x + y*y);
    }

    template<typename PointType>
    float SquareDistance(const PointType& a, const PointType& b) noexcept
    {
        const auto delta = a - b;
        const auto x = delta.x();
        const auto y = delta.y();
        return x*x + y*y;
    }

    template<typename PointType>
    bool DistanceIsLess(const PointType& target, const PointType& current, const float maximum) noexcept
    {
        const auto& diff = target - current;
        const auto x = diff.x();
        const auto y= diff.y();
        return x*x + y*y < maximum*maximum;
    }

    template<typename PointType>
    bool DistanceIsLessOrEqual(const PointType& target, const PointType& current, const float maximum) noexcept
    {
        const auto& diff = target - current;
        const auto x = diff.x();
        const auto y = diff.y();
        return x*x + y*y <= maximum*maximum;
    }

    template<typename PointType>
    bool DistanceIsMore(const PointType& target, const PointType& current, const float minimum) noexcept
    {
        const auto& diff = target - current;
        const auto x = diff.x();
        const auto y = diff.y();
        return x*x + y*y > minimum*minimum;
    }
    template<typename PointType>
    bool DistanceIsMoreOrEqual(const PointType& target, const PointType& current, const float minimum) noexcept
    {
        const auto& diff = target - current;
        const auto x = diff.x();
        const auto y = diff.y();
        return x*x + y*y >= minimum*minimum;
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

    inline float Distance(const glm::vec2& target, const glm::vec2& current) noexcept
    {
        return glm::length(target - current);
    }

    inline float SquareDistance(const glm::vec2& target, const glm::vec2& current) noexcept
    {
        const auto delta = target - current;
        return delta.x*delta.x + delta.y*delta.y;
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