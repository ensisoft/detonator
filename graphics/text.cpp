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

#include "warnpush.h"
#  include <hb.h>
#  include <hb-ft.h>

#  include <ft2build.h>
#  include FT_FREETYPE_H
#  include FT_SIZES_H
#include "warnpop.h"

#include <stdexcept>
#include <sstream>

#include "base/logging.h"
#include "base/utility.h"
#include "text.h"

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

namespace gfx 
{

// RAII type for initializing and freeing the Freetype library
struct TextBuffer::FontLibrary {
    FT_Library library;
    FontLibrary(const FontLibrary&) = delete;
    FontLibrary& operator=(const FontLibrary&) = delete;
    FontLibrary()
    {
        if (FT_Init_FreeType(&library))
            throw std::runtime_error("FT_Init_FreeType failed");
        //DEBUG("Initialized FreeType");
    }
   ~FontLibrary()
    {
        FT_Done_FreeType(library);
        //DEBUG("Done with FreeType");
    }
};
std::weak_ptr<TextBuffer::FontLibrary> TextBuffer::Freetype;


TextBuffer::TextBuffer(const std::string& font, const std::string& text, float font_size)
{
    // this is some fucking weird magic scaling factor, not sure
    // if anyone really knows what the fuck it's supposed to do.
    // but I've seen it in several sources using these dogshit libs (harfbuzz and freetype)
    // https://stackoverflow.com/questions/50292283/units-used-by-hb-position-t-in-harfbuzz
    const auto FUCKING_MAGIC_SCALE = 64;

    mFreetype = Freetype.lock();
    if (!mFreetype) 
    {
        mFreetype = std::make_shared<TextBuffer::FontLibrary>();
        Freetype  = mFreetype;
    }
    FT_Face face;

    if (FT_New_Face(mFreetype->library, font.c_str(), 0, &face))
        throw std::runtime_error("Failed to load font file: " + font);
    auto face_raii = base::make_unique_ptr(face, FT_Done_Face);

    if (FT_Select_Charmap(face, FT_ENCODING_UNICODE))
        throw std::runtime_error("Font doesn't support Unicode");
    if (FT_Set_Char_Size(face, font_size * FUCKING_MAGIC_SCALE, font_size * FUCKING_MAGIC_SCALE, 0, 0))
        throw std::runtime_error("Font doesn't support pixel size: " + std::to_string(font_size));

    // simple example for harfbuzz is here
    // https://github.com/harfbuzz/harfbuzz-tutorial/blob/master/hello-harfbuzz-freetype.c
    hb_font_t* hb_font = hb_ft_font_create(face, nullptr);    
    hb_buffer_t* hb_buff = hb_buffer_create();
    hb_buffer_add_utf8(hb_buff, text.c_str(), 
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
        unsigned bearing_x = 0;
        unsigned bearing_y = 0;
        Bitmap<Grayscale> bitmap;
    };
        
    std::map<unsigned, GlyphRasterInfo> glyph_raster_info;

    // rasterize the required glyphs
    // https://www.freetype.org/freetype2/docs/glyphs/glyphs-3.html

    // the distance from the baseline to the highest or upper grid coordinate
    // used to place an outline point. It's a positive value due to the grid's
    // orientation with the Y axis upwards.
    float ascent = 0.0f;
    // the distance from the baseline to the lowest grid coordinate to used to
    // place an outline point. It's a negative value due to the grid's 
    // orientation with the Y axis upwards.
    float descent = 0.0f;

    float pen_x = 0.0f;
    float pen_y = 0.0f;

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
        // of the bitmap into which to composit the glyphs

        const float xa = glyph_pos[i].x_advance / FUCKING_MAGIC_SCALE;
        const float ya = glyph_pos[i].y_advance / FUCKING_MAGIC_SCALE;        
        // the x and y offsets from harfbuzz seem to be just for modifying
        // the x and y offsets (bearings) from freetype
        const float xo = glyph_pos[i].x_offset / FUCKING_MAGIC_SCALE;        
        const float yo = glyph_pos[i].y_offset / FUCKING_MAGIC_SCALE;        
        
        // this is the glyph top left corner relative to the imaginery baseline
        // where the baseline is at y=0 and y grows up
        const auto x = pen_x + info.bearing_x + xo;
        const auto y = pen_y + info.bearing_y + yo;
        
        const auto glyph_top = y;
        const auto glyph_bot = y - info.height;

        ascent  = std::max(ascent, glyph_top);
        descent = std::min(descent, glyph_bot);

        mHeight = ascent - descent; // todo: + linegap (where to find linegap?)
        mWidth  = x + info.width;

        pen_x += xa;
        pen_y += ya;
    }    
    
    mBitmap.Resize(mWidth, mHeight);

    // the bitmap has 0,0 at top left and y grows down.
    //
    // 0,0 ____________________
    //     |                  | ascent (above baseline)
    //     |  ---baseline---  |
    //     |__________________| descent (below baseline)
    //
    const auto baseline = ascent;

    // finally compose the glyphs into a text buffer starting at the current pen position
    pen_x = 0.0f;
    pen_y = 0.0f;

    for (unsigned i=0; i<glyph_count; ++i) 
    {
        const auto codepoint = glyph_info[i].codepoint;
        const auto& info = glyph_raster_info[codepoint];

        // advances tell us how much to move the pen in x/y direction for the next glyph
        const float xa = glyph_pos[i].x_advance / FUCKING_MAGIC_SCALE;
        const float ya = glyph_pos[i].y_advance / FUCKING_MAGIC_SCALE;

        // offsets tell us how the glyph should be offset wrt the current pen position
        const float xo = glyph_pos[i].x_offset / FUCKING_MAGIC_SCALE;
        const float yo = glyph_pos[i].y_offset / FUCKING_MAGIC_SCALE;

        const auto x = pen_x + info.bearing_x + xo;
        const auto y = pen_y + info.bearing_y + yo;

        mBitmap.Copy(x, baseline - y, info.bitmap);

        pen_x += xa;
        pen_y += ya; 
    }

    hb_font_destroy(hb_font);
    hb_buffer_destroy(hb_buff);

    // for debugging purposes dump the rasterized bitmap as .ppm file
#if 0
    Bitmap<RGB> tmp;
    tmp.Resize(mBitmap.GetWidth(), mBitmap.GetHeight());
    tmp.Copy(0, 0, mBitmap);
    WritePPM(tmp, "/tmp/text-buffer.ppm");
    DEBUG("Wrote rasterized text buffer in '%1'", "/tmp/text-buffer.ppm");
#endif    
}

}  //namespace
