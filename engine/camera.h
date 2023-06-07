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

#include "warnpush.h"
#  include <glm/mat4x4.hpp>
#  include <glm/gtc/matrix_transform.hpp>
#include "warnpop.h"

#include <optional>

#include "base/utility.h"
#include "game/enum.h"

namespace engine
{
    class Camera
    {
    public:
        using Perspective = game::Perspective;

        // Set camera position and view direction vector from a predefined
        // perspective setting. However keep in mind that setting the camera
        // perspective is not by itself enough to create the final rendering.
        // For example with dimetric rendering the projection matrix also needs
        // to be set to a orthographic projection.
        inline void SetFromPerspective(game::Perspective perspective) noexcept
        {
            if (perspective == Perspective::Dimetric)
            {
                // jump to a position for dimetric projection.
                // 45 degrees around the UP axis (yaw) and 30 degrees down (pitch)
                SetPosition({0.0f, 0.0f, 0.0f});
                SetYaw(-90.0f - 45.0f);
                SetPitch(-30.0f);
            }
            else if (perspective == Perspective::AxisAligned)
            {
                SetPosition({0.0f, 0.0f, 0.0f});
                SetDirection({0.0f, 0.0f, -1.0f});
            }
        }

        // Rotate the camera around the Y axis (vertical, yaw) and around the
        // X (horizontal, pitch, tilt) axis. The order of transformations is
        // first yaw then pitch.
        // Negative pitch = look down, positive pitch = look up
        // Negative yaw = look left, positive yaw = look right
        inline void SetRotation(float yaw, float pitch) noexcept
        {
            mYaw = yaw;
            mPitch = pitch;
        }
        // Set the current camera position in world coordinates. This is
        // the vantage point from which the camera looks to the specified
        // camera direction (forward) vector.
        inline void SetPosition(const glm::vec3& position) noexcept
        { mPosition = position; }
        // Set the direction the camera is looking at. Direction should
        // a normalized, i.e. unit length direction vector.
        inline void SetDirection(const glm::vec3& dir) noexcept
        {
            // atan is the tangent value of 2 arguments y,x, (range (-pi pi] ) so using z vector as the "y"
            // and x as the x means the angle from our z-axis towards the x axis (looking down on the Y axis)
            // which is the same as rotation around the up axis (aka yaw)
            mYaw   = glm::degrees(glm::atan(dir.z, dir.x));
            mPitch = glm::degrees(glm::asin(dir.y));
        }
        inline void LookAt(const glm::vec3& pos) noexcept
        {
            SetDirection(glm::normalize(pos - mPosition));
        }
        // Set the camera yaw (in degrees), i.e. rotation around the vertical axis.
        inline void SetYaw(float yaw) noexcept
        { mYaw = yaw; }
        // Set the camera pitch (aka tilt) (in degrees), i.e. rotation around the horizontal axis.
        inline void SetPitch(float pitch) noexcept
        { mPitch = pitch; }
        // Translate the camera by accumulating a change in position by some delta.
        inline void Translate(const glm::vec3& delta) noexcept
        { mPosition += delta; }
        // Translate the camera by accumulating a change in position by some delta values on each axis.
        inline void Translate(float dx, float dy, float dz) noexcept
        { mPosition += glm::vec3(dx, dy, dz); }
        inline void SetX(float x) noexcept
        { mPosition.x = x; }
        inline void SetY(float y) noexcept
        { mPosition.y = y; }
        inline void SetZ(float z) noexcept
        { mPosition.z = z; }
        // Change the camera yaw in degrees by some delta value.
        inline void Yaw(float d) noexcept
        { mYaw += d; }
        // Change the camera pitch in degrees by some delta value.
        inline void Pitch(float d) noexcept
        { mPitch += d; }

        inline float GetYaw() const noexcept
        { return mYaw; }
        // Get the the current camera pitch in degrees. Positive value indicates
        // that the camera is looking upwards (towards the sky) and negative value
        // indicates that the camera is looking downwards (towards the floor)
        inline float GetPitch() const noexcept
        { return mPitch; }
        //
        inline glm::vec3 GetViewVector() const noexcept
        { return mViewDir; }
        inline glm::vec3 GetRightVector() const noexcept
        { return mRight; }
        inline glm::vec3 GetPosition() const noexcept
        { return mPosition; }

        inline glm::mat4 GetViewMatrix() const noexcept
        {
            return glm::lookAt(mPosition, mPosition + mViewDir, glm::vec3{0.0f, 1.0f, 0.0f});
        }

        // Call this after adjusting any camera parameters in order to
        // recompute the view direction vector and the camera right vector.
        void Update() noexcept
        {
            constexpr const static glm::vec3 WorldUp = {0.0f, 1.0f, 0.0f};
            mViewDir.x = std::cos(glm::radians(mYaw)) * std::cos(glm::radians(mPitch));
            mViewDir.y = std::sin(glm::radians(mPitch));
            mViewDir.z = std::sin(glm::radians(mYaw)) * std::cos(glm::radians(mPitch));
            mViewDir = glm::normalize(mViewDir);
            mRight = glm::normalize(glm::cross(mViewDir, WorldUp));
        }

    private:
        // Camera's local (relative to it's base node) translation.
        glm::vec3 mPosition = {0.0f, 0.0f, 0.0f};
        // Camera's local right vector.
        glm::vec3 mRight    = {0.0f, 0.0f, 0.0f};
        // Camera's local view direction vector. Remember this
        // actually the inverse of the "objects" forward vector.
        glm::vec3 mViewDir  = {0.0f, 0.0f, 0.0f};
        // Camera rotation around the vertical axis.
        float mYaw   = 0.0f;
        // Aka tilt, camera rotation around the horizontal axis.
        float mPitch = 0.0f;
    };
} // namespace
