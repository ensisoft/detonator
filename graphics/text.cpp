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

#include "config.h"

#include "warnpush.h"
#  include <hb.h>
#  include <hb-ft.h>

#  include <ft2build.h>
#  include FT_FREETYPE_H
#  include FT_SIZES_H
#include "warnpop.h"

#include <functional>
#include <stdexcept>
#include <sstream>
#include <map>

#include "base/logging.h"
#include "base/utility.h"
#include "base/json.h"
#include "graphics/resource.h"
#include "graphics/text.h"

// some good information here about text rendering
// https://gankra.github.io/blah/text-hates-you/

/*
Scalar:     A Unicode Scalar, the "smallest unit" unicode describes (AKA a code point).
Character:  A Unicode Extended Grapheme Cluster (EGC), the "biggest unit" unicode describes
            (potentially composed of multiple scalars).
Glyph:      An atomic unit of rendering yielded by the font. Generally this will have a unique ID in the font.
Ligature:   A glyph that is made up of several scalars, and potentially even several characters
            (native speakers may or may not think of a ligature as multiple "characters", but to the font it's
            just one "character").
Emoji:      A "full color" glyph. 🙈🙉🙊
Font:       A document that maps characters to glyphs.
Script:     The set of glyphs that make up some language (fonts tend to implement particular scripts).
Cursive     Script: Any script where glyphs touch and flow into each other (like Arabic).
Color:      RGB and Alpha values for fonts (alpha isn't needed for some usecases, but it's interesting).
Style:      Bold and Italics modifiers for fonts (hinting, aliasing, and other settings tend to also get
            crammed in here in practical implementations).
*/

namespace {
// RAII type for initializing and freeing the Freetype library
struct FontLibrary {
    FT_Library library;
    FontLibrary(const FontLibrary&) = delete;
    FontLibrary& operator=(const FontLibrary&) = delete;
    FontLibrary()
    {
        if (FT_Init_FreeType(&library))
            throw std::runtime_error("FT_Init_FreeType failed");
        DEBUG("Initialized FreeType");
    }
   ~FontLibrary()
    {
        FT_Done_FreeType(library);
        //DEBUG("Done with FreeType");
    }
};
} // namespace

