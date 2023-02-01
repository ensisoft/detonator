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
#  include <QWidget>
#include "warnpop.h"

#include <algorithm>
#include <cstdio>

#include "editor/app/utility.h"
#include "editor/gui/drawing.h"
#include "editor/gui/utility.h"
#include "graphics/painter.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "graphics/transform.h"
#include "graphics/drawing.h"
#include "game/types.h"
#include "base/math.h"

namespace {
    gfx::Color4f DefaultGridColor = gfx::Color::LightGray;
} // namespace

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

ToolHotspot TestToolHotspot(gfx::Transform& trans, const gfx::FRect& box, const glm::vec4& view_pos)
{
    // basic idea for tool hot spot testing (in the corners of some selection rectangle)
    // is to take the incoming view coordinate and transform that by the inverse transform
    // from view coord to model coord and then compare whether it's inside the box(es) or not.

    // decompose the incoming transformation matrix
    // in order to figure out the scaling factor. we'll use the inverse
    // scale for the indicators in order to keep them a constant size
    // regardless of the scene node's scaling.
    glm::vec3 scale;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::quat orientation;
    glm::decompose(trans.GetAsMatrix(), scale, orientation, translation, skew, perspective);

    glm::vec4 rotate_hit_pos;
    glm::vec4 resize_hit_pos;
    glm::vec4 remove_hit_pos;

    // rotation circle
    trans.Push();
        trans.Scale(10.0f/scale.x, 10.0f/scale.y);
        trans.Translate(box.GetPosition());
        rotate_hit_pos = glm::inverse(trans.GetAsMatrix()) * view_pos;
    trans.Pop();

    if (rotate_hit_pos.x >= 0.0f && rotate_hit_pos.x <= 1.0f &&
        rotate_hit_pos.y >= 0.0f && rotate_hit_pos.y <= 1.0f)
        return ToolHotspot::Rotate;

    const auto [_0, _1, _2, bottom_right] = box.GetCorners();

    // resize box
    trans.Push();
        trans.Scale(10.0f/scale.x, 10.0f/scale.y);
        trans.Translate(bottom_right);
        trans.Translate(-10.0f/scale.x, -10.0f/scale.y);
        resize_hit_pos = glm::inverse(trans.GetAsMatrix()) * view_pos;
    trans.Pop();

    if (resize_hit_pos.x >= 0.0f && resize_hit_pos.x <= 1.0f &&
        resize_hit_pos.y >= 0.0f && resize_hit_pos.y <= 1.0f)
        return ToolHotspot::Resize;

    remove_hit_pos = glm::inverse(trans.GetAsMatrix()) * view_pos;
    if (box.TestPoint(remove_hit_pos.x, remove_hit_pos.y))
        return ToolHotspot::Remove;

    return ToolHotspot::None;
}

void DrawSelectionBox(gfx::Painter& painter, gfx::Transform& trans, const gfx::FRect& box)
{
    // all the transformations below are relative to the scene node
    // (the incoming transformation)

    // decompose the incoming transformation matrix
    // in order to figure out the scaling factor. we'll use the inverse
    // scale for the indicators in order to keep them a constant size
    // regardless of the scene node's scaling.
    glm::vec3 scale;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::quat orientation;
    glm::decompose(trans.GetAsMatrix(), scale, orientation, translation, skew, perspective);

    // selection rect
    trans.Push();
        trans.Scale(box.GetSize());
        trans.Translate(box.GetPosition());
        painter.Draw(gfx::Rectangle(gfx::Drawable::Style::Outline, 2.0f), trans,
                     gfx::CreateMaterialFromColor(gfx::Color::Green));
    trans.Pop();

    // rotation circle
    trans.Push();
        trans.Scale(10.0f/scale.x, 10.0f/scale.y);
        trans.Translate(box.GetPosition());
        painter.Draw(gfx::Circle(gfx::Drawable::Style::Outline, 2.0f), trans,
                     gfx::CreateMaterialFromColor(gfx::Color::Green));
    trans.Pop();

    const auto [_0, _1, _2, bottom_right] = box.GetCorners();

    // resize box
    trans.Push();
        trans.Scale(10.0f/scale.x, 10.0f/scale.y);
        trans.Translate(bottom_right);
        trans.Translate(-10.0f/scale.x, -10.0f/scale.y);
        painter.Draw(gfx::Rectangle(gfx::Drawable::Style::Outline, 2.0f), trans,
                     gfx::CreateMaterialFromColor(gfx::Color::Green));
    trans.Pop();
}

