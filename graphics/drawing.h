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

#include <string>

#include "types.h"

// helper functionality to simplify common operations
// into one-liners in the application

namespace gfx
{

class Painter;
class Transform;
class Material;
class Color4f;

// Text alignment inside a rect
enum TextAlign {
    // Vertical text alignment
    AlignTop     = 0x1,
    AlignVCenter = 0x2,
    AlignBottom  = 0x4,
    // Horizontal text aligment
    AlignLeft    = 0x10,
    AlignHCenter = 0x20,
    AlignRight   = 0x40
};

// Draw text inside the given rectangle. The rectangle is defined in 
// pixels and positioned relative to the top left corner of the rendering
// target (e.g. the window surface)
void DrawTextRect(Painter& painter,
    const std::string& text,
    const std::string& font,
    unsigned font_size_px,
    const FRect& rect,     
    const Color4f& color,    
    unsigned alignment = TextAlign::AlignVCenter | TextAlign::AlignHCenter,
    bool underline = false);

// Draw the outline of a rectangle. the rectangle is defined in pixels
// and positioned relative to the top left corer of the render target/surface.
void DrawRectOutline(Painter&,
    const FRect& rect, 
    const Color4f& color,
    float line_width);
void DrawRectOutline(Painter&,
    const FRect& rect, 
    const Material& material, 
    float line_width);

} // namespace
