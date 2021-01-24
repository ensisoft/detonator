// Copyright (c) 2010-2019 Sami Väisänen, Ensisoft
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

#include <cmath>

#include "base/assert.h"
#include "base/utility.h"
#include "graphics/bitmap.h"
#include "graphics/drawing.h"
#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/drawable.h"
#include "graphics/transform.h"
#include "graphics/text.h"

namespace gfx
{

void DrawTextRect(Painter& painter,
    const std::string& text,
    const std::string& font, unsigned font_size_px,
    const FRect& rect,
    const Color4f& color,
    unsigned alignment,
    unsigned properties)
{
    TextBuffer buff(rect.GetWidth(), rect.GetHeight());
    if ((alignment & 0xf) == AlignTop)
        buff.SetAlignment(TextBuffer::VerticalAlignment::AlignTop);
    else if((alignment & 0xf) == AlignVCenter)
        buff.SetAlignment(TextBuffer::VerticalAlignment::AlignCenter);
    else if ((alignment & 0xf) == AlignBottom)
        buff.SetAlignment(TextBuffer::VerticalAlignment::AlignBottom);

    if ((alignment & 0xf0) == AlignLeft)
        buff.SetAlignment(TextBuffer::HorizontalAlignment::AlignLeft);
    else if ((alignment & 0xf0) == AlignHCenter)
        buff.SetAlignment(TextBuffer::HorizontalAlignment::AlignCenter);
    else if ((alignment & 0xf0) == AlignRight)
        buff.SetAlignment(TextBuffer::HorizontalAlignment::AlignRight);

    const bool underline = properties & TextProp::Underline;
    const bool blinking  = properties & TextProp::Blinking;

    // Add blob of text in the buffer.
    TextBuffer::Text text_and_style;
    text_and_style.text = text;
    text_and_style.font = font;
    text_and_style.fontsize = font_size_px;
    text_and_style.underline = underline;
    buff.AddText(text_and_style);

    // Setup material to shade the text.
    static auto klass = std::make_shared<gfx::MaterialClass>();
    klass->SetType(MaterialClass::Type::Texture);
    klass->SetSurfaceType(MaterialClass::SurfaceType::Transparent);
    klass->SetBaseColor(color);
    if (klass->GetNumTextures() == 0)
    {
        klass->AddTexture(buff);
        // let this texture object be garbage collected
        // when it's no longer used.
        klass->SetTextureGc(0, true); //
    }
    else
    {
        auto& source = klass->GetTextureSource(0);
        auto& text = static_cast<detail::TextureTextBufferSource&>(source);
        text.SetTextBuffer(std::move(buff));
    }

    // if the text is set to be blinking do a sharp cut off
    // and when we have the "off" interval then simply don't
    // render the text.
    if (blinking)
    {
        const auto fps = 1.5;
        const auto full_period = 2.0 / fps;
        const auto half_period = full_period * 0.5;
        const auto time = fmodf(base::GetRuntimeSec(), full_period);
        if (time >= half_period)
            return;
    }

    Transform t;
    t.Resize(rect);
    t.MoveTo(rect);
    painter.Draw(Rectangle(), t, Material(klass));
}

void FillRect(Painter& painter, const FRect& rect, const Color4f& color)
{
    const float alpha = color.Alpha();

    FillRect(painter, rect,
        SolidColor(color).SetSurfaceType(alpha == 1.0f
            ? MaterialClass::SurfaceType::Opaque
            : MaterialClass::SurfaceType::Transparent));

}

void FillRect(Painter& painter, const FRect& rect, const Material& material)
{
    const auto width  = rect.GetWidth();
    const auto height = rect.GetHeight();
    const auto x = rect.GetX();
    const auto y = rect.GetY();

    Transform trans;
    trans.Resize(width, height);
    trans.Translate(x, y);
    painter.Draw(Rectangle(), trans, material);
}

void DrawRectOutline(Painter& painter, const FRect& rect, const Color4f& color, float line_width)
{
    const float alpha = color.Alpha();

    DrawRectOutline(painter, rect,
        SolidColor(color).SetSurfaceType(alpha == 1.0f
            ? MaterialClass::SurfaceType::Opaque
            : MaterialClass::SurfaceType::Transparent),
        line_width);
}

void DrawRectOutline(Painter& painter, const FRect& rect, const Material& material, float line_width)
{
    const auto width  = rect.GetWidth();
    const auto height = rect.GetHeight();
    const auto x = rect.GetX();
    const auto y = rect.GetY();

    Transform outline_transform;
    outline_transform.Resize(width, height);
    outline_transform.Translate(x, y);

    Transform mask_transform;
    const auto mask_width  = width - 2 * line_width;
    const auto mask_height = height - 2 * line_width;
    mask_transform.Resize(mask_width, mask_height);
    mask_transform.Translate(x + line_width, y + line_width);

    // todo: if the stencil buffer is not multisampled this
    // could produce some aliasing artifacts if there's a rotational
    // component in the transformation. Not sure if a better way to
    // draw the outline then would be to just draw lines.
    // However the line rasterization leaves the gaps at the end where
    // the lines meet which becomes clearly visible at higher
    // line widths. One possible solution could be to use
    // NV_path_rendering (when available) or then manually fill
    // the line gaps with quads.
    painter.Draw(Rectangle(), outline_transform,
                 Rectangle(), mask_transform,
                 material);
}

void DrawLine(Painter& painter, const FPoint& a, const FPoint& b, const Color4f& color, float line_width)
{
    DrawLine(painter, a, b, SolidColor(color), line_width);
}

void DrawLine(Painter& painter, const FPoint& a, const FPoint& b, const Material& material, float line_width)
{
    // The line shape defines a horizonal line so in order to
    // support lines with arbitrary directions we need to figure
    // out which way to rotate the line shape in order to have a matchin
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
    // line alings with the point. important when using line widths > 1.0f
    trans.Translate(0, -0.5*line_width);
    trans.Rotate(angle);
    trans.Translate(a);

    // Draw the shape (line)
    painter.Draw(Line(line_width), trans, material);
}

} // namespace
