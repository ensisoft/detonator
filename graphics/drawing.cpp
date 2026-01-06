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

#include <cmath>
#include <algorithm>

#include "base/assert.h"
#include "base/utility.h"
#include "base/math.h"
#include "graphics/bitmap.h"
#include "graphics/drawing.h"
#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/material_instance.h"
#include "graphics/material_class.h"
#include "graphics/drawable.h"
#include "graphics/transform.h"
#include "graphics/text_buffer.h"
#include "graphics/renderpass.h"
#include "graphics/simple_shape.h"
#include "graphics/linebatch.h"
#include "graphics/text_material.h"
#include "graphics/material_class.h"
#include "graphics/texture_map.h"

namespace {
gfx::MaterialInstance MakeMaterial(const gfx::Color4f& color)
{
    static std::shared_ptr<gfx::ColorClass> klass;
    if (!klass)
        klass = std::make_shared<gfx::ColorClass>(gfx::MaterialClass::Type::Color);

    const auto alpha = color.Alpha();
    klass->SetBaseColor(color);
    klass->SetSurfaceType(alpha == 1.0f
                       ? gfx::MaterialClass::SurfaceType::Opaque
                       : gfx::MaterialClass::SurfaceType::Transparent);
    return gfx::MaterialInstance(klass);
}
} // namespace

