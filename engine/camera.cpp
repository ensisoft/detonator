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

glm::mat4 CreateViewMatrix(const glm::vec2& camera_pos,
                           const glm::vec2& world_scale,
                           game::Perspective perspective,
                           float rotation)
{
    // hack because of the orthographic projection matrix axis setup.
    rotation = (perspective == game::Perspective::Dimetric) ? -rotation : rotation;

    // this is the last thing we do in the series of transformations combined
    // in the transformation matrix. used only in the editor now but could also
    // be used by the game
    glm::mat4 mat = glm::rotate(glm::mat4(1.0f), glm::radians(rotation), glm::vec3{0.0f, 0.0f, 1.0f});

    mat *= CreateViewMatrix(perspective);

    if (perspective == game::Perspective::Dimetric)
    {
        mat = glm::scale(mat, glm::vec3{world_scale.x, 1.0f, world_scale.y});
        mat = glm::translate(mat, glm::vec3{-camera_pos.x, 0.0f, -camera_pos.y});
        mat = glm::rotate(mat, glm::radians(90.0f), glm::vec3{1.0f, 0.0f, 0.0f});
    }
    else if (perspective == game::Perspective::AxisAligned)
    {
        mat = glm::scale(mat, glm::vec3{world_scale.x, world_scale.y, 1.0f});
        mat = glm::translate(mat, glm::vec3{-camera_pos.x, -camera_pos.y, 0.0f});
    }
    return mat;
}

glm::vec2 MapToWorldPlane(const glm::mat4& view_to_clip,
                          const glm::mat4& world_to_view,
                          const glm::vec2& window_coord,
                          const glm::vec2& window_size)
{
    constexpr const auto plane_origin_world = glm::vec4 {0.0f, 0.0f, 0.0f, 1.0f};
    constexpr const auto plane_normal_world = glm::vec4 {0.0f, 0.0f, 1.0f, 0.0f};
    const auto plane_origin_view = world_to_view * plane_origin_world;
    const auto plane_normal_view = glm::normalize(glm::transpose(glm::inverse(world_to_view)) * plane_normal_world);

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

    //const auto& view_to_clip = projection;
    const auto& clip_to_view = glm::inverse(view_to_clip);

    // original window coordinate in view space on near plane.
    const auto& view_pos = clip_to_view * clip;

    const auto& ray_origin = view_pos;
    // do a ray cast from the view_pos towards the depth
    // i.e. the ray is collinear with the -z vector
    constexpr const auto ray_direction = glm::vec4 {0.0f, 0.0f, -1.0f, 0.0f};

    float intersection_distance = 0.0f;
    glm::intersectRayPlane(ray_origin,
                           ray_direction,
                           plane_origin_view,
                           plane_normal_view,
                           intersection_distance);

    const auto& view_to_world = glm::inverse(world_to_view);

    const auto& intersection_point_view  = ray_origin + ray_direction * intersection_distance;
    const auto& intersection_point_world = view_to_world * intersection_point_view;
    return {intersection_point_world.x, intersection_point_world.y};
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