void DrawInvisibleItemBox(gfx::Painter& painter, gfx::Transform& trans, const gfx::FRect& box)
{
    trans.Push();
        trans.Scale(box.GetSize());
        trans.Translate(box.GetPosition());
        painter.Draw(gfx::Rectangle(gfx::Drawable::Style::Outline, 2.0f), trans,
                     gfx::CreateMaterialFromColor(gfx::Color::DarkYellow));
    trans.Pop();
}

void DrawBasisVectors(gfx::Painter& painter, gfx::Transform& trans)
{
    // draw the X vector
    trans.Push();
        trans.Scale(100.0f, 5.0f);
        trans.Translate(0.0f, -2.5f);
        painter.Draw(gfx::Arrow(), trans, gfx::CreateMaterialFromColor(gfx::Color::Green));
    trans.Pop();

    // draw the Y vector
    trans.Push();
        trans.Scale(100.0f, 5.0f);
        trans.Translate(-50.0f, -2.5f);
        trans.Rotate(math::Pi * 0.5f);
        trans.Translate(0.0f, 50.0f);
        painter.Draw(gfx::Arrow(), trans, gfx::CreateMaterialFromColor(gfx::Color::Red));
    trans.Pop();

    trans.Push();
        trans.Scale(2.5f, 2.5f);
        trans.Translate(-1.25f, -1.25f);
        painter.Draw(gfx::RoundRectangle(), trans, gfx::CreateMaterialFromColor(gfx::Color::Yellow));
    trans.Pop();
}

void DrawBasisVectors(gfx::Transform& view, std::vector<engine::DrawPacket>& packets, int layer)
{
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
        x.entity_node_layer = layer;
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
        y.entity_node_layer = layer;
        packets.push_back(std::move(y));
    view.Pop();

    view.Push();
        view.Scale(2.5f, 2.5f);
        view.Translate(-1.25f, -1.25f);
        //painter.Draw(gfx::RoundRectangle(), view, gfx::CreateMaterialFromColor(gfx::Color::Yellow));
    view.Pop();
}

void DrawSelectionBox(gfx::Transform& trans, std::vector<engine::DrawPacket>& packets, const gfx::FRect& rect, int layer)
{
    static const auto green  = std::make_shared<gfx::MaterialClassInst>(
            gfx::CreateMaterialClassFromColor(gfx::Color::Green));
    static const auto outline = std::make_shared<gfx::Rectangle>(gfx::Drawable::Style::Outline, 2.0f);
    static const auto circle  = std::make_shared<gfx::Circle>(gfx::Drawable::Style::Outline, 2.0f);

    const auto width  = rect.GetWidth();
    const auto height = rect.GetHeight();

    trans.Push();
        trans.Scale(width, height);
        trans.Translate(-width*0.5f, -height*0.5f);
        engine::DrawPacket selection;
        selection.transform = trans.GetAsMatrix();
        selection.material  = green;
        selection.drawable  = outline;
        selection.entity_node_layer = layer;
        packets.push_back(selection);
    trans.Pop();


    // decompose the matrix in order to get the combined scaling component
    // so that we can use the inverse scale to keep the resize and rotation
    // indicators always with same size.
    const auto& mat = trans.GetAsMatrix();
    glm::vec3 scale;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::quat orientation;
    glm::decompose(mat, scale, orientation, translation, skew,  perspective);

    // draw the resize indicator. (lower right corner box)
    trans.Push();
        trans.Scale(10.0f/scale.x, 10.0f/scale.y);
        trans.Translate(width*0.5f-10.0f/scale.x, height*0.5f-10.0f/scale.y);
        engine::DrawPacket sizing_box;
        sizing_box.transform = trans.GetAsMatrix();
        sizing_box.material  = green;
        sizing_box.drawable  = outline;
        sizing_box.entity_node_layer = layer;
        packets.push_back(sizing_box);
    trans.Pop();

    // draw the rotation indicator. (upper left corner circle)
    trans.Push();
        trans.Scale(10.0f/scale.x, 10.0f/scale.y);
        trans.Translate(-width*0.5f, -height*0.5f);
        engine::DrawPacket rotation_circle;
        rotation_circle.transform = trans.GetAsMatrix();
        rotation_circle.material  = green;
        rotation_circle.drawable  = circle;
        rotation_circle.entity_node_layer = layer;
        packets.push_back(rotation_circle);
    trans.Pop();
}

