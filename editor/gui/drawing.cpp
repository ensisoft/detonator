// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

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
#include "base/math.h"

namespace gui
{

void DrawBasisVectors(gfx::Painter& painter, gfx::Transform& view)
{
    // draw the X vector
    view.Push();
        view.Scale(100.0f, 5.0f);
        view.Translate(0.0f, -2.5f);
        painter.Draw(gfx::Arrow(), view, gfx::SolidColor(gfx::Color::Green));
    view.Pop();

    // draw the Y vector
    view.Push();
        view.Scale(100.0f, 5.0f);
        view.Translate(-50.0f, -2.5f);
        view.Rotate(math::Pi * 0.5f);
        view.Translate(0.0f, 50.0f);
        painter.Draw(gfx::Arrow(), view, gfx::SolidColor(gfx::Color::Red));
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
    auto material = gfx::SolidColor(gfx::Color4f(gfx::Color::LightGray)); //, 0.7f));
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

} // namespace
