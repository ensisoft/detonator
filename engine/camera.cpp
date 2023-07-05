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
// TODO: See if the same orthographic perspective matrix could be used in both cases.

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
}

namespace engine
{

    // glm::ortho, left, right, bottom, top, near, far

glm::mat4 CreateProjectionMatrix(game::Perspective perspective, const glm::vec2& surface_size)
{
    const auto half_width  = surface_size.x*0.5f;
    const auto half_height = surface_size.y*0.5f;
    if (perspective == game::Perspective::AxisAligned)
        return glm::ortho(-half_width, half_width, half_height, -half_height, -1.0f, 1.0f);
    else if (perspective == game::Perspective::Dimetric)
        return glm::ortho(-half_width, half_width, -half_height, half_height, -10000.0f, 10000.0f);
    else BUG("Unknown perspective");
    return glm::mat4(1.0f);
}

glm::mat4 CreateProjectionMatrix(game::Perspective perspective, const game::FRect& viewport)
{
    // the incoming game viewport is defined assuming the axis-aligned (Y points downwards)
    // projection matrix.
    const auto width  = viewport.GetWidth();
    const auto height = viewport.GetHeight();
    const auto xpos   = viewport.GetX();
    const auto ypos   = viewport.GetY();
    const auto left   = xpos;
    const auto right  = xpos + width;
    const auto top    = ypos;
    const auto bottom = ypos + height;

    if (perspective == game::Perspective::AxisAligned)
        return glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
    else if (perspective == game::Perspective::Dimetric)
        return glm::ortho(left, right, -bottom, -top, -10000.0f, 10000.0f);
    else BUG("Unknown perspective");
    return glm::mat4(1.0f);
}
glm::mat4 CreateProjectionMatrix(game::Perspective perspective, float surface_width, float surface_height)
{
    return CreateProjectionMatrix(perspective, glm::vec2{surface_width, surface_height});
}

glm::mat4 CreateViewMatrix(game::Perspective perspective)
{
    if (perspective == game::Perspective::AxisAligned)
    {
        static Camera cam(perspective);
        return cam.GetViewMatrix();
    }
    else if (perspective == game::Perspective::Dimetric)
    {
        static Camera cam(perspective);
        return cam.GetViewMatrix();
    }
    else BUG("Unknown perspective");
    return glm::mat4(1.0f);
}

glm::mat4 CreateViewMatrix(game::Perspective perspective,
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

    glm::mat4 mat(1.0f);
    if (perspective == game::Perspective::Dimetric)
    {
        mat = glm::scale(mat, glm::vec3{camera_scale.x, camera_scale.y, 1.0f});
        mat = glm::translate(mat, glm::vec3{-camera_pos.x, camera_pos.y, 0.0f});
        mat = glm::rotate(mat, glm::radians(-camera_rotation), glm::vec3{0.0f, 0.0f, 1.0f});
    }
    else if (perspective == game::Perspective::AxisAligned)
    {
        mat = glm::scale(mat, glm::vec3{camera_scale.x, camera_scale.y, 1.0f});
        mat = glm::translate(mat, glm::vec3{-camera_pos.x, -camera_pos.y, 0.0f});
        mat = glm::rotate(mat, glm::radians(camera_rotation), glm::vec3{0.0f, 0.0f, 1.0f});
    }

    mat *= CreateViewMatrix(perspective);

    if (perspective == game::Perspective::Dimetric)
    {
        mat = glm::rotate(mat, glm::radians(90.0f), glm::vec3{1.0f, 0.0f, 0.0f});
    }
    else if (perspective == game::Perspective::AxisAligned)
    {

    }
    return mat;
}

glm::vec4 WindowToWorldPlane(const glm::mat4& view_to_clip,
                             const glm::mat4& world_to_view,
                             const glm::vec2& window_coord,
                             const glm::vec2& window_size)
{

    return WindowToWorldPlane(view_to_clip, world_to_view, window_size, std::vector<glm::vec2>{window_coord})[0];
}

std::vector<glm::vec4> WindowToWorldPlane(const glm::mat4& view_to_clip,
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

glm::vec4 SceneToWorldPlane(const glm::mat4& scene_view_to_clip,
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

glm::vec4 WorldPlaneToScene(const glm::mat4& scene_view_to_clip,
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


glm::vec4 WindowToWorld(const glm::mat4& view_to_clip,
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
                                game::Perspective perspective)
{
    const auto tile_width_units  = tile_size.x;
    const auto tile_height_units = tile_size.y;

    const auto tile_left_bottom  = tile_to_render * glm::vec4{0.0f, tile_height_units, 0.0f, 1.0f};
    const auto tile_right_top    = tile_to_render * glm::vec4{tile_width_units, 0.0f, 0.0f, 1.0f};
    const auto tile_left_top     = tile_to_render * glm::vec4{0.0f, 0.0f, 0.0f, 1.0f};
    const auto tile_right_bottom = tile_to_render * glm::vec4{tile_width_units, tile_height_units, 0.0f, 1.0f};

    if (perspective == game::Perspective::Dimetric)
    {
        const auto tile_width_render_units  = glm::length(tile_left_bottom - tile_right_top);
        const auto tile_height_render_units = glm::length(tile_left_top - tile_right_bottom);
        return {tile_width_render_units, tile_height_render_units};
    }
    else if (perspective == game::Perspective::AxisAligned)
    {
        const auto tile_width_render_units  = glm::length(tile_left_top - tile_right_top);
        const auto tile_height_render_units = glm::length(tile_left_top - tile_left_bottom);
        return {tile_width_render_units, tile_height_render_units};
    } else BUG("Unknown perspective");
    return {0.0f, 0.0f};
}

} // namespace
