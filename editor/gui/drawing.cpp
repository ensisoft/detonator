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

#include "base/math.h"
#include "engine/camera.h"
#include "game/types.h"
#include "editor/app/utility.h"
#include "editor/gui/drawing.h"
#include "editor/gui/utility.h"
#include "graphics/algo.h"
#include "graphics/painter.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "graphics/transform.h"
#include "graphics/drawing.h"
#include "graphics/framebuffer.h"

namespace {
    gfx::Color4f DefaultGridColor = gfx::Color::LightGray;
} // namespace

namespace gui
{
void DrawLine(gfx::Painter& painter, const glm::vec2& src, const glm::vec2& dst)
{
    gfx::DebugDrawLine(painter, gfx::FPoint(src.x, src.y), gfx::FPoint(dst.x, dst.y), gfx::Color::DarkYellow, 2.0f);
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
        painter.Draw(gfx::Rectangle(gfx::Drawable::Style::Outline), trans,
                     gfx::CreateMaterialFromColor(gfx::Color::Green), 2.0f);
    trans.Pop();

    // rotation circle
    trans.Push();
        trans.Scale(10.0f/scale.x, 10.0f/scale.y);
        trans.Translate(box.GetPosition());
        painter.Draw(gfx::Circle(gfx::Drawable::Style::Outline), trans,
                     gfx::CreateMaterialFromColor(gfx::Color::Green), 2.0f);
    trans.Pop();

    const auto [_0, _1, _2, bottom_right] = box.GetCorners();

    // resize box
    trans.Push();
        trans.Scale(10.0f/scale.x, 10.0f/scale.y);
        trans.Translate(bottom_right);
        trans.Translate(-10.0f/scale.x, -10.0f/scale.y);
        painter.Draw(gfx::Rectangle(gfx::Drawable::Style::Outline), trans,
                     gfx::CreateMaterialFromColor(gfx::Color::Green), 2.0f);
    trans.Pop();
}

void DrawInvisibleItemBox(gfx::Painter& painter, gfx::Transform& trans, const gfx::FRect& box)
{
    trans.Push();
        trans.Scale(box.GetSize());
        trans.Translate(box.GetPosition());
        painter.Draw(gfx::Rectangle(gfx::Drawable::Style::Outline), trans,
                     gfx::CreateMaterialFromColor(gfx::Color::DarkYellow), 2.0f);
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
        trans.RotateAroundZ(math::Pi * 0.5f);
        trans.Translate(0.0f, 50.0f);
        painter.Draw(gfx::Arrow(), trans, gfx::CreateMaterialFromColor(gfx::Color::Red));
    trans.Pop();

    trans.Push();
        trans.Scale(2.5f, 2.5f);
        trans.Translate(-1.25f, -1.25f);
        painter.Draw(gfx::RoundRectangle(), trans, gfx::CreateMaterialFromColor(gfx::Color::Yellow));
    trans.Pop();
}

void DrawBasisVectors(gfx::Transform& model, std::vector<engine::DrawPacket>& packets)
{
    static auto green = std::make_shared<gfx::MaterialClassInst>(
            gfx::CreateMaterialClassFromColor(gfx::Color::Green));
    static auto red   = std::make_shared<gfx::MaterialClassInst>(
            gfx::CreateMaterialClassFromColor(gfx::Color::Red));
    static auto arrow = std::make_shared<gfx::Arrow>();

    // draw the X vector
    {
        model.Push();
            model.Scale(100.0f, 5.0f);
            model.Translate(0.0f, -2.5f);
            engine::DrawPacket packet;
            packet.domain       = engine::DrawPacket::Domain::Editor;
            packet.transform    = model;
            packet.material     = green;
            packet.drawable     = arrow;
            packet.render_layer = 0;
            packet.packet_index = 0;
            packets.push_back(std::move(packet));
        model.Pop();
    }

    // draw the Y vector
    {
        model.Push();
            model.Scale(100.0f, 5.0f);
            model.Translate(-50.0f, -2.5f);
            model.RotateAroundZ(math::Pi * 0.5f);
            model.Translate(0.0f, 50.0f);
            engine::DrawPacket packet;
            packet.domain = engine::DrawPacket::Domain::Editor;
            packet.transform    = model;
            packet.material     = red;
            packet.drawable     = arrow;
            packet.render_layer = 0;
            packet.packet_index = 0;
            packets.push_back(std::move(packet));
        model.Pop();
    }
}

void DrawSelectionBox(gfx::Transform& model, std::vector<engine::DrawPacket>& packets, const gfx::FRect& rect)
{
    static const auto green  = std::make_shared<gfx::MaterialClassInst>(
            gfx::CreateMaterialClassFromColor(gfx::Color::Green));
    static const auto outline = std::make_shared<gfx::Rectangle>(gfx::Drawable::Style::Outline);
    static const auto circle  = std::make_shared<gfx::Circle>(gfx::Drawable::Style::Outline);

    const auto width  = rect.GetWidth();
    const auto height = rect.GetHeight();

    model.Push();
        model.Scale(width, height);
        model.Translate(-width*0.5f, -height*0.5f);
        engine::DrawPacket selection;
        selection.domain       = engine::DrawPacket::Domain::Editor;
        selection.transform    = model;
        selection.material     = green;
        selection.drawable     = outline;
        selection.render_layer = 0;
        selection.packet_index = 0;
        selection.line_width   = 2.0f;
        packets.push_back(selection);
    model.Pop();

    // decompose the matrix in order to get the combined scaling component
    // so that we can use the inverse scale to keep the resize and rotation
    // indicators always with same size.
    const glm::mat4 mat = model;
    glm::vec3 scale;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::quat orientation;
    glm::decompose(mat, scale, orientation, translation, skew,  perspective);

    // draw the resize indicator. (lower right corner box)
    model.Push();
        model.Scale(10.0f/scale.x, 10.0f/scale.y);
        model.Translate(width*0.5f-10.0f/scale.x, height*0.5f-10.0f/scale.y);
        engine::DrawPacket sizing_box;
        sizing_box.domain       = engine::DrawPacket::Domain::Editor;
        sizing_box.transform    = model;
        sizing_box.material     = green;
        sizing_box.drawable     = outline;
        sizing_box.render_layer = 0;
        sizing_box.packet_index = 0;
        sizing_box.line_width   = 2.0f;
        packets.push_back(sizing_box);
    model.Pop();

    // draw the rotation indicator. (upper left corner circle)
    model.Push();
        model.Scale(10.0f/scale.x, 10.0f/scale.y);
        model.Translate(-width*0.5f, -height*0.5f);
        engine::DrawPacket rotation_circle;
        rotation_circle.domain       = engine::DrawPacket::Domain::Editor;
        rotation_circle.transform    = model;
        rotation_circle.material     = green;
        rotation_circle.drawable     = circle;
        rotation_circle.render_layer = 0;
        rotation_circle.packet_index = 0;
        rotation_circle.line_width   = 2.0f;
        packets.push_back(rotation_circle);
    model.Pop();
}

void DrawInvisibleItemBox(gfx::Transform& model, std::vector<engine::DrawPacket>& packets, const gfx::FRect& rect)
{
    static const auto yellow = std::make_shared<gfx::MaterialClassInst>(
            gfx::CreateMaterialClassFromColor(gfx::Color::DarkYellow));
    static const auto shape  = std::make_shared<gfx::Rectangle>(gfx::Drawable::Style::Outline);

    const auto width  = rect.GetWidth();
    const auto height = rect.GetHeight();

    model.Push();
        model.Scale(rect.GetSize());
        model.Translate(-width*0.5, -height*0.5);
        engine::DrawPacket box;
        box.domain            = engine::DrawPacket::Domain::Editor;
        box.transform         = model;
        box.material          = yellow;
        box.drawable          = shape;
        box.render_layer      = 0;
        box.packet_index      = 0;
        box.line_width        = 2.0f;
        packets.push_back(box);
    model.Pop();
}

void DrawEdges(const gfx::Painter& scene_painter,
               const gfx::Painter& window_painter,
               const gfx::Drawable& drawable,
               const gfx::Material& material,
               const glm::mat4& model,
               const std::string& widget_id)
{
    // in order to draw edges we basically create a secondary FBO with
    // texture color attachment. then render the actual drawable with the
    // actual material in that texture, perform edge detection on the texture
    // and then draw in the whole window using the edge detection texture.
    //
    // This is currently inefficient since the FBO allocated is rather large
    // and it should be possible to use a smaller one. Size could be derived
    // from the object's bounding when transformed to window coordinates.

    ASSERT(!scene_painter.HasScissor());

    gfx::Painter painter(scene_painter);

    auto* device = painter.GetDevice();
    auto* fbo = device->FindFramebuffer("Editor/Widget/" + widget_id);
    if (!fbo)
        fbo = device->MakeFramebuffer("Editor/Widget/" + widget_id);

    const auto surface = painter.GetSurfaceSize();
    const unsigned src_width  = surface.GetWidth();
    const unsigned src_height = surface.GetHeight();
    // down sample by half rez to reduce the memory drag a bit.
    const unsigned target_width = src_width / 2;
    const unsigned target_height = src_height / 2;

    gfx::Framebuffer::Config conf;
    conf.width  = target_width;
    conf.height = target_height;
    conf.format = gfx::Framebuffer::Format::ColorRGBA8;
    fbo->SetConfig(conf);

    painter.SetFramebuffer(fbo);
    painter.SetSurfaceSize(target_width, target_height);
    painter.SetViewport(0, 0, target_width, target_height);
    painter.ClearColor(gfx::Color::Transparent);
    painter.Draw(drawable, model, material);

    // get the output
    gfx::Texture* result = nullptr;
    fbo->Resolve(&result);

    gfx::algo::FlipTexture("Editor/Widget/" + widget_id + "/EdgeTexture", result, device, gfx::algo::FlipDirection::Horizontal);

    // detect the edges (updates the texture in place)
    gfx::algo::DetectSpriteEdges("Editor/Widget/" + widget_id + "/EdgeTexture", result, device, gfx::Color::HotPink);

    // blend it back in the scene
    {
        const auto window_size = window_painter.GetSurfaceSize();

        gfx::Transform model;
        model.Resize(window_size.GetWidth(), window_size.GetHeight());
        model.MoveTo(0.0f, 0.0f);
        window_painter.Draw(gfx::Rectangle(), model, gfx::CreateMaterialFromTexture("", result));
    }
}


void SetGridColor(const gfx::Color4f& color)
{
    DefaultGridColor = color;
}

void DrawCoordinateGrid(gfx::Painter& painter, gfx::Transform& view,
    GridDensity grid,
    float zoom,
    float xs,
    float ys,
    unsigned width,
    unsigned height)
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
    // of view basis axis puts us in the middle of the window in window space.)
    auto view_to_model = glm::inverse(view.GetAsMatrix());
    auto view_origin_in_model = view_to_model * glm::vec4(width / 2.0f, height / 2.0, 1.0f, 1.0f);

