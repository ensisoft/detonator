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

#include "config.h"

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <cwctype>

#include "base/logging.h"
#include "base/json.h"
#include "base/utility.h"
#include "graphics/text_font.h"
#include "graphics/loader.h"

namespace gfx
{

bool BitmapFontGlyphPack::ParseFont(const std::string& uri)
{
    gfx::Loader::ResourceDesc desc;
    desc.type = gfx::Loader::Type::Font;
    desc.uri  = uri;
    const auto& fontbuff = gfx::LoadResource(desc);
    if (!fontbuff || !fontbuff->GetByteSize())
        ERROR_RETURN(false, "Failed to load font file. [file='%1']", uri);

    const char* beg = (const char*)fontbuff->GetData();
    const char* end = (const char*)fontbuff->GetData() + fontbuff->GetByteSize();
    const auto& [success, json, error] = base::JsonParse(beg, end);
    if (!success)
        ERROR_RETURN(false, "Failed to parse font JSON. [file='%1']", uri);

    unsigned texture_width  = 0;
    unsigned texture_height = 0;
    unsigned font_width  = 0;
    unsigned font_height = 0;
    std::string texture_file;

    if (!base::JsonReadSafe(json, "image_width",  &texture_width))
        ERROR_RETURN(false, "Bitmap font is missing 'image_width' attribute. [file='%1']", uri);
    if (!base::JsonReadSafe(json, "image_height", &texture_height))
        ERROR_RETURN(false, "Bitmap font is missing 'image_height' attribute. [file='%1']", uri);
    if (!base::JsonReadSafe(json, "image_file",   &texture_file))
        ERROR_RETURN(false, "Bitmap font is missing 'image_file' attribute. [file='%1']", uri);

    bool premultiply_alpha_hint = false;
    bool case_sensitive = true;

    if (!base::JsonReadSafe(json, "font_width", &font_width))
        WARN("Bitmap font is missing 'font_width' attribute.[file='%1']", uri);
    if (!base::JsonReadSafe(json, "font_height", &font_height))
        WARN("Bitmap font is missing 'font_height' attribute.[file='%1']", uri);
    if (!base::JsonReadSafe(json, "premultiply_alpha_hint", &premultiply_alpha_hint))
        WARN("Bitmap font is missing 'premultiply_alpha_hint' attribute. [file='%1']", uri);
    if (!base::JsonReadSafe(json, "case_sensitive", &case_sensitive))
        WARN("Bitmap font is missing 'case_sensitive' attribute. [file='%1']", uri);

    std::unordered_map<uint32_t, Glyph> glyphs;
    for (const auto& item : json["images"].items())
    {
        const auto& img_json = item.value();
        std::string utf8_char_string;
        Glyph glyph;
        unsigned width  = font_width;
        unsigned height = font_height;
        unsigned xpos   = 0;
        unsigned ypos   = 0;
        bool success    = false;
        if (!base::JsonReadSafe(img_json, "char", &utf8_char_string))
            WARN("Font glyph is missing 'char' attribute. [file='%1]", uri);
        else if (!base::JsonReadSafe(img_json, "xpos", &xpos))
            WARN("Font glyph is missing 'xpos' attribute. [file='%1']", uri);
        else if (!base::JsonReadSafe(img_json, "ypos", &ypos))
            WARN("Font glyph is missing 'ypos' attribute. [file='%1']", uri);
        else if (!base::JsonReadSafe(img_json, "width",  &width) && !font_width)
            WARN("Font glyph is missing 'width' attribute. [file='%1']", uri);
        else if (!base::JsonReadSafe(img_json, "height",  &width) && !font_height)
            WARN("Font glyph is missing 'height' attribute. [file='%1']", uri);
        else success = true;

        if (!success)
            continue;

        const auto& wide_char_string = base::FromUtf8(utf8_char_string);
        if (wide_char_string.empty())
            continue;

        glyph.px_width  = width;
        glyph.px_height = height;
        glyph.width  = (float)width / (float)texture_width;
        glyph.height = (float)height / (float)texture_height;
        glyph.xpos   = (float)xpos / (float)texture_width;
        glyph.ypos   = (float)ypos / (float)texture_height;
        mCaseSensitive = case_sensitive;
        mPremulAlpha   = premultiply_alpha_hint;
        // taking only the first character of the string into account.
        glyphs[wide_char_string[0]] = std::move(glyph);
    }
    mTextureHeight = texture_height;
    mTextureWidth  = texture_width;
    mFontWidth     = font_width;
    mFontHeight    = font_height;
    mTextureFile   = std::move(texture_file);
    mGlyphs        = std::move(glyphs);
    mFontUri       = uri;
    mValid         = true;
    DEBUG("Loaded bitmap font JSON. [file='%1']", uri);
    return true;
}

const BitmapFontGlyphPack::Glyph* BitmapFontGlyphPack::FindGlyph(uint32_t character) const
{
    auto* ret = base::SafeFind(mGlyphs, character);
    if (ret)
        return ret;
    else if (!mCaseSensitive && std::iswlower(character))
        return base::SafeFind(mGlyphs, (uint32_t)std::towupper(character));
    else if (!mCaseSensitive && std::iswupper(character))
        return base::SafeFind(mGlyphs, (uint32_t)std::towlower(character));
    return nullptr;
}

} // namespace
