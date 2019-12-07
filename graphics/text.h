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
        // This creates an "unnamed" TextBuffer which means
        // that the contents are always updated on the device
        // when it's being rendered. 
        TextBuffer(unsigned width, unsigned height)
          : mWidth(width)
          , mHeight(height)
        {}  

        // Like above except that gives a name to the text buffer.
        // When a buffer is named the contents are assumed to be static
        // and are only updated on the GPU if the buffer dimensions have
        // changed. 
        TextBuffer(const std::string& name,  unsigned width, unsigned height)
          : mName(name)
          , mWidth(width)
          , mHeight(height)
        {}

        // get the width (length) of the buffer
        unsigned GetWidth() const 
        { return mWidth; }

        // get the height of the buffer.
        unsigned GetHeight() const 
        { return mHeight; }

        // Rasterize the text buffer contents into a bitmap
        std::shared_ptr<Bitmap<Grayscale>> Rasterize() const;

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

        // An unnamed text buffer is considered dynamic
        // i.e. the contents are expected to change continuously 
        // A dynamic text buffer will get rasterized and uploaded
        // to the GPU every time it's rendered.
        bool IsDynamic() const 
        { return mName.empty(); }
        
        // Get the current buffer name (if any)
        // See comments in IsDynamic and in the ctor about
        // the semantic meaning of a buffer name.
        const std::string& GetName() const
        { return mName; }

    private:
        struct FontLibrary;
        mutable std::shared_ptr<FontLibrary> mFreetype;
        static std::weak_ptr<FontLibrary> Freetype;

        std::shared_ptr<Bitmap<Grayscale>> Rasterize(const std::string& text, 
            const std::string& font, float font_size_pt) const;

    private:
        const std::string mName;
        const unsigned mWidth  = 0;
        const unsigned mHeight = 0;

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
