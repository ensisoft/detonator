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
#include "game/tilemap.h"

namespace engine
{
    // Graphical projections can be classified as follows:
    //
    // - Perspective projection (aka linear projection)
    //    - normally the projection ray through the center of the projection plane
    //      is perpendicular to the plane but it doesn't have to be.
    //
    //  - Parallel projection.
    //    In parallel projection the projection rays are all parallel. I.e. they all point at
    //    at the same direction from the projection plane. The angle of each vector wrt the
    //    projection plane can be perpendicular (orthographic projection) or at some angle (oblique projection)
    //
    //    - Orthographic projection.
    //        In orthogonal all projection rays are parallel *and* perpendicular to the projection plane.
    //        Depending on the view angles this projection can further be split into:
    //        - "Plan" or "Elevation" projection. The view is directly aligned (projection rays are
    //          parallel to one axis) which shortens one axis away when projected. The two remaining projected
    //          axis are at 90 degree angle.
    //       - Axonometric projections. The view is at some angle that lets all 3 (x,y,z) axis be projected onto
    //         the projection plane at various projection angles (i.e. the angles of the axis after projection).
    //         - Isometric projection.
    //            Angles between all projected axis vectors is 120 degrees. 1.732:1 pixel ratio.
    //         - Dimetric projection
    //            Two angles between projected vectors are 105 degrees and the third angle is 150.
    //             This creates a pixel ratio of 2:1 width:height. Tiles will be 2 as wide as they're
    //             high and lines parallel with the x, y rays in the coordinate grid move up/down 1 pixels
    //             for every 1 pixel horizontally (when projected)
    //         - Trimetric projection
    //            todo

    //    - Oblique projection.
    //        In oblique projection all projection rays are parallel but non perpendicular. In other words
    //        the projection rays are at slanted/skewed wrt the projection plane.
    //        - Military
    //        - Cavalier
    //        - Topdown
    //

    // High level perspective (view point, vantage point) defines how
    // game objects are displayed when rendered.
    // Each perspective requires a combination of a specific view/camera
    // vantage point and the right graphical projection in order to produce
    // the desired rendering. For example the dimetric perspective (typically
    // referred to as 'isometric' in 2D games) is a camera angle of 45 deg
    // around UP axis and 30 deg down tilt combined with orthographic projection.
    struct GameView
    {
        // NOTE: no *class* on purpose, the outer struct is the class
        enum EnumValue {
            // Orthographic axis aligned projection infers a camera position that
            // is perpendicular to one of the coordinate space axis. This can be
            // used to produce "top down" or "side on" views.
            // When used in games this view can be used for example for side-scrollers,
            // top-down shooters, platform and puzzle games.
            AxisAligned,
            // Orthographic dimetric perspective infers a camera position that is
            // angled at a fixed yaw and tilt (pitch) to look in a certain direction.
            // This camera vantage point is then combined with an orthographic
            // projection to produce a 2D rendering where multiple sides of an
            // object are visible but without any perspective foreshortening.
            // This type of perspective is common in strategy and simulation games.
            // This is often (incorrectly) called "isometric" even though mathematically
            // isometric and dimetric are not the same projections.
            Dimetric,
            // todo: ObliqueTopDown
            //Oblique
        };
        GameView(EnumValue value)
          : value(value)
        {}
        GameView(game::Tilemap::Perspective perspective)
        {
            if (perspective == game::Tilemap::Perspective::AxisAligned)
                value = EnumValue::AxisAligned;
            else if (perspective == game::Tilemap::Perspective::Dimetric)
                value = EnumValue::Dimetric;
            else BUG("Unknown tilemap perspective.");
        }
        inline operator EnumValue () const noexcept
        { return value; }

        EnumValue value;
    };

    inline bool operator==(GameView lhs, GameView rhs)
    { return lhs.value == rhs.value; }
    inline bool operator==(GameView perspective, GameView::EnumValue value)
    { return perspective.value == value; }
    inline bool operator!=(GameView perspective, GameView::EnumValue value)
    { return perspective.value != value; }
    inline bool operator==(GameView::EnumValue value, GameView perspective)
    { return perspective.value == value; }
    inline bool operator!=(GameView::EnumValue value, GameView perspective)
    { return perspective.value != value; }

    enum class Projection {
        Orthographic,
        Perspective
    };

