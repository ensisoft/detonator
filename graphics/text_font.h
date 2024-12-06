// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#include <string>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace gfx
{
    class Device;
    class Texture;

    // Glyphs are pre-rendered offline in some image editor tool
    // and packed into a texture. JSON meta file describes the
    // glyphs and (optionally) kerning pairs. Appropriate data
    // files can be produced by the Editor's image packer.
    class BitmapFontGlyphPack
    {
    public:
        struct Glyph {
            unsigned short px_width  = 0;
            unsigned short px_height = 0;
            float width  = 0;
            float height = 0;
            float xpos   = 0;
            float ypos   = 0;
        };

        inline unsigned GetFontWidth() const noexcept
        { return mFontWidth; }
        inline unsigned GetFontHeight() const noexcept
        { return mFontHeight; }
        inline unsigned GetTextureWidth() const noexcept
        { return mTextureWidth; }
        inline unsigned GetTextureHeight() const noexcept
        { return mTextureHeight; }
        inline std::string GetTextureFile() const noexcept
        { return mTextureFile; }
        inline std::string GetFontUri() const noexcept
        { return mFontUri; }
        inline bool IsValid() const
        { return mValid; }

        bool ParseFont(const std::string& uri);

        const Glyph* FindGlyph(uint32_t character) const;
    private:
        std::unordered_map<uint32_t, Glyph> mGlyphs;
        std::string mFontUri;
        std::string mTextureFile;
        unsigned mTextureWidth  = 0;
        unsigned mTextureHeight = 0;
        unsigned mFontHeight    = 0;
        unsigned mFontWidth     = 0;
        bool mValid = false;
        bool mCaseSensitive = true;
        bool mPremulAlpha   = false;
    };

} // namespace