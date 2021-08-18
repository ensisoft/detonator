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
#include <map>

#include "base/logging.h"
#include "base/utility.h"
#include "data/reader.h"
#include "data/writer.h"
#include "graphics/resource.h"
#include "graphics/loader.h"
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
// FreeType 2 uses size objects to model all information related to a given character
// size for a given face. For example, a size object holds the value of certain metrics
// like the ascender or text height, expressed in 1/64th of a pixel, for a character
// size of 12 points (however, those values are rounded to integers, i.e., multiples of 64).
// https://www.freetype.org/freetype2/docs/tutorial/step1.html
const auto EFFIN_MAGIC_SCALE = 64;

using gfx::Bitmap;
using gfx::Grayscale;

template<size_t Pool>
std::shared_ptr<Bitmap<Grayscale>> AllocateBitmap(unsigned width, unsigned height)
{
    // the repeated allocation bitmaps for rasterizing content is
    // actually more expensive than the actual rasterization.
    // we can keep a small cache of frequently used bitmap sizes.
    static std::unordered_map<std::uint64_t,
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
    return bmp;
}

struct LineRaster {
    // bitmap can be nullptr if the line was in fact
    // empty with no text in it. (no rasterization was done)
    std::shared_ptr<const Bitmap<Grayscale>> bitmap;
    // the position of the baseline within the bitmap
    // measured from the top of the bitmap.
    short baseline = 0;
};

struct TextBlock {
    int line_height = 0;
    std::vector<LineRaster> lines;
    gfx::TextBuffer::HorizontalAlignment halign;
    gfx::TextBuffer::VerticalAlignment valign;
};

struct TextComposite {
    int line_height = 0;
    int num_lines   = 0;
    std::shared_ptr<const Bitmap<Grayscale>> bitmap;
};

int AlignLine(int line_width, int block_width, gfx::TextBuffer::HorizontalAlignment alignment)
{
    if (alignment == gfx::TextBuffer::HorizontalAlignment::AlignCenter)
        return (block_width - line_width) / 2;
    else if (alignment == gfx::TextBuffer::HorizontalAlignment::AlignRight)
        return block_width - line_width;
    return 0;
}

// Composite multiple lines of text into a single bitmap. The biggest problem here
// is how to figure out the 1st baseline position. The baseline position must be
// fixed somehow so that if several text blocks with identical font settings are
// being displayed the text is vertically aligned on the screen when the objects
// displaying the text are vertically aligned.
TextComposite CompositeTextBlock(const TextBlock& block, FT_Face face)
{
    int block_width  = 0;
    for (const auto& line : block.lines)
        block_width = std::max(block_width, line.bitmap ? (int)line.bitmap->GetWidth() : 0);
    // reserve just enough vertical space to fit all the lines based on the
    // current line height setting. this is relevant when considering the possible
    // positions for the baselines.
    int block_height = block.lines.size() * block.line_height;

    auto bmp = AllocateBitmap<0>(block_width, block_height);

    // the problem here is to figure out where to put the first baseline.
    // freetype "documentation" mentions that the font metrics provide
    // ascender and descender values but unfortunately they're unreliable
    // and inconsistent between fonts. It seems that the the safest bet
    // would be to allocate 2 rows for each line of text and put the
    // baselines between rows but this has the problem that there might
    // be too much empty space above and below the first/last line of text
    // and this will make the final text output look weird when top/bottom
    // alignment is chosen.
    //
    // I really don't know how to solve this properly and reliably but for
    // now we're just going to assume that 75% of the line height above the
    // baseline is enough for the max glyph ascent and 25% below the baseline
    // is enough for max glyph descent. This reduces the amount of "empty"
    // pixels above and below text and lets the top/bottom aligned text get
    // closer to the text object's top/bottom borders (as is visually expected)
    // However one can expect this "solution" to fail for some fonts and indeed
    // with it does fail with for example AtariFontFullVersion.ttf. But the user
    // can fix this issue by adjusting the line height.

    // https://www.freetype.org/freetype2/docs/tutorial/step2.html
    int baseline = block.line_height * 0.75;

    for (const auto& line : block.lines)
    {
        if (line.bitmap)
        {
            const int left = AlignLine(line.bitmap->GetWidth(), block_width, block.halign);
            bmp->Copy(left, baseline-line.baseline, *line.bitmap);
        }
        baseline += block.line_height;
    }
    TextComposite ret;
    ret.line_height = block.line_height;
    ret.num_lines   = block.lines.size();
    ret.bitmap      = bmp;
    return ret;
}