namespace gfx
{

void TextBuffer::SetBufferSize(unsigned int width, unsigned int height)
{
    mBufferWidth  = width;
    mBufferHeight = height;
}

std::shared_ptr<Bitmap<Grayscale>> TextBuffer::Rasterize() const
{
    std::vector<std::shared_ptr<Bitmap<Grayscale>>> line_bitmaps;
    int text_height = 0;
    int text_width = 0;
    // rasterize the lines and accumulate the metrics for the
    // height of all the text blocks and the maximum line width.
    for (const auto& text : mText)
    {
        std::stringstream ss(text.text);
        std::string line;
        // rasterize each line
        while (std::getline(ss, line))
        {
            // if there's a new line without content what's the height of the line ?
            // we could probably solve this by looking at some font metrics
            // but for now we're just going to rasterize some character with the font
            // and font size setting and use the height of that as the height of an empty line.
            const bool was_empty = line.empty();
            if (line.empty())
                line = "k";

            auto bmp = Rasterize(line, text);
            if (was_empty)
                bmp->Fill(Grayscale{0});

            text_height += bmp->GetHeight();
            text_width = std::max(text_width, (int)bmp->GetWidth());
            line_bitmaps.push_back(std::move(bmp));
        }
    }
    // if we have expected text buffer dimensions set
    // then honor those, otherwise base the size on the
    // metrics from above.
    const auto width = mBufferWidth ? (int)mBufferWidth : text_width;
    const auto height = mBufferHeight ? (int)mBufferHeight : text_height;

    // the repeated allocation bitmaps for rasterizing content is
    // actually more expensive than the actual rasterization.
    // we can keep a small cache of frequently used bitmap sizes.
    static std::map<std::uint64_t,
            std::shared_ptr<gfx::Bitmap<gfx::Grayscale>>> cache;

    std::shared_ptr<Bitmap<Grayscale>> out;

    const std::uint64_t key = ((uint64_t)width << 32) | height;
    auto it = cache.find(key);
    if (it == std::end(cache))
    {
        out = std::make_shared<Bitmap<Grayscale>>(width, height);
        cache[key] = out;
    }
    else if (it->second.use_count() == 1)
    {
        // the cached bitmap can only be shared when there's nobody
        // else using it at the moment. in other words the only shared_ptr
        // pointing to it is the shared_ptr in the cache.
        // note that the use_count is *NOT* thread safe or correct in
        // multi-threaded application. However in this particular case
        // that should not be a problem per-se since the whole cache itself
        // is not MT safe atm.
        out = it->second;
        // clear it so it's ready for re-use
        out->Fill(Grayscale(0));
    }
    else
    {
        // make a private copy. this cannot be added to the
        // cache since there already exists a bitmap with the
        // same key. would require having multiple items
        // with same key and more complicated caching.
        // the expectation currently is that the text raster buffers
        // are used to upload to the GPU and then discarded and not
        // being held onto.
        out = std::make_shared<Bitmap<Grayscale>>(width, height);
    }

    int ypos = 0;
    int xpos = 0;
    if (mVerticalAlign == VerticalAlignment::AlignTop)
        ypos = 0;
    else if (mVerticalAlign == VerticalAlignment::AlignCenter)
        ypos = (height - text_height) / 2;
    else if (mVerticalAlign == VerticalAlignment::AlignBottom)
        ypos = height - text_height;

    for (const auto& bmp : line_bitmaps)
    {
        xpos = 0;
        if (mHorizontalAlign == HorizontalAlignment::AlignLeft)
            xpos = 0;
        else if (mHorizontalAlign == HorizontalAlignment::AlignCenter)
            xpos = (width - (int)bmp->GetWidth()) / 2;
        else if (mHorizontalAlign == HorizontalAlignment::AlignRight)
            xpos = width - (int)bmp->GetWidth();

        out->Copy(xpos, ypos, *bmp);
        ypos += bmp->GetHeight();
    }

    // for debugging purposes dump the rasterized bitmap as .ppm file
#if 0
    Bitmap<RGB> tmp;
    tmp.Resize(out->GetWidth(), out->GetHeight());
    tmp.Copy(0, 0, *out);
    WritePPM(tmp, "/tmp/text-buffer.ppm");
    DEBUG("Wrote rasterized text buffer in '%1'", "/tmp/text-buffer.ppm");
#endif

    return out;
}

std::size_t TextBuffer::GetHash() const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mBufferWidth);
    hash = base::hash_combine(hash, mBufferHeight);
    hash = base::hash_combine(hash, mVerticalAlign);
    hash = base::hash_combine(hash, mHorizontalAlign);
    for (const auto& t : mText)
    {
        hash = base::hash_combine(hash, t.text);
        hash = base::hash_combine(hash, t.font);
        // have the style properties also contribute to the hash so that
        // text buffer with similar text content but different properties
        // generates a different hash.
        hash = base::hash_combine(hash, (unsigned)t.lineheight);
        hash = base::hash_combine(hash, (unsigned)t.fontsize);
        hash = base::hash_combine(hash, (unsigned)t.underline);
    }
    return hash;
}

nlohmann::json TextBuffer::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "width", mBufferWidth);
    base::JsonWrite(json, "height", mBufferHeight);
    base::JsonWrite(json, "horizontal_alignment", mHorizontalAlign);
    base::JsonWrite(json, "vertical_alignment", mVerticalAlign);
    for (const auto& text : mText)
    {
        nlohmann::json js;
        base::JsonWrite(js, "string", text.text);
        base::JsonWrite(js, "font_file", text.font);
        base::JsonWrite(js, "font_size", text.fontsize);
        base::JsonWrite(js, "line_height", text.lineheight);
        base::JsonWrite(js, "underline", text.underline);
        json["texts"].push_back(js);
    }
    return json;
}

// static
std::optional<TextBuffer> TextBuffer::FromJson(const nlohmann::json& json)
{
    TextBuffer buffer;
    if (!base::JsonReadSafe(json, "width", &buffer.mBufferWidth) ||
        !base::JsonReadSafe(json, "height", &buffer.mBufferHeight) ||
        !base::JsonReadSafe(json, "horizontal_alignment", &buffer.mHorizontalAlign) ||
        !base::JsonReadSafe(json, "vertical_alignment", &buffer.mVerticalAlign))
        return std::nullopt;

    if (!json.contains("texts"))
        return buffer;

    for (const auto& json_text : json["texts"].items())
    {
        const auto& js = json_text.value();
        Text t;
        if (!base::JsonReadSafe(js, "string", &t.text) ||
            !base::JsonReadSafe(js, "font_file", &t.font) ||
            !base::JsonReadSafe(js, "font_size", &t.fontsize) ||
            !base::JsonReadSafe(js, "line_height", &t.lineheight) |
            !base::JsonReadSafe(js, "underline", &t.underline))
            return std::nullopt;
        buffer.mText.push_back(std::move(t));
    }
    return buffer;
}

