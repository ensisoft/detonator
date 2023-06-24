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

#include "graphics/transform.h"
#include "engine/camera.h"
#include "editor/gui/types.h"

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
glm::mat4 CreatePerspectiveCorrectViewMatrix(const UI& ui, const State& state, game::Perspective perspective)
{
    const float zoom = GetValue(ui.zoom);
    const float xs = GetValue(ui.scaleX);
    const float ys = GetValue(ui.scaleY);
    const float rotation = GetValue(ui.rotation);

    return engine::CreateViewMatrix(state.camera_offset_x,
                                    state.camera_offset_y,
                                    zoom*xs, zoom*ys,
                                    perspective,
                                    rotation);
}

template<typename UI>
glm::mat4 CreatePerspectiveCorrectProjMatrix(const UI& ui, game::Perspective perspective)
{
    const auto width  = ui.widget->width();
    const auto height = ui.widget->height();
    return engine::CreateProjectionMatrix(perspective, width, height);
}

template<typename UI, typename State>
Point2Df MapWindowCoordinateToWorld(const UI& ui,
                                   const State& state,
                                   const Point2Df& window_point,
                                   const Size2Df& window_size,
                                   game::Perspective perspective)
{
    const auto& proj_matrix = CreatePerspectiveCorrectProjMatrix(ui, perspective);
    const auto& view_matrix = CreatePerspectiveCorrectViewMatrix(ui, state, perspective);
    const auto pos = engine::MapToWorldPlane(proj_matrix, view_matrix, window_point, window_size);
    return pos;
}


} // namespace