void DrawInvisibleItemBox(gfx::Transform& trans, std::vector<engine::DrawPacket>& packets, const gfx::FRect& rect, int layer)
{
    static const auto yellow = std::make_shared<gfx::MaterialClassInst>(
            gfx::CreateMaterialClassFromColor(gfx::Color::DarkYellow));
    static const auto shape  = std::make_shared<gfx::Rectangle>(gfx::Drawable::Style::Outline, 2.0f);

    const auto width  = rect.GetWidth();
    const auto height = rect.GetHeight();

    trans.Push();
        trans.Scale(rect.GetSize());
        trans.Translate(-width*0.5, -height*0.5);
        engine::DrawPacket box;
        box.transform         = trans.GetAsMatrix();
        box.material          = yellow;
        box.drawable          = shape;
        box.scene_node_layer  = 0;
        box.entity_node_layer = layer;
        packets.push_back(box);
    trans.Pop();
}

void SetGridColor(const gfx::Color4f& color)
{
    DefaultGridColor = color;
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

    auto grid_0 = gfx::Grid(num_grid_lines, num_grid_lines, true);
    auto grid_1 = gfx::Grid(num_grid_lines, num_grid_lines, false);
    auto material = gfx::CreateMaterialFromColor(DefaultGridColor);
    //material.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);

    view.Translate(grid_origin_x, grid_origin_y);
    painter.Draw(grid_0, view, material);

    view.Translate(-grid_width, 0);
    painter.Draw(grid_1, view, material);

    view.Translate(0, -grid_height);
    painter.Draw(grid_0, view, material);

    view.Translate(grid_width, 0);
    painter.Draw(grid_1, view, material);

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

void ShowMessage(const std::string& msg, gfx::Painter& painter)
{
    const gfx::FRect rect(10.0f, 10.0f, 500.0f, 20.0f);
    gfx::DrawTextRect(painter, msg, "app://fonts/orbitron-medium.otf", 14, rect,
                      gfx::Color::HotPink,
                      gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter);
}

void ShowMessage(const std::string& msg, const gfx::FRect& rect, gfx::Painter& painter)
{
    gfx::DrawTextRect(painter, msg, "app://fonts/orbitron-medium.otf", 14, rect,
                      gfx::Color::HotPink,
                      gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter);
}
void ShowMessage(const std::string& msg, const gfx::FPoint& pos, gfx::Painter& painter)
{
    // using 0 for rect width and height, this will create a raster buffer
    // with dimensions derived from the rasterized text extents.
    gfx::DrawTextRect(painter, msg, "app://fonts/orbitron-medium.otf", 14,
                      gfx::FRect(pos, 0.0f, 0.0f),
                      gfx::Color::HotPink,
                      gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter);
}

void ShowError(const std::string& msg, const gfx::FPoint& pos, gfx::Painter& painter)
{
    // using 0 for rect width and height, this will create a raster buffer
    // with dimensions derived from the rasterized text extents.
    gfx::DrawTextRect(painter, msg, "app://fonts/orbitron-medium.otf", 14,
                      gfx::FRect(pos, 0.0f, 0.0f),
                      gfx::Color::Red,
                      gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter,
                      gfx::TextProp::Blinking);
}

void PrintMousePos(const gfx::Transform& view, gfx::Painter& painter, QWidget* widget)
{
    // where's the mouse in the widget
    const auto& mickey = widget->mapFromGlobal(QCursor::pos());
    // can't use underMouse here because of the way the gfx widget
    // is constructed i.e. QWindow and Widget as container
    if (mickey.x() < 0 || mickey.y() < 0 ||
        mickey.x() > widget->width() ||
        mickey.y() > widget->height())
        return;

    const auto& view_to_scene = glm::inverse(view.GetAsMatrix());
    const auto& mouse_in_scene = view_to_scene * ToVec4(mickey);
    char hallelujah[128] = {};
    std::snprintf(hallelujah, sizeof(hallelujah), "%.2f, %.2f", mouse_in_scene.x, mouse_in_scene.y);
    ShowMessage(hallelujah, painter);
}

} // namespace
