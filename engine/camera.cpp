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

#include "warnpush.h"
#  include <glm/gtx/intersect.hpp>
#include "warnpop.h"

#include "base/assert.h"
#include "engine/camera.h"

//
// TODO: Maybe get rid of that ugly rotation on dimetric projection in the view matrix creation
//       -> Instead rotating the world, work out the camera rotation around different axis for having
//          the same angle/perspective but on a different plane instead of the XY plane. OTOH, all
//          the current drawing functionality is on the XY plane so then there'd still have to be
//          some transformation for mapping the draws onto some other plane.
//
// DONE: See if the same orthographic perspective matrix could be used in both cases.

// TODO: Solve the depth problem with dimetric projection. Currently the way the camera moves
//       the fact that the camera and the plane are not perpendicular the plane will at some
//       point clip the near/far planes.
//       -> Find a transformation that would translate objects based on the current camera distance
//          to the plane to in order to keep a constant distance. This means that objects within
//          the current viewing volume would then fit inside the near/far planes
//       -> or maybe map the draw vertices from dimetric space to orthographic space and then draw
//          with orthographic (axis aligned) perspective. (Would require changes everywhere where a
//          a tile painter is used!)

// TODO: Solve the issue (is this still an issue?) regarding finding some position on the plane.
//       Similar problem to the view clipping, the relative camera/plane position (angle of the plane
//       relative to the camera) means that the plane will clip the camera's position at some point
//       and then the intersection point will be at a negative distance. Current fix here right now
//       is to use a custom version of the IntersectRayPlane.
//       Either find a different algorithm to do this or maybe fix the camera clipping issue
//       (regarding near/far clipping planes) by keeping the camera - plane distance constant.
//

namespace {
    // this is based on GLM. removed the check on positive vector distance
    float IntersectRayPlane(const glm::vec4& orig, const glm::vec4& dir,
                            const glm::vec4& planeOrig, const glm::vec4& planeNormal)
    {
        const float angle = glm::dot(dir, planeNormal);

        // check for collinear vectors
        ASSERT(glm::abs(angle) > std::numeric_limits<float>::epsilon());

        return glm::dot(planeOrig - orig, planeNormal) / angle;
    }

    glm::mat4 CreateDimetricModelTransform()
    {
        constexpr const static glm::vec3 WorldUp = {0.0f, 1.0f, 0.0f};
        constexpr const static float Yaw   = -90.0f - 45.0f;
        constexpr const static float Pitch = -30.0f;

        glm::vec3 position = {0.0f, 0.0f, 0.0f};

        glm::vec3 direction;
        direction.x = std::cos(glm::radians(Yaw)) * std::cos(glm::radians(Pitch));
        direction.y = std::sin(glm::radians(Pitch));
        direction.z = std::sin(glm::radians(Yaw)) * std::cos(glm::radians(Pitch));
        direction = glm::normalize(direction);

        return glm::lookAt(position, position + direction, WorldUp);
    }
} // namespace

