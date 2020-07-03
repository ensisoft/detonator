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
        TextBuffer(unsigned width, unsigned height)
          : mWidth(width)
          , mHeight(height)
        {}
        // Construct an empty text buffer without any dimensions.
        // The buffer needs to be resized before the content can
        // be rasterized.
        TextBuffer() = default;

        // get the width (length) of the buffer
        unsigned GetWidth() const
        { return mWidth; }

        // get the height of the buffer.
        unsigned GetHeight() const
        { return mHeight; }

        // Set the new text buffer size.
        void SetSize(unsigned width, unsigned height)
        {
            mWidth = width;
            mHeight = height;
        }

        // Rasterize the text buffer contents into a bitmap
        std::shared_ptr<Bitmap<Grayscale>> Rasterize() const;

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

        // TextStyle object can be used to set some text styling properties
        // such as the line height scaling factor, underline and text
        // alignment.
        // Some general notes about text styling:
        // Common "styling" options such as Italic and Bold text are normally
        // variations of the "Regular" font. Therefore this API provides no
        // facilities for dealing with "bold" or "italic" text. Simply use
        // the appropriate font file when adding text to the TextBuffer.
        class TextStyle
        {
        public:
            // Set underlining on/off.
            TextStyle& SetUnderline(bool underline)
            {
                mUnderline = underline;
                return *this;
            }
            // Set text horizontal alignment wrt TextBuffer bounds.
            TextStyle& SetAlign(HorizontalAlignment align)
            {
                mHAlignment = align;
                return *this;
            }
            // Set text vertical alignment wrt TextBuffer bounds.
            TextStyle& SetAlign(VerticalAlignment align)
            {
                mVAlignment = align;
                return *this;
            }
            // Set text font size.
            TextStyle& SetFontsize(unsigned fontsize)
            {
                mFontsize = fontsize;
                return *this;
            }
            // Set the line height scaling factor. Default is 1.0
            // which uses the line height from the font unscaled.
            TextStyle& SetLineHeight(float height)
            {
                mLineHeight = height;
                return *this;
            }
        private:
            float mLineHeight = 1.0f;
            bool mUnderline = false;
            unsigned mFontsize = 0;
            HorizontalAlignment mHAlignment = HorizontalAlignment::AlignCenter;
            VerticalAlignment mVAlignment = VerticalAlignment::AlignCenter;
            friend class TextBuffer;
        };

        // Add text to the buffer and position automatically wrt TextBuffer bounds.
        // Returns a TextStyle object which can be used to set individual
        // text style properties.
        TextStyle& AddText(const std::string& text, const std::string& font, unsigned font_size_px);

        void AddText(const std::string& text, const std::string& font, const TextStyle& style);

        // Compute hash of the contents of the string buffer.
        std::size_t GetHash() const;

    private:
        struct FontLibrary;
        mutable std::shared_ptr<FontLibrary> mFreetype;
        static std::weak_ptr<FontLibrary> Freetype;
        struct Text;
        std::shared_ptr<Bitmap<Grayscale>> Rasterize(
            const std::string& text,
            const std::string& font,
            const TextStyle& style) const;

    private:
        unsigned mWidth  = 0;
        unsigned mHeight = 0;

        struct Text {
            std::string text;
            std::string font;
            TextStyle style;
        };
        std::vector<Text> mText;
    };

} // namespace
