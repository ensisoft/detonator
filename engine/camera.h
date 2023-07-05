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

#include <vector>

#include "game/enum.h"
#include "game/types.h"

namespace engine
{
    class Camera
    {
    public:
        using Perspective = game::Perspective;

        Camera() = default;
        explicit Camera(game::Perspective perspective)
        {
            SetFromPerspective(perspective);
            Update();
        }

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

    glm::mat4 CreateProjectionMatrix(game::Perspective perspective, const glm::vec2& surface_size);
    glm::mat4 CreateProjectionMatrix(game::Perspective perspective, const game::FRect& viewport);
    glm::mat4 CreateProjectionMatrix(game::Perspective perspective, float surface_width, float surface_height);

    // Create model transformation matrix for a certain type of game perspective.
    // This matrix adds a perspective specific rotation to the model transformation.
    glm::mat4 CreateModelMatrix(game::Perspective perspective);

    // Create view transformation matrix for a certain type of game perspective
    // assuming a world translation and world scale. In other words this matrix
    // transforms the world space objects into "view/eye/camera" space.
    glm::mat4 CreateModelViewMatrix(game::Perspective perspective,
                                    const glm::vec2& camera_pos,
                                    const glm::vec2& camera_scale,
                                    float camera_rotation = 0.0f); // rotation around the Z axis in degrees

    inline glm::mat4 CreateModelViewMatrix(game::Perspective perspective,
                                           float camera_pos_x,
                                           float camera_pos_y,
                                           float world_scale_x,
                                           float world_scale_y,
                                           float rotation = 0.0f)
    {
        return CreateModelViewMatrix(perspective,
                                     glm::vec2{camera_pos_x, camera_pos_y},
                                     glm::vec2{world_scale_x, world_scale_y},
                                     rotation);
    }

    // We have 2 noteworthy and relevant planes in the game world space,
    // and actually you could consider them separate spaces. I.e the tile
    // space where vectors live on the tile plane and the scene space where
    // the vectors live in the scene plane.
    //
    // Tile plane is a on the XY coordinate plane at an dimetric projection
    // angle to the camera. I.e. the camera's forward (look at) vector  is not
    // perpendicular to the plane. The tile plane is also known as the "map plane"
    // or just "map" or tilemap.
    //
    // Scene plane is the same XY plane but that is directly ahead of the camera.
    // In other words the XY plane is parallel to the projection plane and is
    // perpendicular to the camera's forward (look at) vector.
    //
    // Currently every vector in either space is assumed to live exactly on the
    // plane, i.e. there's no 3rd dimension (Z values are 0.0f).

    // Conceptually when we just want to map a coordinate to either game plane
    // we just call it the "world plane". This is useful when doing something
    // such as mapping a mouse coordinate inside the rendering window to a
    // position on the plane.

    // Map a window (2D projection surface coordinate) to game plane
    glm::vec4 WindowToWorldPlane(const glm::mat4& view_to_clip, // aka projection matrix/transform
                                 const glm::mat4& world_to_view, // aka view/camera matrix/transform
                                 const glm::vec2& window_coord,
                                 const glm::vec2& window_size);
    std::vector<glm::vec4> WindowToWorldPlane(const glm::mat4& view_to_clip,
                                              const glm::mat4& world_to_view,
                                              const glm::vec2& window_size,
                                              const std::vector<glm::vec2>& coordinates);
    // Map a window (2D projection surface coordinate) to world space.
    glm::vec4 WindowToWorld(const glm::mat4& view_to_clip,
                            const glm::mat4& world_to_view,
                            const glm::vec2& window_coord,
                            const glm::vec2& window_size);

    // Map a vector from scene plane to a vector on the tile plane.
    glm::vec4 SceneToTilePlane(const glm::mat4& scene_view_to_clip,
                               const glm::mat4& scene_world_to_view,
                               const glm::mat4& plane_view_to_clip,
                               const glm::mat4& plane_world_to_view,
                               const glm::vec4& scene_pos);

    // Map a vector from the tile plane coordinate space to scene plane.
    glm::vec4 TilePlaneToScene(const glm::mat4& scene_view_to_clip,
                               const glm::mat4& scene_world_to_view,
                               const glm::mat4& plane_view_to_clip,
                               const glm::mat4& plane_world_to_view,
                               const glm::vec4& plane_pos);


    // Produce a matrix that transforms a vertex from one coordinate space
    // to another. But remember that this transforms *any* coordinate inside
    // a 3D space from one space to another and not all such coordinates are
    // on the *planes* of interest. I.e. 3rd axis value need to be considered.
    inline glm::mat4 GetProjectionTransformMatrix(const glm::mat4& src_view_to_clip,
                                                  const glm::mat4& src_world_to_view,
                                                  const glm::mat4& dst_view_to_clip,
                                                  const glm::mat4& dst_world_to_view)
    {
        return glm::inverse(dst_view_to_clip * dst_world_to_view) * src_view_to_clip * src_world_to_view;
    }

    // Project a point in one world coordinate space one space to another space.
    // For example let's assume that we have some world coordinate in "isometric"
    // space and wish to know where this point maps in the 2D axis aligned space.
    // The solution can be found by applying these transformations.
    inline glm::vec4 ProjectPoint(const glm::mat4& src_view_to_clip, // aka projection matrix
                                  const glm::mat4& src_world_to_view, // aka view/camera matrix
                                  const glm::mat4& dst_view_to_clip, // aka projection matrix
                                  const glm::mat4& dst_world_to_view, // aka view/camera matrix
                                  const glm::vec3& src_world_point)
    {
        return GetProjectionTransformMatrix(src_view_to_clip, src_world_to_view,
                                            dst_view_to_clip, dst_world_to_view) * glm::vec4{src_world_point, 1.0f};
    }

    glm::vec2 ComputeTileRenderSize(const glm::mat4& tile_to_render,
                                    const glm::vec2& tile_size,
                                    game::Perspective perspective);




} // namespace