namespace engine
{

glm::mat4 CreateProjectionMatrix(Projection projection, const game::FRect& viewport)
{
    ASSERT(projection == Projection::Orthographic);

    const auto width  = viewport.GetWidth();
    const auto height = viewport.GetHeight();
    const auto xpos   = viewport.GetX();
    const auto ypos   = viewport.GetY();

    // the incoming game viewport is defined with positive Y growing down.
    //
    // -x,-y
    //    ---------
    //    |       |
    //    |       |
    //    |       |
    //    ---------
    //            -x+w, -y+h
    //
    // we're flipping that here in order to have the axis the same
    // way for both orthographic and perspective projection, which
    // simplifies the camera movement.

    const auto left = xpos;
    const auto right = xpos + width;
    const auto top = ypos;
    const auto bottom = ypos + height;

    // glm::ortho, left, right, bottom, top, near, far
    return glm::ortho(left, right, -bottom, -top, -10000.0f, 10000.0f);
}

glm::mat4 CreateProjectionMatrix(Projection projection, const glm::vec2& surface_size)
{
    const auto xpos = surface_size.x / -2.0f;
    const auto ypos = surface_size.y / -2.0f;
    const auto width = surface_size.x;
    const auto height = surface_size.y;
    return CreateProjectionMatrix(projection, game::FRect(xpos, ypos, width, height));
}

glm::mat4 CreateProjectionMatrix(Projection projection, float surface_width, float surface_height)
{
    return CreateProjectionMatrix(projection, glm::vec2{surface_width, surface_height});
}

PerspectiveProjectionArgs ComputePerspectiveProjection(const game::FRect& viewport)
{
    const auto width  = viewport.GetWidth();
    const auto height = viewport.GetHeight();

    const auto Fov = 45.0f;
    const auto zFar = 10000.0f;

    PerspectiveProjectionArgs args;
    args.fov        = Fov;
    args.aspect     = width / height;
    args.far_plane  = zFar;
    args.near_plane = (height*0.5f) / std::tan(glm::radians(Fov*0.5f));
    return args;
}


glm::mat4 CreateProjectionMatrix(const game::FRect& viewport, const PerspectiveProjectionArgs& args)
{
    const auto& ortho = CreateProjectionMatrix(Projection::Orthographic, viewport);

    const auto zFar  = args.far_plane; //10000.0f;
    const auto zNear = args.near_plane; //1.0f;
    const auto Fov   = args.fov; //45.0f;

    // with perspective projection we want to map the drawable shape to the screen so
    // that the center of the shape aligns at the same screen coordinate in both
    // orthographic and perspective projections. The way we can achieve this (without
    // changing the objects X,Y position and only manipulating the the Z, i.e. the
    // distance from the camera) is by setting up the projection transformation so
    // that the near plane half height equals the half height of the orthographic
    // projection plane and then by translating the object to a depth value which maps
    // it to the near plane.
    //
    // tan(f)     = y / x
    // tan(f) * x = y
    //          x = y / tan(f)
    //
    // Remember that the FOV (Field of View) angle includes above and below the "horizon"
    // i.e. above and below y=0.0f.
    //
    // This all works except that placing objects at the near plane has a problem with
    // clipping since the front part of the object might get clipped.
    // So this fix this we transform the vertices from clipping space back into world
    // space and add another translate and then perform the clip space mapping again.
    // Using an orthographic transformation avoids issues with perspective transformation
    // since we just want to offset on the Z axis after all perspective transformation
    // has already been done!

    const auto width  = viewport.GetWidth();
    const auto height = viewport.GetHeight();

    //const auto aspect = width / height;
    //const auto near = (height*0.5f) / std::tan(glm::radians(Fov*0.5f));
    const auto aspect = args.aspect;
    const auto near = args.near_plane;

    return ortho *
             glm::translate(glm::mat4(1.0f), glm::vec3 { 0.0f, 0.0f, -10000} ) *
               glm::inverse(ortho) *
                 glm::perspective(glm::radians(Fov), aspect, near, zFar) *
                   glm::translate(glm::mat4(1.0f), glm::vec3 { 0.0f, 0.0f, -near });
}


glm::mat4 CreateModelMatrix(GameView view)
{
    if (view == GameView::AxisAligned)
    {
        const static auto model_rotation = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3{1.0f,  0.0f, 0.0f});
        return model_rotation;
    }
    else if (view == GameView::Dimetric)
    {
        const static auto dimetric_rotation = CreateDimetricModelTransform();
        const static auto plane_rotation = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3 { 1.0f, 0.0f, 0.0f });
        const static auto model_matrix = dimetric_rotation * plane_rotation;
        return model_matrix;
    }
    else BUG("Unknown perspective");
    return glm::mat4(1.0f);
}

glm::mat4 CreateViewMatrix(const glm::vec2& camera_pos,
                           const glm::vec2& camera_scale,
                           float camera_rotation)
{
    // remember the camera generates a "model to view" which is conceptually
    // the inverse of camera's "logical" transform.
    // I.e. if the camera is moving to the left it's the same as if the camera
    // stays still and the world moves to the right

    glm::mat4 model_to_view(1.0f);
    model_to_view = glm::scale(model_to_view, glm::vec3{camera_scale.x, camera_scale.y, 1.0f});

    // Y is here flipped because in the renderer we have Y going up and -Y down
    // but in the logical game world Y grows down.
    model_to_view = glm::translate(model_to_view, glm::vec3{-camera_pos.x, camera_pos.y, 0.0f});
    model_to_view = glm::rotate(model_to_view, glm::radians(-camera_rotation), glm::vec3{0.0f, 0.0f, 1.0f});
    return model_to_view;
}

glm::mat4 CreateModelViewMatrix(GameView game_view,
                                const glm::vec2& camera_pos,
                                const glm::vec2& camera_scale,
                                float camera_rotation)
{
    // remember that if you use operator *= as in a *= b; it's the same as
    // a = a * b;
    // this means that multiple statements such as
    // mat *= foo;
    // mat *= bar;
    // will be the same as mat = foo * bar. This means that when this matrix
    // is used to transform the vertices the bar operation will take place
    // first, then followed by foo.
    return CreateViewMatrix(camera_pos, camera_scale, camera_rotation) *
            CreateModelMatrix(game_view);
}

