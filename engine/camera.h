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

    // We have 2 noteworthy and relevant planes/spaces in the game world
    // and actually you should consider them separate spaces. I.e the tile
    // space where vectors live on the tile plane and the scene space where
    // the vectors live on the scene plane. Conceptually these are different
    // coordinate spaces, even though when using axis aligned tile plane these
    // two spaces are identical.
    //
    // Tile plane (both dimetric or axis aligned)
    // - Tiles are laid out on the XY plane which is then transformed with a model
    //   and camera transformation to a certain position relative to
    //   the camera. Along with an orthographic projection the objects are projected
    //   onto the projection plane with dimetric angles between the basis vectors.
    // - When using axis aligned tile plane the plane aligns completely with the
    //   scene plane even though it's conceptually different plane.
    //
    // Scene plane
    // - Entities (and entity nodes) are laid out on this XY plane which is then
    //   transformed with a model and a camera transformation to a certain position
    //   relative to the camera.
    // - The scene plane is parallel to the orthographic projection plane.
    //
    // Currently every vector in either space is assumed to live exactly on the
    // plane, i.e. there's no 3rd dimension (Z values are 0.0f).
    // Even though there are instances when the coordinate spaces align it's wise
    // to keep the conceptual distinction in mind and use the functions to map
    // vectors from one space to another. This should be more future proof when/if
    // another perspective is added.
    //
    // Finally there's the concept of a "world plane" or "game plane" which is
    // used when we don't really care about which plane it is that we're dealing
    // with. This is mostly useful for the editor to transform for example mouse
    // and window coordinates to a coordinate on some plane for placing objects.

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

    glm::vec4 PlaneToPlane(const glm::vec4& pos, game::Perspective src, game::Perspective dst);

    inline glm::vec4 TilePlaneToScene(const glm::vec4& tile_pos, game::Perspective tile_plane) noexcept
    {
        return PlaneToPlane(tile_pos, tile_plane, game::Perspective::AxisAligned);
    }
    inline glm::vec4 SceneToTilePlane(const glm::vec4& scene_pos, game::Perspective tile_plane) noexcept
    {
        return PlaneToPlane(scene_pos, game::Perspective::AxisAligned, tile_plane);
    }

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

    glm::vec3 GetTileCuboidFactors(game::Perspective perspective);

} // namespace
