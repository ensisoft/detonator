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

#pragma once

#include "config.h"

#include <string>
#include <vector>
#include <memory>
#include <map>

#include "base/assert.h"
#include "bitmap.h"

namespace gfx
{
    // Shape a string of text into a series of glyphs
    // with relative positioning to the (imaginary) baseline
    // and then rasterize them into a CPU based buffer.
    class TextBuffer
    {
    public:
        // Construct a text object with the given font font size and content.
        TextBuffer(const std::string& font, const std::string& text, float font_size);
       
        // Get handle to the rasterized text.
        const Bitmap<Grayscale>& GetBitmap() const 
        { return mBitmap; }

        // get the width (length) of the text in the buffer 
        // in pixels
        const float GetLineWidth() const 
        { return mWidth; }

        // get the height of the text in the buffer. 
        // this is the distance between the minimum point (of any glyph)
        // below the baseline and the maximum point (of any glyph) above
        // the baseline.
        const float GetLineHeight() const 
        { return mHeight; }

    private:
        struct FontLibrary;
        std::shared_ptr<FontLibrary> mFreetype;
        static std::weak_ptr<FontLibrary> Freetype;

    private:
        float mWidth  = 0;
        float mHeight = 0;

        // rasterized text
        Bitmap<Grayscale> mBitmap;
    };

} // namespace