    view.Scale(cell_scale_factor, cell_scale_factor);

    // to make the grid cover the whole viewport we can easily do it by rendering
    // the grid in each quadrant of the coordinate space aligned around the center
    // point of the viewport. Then it doesn't matter if the view transformation
    // includes rotation or not.
    const auto grid_origin_x = (int)view_origin_in_model.x / cell_size_units * cell_size_units;
    const auto grid_origin_y = (int)view_origin_in_model.y / cell_size_units * cell_size_units;
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

void DrawCoordinateGrid(gfx::Painter& painter,
    GridDensity grid,
    float zoom,
    float xs,
    float ys,
    unsigned width,
    unsigned height)
{
    const int grid_size = std::max(width/xs, height/ys) / zoom * 2.0f;

    const auto cell_size_units = static_cast<int>(grid);
    const auto num_grid_lines = (grid_size / cell_size_units) - 1;
    const auto num_cells = num_grid_lines + 1;
    const auto cell_size_normalized = 1.0f / num_cells;
    const auto cell_scale_factor = cell_size_units / cell_size_normalized;
    const auto grid_width = cell_size_units * num_cells;
    const auto grid_height = cell_size_units * num_cells;

    const gfx::Grid grid_0(num_grid_lines, num_grid_lines, true);
    const gfx::Grid grid_1(num_grid_lines, num_grid_lines, false);
    const auto material = gfx::CreateMaterialFromColor(DefaultGridColor);

    // map the center of the screen to a position on the world plane.
    // basically this means that when we draw the grid at this position
    // it is always visible in the viewport. (this world position maps
    // to the center of the window!)
    const auto world_pos = engine::MapFromWindowToWorldPlane(painter.GetProjMatrix(),
                                                             painter.GetViewMatrix(),
                                                             glm::vec2{width * 0.5f, height * 0.5f},
                                                             glm::vec2{width, height});
    // align the grid's world position on a coordinate that is a multiple of
    // the grid cell on both axis.
    const float grid_origin_x = (int)world_pos.x / cell_size_units * cell_size_units;
    const float grid_origin_y = (int)world_pos.y / cell_size_units * cell_size_units;

    // draw 4 quadrants of the grid around the grid origin
    gfx::Transform transform;
    transform.Scale(cell_scale_factor, cell_scale_factor);

    transform.Translate(grid_origin_x, grid_origin_y);
    painter.Draw(grid_0, transform, material);

    transform.Translate(-grid_width, 0.0f, 0.0f);
    painter.Draw(grid_1, transform, material);

    transform.Translate(0.0f, -grid_height, 0.0f);
    painter.Draw(grid_0, transform, material);

    transform.Translate(grid_width, 0.0f, 0.0f);
    painter.Draw(grid_1, transform, material);

    // debug, draw a little dot to indicate the grid's world position
#if 0
    {
        gfx::Transform transform;
        transform.Scale(10.0f, 10.0f);
        transform.Translate(grid_origin_x, grid_origin_y);
        transform.Translate(-5.0f, -5.0f);
        painter.Draw(gfx::Circle(), transform, gfx::CreateMaterialFromColor(gfx::Color::HotPink));
    }
#endif
}

void DrawViewport(gfx::Painter& painter,
                  gfx::Transform& view,
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

    const gfx::FRect text(game_viewport_x_in_window +  0.0f,
                          game_viewport_y_in_window + game_viewport_height_in_window + 10.0f,
                          200.0f, 20.0f);
    gfx::DrawTextRect(painter, base::FormatString("%1 x %2", (int)game_viewport_width, (int)game_viewport_height),
                      "app://fonts/orbitron-medium.otf", 14, text,
                      gfx::Color::HotPink,
                      gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter);
}

void ShowMessage(const app::AnyString& msg, gfx::Painter& painter)
{
    const gfx::FRect rect(10.0f, 10.0f, 500.0f, 20.0f);
    gfx::DrawTextRect(painter, msg, "app://fonts/orbitron-medium.otf", 14, rect,
                      gfx::Color::HotPink,
                      gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter);
}

void ShowMessage(const app::AnyString& msg, const Rect2Df& rect, gfx::Painter& painter)
{
    gfx::DrawTextRect(painter, msg, "app://fonts/orbitron-medium.otf", 14, rect,
                      gfx::Color::HotPink,
                      gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter);
}
void ShowMessage(const app::AnyString& msg, const Point2Df& pos, gfx::Painter& painter)
{
    // using 0 for rect width and height, this will create a raster buffer
    // with dimensions derived from the rasterized text extents.
    gfx::DrawTextRect(painter, msg, "app://fonts/orbitron-medium.otf", 14,
                      gfx::FRect(pos, 0.0f, 0.0f),
                      gfx::Color::HotPink,
                      gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter);
}

void ShowError(const app::AnyString& msg, const Point2Df& pos, gfx::Painter& painter)
{
    // using 0 for rect width and height, this will create a raster buffer
    // with dimensions derived from the rasterized text extents.
    gfx::DrawTextRect(painter, msg, "app://fonts/orbitron-medium.otf", 14,
                      gfx::FRect(pos, 0.0f, 0.0f),
                      gfx::Color::Red,
                      gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter,
                      gfx::TextProp::Blinking);
}

void ShowInstruction(const app::AnyString& msg, const Rect2Df& rect, gfx::Painter& painter, unsigned font_size_px)
{
    gfx::DrawTextRect(painter, msg, "app://fonts/orbitron-medium.otf", font_size_px,
                      rect,
                      gfx::Color::Silver,
                      gfx::TextAlign::AlignVCenter | gfx::TextAlign::AlignHCenter,
                      gfx::TextProp::None,
                      2.0f);
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

void PrintMousePos(const glm::mat4& view_to_clip,
                   const glm::mat4& world_to_view,
                   gfx::Painter& painter,
                   QWidget* widget)

{
    const auto width = widget->width();
    const auto height = widget->height();

    // where's the mouse in the widget
    const auto& mickey = widget->mapFromGlobal(QCursor::pos());
    // can't use underMouse here because of the way the gfx widget
    // is constructed i.e. QWindow and Widget as container
    if (mickey.x() < 0 || mickey.y() < 0 ||
        mickey.x() > width || mickey.y() > height)
        return;

    const auto& world_pos = engine::MapFromWindowToWorldPlane(view_to_clip,
                                                              world_to_view,
                                                              glm::vec2{mickey.x(), mickey.y()},
                                                              glm::vec2{width, height});
    char hallelujah[128] = {};
    std::snprintf(hallelujah, sizeof(hallelujah), "%.2f, %.2f", world_pos.x, world_pos.y);
    ShowMessage(hallelujah, painter);
}




} // namespace