    glm::mat4 CreateProjectionMatrix(Projection projection, const game::FRect& viewport);
    glm::mat4 CreateProjectionMatrix(Projection projection, const glm::vec2& surface_size);
    glm::mat4 CreateProjectionMatrix(Projection projection, float surface_width, float surface_height);

    // Create model transformation matrix for a certain type of game perspective.
    // This matrix adds a perspective specific rotation to the model transformation.
    glm::mat4 CreateModelMatrix(GameView view);

    glm::mat4 CreateViewMatrix(const glm::vec2& camera_pos,
                               const glm::vec2& camera_scale,
                               float camera_rotation = 0.0f); // rotation around the Z axis in degrees

    // Create view transformation matrix for a certain type of game perspective
    // assuming a world translation and world scale. In other words this matrix
    // transforms the world space objects into "view/eye/camera" space.
    glm::mat4 CreateModelViewMatrix(GameView view,
                                    const glm::vec2& camera_pos,
                                    const glm::vec2& camera_scale,
                                    float camera_rotation = 0.0f); // rotation around the Z axis in degrees

    glm::mat4 CreateModelViewMatrix(GameView view,
                                    float camera_pos_x,
                                    float camera_pos_y,
                                    float world_scale_x,
                                    float world_scale_y,
                                    float rotation = 0.0f);

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
    // - This 2d plane is the is axis aligned game plane of the underlying 3D space,
    //   in other words the game world takes place on this plane.
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
    // In a tilemap editor the mouse is mapped to the tile plane which is either
    // the dimetric plane or the axis aligned plane and in the scene / entity
    // editors the plane is the xy plane.

    glm::vec2 MapFromWorldPlaneToWindow(const glm::mat4& view_to_clip,
                                        const glm::mat4& world_to_view,
                                        const glm::vec2& world_coord,
                                        const glm::vec2& window_size);

    // Map a window (2D projection surface coordinate) to game plane
    glm::vec4 MapFromWindowToWorldPlane(const glm::mat4& view_to_clip, // aka projection matrix/transform
                                        const glm::mat4& world_to_view, // aka view/camera matrix/transform
                                        const glm::vec2& window_coord,
                                        const glm::vec2& window_size);
    std::vector<glm::vec4> MapFromWindowToWorldPlane(const glm::mat4& view_to_clip,
                                                     const glm::mat4& world_to_view,
                                                     const glm::vec2& window_size,
                                                     const std::vector<glm::vec2>& coordinates);
    // Map a window (2D projection surface coordinate) to world space.
    glm::vec4 MapFromWindowToWorld(const glm::mat4& view_to_clip,
                                   const glm::mat4& world_to_view,
                                   const glm::vec2& window_coord,
                                   const glm::vec2& window_size);

    // Map a vector from scene plane to a vector on the tile plane.
    glm::vec4 MapFromScenePlaneToTilePlane(const glm::mat4& scene_view_to_clip,
                                           const glm::mat4& scene_world_to_view,
                                           const glm::mat4& plane_view_to_clip,
                                           const glm::mat4& plane_world_to_view,
                                           const glm::vec4& scene_pos);

    // Map a vector from the tile plane coordinate space to scene plane.
    glm::vec4 MapFromTilePlaneToScenePlane(const glm::mat4& scene_view_to_clip,
                                           const glm::mat4& scene_world_to_view,
                                           const glm::mat4& plane_view_to_clip,
                                           const glm::mat4& plane_world_to_view,
                                           const glm::vec4& plane_pos);

    glm::vec4 MapFromPlaneToPlane(const glm::vec4& pos, GameView src, GameView dst);

    inline glm::vec4 MapFromTilePlaneToScenePlane(const glm::vec4& tile_pos, GameView tile_plane) noexcept
    {
        return MapFromPlaneToPlane(tile_pos, tile_plane, GameView::AxisAligned);
    }
    inline glm::vec4 MapFromScenePlaneToTilePlane(const glm::vec4& scene_pos, GameView tile_plane) noexcept
    {
        return MapFromPlaneToPlane(scene_pos, GameView::AxisAligned, tile_plane);
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
                                    game::Tilemap::Perspective perspective);

    glm::vec3 GetTileCuboidFactors(game::Tilemap::Perspective perspective);

} // namespace
