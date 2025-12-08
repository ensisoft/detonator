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
#  include <QPoint>
#  include <QtMath>
#  include <glm/glm.hpp>
#  include <glm/gtx/matrix_decompose.hpp>
#include "warnpop.h"

#include <functional>

#include "base/assert.h"
#include "game/enum.h"
#include "graphics/transform.h"
#include "engine/camera.h"
#include "editor/gui/types.h"
#include "editor/gui/utility.h"

// this header was originally named math.h but that caused a weird
// conflict in Box2D. Can't be bothered to spend time solving such
// nonsense meta-issue (thanks C++!) so renaming this file with a
// very imaginative name!

namespace gui
{

template<typename UI, typename State>
void MakeViewTransform(const UI& ui, const State& state, gfx::Transform& view)
{
    view.Scale(GetValue(ui.scaleX), GetValue(ui.scaleY));
    view.Scale(GetValue(ui.zoom), GetValue(ui.zoom));
    view.RotateAroundZ(qDegreesToRadians(ui.rotation->value()));
    view.Translate(state.camera_offset_x, state.camera_offset_y);
}

template<typename UI, typename State>
void MakeViewTransform(const UI& ui, const State& state, gfx::Transform& view, float rotation)
{
    view.Scale(GetValue(ui.scaleX), GetValue(ui.scaleY));
    view.Scale(GetValue(ui.zoom), GetValue(ui.zoom));
    view.RotateAroundZ(qDegreesToRadians(rotation));
    view.Translate(state.camera_offset_x, state.camera_offset_y);
}


template<typename UI, typename State>
glm::mat4 CreateViewMatrix(const UI& ui, const State& state, engine::GameView view = engine::GameView::AxisAligned)
{
    const float zoom = GetValue(ui.zoom);
    const float xs = GetValue(ui.scaleX);
    const float ys = GetValue(ui.scaleY);
    const float rotation = GetValue(ui.rotation);

    return engine::CreateModelViewMatrix(view,
                                         state.camera_offset_x,
                                         state.camera_offset_y,
                                         zoom * xs, zoom * ys,
                                         rotation);
}

template<typename UI>
glm::mat4 CreateProjectionMatrix(const UI& ui, engine::Projection projection = engine::Projection::Orthographic)
{
    const auto width  = ui.widget->width();
    const auto height = ui.widget->height();
    return engine::CreateProjectionMatrix(projection, width, height);
}

template<typename UI, typename State>
Point2Df MapWindowCoordinateToWorld(const UI& ui,
                                   const State& state,
                                   const Point2Df& window_point,
                                   engine::GameView view = engine::GameView::AxisAligned,
                                   engine::Projection proj = engine::Projection::Orthographic)
{
    const Size2Df window_size = ui.widget->size();

    const auto& proj_matrix = CreateProjectionMatrix(ui, proj);
    const auto& view_matrix = CreateViewMatrix(ui, state, view);
    const auto pos = engine::MapFromWindowToWorldPlane(proj_matrix, view_matrix, window_point, window_size);
    return {pos.x, pos.y};
}

template<typename UI, typename State>
Point2Df MapWindowCoordinateToWorld(const UI& ui,
                                   const State& state,
                                   const Point2Df& window_point,
                                   game::SceneProjection projection)
{
    if (projection == game::SceneProjection::AxisAlignedOrthographic)
        return MapWindowCoordinateToWorld(ui, state, window_point,
            engine::GameView::AxisAligned, engine::Projection::Orthographic);
    else if (projection == game::SceneProjection::AxisAlignedPerspective)
        return MapWindowCoordinateToWorld(ui, state, window_point,
            engine::GameView::AxisAligned, engine::Projection::Perspective);
    else if (projection == game::SceneProjection::Dimetric)
        return MapWindowCoordinateToWorld(ui, state, window_point,
            engine::GameView::Dimetric, engine::Projection::Orthographic);
    else if (projection == game::SceneProjection::Isometric)
        return MapWindowCoordinateToWorld(ui, state, window_point,
            engine::GameView::Isometric, engine::Projection::Orthographic);

    BUG("Missing projection handling");
    return {};
}

template<typename UI, typename State>
Point2Df MapWorldCoordinateToWindow(const UI& ui,
                                    const State& state,
                                    const Point2Df& world_point,
                                    engine::GameView view = engine::GameView::AxisAligned,
                                    engine::Projection proj = engine::Projection::Orthographic)
{
    const Size2Df window_size = ui.widget->size();

    const auto& proj_matrix = CreateProjectionMatrix(ui, proj);
    const auto& view_matrix = CreateViewMatrix(ui, state, view);
    const auto pos = engine::MapFromWorldPlaneToWindow(proj_matrix, view_matrix, world_point, window_size);
    return {pos.x, pos.y};
}

template<typename UI, typename State>
Point2Df MapWorldCoordinateToWindow(const UI& ui,
                                    const State& state,
                                    const Point2Df& world_point,
                                    game::SceneProjection projection)
{
    if (projection == game::SceneProjection::AxisAlignedOrthographic)
        return MapWorldCoordinateToWindow(ui, state, world_point,
            engine::GameView::AxisAligned, engine::Projection::Orthographic);
    else if (projection == game::SceneProjection::AxisAlignedPerspective)
        return MapWorldCoordinateToWindow(ui, state, world_point,
            engine::GameView::AxisAligned, engine::Projection::Perspective);
    else if (projection == game::SceneProjection::Dimetric)
        return MapWorldCoordinateToWindow(ui, state, world_point,
            engine::GameView::Dimetric, engine::Projection::Orthographic);
    else if (projection == game::SceneProjection::Isometric)
        return MapWorldCoordinateToWindow(ui, state, world_point,
            engine::GameView::Isometric, engine::Projection::Orthographic);

    BUG("Missing projection handling");
    return {};
}

template<typename UI, typename State>
bool MouseZoom(const UI& ui, State& state, const std::function<void(void)>& zoom_function)
{
    const auto width  = ui.widget->width();
    const auto height = ui.widget->height();

    // where's the mouse in the widget
    const auto& mickey_widget_pos = ui.widget->mapFromGlobal(QCursor::pos());
    // can't use underMouse here because of the way the gfx widget
    // is constructed i.e QWindow and Widget as container
    if (mickey_widget_pos.x() < 0 || mickey_widget_pos.y() < 0 ||
        mickey_widget_pos.x() > width || mickey_widget_pos.y() > height)
        return false;

    glm::vec4 world_pos_before_zoom;
    glm::vec4 world_pos_after_zoom;
    const auto window_size = glm::vec2{width, height};
    const float rotation = 0.0f; // ignored so that the camera movement stays
    // irrespective of the camera rotation

    {

        const float zoom   = GetValue(ui.zoom);
        const float xs     = GetValue(ui.scaleX);
        const float ys     = GetValue(ui.scaleY);
        const auto view_to_clip  = engine::CreateProjectionMatrix(engine::Projection::Orthographic, width, height);
        const auto world_to_view = engine::CreateModelViewMatrix(engine::GameView::AxisAligned,
                                                     state.camera_offset_x,
                                                     state.camera_offset_y,
                                                     zoom * xs, zoom * ys,
                                                     rotation);
        world_pos_before_zoom = engine::MapFromWindowToWorld(view_to_clip, world_to_view, ToVec2(mickey_widget_pos), window_size);
    }

    zoom_function();

    {
        const float zoom   = GetValue(ui.zoom);
        const float xs     = GetValue(ui.scaleX);
        const float ys     = GetValue(ui.scaleY);
        const auto view_to_clip  = engine::CreateProjectionMatrix(engine::Projection::Orthographic, width, height);
        const auto world_to_view = engine::CreateModelViewMatrix(engine::GameView::AxisAligned,
                                                     state.camera_offset_x,
                                                     state.camera_offset_y,
                                                     zoom * xs, zoom * ys,
                                                     rotation);
        world_pos_after_zoom = engine::MapFromWindowToWorld(view_to_clip, world_to_view, ToVec2(mickey_widget_pos), window_size);
    }

    const auto world_delta = world_pos_after_zoom - world_pos_before_zoom;
    state.camera_offset_x -= world_delta.x;
    state.camera_offset_y -= world_delta.y;
    return true;
}

} // namespace
