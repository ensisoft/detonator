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
        // Construct the text buffer with given maximum
        // width and height dimensions. 
        // The dimensions are used when aligning and positioning
        // the rasterized text in the buffer.
        // the units are pixels. 
        TextBuffer(float width, float height)
          : mWidth(width)
          , mHeight(height)
        {}

        // get the width (length) of the buffer
        float GetWidth() const 
        { return mWidth; }

        // get the height of the buffer.
        float GetHeight() const 
        { return mHeight; }

        // Rasterize the text buffer contents into a bitmap
        Bitmap<Grayscale> Rasterize() const;

        // Add text relative to the top left origin 
        // of the TextBuffer. Origin is 0,0 and y growns down.
        void AddText(const std::string& text, const std::string& font,
            float font_size_pt,
            float xpos, float ypos);

        enum class HorizontalAlignment {
            AlignLeft,
            AlignCenter,
            AlignRight
        };
        enum class VerticalAlignment {
            AlignTop,
            AlignCenter,
            AlignBottom
        };

        // Add text to the buffer and position automatically 
        // relative to the TextBuffer origin. 
        void AddText(const std::string& text, const std::string& font, float font_size_pt,
            HorizontalAlignment ha = HorizontalAlignment::AlignCenter, 
            VerticalAlignment va = VerticalAlignment::AlignCenter);
    private:
        struct FontLibrary;
        mutable std::shared_ptr<FontLibrary> mFreetype;
        static std::weak_ptr<FontLibrary> Freetype;

        Bitmap<Grayscale> Rasterize(const std::string& text, const std::string& font, 
            float font_size_pt) const;

    private:
        const float mWidth  = 0;
        const float mHeight = 0;

        struct Text {
            std::string text;
            std::string font;
            float font_size = 0.0f;
            float xpos = 0.0f;
            float ypos = 0.0f;
            bool use_absolute_position = false;
            HorizontalAlignment ha;
            VerticalAlignment va;
        };
        std::vector<Text> mText;
    };

} // namespace
