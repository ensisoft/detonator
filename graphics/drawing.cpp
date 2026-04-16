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
    klass->SetUniform<gfx::kBaseColor>(color);
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

bool DrawImage(Painter& painter, const FRect& rect, const std::string& image_uri,
                   const Color4f& base_color, BlendMode blending)
{
    auto material = CreateMaterialFromImage(image_uri, blending == BlendMode::Alpha
        ? MaterialClass::SurfaceType::Transparent
        : MaterialClass::SurfaceType::Opaque);
    material.SetUniform("kBaseColor", base_color);
    return FillRect(painter, rect, material);
}

bool DrawTextureSource(Painter& painter, const FRect& rect, const MaterialClass& material,
    const TextureSource& texture_source, const FRect& texture_rect)
{
    MaterialClass temp(gfx::MaterialClass::Type::Texture);
    temp.SetSurfaceType(material.GetSurfaceType());
    temp.SetUniform<kBaseColor>(material.GetUniformValue<kBaseColor>());
    temp.SetUniform<kAlphaCutoff>(material.GetUniformValue<kAlphaCutoff>());
    temp.SetTextureMinFilter(material.GetTextureMinFilter());
    temp.SetTextureMagFilter(material.GetTextureMagFilter());
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
    return DrawSDFShapeOutline(painter, rect, color, MaterialClass::SDFShape::Rect, line_width, 0.05f);
}

bool DrawRectOutline(Painter& painter, const FRect& rect, const Material& material, float line_width)
{
    return DrawShapeOutline(painter, rect, gfx::Rectangle(), material, line_width);
}

SDFShape PerformOutlineSDFShaping(const FRect& rect, MaterialClass::SDFShape shape, float line_width, float corner_radius)
{
    const auto width = rect.GetWidth();
    const auto height = rect.GetHeight();
    const auto max_size = std::max(width, height);
    const auto min_size = std::min(width, height);

    float aspect_ratio  = 1.0f;
    float outline_width = line_width / max_size;

    auto sdf_rect = rect;

    // so let's say I have an outline with outline width at 10 (logical pixels).
    // for the SDF functions the outline width must be a normalized value.
    // so we create a normalized valued by dividing the outline width with the
    // shape width. so for example if shape is 100x100 and outline is 10
    // then outline is 0.1 in SDF units, but when the shape covers the pixels
    // 0.1 will map back to 10 pixels.

    // but here's the first problem.
    // What if the shape isn't a square. but has elongated shape for example
    // 100x200 or 200x100. what should be the normalized outline width then?

    // here's the second problem, applying a non-uniform scale on the shape
    // will cause different axis to map to different rasterized pixel amounts.
    // for example 0.1 of 100 is 10 but 0.1 of 200 is 20.
    // therefore a rectangle that has size 100x200 and outline width 0.1 will
    // have outline that is much thicker on the left and right edges than
    // what it is at the top.

    // It turns out that this issue can be solved for some shapes by
    // messing with the aspect ratio, i.e. by replacing the incoming rect
    // with a square rect and by baking the aspect ratio in the SDF shape
    // itself.

    if (shape == MaterialClass::SDFShape::HorizontalCapsule ||
        shape == MaterialClass::SDFShape::VerticalCapsule)
    {
        // we know that the simple shape capsule wants keep the rendered
        // shape isotropic to the expected capsule shape regardless how
        // the shape's bounding box is dimensioned.
        // we'll implement the same thing here by first computing the
        // capsule properties based on the end cap radius

        // original corner radius in uv units based on the incoming (normalized)
        // corner radius and the minium size
        const auto corner_radius_uv_units = corner_radius * min_size;

        const auto width_delta  = max_size - width;
        const auto height_delta = max_size - height;
        sdf_rect.Resize(max_size, max_size);
        sdf_rect.Translate(-width_delta*0.5f, -height_delta*0.5f);

        corner_radius = corner_radius_uv_units / max_size;

        // (ab) using the aspect ratio to reduce the width of the capsule
        // to match the expected size
        if (shape == gfx::MaterialClass::SDFShape::HorizontalCapsule && height > width)
            aspect_ratio = width / height;
        else if (shape == gfx::MaterialClass::SDFShape::VerticalCapsule && width > height)
            aspect_ratio = height / width;
    }
    else if (shape == MaterialClass::SDFShape::Rect ||  shape == MaterialClass::SDFShape::RoundRect)
    {
        aspect_ratio = rect.GetAspectRatio();

        const auto width_delta  = max_size - width;
        const auto height_delta = max_size - height;

        sdf_rect.Resize(max_size, max_size);
        sdf_rect.Translate(-width_delta * 0.5f, -height_delta*0.5f);

        corner_radius = corner_radius * (min_size / max_size);
    }

    SDFShape ret;
    ret.outline_width = outline_width;
    ret.aspect_ratio  = aspect_ratio;
    ret.corner_radius = corner_radius;
    ret.rect = sdf_rect;
    return ret;
}

