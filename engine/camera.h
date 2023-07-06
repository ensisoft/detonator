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
