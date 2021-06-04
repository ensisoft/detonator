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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <nlohmann/json_fwd.hpp>
#include "warnpop.h"

#include <string>
#include <vector>
#include <memory>
#include <optional>

#include "base/assert.h"
#include "graphics/bitmap.h"

namespace gfx
{
    // Shape a string of text into a series of glyphs
    // with relative positioning to the (imaginary) baseline
    // and then rasterize them into a CPU based buffer.
    class TextBuffer
    {
    public:
        // Construct the text buffer with given buffer dimensions.
        // The dimensions are used when aligning and positioning
        // the rasterized text in the buffer.
        // the units are pixels. 
        TextBuffer(unsigned width, unsigned height)
          : mBufferWidth(width)
          , mBufferHeight(height)
        {}
        // Construct an empty text buffer without any dimensions.
        // The buffer needs to be resized before the content can
        // be rasterized.
        TextBuffer() = default;

        // Get the width of the raster buffer if set. This is the
        // static dimension when requesting a fixed size buffer.
        unsigned GetBufferWidth() const
        { return mBufferWidth; }
        // get the height of the buffer.
        unsigned GetBufferHeight() const
        { return mBufferHeight; }
        // Set the new text buffer size.
        void SetBufferSize(unsigned width, unsigned height);

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
        void SetAlignment(HorizontalAlignment align)
        { mHorizontalAlign = align; }
        void SetAlignment(VerticalAlignment align)
        { mVerticalAlign = align; }
        HorizontalAlignment GetHorizontalAligment() const
        { return mHorizontalAlign; }
        VerticalAlignment GetVerticalAlignment() const
        { return mVerticalAlign; }

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
        std::shared_ptr<Bitmap<Grayscale>> Rasterize(const std::string& text, const Text& style) const;

    private:
        // static raster buffer (bitmap) width or 0 if size to content is wanted.
        unsigned mBufferWidth  = 0;
        // static buffer buffer (bitmap) height or 0 if size to content is wanted.
        unsigned mBufferHeight = 0;
        // horizontal text alignment with respect to the rasterized buffer.
        HorizontalAlignment mHorizontalAlign = HorizontalAlignment::AlignCenter;
        // vertical text alignment with respect to the rasterized buffer.
        VerticalAlignment mVerticalAlign = VerticalAlignment::AlignCenter;
        std::vector<Text> mText;
    };

} // namespace
