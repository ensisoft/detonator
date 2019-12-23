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
    bool underline)
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

    buff.AddText(text, font, font_size_px)
        .SetAlign(va).SetAlign(va).SetUnderline(underline);

    Transform t;
    t.Resize(rect.GetWidth(), rect.GetHeight());    
    t.MoveTo(rect.GetX(), rect.GetY());
    painter.Draw(Rectangle(), t, 
        gfx::BitmapText(buff).SetBaseColor(color));
}

void DrawRectOutline(Painter& painter,
    const FRect& rect,
    const Color4f& color,
    float line_width)
{
    DrawRectOutline(painter, rect, SolidColor(color), line_width);
}

void DrawRectOutline(Painter& painter, 
    const FRect& rect, 
    const Material& material,
    float line_width)
{
    const auto width  = rect.GetWidth();
    const auto height = rect.GetHeight();
    const auto x = rect.GetX();
    const auto y = rect.GetY();

    Transform outline_transform;
    outline_transform.Resize(width, height);
    outline_transform.MoveTo(x, y);

    Transform mask_transform;
    mask_transform.Resize(width - 2 * line_width, height - 2 * line_width);
    mask_transform.MoveTo(x + line_width, y + line_width);
    painter.DrawMasked(
        Rectangle(), outline_transform,
        Rectangle(), mask_transform, 
        material);

}
} // namespace