glm::mat4 CreateModelViewMatrix(GameView game_view,
                                float camera_pos_x,
                                float camera_pos_y,
                                float world_scale_x,
                                float world_scale_y,
                                float rotation)
{
    return CreateModelViewMatrix(game_view,
                                 glm::vec2 { camera_pos_x, camera_pos_y },
                                 glm::vec2 { world_scale_x, world_scale_y },
                                 rotation);
}

glm::vec2 MapFromWorldPlaneToWindow(const glm::mat4& view_to_clip,
                                    const glm::mat4& world_to_view,
                                    const glm::vec2& world_coord,
                                    const glm::vec2& window_size)
{
    const auto clip = view_to_clip * world_to_view * glm::vec4 {world_coord, 0.0f, 1.0f};

    const auto x = (clip.x + 1.0f) * window_size.x*0.5f;
    const auto y = window_size.y - (clip.y + 1.0f) * window_size.y*0.5f;
    return glm::vec2 { x, y, };
}

glm::vec4 MapFromWindowToWorldPlane(const glm::mat4& view_to_clip,
                                    const glm::mat4& world_to_view,
                                    const glm::vec2& window_coord,
                                    const glm::vec2& window_size)
{

    return MapFromWindowToWorldPlane(view_to_clip, world_to_view, window_size, std::vector<glm::vec2>{window_coord})[0];
}

std::vector<glm::vec4> MapFromWindowToWorldPlane(const glm::mat4& view_to_clip,
                                                 const glm::mat4& world_to_view,
                                                 const glm::vec2& window_size,
                                                 const std::vector<glm::vec2>& coordinates)
{
    constexpr const auto plane_origin_world = glm::vec4 {0.0f, 0.0f, 0.0f, 1.0f};
    constexpr const auto plane_normal_world = glm::vec4 {0.0f, 0.0f, 1.0f, 0.0f};
    const auto plane_origin_view = world_to_view * plane_origin_world;
    const auto plane_normal_view = glm::normalize(glm::transpose(glm::inverse(world_to_view)) * plane_normal_world);

    std::vector<glm::vec4> ret;

    for (const auto& window_coord : coordinates)
    {
        // normalize the window coordinate. remember to flip the Y axis.
        glm::vec2 norm;
        norm.x = window_coord.x / (window_size.x * 0.5) - 1.0f;
        norm.y = 1.0f - (window_coord.y / (window_size.y * 0.5));

        constexpr auto depth_value = -1.0f; // maps to view volume near plane
        // remember this is in the NDC, so on z axis -1.0 is less depth
        // i.e. closer to the viewer and 1.0 is more depth, farther away
        const auto ndc = glm::vec4{norm.x, norm.y, depth_value, 1.0f};
        // transform into clip space. remember that when the clip space
        // coordinate is transformed into NDC (by OpenGL) the clip space
        // vectors are divided by the w component to yield normalized
        // coordinates.
        constexpr auto w = 1.0f;
        const auto clip = ndc * w;

        //const auto& view_to_clip = projection;
        const auto& clip_to_view = glm::inverse(view_to_clip);

        // original window coordinate in view space on near plane.
        const auto& view_pos = clip_to_view * clip;

        const auto& ray_origin = view_pos;
        // do a ray cast from the view_pos towards the depth
        // i.e. the ray is collinear with the -z vector
        constexpr const auto ray_direction = glm::vec4{0.0f, 0.0f, -1.0f, 0.0f};

        // !! if the camera isn't perpendicular to the plane
        // at some point when the camera moves far enough the plane
        // will clip the camera's view position and at that point the
        // plane ray intersection point is actually *behind* the camera
        // i.e with negative distance.
        //
        // Todo: maybe this needs some better solution? For now we're
        // going to use a custom version of the ray plane intersection
        // with the relevant check disabled so the distance value can
        // be negative also.

        const auto intersection_distance = IntersectRayPlane(ray_origin,
                                                             ray_direction,
                                                             plane_origin_view,
                                                             plane_normal_view);

        const auto& view_to_world = glm::inverse(world_to_view);
        const auto& intersection_point_view = ray_origin + ray_direction * intersection_distance;
        const auto& intersection_point_world = view_to_world * intersection_point_view;
        ret.push_back(intersection_point_world);
    }
    return ret;
}