bool DrawSDFShapeOutline(const Painter& painter, const FRect& rect,
                         const Color4f& color, const MaterialClass::SDFShape shape,
                         float line_width, float corner_radius)

{
    static std::shared_ptr<gfx::ColorClass> klass;
    if (!klass)
        klass = std::make_shared<gfx::ColorClass>(gfx::MaterialClass::Type::Color);

    klass->SetUniform<kBaseColor>(color);
    klass->SetSurfaceType(MaterialClass::SurfaceType::Transparent);
    klass->SetFlag(MaterialClass::Flags::EnableSDF, true); // enable SDF shader support

    const auto& shaping_result = PerformOutlineSDFShaping(rect, shape, line_width, corner_radius);

    MaterialInstance material(klass);
    material.SetFlag(MaterialFlags::EnableSDF, true); // request to render in SDF mode
    material.SetSDFShape(shape);
    material.SetSDFShapeFillMode(MaterialInstance::SDFShapeFillMode::Outline);
    material.SetSDFShapeOutlineWidth(shaping_result.outline_width);
    material.SetSDShapeCornerRadius(shaping_result.corner_radius);
    material.SetSDFShapeAspectRatio(shaping_result.aspect_ratio);

    Transform transform;
    transform.MoveTo(shaping_result.rect);
    transform.Resize(shaping_result.rect);
    return painter.Draw(gfx::Rectangle(), transform, material);
}


bool DrawShapeOutline(Painter& painter, const FRect& rect, const Drawable& shape, const Color4f& color,
    float line_width, OutlineMethod method)
{
    // see if we can replace the shape with a built-in SDF material shape
    // for improved smoothness.
    if (shape.GetType() == Drawable::Type::SimpleShape &&
        (method == OutlineMethod::SDF || method == OutlineMethod::Automatic))
    {
        const auto simple_shape = GetSimpleShapeType(shape);
        if (simple_shape == SimpleShapeType::Rectangle)
        {
            return DrawSDFShapeOutline(painter, rect, color, MaterialClass::SDFShape::Rect, line_width, 0.05f);
        }
        else if (simple_shape == SimpleShapeType::RoundRect)
        {
            const float corner_radius = GetSimpleShapeAttribute(shape, SimpleShapeAttribute::CornerRadius);
            return DrawSDFShapeOutline(painter, rect, color, MaterialClass::SDFShape::RoundRect, line_width, corner_radius);
        }
        else if (simple_shape == SimpleShapeType::Parallelogram)
        {
            return DrawSDFShapeOutline(painter, rect, color, MaterialClass::SDFShape::Parallelogram, line_width, 0.05f);
        }
        else if (simple_shape == SimpleShapeType::Circle)
        {
            return DrawSDFShapeOutline(painter, rect, color, MaterialClass::SDFShape::Circle, line_width, 0.05f);
        }
        else if (simple_shape == SimpleShapeType::Capsule)
        {
            const auto orientation = static_cast<SimpleShapeOrientation>(GetSimpleShapeAttribute(shape, SimpleShapeAttribute::Orientation));
            const float radius = GetSimpleShapeAttribute(shape, SimpleShapeAttribute::CornerRadius);

            if (orientation == SimpleShapeOrientation::Horizontal)
                return DrawSDFShapeOutline(painter, rect, color, MaterialClass::SDFShape::HorizontalCapsule, line_width, radius);
            else if (orientation == SimpleShapeOrientation::Vertical)
                return DrawSDFShapeOutline(painter, rect, color, MaterialClass::SDFShape::VerticalCapsule, line_width, radius);
        }
    }

    return DrawShapeOutline(painter, rect, shape, MakeMaterial(color), line_width);
}
bool DrawShapeOutline(Painter& painter, const FRect& rect, const Drawable& shape, const Material& material, float line_width)
{
    const auto width  = rect.GetWidth();
    const auto height = rect.GetHeight();
    const auto x = rect.GetX();
    const auto y = rect.GetY();

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
