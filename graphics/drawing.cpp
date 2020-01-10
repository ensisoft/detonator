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

#include "base/assert.h"
#include "base/utility.h"
#include "bitmap.h"
#include "drawing.h"
#include "painter.h"
#include "material.h"
#include "drawable.h"
#include "transform.h"
#include "text.h"

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

    TextBuffer::HorizontalAlignment ha;
    TextBuffer::VerticalAlignment va;
    if ((alignment & 0xf) == AlignTop)
        va = TextBuffer::VerticalAlignment::AlignTop;
    else if((alignment & 0xf) == AlignVCenter)
        va = TextBuffer::VerticalAlignment::AlignCenter;
    else if ((alignment & 0xf) == AlignBottom)
        va = TextBuffer::VerticalAlignment::AlignBottom;

    if ((alignment & 0xf0) == AlignLeft)
        ha = TextBuffer::HorizontalAlignment::AlignLeft;
    else if ((alignment & 0xf0) == AlignHCenter)
        ha = TextBuffer::HorizontalAlignment::AlignCenter;
    else if ((alignment & 0xf0) == AlignRight)
        ha = TextBuffer::HorizontalAlignment::AlignRight;

    const bool underline = properties & TextProp::Underline;
    const bool blinking  = properties & TextProp::Blinking;

    buff.AddText(text, font, font_size_px)
        .SetAlign(va).SetAlign(ha).SetUnderline(underline);

    auto material = BitmapText(buff);
    material.SetColorA(color);
    material.SetColorB(color);
    if (blinking)
    {
        // create 1x1 transparent texture and 
        // set the material frame rate so that it animates between these 
        // two.
        static Bitmap<Grayscale> nada(1, 1);
        nada.SetPixel(0, 0, 0);
        material.AddTexture(nada);
        material.SetTextureGc(1, false);
        material.SetFps(1.5f);
        material.SetRuntime(base::GetRuntimeSec());
        material.SetTextureBlendWeight(0.0f); // sharp cut off i.e. no blending betwen textures.
    }

    Transform t;
    t.Resize(rect.GetWidth(), rect.GetHeight());    
    t.MoveTo(rect.GetX(), rect.GetY());
    painter.Draw(Rectangle(), t, material);
}

void FillRect(Painter& painter, 
    const FRect& rect, 
    const Color4f& color, 
    float rotation) 
{
    const float alpha = color.Alpha();

    FillRect(painter, rect, 
        SolidColor(color).SetSurfaceType(alpha == 1.0f
            ? Material::SurfaceType::Opaque
            : Material::SurfaceType::Transparent), 
        rotation);
}

void FillRect(Painter& painter, 
    const FRect& rect, 
    const Material& material, 
    float rotation)
{
    const auto width  = rect.GetWidth();
    const auto height = rect.GetHeight();
    const auto x = rect.GetX();
    const auto y = rect.GetY();

    Transform trans;
    trans.Resize(width, height);
    if (rotation > 0.0f)
    {
        trans.Translate(-width*0.5, -height*0.5);
        trans.Rotate(rotation);
        trans.Translate(width*0.5, height*0.5);
    }
    trans.Translate(x, y);
    painter.Draw(Rectangle(), trans, material);
}

void DrawRectOutline(Painter& painter,
    const FRect& rect,
    const Color4f& color,
    float line_width,
    float rotation)
{
    const float alpha = color.Alpha();

    DrawRectOutline(painter, rect, 
        SolidColor(color).SetSurfaceType(alpha == 1.0f 
            ? Material::SurfaceType::Opaque 
            : Material::SurfaceType::Transparent),
        line_width, rotation);
}

void DrawRectOutline(Painter& painter, 
    const FRect& rect, 
    const Material& material,
    float line_width, 
    float rotation)
{
    const auto width  = rect.GetWidth();
    const auto height = rect.GetHeight();
    const auto x = rect.GetX();
    const auto y = rect.GetY();

    Transform outline_transform;
    outline_transform.Resize(width, height);
    if (rotation > 0.0f)  // todo: some delta value ?
    {
        outline_transform.Translate(-width*0.5, -height*0.5);
        outline_transform.Rotate(rotation);
        outline_transform.Translate(width*0.5, height*0.5);
    }

    outline_transform.Translate(x, y);

    Transform mask_transform;
    const auto mask_width  = width - 2 * line_width;
    const auto mask_height = height - 2 * line_width;
    mask_transform.Resize(mask_width, mask_height);
    if (rotation > 0.0f) // todo: some delta value check ?
    {
        mask_transform.Translate(-mask_width *0.5, -mask_height * 0.5);
        mask_transform.Rotate(rotation);
        mask_transform.Translate(mask_width * 0.5, mask_height * 0.5);
    }

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

    painter.DrawMasked(
        Rectangle(), outline_transform,
        Rectangle(), mask_transform, 
        material);
}
} // namespace