namespace gfx
{

bool DrawTextRect(Painter& painter,
    const std::string& text,
    const std::string& font,
    unsigned font_size_px,
    const FRect& rect,
    const Color4f& color,
    unsigned alignment,
    unsigned properties,
    float line_height)
{
    auto raster_width  =  (unsigned)math::clamp(0.0f, 2048.0f, rect.GetWidth());
    auto raster_height =  (unsigned)math::clamp(0.0f, 2048.0f, rect.GetHeight());
    const bool underline = properties & TextProp::Underline;
    const bool blinking  = properties & TextProp::Blinking;

    // if the text is set to be blinking do a sharp cut off
    // and when we have the "off" interval then simply don't
    // render the text.
    if (blinking)
    {
        const auto fps = 1.5;
        const auto full_period = 2.0 / fps;
        const auto half_period = full_period * 0.5;
        const auto time = fmodf(base::GetTime(), full_period);
        if (time >= half_period)
            return true;
    }

    TextMaterial material = CreateMaterialFromText(text, font, color, font_size_px,
                                                   raster_width, raster_height,
                                                   alignment, properties,
                                                   line_height);

    // unfortunately if no raster buffer dimensions were
    // specified the only way to figure them out is to
    // basically rasterize the text once and then see what
    // are the dimensions of the bitmap. the other way to
    // do this would be to add some font metrics. this
    // code path however should not be something that is
    // frequently used right now, so we're not doing font
    // metrics right now.
    if (raster_width == 0 || raster_height == 0)
    {
        material.ComputeTextMetrics(&raster_width, &raster_height);
    }

    // todo: we should/could check the painter whether it has a
    // a view transformation set that will change the texture mapping
    // between the rasterized fragments and the underlying texture object.
    // if there's no such transform i.e. the rectangle to be shaded on the
    // screen maps closely to the texture buffer, we can use fast point
    // sampling (using NEAREST filtering).
    material.SetPointSampling(true);

    Transform t;
    t.Resize(raster_width, raster_height);
    t.MoveTo(rect);
    return painter.Draw(Rectangle(), t, material);
}

bool DrawButtonIcon(const Painter& painter, const FRect& rect, const Color4f& color, const ButtonIcon btn)
{
    const auto btn_width  = rect.GetWidth();
    const auto btn_height = rect.GetHeight();
    const auto min_side = std::min(btn_width, btn_height);
    const auto ico_size = min_side * 0.4f;

    float rotation = 0.0f;
    if (btn == ButtonIcon::ArrowDown)
        rotation = math::Pi;
    else if (btn == ButtonIcon::ArrowLeft)
        rotation = math::Pi * 0.5 * -1.0;
    else if (btn == ButtonIcon::ArrowRight)
        rotation = math::Pi * 0.5;

    Transform model;
    model.Resize(ico_size, ico_size);
    model.Translate(ico_size*-0.5, ico_size*-0.5);
    model.RotateAroundZ(rotation);
    model.Translate(ico_size*0.5, ico_size*0.5);
    model.Translate(rect.GetPosition());
    model.Translate(btn_width*0.5, btn_height*0.5);
    model.Translate(ico_size*-0.5, ico_size*-0.5);

    return painter.Draw(IsoscelesTriangle(), model, MakeMaterial(color));
}

bool DrawHLine(Painter& painter, const FRect& rect, const Color4f& color, float line_width)
{
    const FPoint a(rect.GetX(), rect.GetY() + rect.GetHeight() * 0.5f);
    const FPoint b(rect.GetX() + rect.GetWidth(),
        rect.GetY() + rect.GetHeight() * 0.5f);
    return DebugDrawLine(painter, a, b, color, line_width);
}

bool DrawImage(Painter& painter, const FRect& rect, const std::string& image_uri, BlendMode blend)
{
    auto material = CreateMaterialFromImage(image_uri, blend == BlendMode::Alpha
        ? MaterialClass::SurfaceType::Transparent
        : MaterialClass::SurfaceType::Opaque);
    return FillRect(painter, rect, material);
}

bool DrawTextureSource(Painter& painter, const FRect& rect, const MaterialClass& material,
    const TextureSource& texture_source, const FRect& texture_rect)
{
    MaterialClass temp(gfx::MaterialClass::Type::Texture);
    temp.SetSurfaceType(material.GetSurfaceType());
    temp.SetBaseColor(material.GetBaseColor());
    temp.SetTextureMinFilter(material.GetTextureMinFilter());
    temp.SetTextureMagFilter(material.GetTextureMagFilter());
    temp.SetAlphaCutoff(material.GetAlphaCutoff());
    temp.AddTexture(texture_source.Copy());
    temp.SetTextureRect(texture_rect);
    return FillRect(painter, rect, MaterialInstance(std::move(temp)));
}

bool DrawBitmap(Painter& painter, const FRect& rect, std::unique_ptr<IBitmap> bitmap,
    std::string bitmap_gpu_id, std::string bitmap_name)
{
    auto material = CreateMaterialFromBitmap(std::move(bitmap), std::move(bitmap_gpu_id), std::move(bitmap_name));
    return FillRect(painter, rect, material);
}

bool DrawBitmap(Painter& painter, const FRect& rect, std::shared_ptr<const IBitmap> bitmap,
    std::string bitmap_gpu_id, std::string bitmap_name)
{
    auto material = CreateMaterialFromBitmap(std::move(bitmap), std::move(bitmap_gpu_id), std::move(bitmap_name));
    return FillRect(painter, rect, material);
}

bool FillRect(Painter& painter, const FRect& rect, const Color4f& color)
{
    return FillRect(painter, rect, MakeMaterial(color));
}

bool FillRect(Painter& painter, const FRect& rect, const Material& material)
{
    return FillShape(painter, rect, Rectangle(), material);
}

bool FillShape(Painter& painter, const FRect& rect, const Drawable& shape, const Color4f& color)
{
    const float alpha = color.Alpha();
    return FillShape(painter, rect, shape, MakeMaterial(color));
}
bool FillShape(Painter& painter, const FRect& rect, const Drawable& shape, const Material& material)
{
    const auto width  = rect.GetWidth();
    const auto height = rect.GetHeight();
    const auto x = rect.GetX();
    const auto y = rect.GetY();

    Transform trans;
    trans.Resize(width, height);
    trans.Translate(x, y);
    return painter.Draw(shape, trans, material);
}

bool DrawRectOutline(Painter& painter, const FRect& rect, const Color4f& color, float line_width)
{
    return DrawRectOutline(painter, rect, MakeMaterial(color));
}

bool DrawRectOutline(Painter& painter, const FRect& rect, const Material& material, float line_width)
{
    return DrawShapeOutline(painter, rect, gfx::Rectangle(), material, line_width);
}

bool DrawShapeOutline(Painter& painter, const FRect& rect, const Drawable& shape,
                      const Color4f& color, float line_width)
{
    return DrawShapeOutline(painter, rect, shape, MakeMaterial(color), line_width);
}
bool DrawShapeOutline(Painter& painter, const FRect& rect, const Drawable& shape,
                      const Material& material, float line_width)
{
    const auto width  = rect.GetWidth();
    const auto height = rect.GetHeight();
    const auto x = rect.GetX();
    const auto y = rect.GetY();

    if (shape.GetType() == Drawable::Type::SimpleShape && line_width < 10.0f)
    {
        if (GetSimpleShapeType(shape) == SimpleShapeType::Rectangle)
        {
            const auto lw50  = line_width * 0.5f;
            const auto lw100 = line_width;

            LineBatch2D batch;
            batch.AddLine(x, y+lw50, x+width, y+lw50);
            batch.AddLine(x, y+height-lw50, x+width, y+height-lw50);

            batch.AddLine(x+lw50, y+lw50, x+lw50, y+height);
            batch.AddLine(x+width-lw50, y+lw50, x+width-lw50, y+height);

            gfx::Transform transform;
            return painter.Draw(batch, transform, material,
                                Painter::LegacyDrawState(line_width));
        }
    }

    // todo: this algorithm produces crappy results with diagonal lines
    // for example when drawing right angled triangle even with line widths > 1.0f
    // the results aren't looking that great.

    Transform outline_transform;
    outline_transform.Resize(width, height);
    outline_transform.Translate(x, y);

    Transform mask_transform;
    const auto mask_width  = width - 2 * line_width;
    const auto mask_height = height - 2 * line_width;
    mask_transform.Resize(mask_width, mask_height);
    mask_transform.Translate(x + line_width, y + line_width);

    const StencilMaskPass mask(1, 0, painter);
    const StencilTestColorWritePass cover(1, painter);

    bool ok = true;
    ok &= mask.Draw(shape, mask_transform, material);
    ok &= cover.Draw(shape, outline_transform, material);
    return ok;
}

bool DebugDrawLine(Painter& painter, const FPoint& a, const FPoint& b, const Color4f& color, float line_width)
{
    return DebugDrawLine(painter, a, b, MakeMaterial(color), line_width);
}

bool DebugDrawLine(Painter& painter, const FPoint& a, const FPoint& b, const Material& material, float line_width)
{
    // The line shape defines a horizontal line so in order to
    // support lines with arbitrary directions we need to figure
    // out which way to rotate the line shape in order to have a matching
    // line (slope) and also how to scale the shape
    const auto& p = b - a;
    const auto x = p.GetX();
    const auto y = p.GetY();
    // pythagorean distance between the points is the length of the
    // line, used for horizontal scaling of the shape (along the X axis)
    const auto length = std::sqrt(x*x + y*y);
    const auto cosine = std::acos(x / length);
    // acos gives the principal angle [0, Pi], need to see if the
    // points would require a negative rotation.
    const auto angle  = a.GetY() > b.GetY() ? -cosine : cosine;

    Transform trans;
    trans.Scale(length, line_width);
    // offset by half the line width so that the vertical center of the
    // line aligns with the point. important when using line widths > 1.0f
    trans.Translate(0, -0.5*line_width);
    trans.RotateAroundZ(angle);
    trans.Translate(a);

    // Draw the shape (line)
    return painter.Draw(StaticLine(), trans, material, line_width);
}

bool DebugDrawCircle(Painter& painter, const FCircle& circle, const Color4f& color, float line_width)
{
    return DebugDrawCircle(painter, circle, MakeMaterial(color), line_width);
}

bool DebugDrawCircle(Painter& painter, const FCircle & circle, const Material& material, float line_width)
{
    const auto radius = circle.GetRadius();

    Transform trans;
    trans.Resize(circle.Inscribe());
    trans.Translate(circle.GetCenter());
    trans.Translate(-radius, -radius);
    return painter.Draw(Circle(SimpleShapeStyle::Outline), trans, material, line_width);
}

bool DebugDrawRect(Painter& painter, const FRect& rect, const Color4f& color, float line_width)
{
    return DebugDrawRect(painter, rect, MakeMaterial(color), line_width);
}
bool DebugDrawRect(Painter& painter, const FRect& rect, const Material& material, float line_width)
{
    bool ok = true;
    const auto [c0, c1, c2, c3] = rect.GetCorners();
    ok &= gfx::DebugDrawLine(painter, c0, c1, material, line_width);
    ok &= gfx::DebugDrawLine(painter, c1, c3, material, line_width);
    ok &= gfx::DebugDrawLine(painter, c3, c2, material, line_width);
    ok &= gfx::DebugDrawLine(painter, c2, c0, material, line_width);
    return ok;
}


} // namespace