// Rasterize and layout a row of glyphs on the a baseline in order to create a "line of text".
// this will (or at least tries to) properly account for the vertical ascent or descent
// (relative to the baseline) for each glyph. The returned value provides reference to the
// grayscale bitmap and also the position of the baseline within the bitmap so that the
// bitmap can be positioned correctly when composited. Keep in mind that using the size of the
// bitmap is not correct way to composite multiple lines since the sizes of the bitmaps can
// vary even when using same font settings.
LineRaster RasterizeLine(const std::string& line, const gfx::TextBuffer::Text& text, FT_Face face)
{
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

        const int xa = glyph_pos[i].x_advance / EFFIN_MAGIC_SCALE;
        const int ya = glyph_pos[i].y_advance / EFFIN_MAGIC_SCALE;
        // the x and y offsets from harfbuzz seem to be just for modifying
        // the x and y offsets (bearings) from freetype
        const int xo = glyph_pos[i].x_offset / EFFIN_MAGIC_SCALE;
        const int yo = glyph_pos[i].y_offset / EFFIN_MAGIC_SCALE;

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
    const auto underline_position  = face->underline_position / EFFIN_MAGIC_SCALE;
    // vertical thickness of the underline.. units ??
    const auto underline_thickness = 2; // face->underline_thickness ? face->underline_thickness : 1;
    //const auto line_spacing = (face->size->metrics.height / EFFIN_MAGIC_SCALE) * text.lineheight;
    //const auto margin = line_spacing > height ? line_spacing - height : 0;
    //height += margin;

    auto bmp = AllocateBitmap<1>(width, height);

    // the bitmap has 0,0 at top left and y grows down.
    //
    // 0,0 ____________________
    //     |                  | ascent (above baseline)
    //     |  ---baseline---  |
    //     |__________________| descent (below baseline)
    //
    const auto baseline = ascent;

    // finally compose the glyphs into a text buffer starting at the current pen position
    pen_x = 0;
    pen_y = 0;

    for (unsigned i=0; i<glyph_count; ++i)
    {
        const auto codepoint = glyph_info[i].codepoint;
        const auto& info = glyph_raster_info[codepoint];

        // advances tell us how much to move the pen in x/y direction for the next glyph
        const int xa = glyph_pos[i].x_advance / EFFIN_MAGIC_SCALE;
        const int ya = glyph_pos[i].y_advance / EFFIN_MAGIC_SCALE;

        // offsets tell us how the glyph should be offset wrt the current pen position
        const int xo = glyph_pos[i].x_offset / EFFIN_MAGIC_SCALE;
        const int yo = glyph_pos[i].y_offset / EFFIN_MAGIC_SCALE;

        const int x = pen_x + info.bearing_x + xo;
        const int y = pen_y + info.bearing_y + yo;

        bmp->Blit(x, baseline - y, info.bitmap, gfx::RasterOp_BitwiseOr<Grayscale>);

        pen_x += xa;
        pen_y += ya;
    }

    if (text.underline)
    {
        const auto width = bmp->GetWidth();
        const gfx::URect underline(0, baseline + underline_position,
                                   width, underline_thickness);
        bmp->Fill(underline, gfx::Grayscale(0xff));
    }

    hb_font_destroy(hb_font);
    hb_buffer_destroy(hb_buff);

    LineRaster ret;
    ret.baseline  = baseline;
    ret.bitmap    = bmp;
    return ret;
}

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
    static FontLibrary freetype;

    std::vector<TextComposite> blocks;
    std::shared_ptr<const Resource> fontbuff;
    std::string fontname;
    unsigned fontsize = 0;

    // make sure to destroy face first on stack unwind before
    // font buff is released.
    FT_Face face = nullptr;
    auto face_raii = base::MakeUniqueHandle(face, FT_Done_Face);

    // rasterize the lines and accumulate the metrics for the
    // height of all the text blocks and the maximum line width.
    for (const auto& text : mText)
    {
        if (fontname != text.font)
        {
            // make sure to call FT_Done_Face before discarding the font data buffer!
            face_raii.reset();
            // make sure to keep the font data buffer around while the face exists.
            fontbuff = gfx::LoadResource(text.font);
            if (!fontbuff)
                throw std::runtime_error("Failed to load font file: " + text.font);
            if (FT_New_Memory_Face(freetype.library, (const FT_Byte*)fontbuff->GetData(),
                                   fontbuff->GetSize(), 0, &face))
                throw std::runtime_error("Failed to load font file: " + text.font);
            face_raii = base::MakeUniqueHandle(face, FT_Done_Face);

            if (FT_Select_Charmap(face, FT_ENCODING_UNICODE))
                throw std::runtime_error("Font doesn't support Unicode");
            if (FT_Set_Pixel_Sizes(face, 0, text.fontsize))
                throw std::runtime_error("Font doesn't support pixel size: " + std::to_string(text.fontsize));

            fontsize = text.fontsize;
            fontname = text.font;
        }
        if (fontsize != text.fontsize)
        {
            if (FT_Set_Pixel_Sizes(face, 0, text.fontsize))
                throw std::runtime_error("Font doesn't support pixel size: " + std::to_string(text.fontsize));
        }

        TextBlock block;
        block.line_height = (face->size->metrics.height / EFFIN_MAGIC_SCALE) * text.lineheight;
        block.halign = mHorizontalAlign;
        block.valign = mVerticalAlign;

        std::stringstream ss(text.text);
        std::string line;
        while (std::getline(ss, line))
        {
            LineRaster raster;
            if (!line.empty())
                raster = RasterizeLine(line, text, face);
            block.lines.push_back(std::move(raster));
        }
        blocks.push_back(CompositeTextBlock(block, face));
    }

    // compute total combined size for text blocks to be laid out vertically.
    int text_width_px  = 0;
    int text_height_px = 0;
    for (const auto& block : blocks)
    {
        text_width_px   = std::max((int)block.bitmap->GetWidth(), text_width_px);
        text_height_px += block.bitmap->GetHeight();
    }

    // if we have some fixed/expected final image size then use that
    // otherwise create use the image size based on the combined text
    // block sizes.
    const int image_width_px  = mBufferWidth  ? (int)mBufferWidth  : text_width_px;
    const int image_height_px = mBufferHeight ? (int)mBufferHeight : text_height_px;

    auto out = AllocateBitmap<2>(image_width_px, image_height_px);

    int block_ypos = 0;
    if (mVerticalAlign == VerticalAlignment::AlignCenter)
        block_ypos = (image_height_px - text_height_px) / 2;
    else if (mVerticalAlign == VerticalAlignment::AlignBottom)
        block_ypos = image_height_px - text_height_px;

    // the base line values have already been "fixed" when the text
    // blocks have been composited. This means that we can just do the
    // final composite pass here using the bitmap sizes directly.
    for (const auto& block : blocks)
    {
        const int block_width_px  = block.bitmap->GetWidth();
        const int block_height_px = block.bitmap->GetHeight();
        if (mHorizontalAlign == HorizontalAlignment::AlignLeft)
            out->Copy(0, block_ypos, *block.bitmap);
        else if (mHorizontalAlign == HorizontalAlignment::AlignCenter)
            out->Copy((image_width_px - block_width_px) / 2, block_ypos, *block.bitmap);
        else if (mHorizontalAlign == HorizontalAlignment::AlignRight)
            out->Copy((image_width_px - block_width_px), block_ypos, *block.bitmap);

        block_ypos += block_height_px;
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
        hash = base::hash_combine(hash, t.lineheight);
        hash = base::hash_combine(hash, t.fontsize);
        hash = base::hash_combine(hash, t.underline);
    }
    return hash;
}

