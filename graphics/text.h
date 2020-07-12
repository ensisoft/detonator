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

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <string>
#include <vector>
#include <memory>
#include <optional>

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

        // Some general notes about text styling:
        // Common "styling" options such as Italic and Bold text are normally
        // variations of the "Regular" font. Therefore this API provides no
        // facilities for dealing with "bold" or "italic" text. Simply use
        // the appropriate font file when adding text to the TextBuffer.
        // block of text that is to be rasterized in the buffer.

        // A block of text with some particular formatting and font setting.
        // Note that the text may contain new lines in which case the content
        // (the text string) is split to multiple lines.
        struct Text {
            // The actual text string. UTF-8 encoded.
            std::string text;

            // The font that will be used.
            // This is a filepath to some particular font file.
            std::string font;
            // The style options that apply to the text;

            // Font size (in pixels)
            unsigned fontsize = 0;

            // the lineheight multiplier that is used to compute the
            // actual text lineheight which is used to advance
            // from one line to line in the buffer when rasterizing
            // lines of text
            float lineheight = 1.0f;

            // Text underline flag.
            bool underline = false;

            // horizontal text alignment with respect to the rasterized buffer.
            HorizontalAlignment halign = HorizontalAlignment::AlignCenter;

            // vertical text alignment with respect to the rasterized buffer.
            VerticalAlignment valign = VerticalAlignment::AlignCenter;
        };

        // Add text to the buffer.
        void AddText(const std::string& text, const std::string& font, unsigned font_size_px)
        {
            Text t;
            t.text = text;
            t.font = font;
            t.fontsize = font_size_px;
            mText.push_back(t);
        }

        // Add text to the buffer for rasterization.
        void AddText(const Text& text)
        {
            mText.push_back(text);
        }

        // add text to the buffer for rasterization.
        void AddText(Text&& text)
        {
            mText.push_back(std::move(text));
        }

        // Clear all texts from the text buffer.
        void ClearText()
        {
            mText.clear();
        }

        // Get the number of text objects currently in the text buffer.
        size_t GetNumTexts() const
        { return mText.size(); }

        // Get the text blob at the given index
        const Text& GetText(size_t index) const
        {
            ASSERT(index < mText.size());
            return mText[index];
        }
        // Get the text blob at the given index.
        Text& GetText(size_t index)
        {
            ASSERT(index < mText.size());
            return mText[index];
        }
        // Returns true if the text buffer contains now text objects
        // otherwise false.
        bool IsEmpty() const
        { return mText.empty(); }

        // Compute hash of the contents of the string buffer.
        std::size_t GetHash() const;

        // Serialize the contents into JSON
        nlohmann::json ToJson() const;

        // Load a TextBuffer from JSON.
        static std::optional<TextBuffer> FromJson(const nlohmann::json& json);

    private:
        struct FontLibrary;
        mutable std::shared_ptr<FontLibrary> mFreetype;
        static std::weak_ptr<FontLibrary> Freetype;

        std::shared_ptr<Bitmap<Grayscale>> Rasterize(const std::string& text, const Text& style) const;

    private:
        unsigned mWidth  = 0;
        unsigned mHeight = 0;

        std::vector<Text> mText;
    };

} // namespace
