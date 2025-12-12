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
#  include <glm/gtc/matrix_transform.hpp>
#  include <glm/gtc/quaternion.hpp>
#  include <glm/gtx/euler_angles.hpp>
#  include <glm/gtx/quaternion.hpp>
#include "warnpop.h"

#include <tuple>

#include "base/types.h"

namespace base
{
    class Rotator
    {
    public:
        template<typename Unit>
        Rotator(FAngle<Unit> x,
                FAngle<Unit> y,
                FAngle<Unit> z) noexcept
           : mX(x)
           , mY(y)
           , mZ(z)
        {}
        // radians
        Rotator(float x, float y, float z) noexcept
           : mX(x)
           , mY(y)
           , mZ(z)
        {}
        explicit Rotator(const glm::vec3& angles) noexcept
           : mX(angles.x)
           , mY(angles.y)
           , mZ(angles.z)
        {}

        explicit Rotator(const glm::quat& quaternion) noexcept
        {
            auto euler = glm::eulerAngles(quaternion);
            // normalize to [-180.0f, 180.0f]
            for (int i=0; i<3; ++i)
            {
                if (euler[i] < 0.0)
                    euler[i] += (2.0 * math::Pi);

                if (euler[i] > math::Pi)
                    euler[i] = -math::Pi + (euler[i] - math::Pi);
            }
            mX = FRadians(euler.x);
            mY = FRadians(euler.y);
            mZ = FRadians(euler.z);
        }

        Rotator() = default;

        inline glm::vec3 ToVector() const noexcept
        {
            return glm::vec3 { mX.ToRadians(), mY.ToRadians(), mZ.ToRadians() };
        }

        glm::vec3 ToDirectionVector() const noexcept
        {
            const auto& quat = GetAsQuaternion();
            return glm::normalize(quat * glm::vec3(1.0f, 0.0f, 0.0f));
        }

        inline glm::quat GetAsQuaternion() const noexcept
        {
            return glm::quat {
                glm::vec3 { mX.ToRadians(),
                            mY.ToRadians(),
                            mZ.ToRadians() }
            };
        }
        inline glm::mat4 GetAsMatrix() const noexcept
        {
            return glm::eulerAngleZYX(mZ.ToRadians(), mY.ToRadians(), mX.ToRadians());
        }
        explicit inline operator glm::quat() const noexcept
        {
            return GetAsQuaternion();
        };

        template<typename Unit = base::detail::Radians> inline
        auto GetEulerAngleX() const noexcept
        {
            return ToAngle<Unit>(mX.ToRadians());
        }

        template<typename Unit = base::detail::Radians> inline
        auto GetEulerAngleY() const noexcept
        {
            return ToAngle<Unit>(mY.ToRadians());
        }

        template<typename Unit = base::detail::Radians> inline
        auto GetEulerAngleZ() const noexcept
        {
            return ToAngle<Unit>(mZ.ToRadians());
        }

        template<typename Unit = base::detail::Radians> inline
        std::tuple<FAngle<Unit>,
                   FAngle<Unit>,
                   FAngle<Unit>>
        GetEulerAngles() const noexcept
        {
            const auto x = mX.ToRadians();
            const auto y = mY.ToRadians();
            const auto z = mZ.ToRadians();
            return {
                FAngle<Unit> {base::detail::ConvertAngle(x, base::detail::Radians{}, Unit{}) },
                FAngle<Unit> {base::detail::ConvertAngle(y, base::detail::Radians{}, Unit{}) },
                FAngle<Unit> {base::detail::ConvertAngle(z, base::detail::Radians{}, Unit{}) }
            };
        }

        template<typename X, typename Y, typename Z>
        static Rotator FromEulerXYZ(FAngle<X> x, FAngle<Y> y,FAngle<Z> z) noexcept
        {
            return Rotator(x, y, z);
        }
        template<typename Unit>
        static Rotator FromEulerXYZ(FAngle<Unit> x, FAngle<Unit> y,FAngle<Unit> z) noexcept
        {
            return Rotator(x, y, z);
        }

        static Rotator FromDirection(const glm::vec3& direction)
        {
            if (glm::length(direction) < 0.0005)
                return Rotator();

            constexpr auto canonical_vector = glm::vec3(1.0f, 0.0f, 0.0f);
            const auto& normal_direction = glm::normalize(direction);
            const auto& rotation_quaternion = glm::rotation(canonical_vector, normal_direction);
            return Rotator(rotation_quaternion);
        }

    private:
        template<typename Unit = base::detail::Radians> inline
        FAngle<Unit> ToAngle(float value) const noexcept
        {
            return FAngle<Unit>(base::detail::ConvertAngle(value, base::detail::Radians{}, Unit{}));
        }
    private:
        FRadians mX;
        FRadians mY;
        FRadians mZ;
    };

    template<typename Unit> inline
    glm::quat QuaternionFromEulerXYZ(FAngle<Unit> x, FAngle<Unit> y, FAngle<Unit> z) noexcept
    {
        return glm::quat {
                glm::vec3 { x.ToRadians(),
                            y.ToRadians(),
                            z.ToRadians() }
        };
    }

    inline glm::quat QuaternionFromEulerZYZ(const Rotator& rotator) noexcept
    {
        return QuaternionFromEulerXYZ(rotator.GetEulerAngleX(),
                                      rotator.GetEulerAngleY(),
                                      rotator.GetEulerAngleZ());
    }

    inline glm::quat slerp(const glm::quat& q0, const glm::quat& q1, float t) noexcept
    {
        return glm::mix(q0, q1, t);
    }

} // namespace
