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
#include "warnpop.h"

#include <tuple>

#include "base/types.h"

namespace base
{
    class Rotator
    {
    public:
        explicit Rotator(const glm::quat& quaternion) noexcept
          : mQuaternion(quaternion)
        {}
        Rotator(float w, float x, float y, float z) noexcept
          : mQuaternion(w, x, y, z)
        {}

        template<typename Unit>
        Rotator(FAngle<Unit> x,
                FAngle<Unit> y,
                FAngle<Unit> z) noexcept
           : mQuaternion(FromEulerXYZ(x, y, z))
        {}

        Rotator() = default;

        inline glm::quat GetAsQuaternion() const noexcept
        { return mQuaternion; }
        inline glm::mat4 GetAsMatrix() const noexcept
        { return glm::mat4_cast(mQuaternion); }
        explicit inline operator glm::quat() const noexcept
        { return mQuaternion; };


        inline float GetX() const noexcept
        { return mQuaternion.x; }
        inline float GetY() const noexcept
        { return mQuaternion.y; }
        inline float GetZ() const noexcept
        { return mQuaternion.z; }
        inline float GetW() const noexcept
        { return mQuaternion.w; }

        template<typename Unit = base::detail::Radians> inline
        auto GetEulerAngleX() const noexcept
        {
            return ToAngle<Unit>(ToEuler().x);
        }

        template<typename Unit = base::detail::Radians> inline
        auto GetEulerAngleY() const noexcept
        {
            return ToAngle<Unit>(ToEuler().y);
        }

        template<typename Unit = base::detail::Radians> inline
        auto GetEulerAngleZ() const noexcept
        {
            return ToAngle<Unit>(ToEuler().z);
        }

        template<typename Unit = base::detail::Radians> inline
        std::tuple<FAngle<Unit>,
                   FAngle<Unit>,
                   FAngle<Unit>>
        GetEulerAngles() const noexcept
        {
            const auto& vec = ToEuler();
            return {
                FAngle<Unit> {base::detail::ConvertAngle(vec.x, base::detail::Radians{}, Unit{}) },
                FAngle<Unit> {base::detail::ConvertAngle(vec.y, base::detail::Radians{}, Unit{}) },
                FAngle<Unit> {base::detail::ConvertAngle(vec.z, base::detail::Radians{}, Unit{}) }
            };
        }

        template<typename X, typename Y, typename Z>
        static Rotator FromEulerXYZ(FAngle<X> x, FAngle<Y> y,FAngle<Z> z) noexcept
        {
            return Rotator { glm::quat {
                              glm::vec3 {
                                x.ToRadians(),
                                y.ToRadians(),
                                z.ToRadians()
                              }}};
        }
        template<typename Unit>
        static Rotator FromEulerXYZ(FAngle<Unit> x, FAngle<Unit> y,FAngle<Unit> z) noexcept
        {
            return Rotator { glm::quat {
                              glm::vec3 {
                                x.ToRadians(),
                                y.ToRadians(),
                                z.ToRadians()
                           }}};
        }


    private:
        template<typename Unit = base::detail::Radians> inline
        FAngle<Unit> ToAngle(float value) const noexcept
        {
            return FAngle<Unit>(base::detail::ConvertAngle(value, base::detail::Radians{}, Unit{}));
        }

        inline glm::vec3 ToEuler() const noexcept
        { return glm::eulerAngles(mQuaternion); }
    private:
        glm::quat mQuaternion = {1.0f, 0.0f, 0.0f, 0.0f};     // zero rotation
    };

} // namespace