void TextBuffer::IntoJson(data::Writer& data) const
{
    data.Write("width", mBufferWidth);
    data.Write("height", mBufferHeight);
    data.Write("horizontal_alignment", mHorizontalAlign);
    data.Write("vertical_alignment", mVerticalAlign);
    for (const auto& text : mText)
    {
        auto chunk = data.NewWriteChunk();
        chunk->Write("string", text.text);
        chunk->Write("font_file", text.font);
        chunk->Write("font_size", text.fontsize);
        chunk->Write("line_height", text.lineheight);
        chunk->Write("underline", text.underline);
        data.AppendChunk("texts", std::move(chunk));
    }
}

// static
std::optional<TextBuffer> TextBuffer::FromJson(const data::Reader& data)
{
    TextBuffer buffer;
    if (!data.Read("width", &buffer.mBufferWidth) ||
        !data.Read("height", &buffer.mBufferHeight) ||
        !data.Read("horizontal_alignment", &buffer.mHorizontalAlign) ||
        !data.Read("vertical_alignment", &buffer.mVerticalAlign))
        return std::nullopt;


    for (unsigned i=0; i<data.GetNumChunks("texts"); ++i)
    {
        const auto& chunk = data.GetReadChunk("texts", i);
        Text t;
        if (!chunk->Read("string", &t.text) ||
            !chunk->Read("font_file", &t.font) ||
            !chunk->Read("font_size", &t.fontsize) ||
            !chunk->Read("line_height", &t.lineheight) |
            !chunk->Read("underline", &t.underline))
            return std::nullopt;
        buffer.mText.push_back(std::move(t));
    }
    return buffer;
}

}  //namespace
