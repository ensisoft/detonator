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

#include "config.h"

#include "warnpush.h"
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <algorithm>

#include "editor/gui/drawing.h"
#include "graphics/painter.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "graphics/transform.h"
#include "graphics/drawing.h"
#include "game/types.h"
#include "base/math.h"

namespace gui
{
void DrawLine(const gfx::Transform& view,
              const glm::vec2& src, const glm::vec2& dst,
              gfx::Painter& painter)
{
    const auto& mat = view.GetAsMatrix();
    const auto& s = mat * glm::vec4(src, 1.0f, 1.0f);
    const auto& d = mat * glm::vec4(dst, 1.0f, 1.0f);
    gfx::DrawLine(painter, gfx::FPoint(s.x, s.y), gfx::FPoint(d.x, d.y), gfx::Color::DarkYellow, 2.0f);
}

void DrawBasisVectors(gfx::Painter& painter, gfx::Transform& view)
{
    // draw the X vector
    view.Push();
        view.Scale(100.0f, 5.0f);
        view.Translate(0.0f, -2.5f);
        painter.Draw(gfx::Arrow(), view, gfx::CreateMaterialFromColor(gfx::Color::Green));
    view.Pop();

    // draw the Y vector
    view.Push();
        view.Scale(100.0f, 5.0f);
        view.Translate(-50.0f, -2.5f);
        view.Rotate(math::Pi * 0.5f);
        view.Translate(0.0f, 50.0f);
        painter.Draw(gfx::Arrow(), view, gfx::CreateMaterialFromColor(gfx::Color::Red));
    view.Pop();

    view.Push();
        view.Scale(2.5f, 2.5f);
        view.Translate(-1.25f, -1.25f);
        painter.Draw(gfx::RoundRectangle(), view, gfx::CreateMaterialFromColor(gfx::Color::Yellow));
    view.Pop();
}

void DrawBasisVectors(gfx::Transform& view, std::vector<engine::DrawPacket>& packets, int layer)
{
    //static auto green = gfx::Create
    static auto green = std::make_shared<gfx::MaterialClassInst>(
            gfx::CreateMaterialClassFromColor(gfx::Color::Green));
    static auto red   = std::make_shared<gfx::MaterialClassInst>(
            gfx::CreateMaterialClassFromColor(gfx::Color::Red));
    static auto arrow = std::make_shared<gfx::Arrow>();

    // draw the X vector
    view.Push();
        view.Scale(100.0f, 5.0f);
        view.Translate(0.0f, -2.5f);
        engine::DrawPacket x;
        x.transform = view.GetAsMatrix();
        x.material  = green;
        x.drawable  = arrow;
        x.layer     = layer;
        packets.push_back(std::move(x));
    view.Pop();

    // draw the Y vector
    view.Push();
        view.Scale(100.0f, 5.0f);
        view.Translate(-50.0f, -2.5f);
        view.Rotate(math::Pi * 0.5f);
        view.Translate(0.0f, 50.0f);
        engine::DrawPacket y;
        y.transform = view.GetAsMatrix();
        y.material  = red;
        y.drawable  = arrow;
        y.layer     = layer;
        packets.push_back(std::move(y));
    view.Pop();

    view.Push();
        view.Scale(2.5f, 2.5f);
        view.Translate(-1.25f, -1.25f);
        //painter.Draw(gfx::RoundRectangle(), view, gfx::CreateMaterialFromColor(gfx::Color::Yellow));
    view.Pop();
}

void DrawCoordinateGrid(gfx::Painter& painter, gfx::Transform& view,
    GridDensity grid,
    float zoom,
    float xs, float ys,
    unsigned width, unsigned height)
{
    view.Push();

    const int grid_size = std::max(width / xs, height / ys) / zoom;
    // work out the scale factor for the grid. we want some convenient scale so that
    // each grid cell maps to some convenient number of units (a multiple of 10)
    const auto cell_size_units  = static_cast<int>(grid);
    const auto num_grid_lines = (grid_size / cell_size_units) - 1;
    const auto num_cells = num_grid_lines + 1;
    const auto cell_size_normalized = 1.0f / (num_grid_lines + 1);
    const auto cell_scale_factor = cell_size_units / cell_size_normalized;

    // figure out what is the current coordinate of the center of the window/viewport in
    // view transformation's coordinate space. (In other words figure out which combination
    // of view basis axis puts me in the middle of the window in window space.)
    auto world_to_model = glm::inverse(view.GetAsMatrix());
    auto world_origin_in_model = world_to_model * glm::vec4(width / 2.0f, height / 2.0, 1.0f, 1.0f);

    view.Scale(cell_scale_factor, cell_scale_factor);

    // to make the grid cover the whole viewport we can easily do it by rendering
    // the grid in each quadrant of the coordinate space aligned around the center
    // point of the viewport. Then it doesn't matter if the view transformation
    // includes rotation or not.
    const auto grid_origin_x = (int)world_origin_in_model.x / cell_size_units * cell_size_units;
    const auto grid_origin_y = (int)world_origin_in_model.y / cell_size_units * cell_size_units;
    const auto grid_width  = cell_size_units * num_cells;
    const auto grid_height = cell_size_units * num_cells;

    auto drawable = gfx::Grid(num_grid_lines, num_grid_lines);
    auto material = gfx::CreateMaterialFromColor(gfx::Color4f(gfx::Color::LightGray)); //, 0.7f));
    //material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);

    view.Translate(grid_origin_x, grid_origin_y);
    painter.Draw(drawable, view, material);

    view.Translate(-grid_width, 0);
    painter.Draw(drawable, view, material);

    view.Translate(0, -grid_height);
    painter.Draw(drawable, view, material);

    view.Translate(grid_width, 0);
    painter.Draw(drawable, view, material);

    view.Pop();
}

void DrawViewport(gfx::Painter& painter, gfx::Transform& view,
                         float game_viewport_width,
                         float game_viewport_height,
                         unsigned widget_width,
                         unsigned widget_height)
{
    game::FBox viewport(game_viewport_width, game_viewport_height);
    viewport.Transform(view.GetAsMatrix());
    // this now the width and height of the game's viewport in the window.
    const auto game_viewport_width_in_window  = viewport.GetWidth();
    const auto game_viewport_height_in_window = viewport.GetHeight();
    const auto game_viewport_x_in_window = (widget_width - game_viewport_width_in_window) / 2;
    const auto game_viewport_y_in_window = (widget_height - game_viewport_height_in_window) / 2;

    const gfx::FRect rect(game_viewport_x_in_window, game_viewport_y_in_window,
                    game_viewport_width_in_window, game_viewport_height_in_window);
    gfx::DrawRectOutline(painter, rect, gfx::Color::HotPink, 2.0f);
}

void ShowMessage(const std::string& msg, gfx::Painter& painter,
                 unsigned widget_width, unsigned widget_height)
{
    gfx::FRect rect(10.0f, 10.0f, 500.0f, 20.0f);
    gfx::DrawTextRect(painter, msg, "app://fonts/orbitron-medium.otf", 14, rect,
                      gfx::Color::HotPink, gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter);
}

} // namespace