std::shared_ptr<Bitmap<Grayscale>> TextBuffer::Rasterize(const std::string& line, const Text& text) const
{
    // FreeType 2 uses size objects to model all information related to a given character
    // size for a given face. For example, a size object holds the value of certain metrics
    // like the ascender or text height, expressed in 1/64th of a pixel, for a character
    // size of 12 points (however, those values are rounded to integers, i.e., multiples of 64).
    // https://www.freetype.org/freetype2/docs/tutorial/step1.html
    const auto FUCKING_MAGIC_SCALE = 64;

    static FontLibrary freetype;

    FT_Face face;

    // todo: refactor away the exceptions.

    const auto& font_buff = gfx::LoadResource(text.font);
    if (!font_buff)
        throw std::runtime_error("Failed to load font file: " + text.font);

    if (FT_New_Memory_Face(freetype.library, (const FT_Byte*)font_buff->GetData(), font_buff->GetSize(),0, &face))
        throw std::runtime_error("Failed to load font file: " + text.font);
    auto face_raii = base::MakeUniqueHandle(face, FT_Done_Face);

    if (FT_Select_Charmap(face, FT_ENCODING_UNICODE))
        throw std::runtime_error("Font doesn't support Unicode");

    if (FT_Set_Pixel_Sizes(face, 0, text.fontsize))
        throw std::runtime_error("Font doesn't support pixel size: " + std::to_string(text.fontsize));
    //if (FT_Set_Char_Size(face, font_size * FUCKING_MAGIC_SCALE, font_size * FUCKING_MAGIC_SCALE, 0, 0))
    //    throw std::runtime_error("Font doesn't support pixel size: " + std::to_string(font_size));

    // simple example for harfbuzz is here
    // https://github.com/harfbuzz/harfbuzz-tutorial/blob/master/hello-harfbuzz-freetype.c
    hb_font_t* hb_font = hb_ft_font_create(face, nullptr);
    hb_buffer_t* hb_buff = hb_buffer_create();
    hb_buffer_add_utf8(hb_buff, line.c_str(),
        -1,  // NUL terminated string
         0, // offset of the first character to add to the buffer
        -1 // number of characters to add to the buffer or -1 for "until the end of text"
    );
    hb_buffer_set_direction(hb_buff, HB_DIRECTION_LTR);
    hb_buffer_set_script(hb_buff, HB_SCRIPT_LATIN);
    hb_buffer_set_language(hb_buff, hb_language_from_string("en", -1));
    hb_shape(hb_font, hb_buff, nullptr, 0);

    struct GlyphRasterInfo {
        unsigned width  = 0;
        unsigned height = 0;
        int bearing_x = 0;
        int bearing_y = 0;
        Bitmap<Grayscale> bitmap;
    };

    std::map<unsigned, GlyphRasterInfo> glyph_raster_info;

    // rasterize the required glyphs
    // https://www.freetype.org/freetype2/docs/glyphs/glyphs-3.html

    // the distance from the baseline to the highest or upper grid coordinate
    // used to place an outline point. It's a positive value due to the grid's
    // orientation with the Y axis upwards.
    int ascent = 0;
    // the distance from the baseline to the lowest grid coordinate to used to
    // place an outline point. It's a negative value due to the grid's
    // orientation with the Y axis upwards.
    int descent = 0;

    // pen position
    int pen_x = 0;
    int pen_y = 0;

    // the extents of the text block when rasterized
    unsigned height = 0;
    unsigned width  = 0;

    const auto glyph_count = hb_buffer_get_length(hb_buff);
    hb_glyph_info_t* glyph_info = hb_buffer_get_glyph_infos(hb_buff, nullptr);
    hb_glyph_position_t* glyph_pos = hb_buffer_get_glyph_positions(hb_buff, nullptr);
    for (unsigned i=0; i<glyph_count; ++i)
    {
        const auto codepoint = glyph_info[i].codepoint;
        auto it = glyph_raster_info.find(codepoint);
        if (it == std::end(glyph_raster_info))
        {
            // load/rasterize the glyph we don't already have
            FT_Load_Glyph(face, codepoint, FT_LOAD_DEFAULT);
            FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
            FT_GlyphSlot slot = face->glyph;

            // copy into our buffer
            Bitmap<Grayscale> bmp(reinterpret_cast<const Grayscale*>(slot->bitmap.buffer),
                slot->bitmap.width,
                slot->bitmap.rows,
                slot->bitmap.pitch);

            GlyphRasterInfo info;
            info.width  = slot->bitmap.width;
            info.height = slot->bitmap.rows;
            // bearing X (left side bearing) is the horizontal distance from the current pen position
            // to the glyph's left edge (the left edge of its bounding box)
            info.bearing_x = slot->bitmap_left;
            // bearing Y (top side bearing) is the vertical distance from the baseline to the top of glyph
            // (to the top of its bounding box)
            info.bearing_y = slot->bitmap_top;
            info.bitmap    = std::move(bmp);
            it = glyph_raster_info.insert(std::make_pair(codepoint, std::move(info))).first;
        }
        const auto& info = it->second;

        // compute the extents of the text i.e. the required height and width
        // of the bitmap into which to composite the glyphs

        const int xa = glyph_pos[i].x_advance / FUCKING_MAGIC_SCALE;
        const int ya = glyph_pos[i].y_advance / FUCKING_MAGIC_SCALE;
        // the x and y offsets from harfbuzz seem to be just for modifying
        // the x and y offsets (bearings) from freetype
        const int xo = glyph_pos[i].x_offset / FUCKING_MAGIC_SCALE;
        const int yo = glyph_pos[i].y_offset / FUCKING_MAGIC_SCALE;

        // this is the glyph top left corner relative to the imaginary baseline
        // where the baseline is at y=0 and y grows up
        const int x = pen_x + info.bearing_x + xo;
        const int y = pen_y + info.bearing_y + yo;

        const int glyph_top = y;
        const int glyph_bot = y - info.height;

        ascent  = std::max(ascent, glyph_top);
        descent = std::min(descent, glyph_bot);

        height = ascent - descent; // todo: + linegap (where to find linegap?)
        width  = x + info.width;

        pen_x += xa;
        pen_y += ya;
    }

    // offset to the baseline. if negative then it's below the baseline
    // if positive it's above the baseline.
    const auto underline_position  = face->underline_position / FUCKING_MAGIC_SCALE;
    // vertical thickness of the underline.. units ??
    const auto underline_thickness = 2; // face->underline_thickness ? face->underline_thickness : 1;
    const auto line_spacing = (face->size->metrics.height / FUCKING_MAGIC_SCALE) * text.lineheight;
    const auto margin = line_spacing > height ? line_spacing - height : 0;

    height += margin;

    // the repeated allocation bitmaps for rasterizing content is
    // actually more expensive than the actual rasterization.
    // we can keep a small cache of frequently used bitmap sizes.
    static std::map<std::uint64_t,
        std::shared_ptr<gfx::Bitmap<gfx::Grayscale>>> cache;

    std::shared_ptr<Bitmap<Grayscale>> bmp;

    const std::uint64_t key = ((uint64_t)width << 32) | height;
    auto it = cache.find(key);
    if (it == std::end(cache))
    {
        bmp = std::make_shared<Bitmap<Grayscale>>(width, height);
        cache[key] = bmp;
    }
    else if (it->second.use_count() == 1)
    {
        bmp = it->second;
        bmp->Fill(Grayscale(0));
    }
    else
    {
        bmp = std::make_shared<Bitmap<Grayscale>>(width, height);
    }

    // the bitmap has 0,0 at top left and y grows down.
    //
    // 0,0 ____________________
    //     |                  | ascent (above baseline)
    //     |  ---baseline---  |
    //     |__________________| descent (below baseline)
    //
    const auto baseline = ascent + (margin / 2);

    // finally compose the glyphs into a text buffer starting at the current pen position
    pen_x = 0;
    pen_y = 0;

    for (unsigned i=0; i<glyph_count; ++i)
    {
        const auto codepoint = glyph_info[i].codepoint;
        const auto& info = glyph_raster_info[codepoint];

        // advances tell us how much to move the pen in x/y direction for the next glyph
        const int xa = glyph_pos[i].x_advance / FUCKING_MAGIC_SCALE;
        const int ya = glyph_pos[i].y_advance / FUCKING_MAGIC_SCALE;

        // offsets tell us how the glyph should be offset wrt the current pen position
        const int xo = glyph_pos[i].x_offset / FUCKING_MAGIC_SCALE;
        const int yo = glyph_pos[i].y_offset / FUCKING_MAGIC_SCALE;

        const int x = pen_x + info.bearing_x + xo;
        const int y = pen_y + info.bearing_y + yo;

        bmp->Copy(x, baseline - y, info.bitmap);

        pen_x += xa;
        pen_y += ya;
    }

    if (text.underline)
    {
        const auto width = bmp->GetWidth();
        const URect underline(0, baseline + underline_position,
            width, underline_thickness);
        bmp->Fill(underline, gfx::Grayscale(0xff));
    }

    hb_font_destroy(hb_font);
    hb_buffer_destroy(hb_buff);
    return bmp;
}

}  //namespace