glm::vec4 MapFromScenePlaneToTilePlane(const glm::mat4& scene_view_to_clip,
                                       const glm::mat4& scene_world_to_view,
                                       const glm::mat4& plane_view_to_clip,
                                       const glm::mat4& plane_world_to_view,
                                       const glm::vec4& scene_pos)
{
    constexpr const auto plane_origin_world = glm::vec4 {0.0f, 0.0f, 0.0f, 1.0f};
    constexpr const auto plane_normal_world = glm::vec4 {0.0f, 0.0f, 1.0f, 0.0f};
    const auto plane_origin_view = plane_world_to_view * plane_origin_world;
    const auto plane_normal_view = glm::normalize(glm::transpose(glm::inverse(plane_world_to_view)) * plane_normal_world);

    // scene position transformed from scene coordinate space into plane coordinate
    // space relative to the camera
    auto scene_pos_in_plane_space_view = glm::inverse(plane_view_to_clip) * scene_view_to_clip * scene_world_to_view * scene_pos;
    scene_pos_in_plane_space_view.z = 100000.0f;

    // do a ray cast from the view_pos towards the depth
    // i.e. the ray is collinear with the -z vector
    constexpr const auto ray_direction = glm::vec4 {0.0f, 0.0f, -1.0f, 0.0f};
    const auto ray_origin = scene_pos_in_plane_space_view;

    const auto intersection_distance = IntersectRayPlane(ray_origin,
                                                         ray_direction,
                                                         plane_origin_view,
                                                         plane_normal_view);

    const auto intersection_point_view = ray_origin + ray_direction * intersection_distance;
    const auto intersection_point_world = glm::inverse(plane_world_to_view) * intersection_point_view;
    return intersection_point_world;
}

glm::vec4 MapFromTilePlaneToScenePlane(const glm::mat4& scene_view_to_clip,
                                       const glm::mat4& scene_world_to_view,
                                       const glm::mat4& plane_view_to_clip,
                                       const glm::mat4& plane_world_to_view,
                                       const glm::vec4& plane_pos)
{
    glm::vec2 clip_space = plane_view_to_clip * plane_world_to_view * plane_pos;

    // clip to near plane
    constexpr auto depth_value_at_near_plane = -1.0f;

    return glm::inverse(scene_view_to_clip * scene_world_to_view) * glm::vec4{clip_space, depth_value_at_near_plane, 1.0f};
}

glm::vec4 MapFromPlaneToPlane(const glm::vec4& pos, GameView src, GameView dst)
{
    if (src == dst)
        return pos;

    const auto& src_plane_to_world = CreateModelMatrix(src);
    const auto& dst_plane_to_world = CreateModelMatrix(dst);
    const auto& world_pos = src_plane_to_world * pos;

    const auto dst_plane_origin = dst_plane_to_world * glm::vec4{0.0f, 0.0f, 0.0f, 0.0f};
    const auto dst_plane_normal = dst_plane_to_world * glm::vec4{0.0f, 0.0f, 1.0f, 0.0f};

    const auto ray_origin = world_pos;
    // actually using the inverse of plane normal vector doesn't work the right way.
    // what we really need is a ray/vector that is collinear with the Z axis and maps
    // directly between the two planes. Using a random -z vector now which should
    // work in any case since the IntersectRayPlane can handle negative distances too!
    const auto ray_direction = glm::vec4{0.0f, 0.0f, -1.0f, 0.0f};  //-dst_plane_normal;

    const auto intersection_distance = IntersectRayPlane(ray_origin,
                                                         ray_direction,
                                                         dst_plane_origin,
                                                         dst_plane_normal);
    const auto intersection_point_world = ray_origin + ray_direction * intersection_distance;
    return glm::inverse(dst_plane_to_world) * intersection_point_world;
}

glm::vec4 MapFromTilePlaneToScenePlane(const glm::vec4& tile_pos, GameView tile_plane) noexcept
{
    return MapFromPlaneToPlane(tile_pos, tile_plane, GameView::AxisAligned);
}

glm::vec4 MapFromScenePlaneToTilePlane(const glm::vec4& scene_pos, GameView tile_plane) noexcept
{
    return MapFromPlaneToPlane(scene_pos, GameView::AxisAligned, tile_plane);
}

glm::vec4 MapFromWindowToWorld(const glm::mat4& view_to_clip,
                               const glm::mat4& world_to_view,
                               const glm::vec2& window_coord,
                               const glm::vec2& window_size)
{
// normalize the window coordinate. remember to flip the Y axis.
    glm::vec2 norm;
    norm.x = window_coord.x / (window_size.x*0.5) - 1.0f;
    norm.y = 1.0f - (window_coord.y / (window_size.y*0.5));

    constexpr auto depth_value = -1.0f; // maps to view volume near plane
    // remember this is in the NDC, so on z axis -1.0 is less depth
    // i.e. closer to the viewer and 1.0 is more depth, farther away
    const auto ndc = glm::vec4 {norm.x, norm.y, depth_value, 1.0f};
    // transform into clip space. remember that when the clip space
    // coordinate is transformed into NDC (by OpenGL) the clip space
    // vectors are divided by the w component to yield normalized
    // coordinates.
    constexpr auto w = 1.0f;
    const auto clip = ndc * w;

    return glm::inverse(view_to_clip * world_to_view) * clip;
}

glm::vec2 ComputeTileRenderSize(const glm::mat4& tile_to_render,
                                const glm::vec2& tile_size,
                                game::Tilemap::Perspective perspective)
{
    const auto tile_width_units  = tile_size.x;
    const auto tile_height_units = tile_size.y;

    const auto tile_left_bottom  = tile_to_render * glm::vec4{0.0f, tile_height_units, 0.0f, 1.0f};
    const auto tile_right_top    = tile_to_render * glm::vec4{tile_width_units, 0.0f, 0.0f, 1.0f};
    const auto tile_left_top     = tile_to_render * glm::vec4{0.0f, 0.0f, 0.0f, 1.0f};
    const auto tile_right_bottom = tile_to_render * glm::vec4{tile_width_units, tile_height_units, 0.0f, 1.0f};

    if (perspective == game::Tilemap::Perspective::Dimetric)
    {
        const auto tile_width_render_units  = glm::length(tile_left_bottom - tile_right_top);
        const auto tile_height_render_units = glm::length(tile_left_top - tile_right_bottom);
        return {tile_width_render_units, tile_height_render_units};
    }
    else if (perspective == game::Tilemap::Perspective::AxisAligned)
    {
        const auto tile_width_render_units  = glm::length(tile_left_top - tile_right_top);
        const auto tile_height_render_units = glm::length(tile_left_top - tile_left_bottom);
        return {tile_width_render_units, tile_height_render_units};
    } else BUG("Unknown perspective");
    return {0.0f, 0.0f};
}

glm::vec3 GetTileCuboidFactors(game::Tilemap::Perspective perspective)
{
    // When dealing with an isometric cube you'd be inclined to think that the isometric
    // sprite represents a cube, but this is actually not the case. Rather the tile
    // sprite represents a cuboid with vertical height not being the same as the base
    // width/height.
    //
    // So in order to map the tiles properly into a 3D elements in the conceptual tile
    // world (for example when adjusting the level height by doing a vertical offset)
    // we need to know the scaling co-efficients for each of the cuboid dimensions.
    //
    // Based on the isometric tile sprite you'd think that the ratios would follow
    // equally. In a sprite tile the square base of the tile has equal width and height.
    // These can be computed as:
    //
    //  base_width = base_height = base_size = sqrt(0.25*0.25 + 0.5*0.5) ~= 0.5590
    //
    // It is already known that the vertical height of the rendered tile is  0.5.
    //
    // so this would imply that the tile vertical height is
    //
    //   vertical_height  = 0.5 / base_size ~= 0.8944
    //
    // BUT... this does not track! Experimenting with this value (directly in 3D, so
    // no tile sprite rendering involved)  shows that the vertical height of the cuboid
    // is too high! Using this value to offset the tile sprites causes misalignment
    // where going vertically up one level does not align with the grid!
    //
    // So either this math above here is wrong and the ratios of sides cannot be
    // discovered from the projected geometry or something else is wrong somewhere
    // else!
    //
    // I spent some time trying to compute the ratios of the projected sides using
    // algebra but didn't manage to do so yet. :-(

    if (perspective == game::Tilemap::Perspective::Dimetric)
    {
        // so this doesn't work right...
        //const auto projected_tile_vertical_height = 0.5f;
        //const auto projected_tile_base_size = std::sqrt(0.25*0.25 + 0.5*0.5);
        //const auto scale = 1.0f / projected_tile_base_size;
        //return glm::vec3{1.0f, 1.0, projected_tile_vertical_height * scale };

        // empirically invented magic value!
        return glm::vec3{1.0f, 1.0f, 0.815f};
    }
    return glm::vec3{1.0f, 1.0f, 1.0f};
}

} // namespace
