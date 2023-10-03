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
#  include <nlohmann/json.hpp>
#  include <glm/glm.hpp>
#  include <glm/mat4x4.hpp>
#  include <glm/gtc/matrix_transform.hpp>
#include "warnpop.h"

#include <functional>
#include <stdexcept>
#include <map>
#include <unordered_map>
#include <cwctype>

#include "base/logging.h"
#include "base/utility.h"
#include "base/json.h"
#include "data/reader.h"
#include "data/writer.h"
#include "graphics/resource.h"
#include "graphics/loader.h"
#include "graphics/text.h"
#include "graphics/device.h"
#include "graphics/framebuffer.h"
#include "graphics/texture.h"
#include "graphics/shader.h"
#include "graphics/program.h"
#include "graphics/geometry.h"
#include "graphics/image.h"
#include "graphics/transform.h"

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
// Glyphs are pre-rendered offline in some image editor tool
// and packed into a texture. JSON meta file describes the
// glyphs and (optionally) kerning pairs. Appropriate data
// files can be produced by the Editor's image packer.
class GamestudioBitmapFontGlyphPack
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
    bool ParseFont(const std::string& uri)
    {
        const auto& fontbuff = gfx::LoadResource(uri);
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
    unsigned GetFontWidth() const
    { return mFontWidth; }
    unsigned GetFontHeight() const
    { return mFontHeight; }
    unsigned GetTextureWidth() const
    { return mTextureWidth; }
    unsigned GetTextureHeight() const
    { return mTextureHeight; }
    std::string GetTextureFile() const
    { return mTextureFile; }
    bool IsValid() const
    { return mValid; }

    gfx::Texture* GetTexture(gfx::Device& device) const
    {
        if (mTextureFile.empty())
            return nullptr;
        // todo: this name is not necessarily unique.
        auto* texture = device.FindTexture(mTextureFile);
        if (texture)
            return texture;

        auto last_slash = mFontUri.find_last_of('/');
        if (last_slash == std::string::npos)
            return nullptr;

        texture = device.MakeTexture(mTextureFile);

        auto uri = mFontUri.substr(0, last_slash);
        uri += '/';
        uri += mTextureFile;

        DEBUG("Loading bitmap font texture. [file='%1']", uri);
        gfx::Image image(uri);
        if (!image.IsValid())
            ERROR_RETURN(nullptr, "Failed to load texture. [file='%1']", uri);

        const auto width  = image.GetWidth();
        const auto height = image.GetHeight();
        const auto format = gfx::Texture::DepthToFormat(image.GetDepthBits(), true); // todo:  sRGB flag ?
        texture->SetName(mTextureFile);
        texture->Upload(image.GetData(), width, height, format);
        DEBUG("Loaded bitmap font texture. [file='%1']", uri);
        return texture;
    }

    const Glyph* FindGlyph(uint32_t character) const
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
using gfx::Pixel_A;

template<size_t Pool>
std::shared_ptr<gfx::AlphaMask> AllocateBitmap(unsigned width, unsigned height)
{
    // the repeated allocation bitmaps for rasterizing content is
    // actually more expensive than the actual rasterization.
    // we can keep a small cache of frequently used bitmap sizes.
    static std::unordered_map<std::uint64_t,
               std::shared_ptr<gfx::AlphaMask>> cache;

    std::shared_ptr<gfx::AlphaMask> bmp;

    const std::uint64_t key = ((uint64_t)width << 32) | height;
    auto it = cache.find(key);
    if (it == std::end(cache))
    {
        bmp = std::make_shared<gfx::AlphaMask>(width, height);
        cache[key] = bmp;
    }
    else if (it->second.use_count() == 1)
    {
        bmp = it->second;
        bmp->Fill(Pixel_A(0));
    }
    else
    {
        bmp = std::make_shared<gfx::AlphaMask>(width, height);
    }
    return bmp;
}

struct LineRaster {
    // bitmap can be nullptr if the line was in fact
    // empty with no text in it. (no rasterization was done)
    std::shared_ptr<const gfx::AlphaMask> bitmap;
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
    std::shared_ptr<const gfx::AlphaMask> bitmap;
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
        gfx::AlphaMask bitmap;
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
            gfx::AlphaMask bmp(reinterpret_cast<const Pixel_A*>(slot->bitmap.buffer),
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

        bmp->Blit(x, baseline - y, info.bitmap, gfx::RasterOp_BitwiseOr<Pixel_A>);

        pen_x += xa;
        pen_y += ya;
    }

    if (text.underline)
    {
        const auto width = bmp->GetWidth();
        const gfx::URect underline(0, baseline + underline_position,
                                   width, underline_thickness);
        bmp->Fill(underline, gfx::Pixel_A(0xff));
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

TextBuffer::RasterFormat TextBuffer::GetRasterFormat() const
{
    if (base::EndsWith(mText.font, ".otf") ||
        base::EndsWith(mText.font, ".ttf") ||
        base::EndsWith(mText.font, ".OTF") ||
        base::EndsWith(mText.font, ".TTF"))
        return RasterFormat::Bitmap;
    if (base::EndsWith(mText.font, ".json") ||
        base::EndsWith(mText.font, ".JSON"))
        return RasterFormat::Texture;
    return RasterFormat::None;
}

std::shared_ptr<AlphaMask> TextBuffer::RasterizeBitmap() const
{
    static FontLibrary freetype;

    // make sure to destroy face first on stack unwind before
    // font buff is released.
    FT_Face face = nullptr;

    // rasterize the lines and accumulate the metrics for the
    // height of all the text blocks and the maximum line width.

    // hmm, maybe not exactly the right place to do this but
    // certainly convenient....
    if (mText.font.empty())
        return nullptr;

    // make sure to keep the font data buffer around while the face exists.
    auto fontbuff = gfx::LoadResource(mText.font);
    if (!fontbuff)
        ERROR_RETURN(nullptr, "Failed to load font file. [font='%1]", mText.font);

    if (FT_New_Memory_Face(freetype.library, (const FT_Byte*)fontbuff->GetData(),
                           fontbuff->GetByteSize(), 0, &face))
        ERROR_RETURN(nullptr, "Failed to load font face. [font='%1']", mText.font);

    auto face_raii = base::MakeUniqueHandle(face, FT_Done_Face);

    if (FT_Select_Charmap(face, FT_ENCODING_UNICODE))
        ERROR_RETURN(nullptr, "Font doesn't support Unicode. [font='%1']", mText.font);
    if (FT_Set_Pixel_Sizes(face, 0, mText.fontsize))
        ERROR_RETURN(nullptr, "Font doesn't support expected pixel size. [font='%1', size='%2']", mText.font, mText.fontsize);

    TextBlock block;
    block.line_height = (face->size->metrics.height / EFFIN_MAGIC_SCALE) * mText.lineheight;
    block.halign = mHorizontalAlign;
    block.valign = mVerticalAlign;

    std::vector<TextComposite> blocks;
    std::stringstream ss(mText.text);
    std::string line;
    while (std::getline(ss, line))
    {
        LineRaster raster;
        if (!line.empty())
            raster = RasterizeLine(line, mText, face);
        block.lines.push_back(std::move(raster));
    }
    blocks.push_back(CompositeTextBlock(block, face));

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
    Bitmap<Pixel_RGB> tmp;
    tmp.Resize(out->GetWidth(), out->GetHeight());
    tmp.Copy(0, 0, *out);
    WritePPM(tmp, "/tmp/text-buffer.ppm");
    DEBUG("Wrote rasterized text buffer in '%1'", "/tmp/text-buffer.ppm");
#endif

    return out;
}

Texture* TextBuffer::RasterizeTexture(const std::string& gpu_id, const std::string& name, Device& device) const
{
    // load the bitmap font json descriptor
    static std::unordered_map<std::string, std::unique_ptr<GamestudioBitmapFontGlyphPack>> font_cache;
    auto it = font_cache.find(mText.font);
    if (it == font_cache.end())
    {
        auto font = std::make_unique<GamestudioBitmapFontGlyphPack>();
        font->ParseFont(mText.font);
        it = font_cache.insert({mText.font, std::move(font)}).first;
    }
    auto& font = (*it).second;
    if (!font->IsValid())
        return nullptr;

    // upload the font texture onto the device.
    auto* font_texture = font->GetTexture(device);
    if (!font_texture)
        return nullptr;

    // create the render target texture that will contain the
    // rasterized texture after we're done. it'll be used as a
    // render target (color attachment) in an FBO, and we render
    // to it by drawing quads that sample from the font's texture.
    auto* result_texture = device.MakeTexture(gpu_id);
    result_texture->SetName(name);

    // setup the glyph array.
    struct Glyph {
        float xpos   = 0.0f;
        float ypos   = 0.0f;
        float width  = 0.0f;
        float height = 0.0f;
        float texture_xpos   = 0.0f;
        float texture_ypos   = 0.0f;
        float texture_width  = 0.0f;
        float texture_height = 0.0f;
    };
    std::vector<Glyph> glyphs;

    std::vector<std::string> lines;
    std::stringstream  ss(mText.text);
    std::string line;
    while (std::getline(ss, line))
    {
        lines.push_back(line);
    }

    unsigned buffer_width  = mBufferWidth;
    unsigned buffer_height = mBufferHeight;
    if (!buffer_height)
    {
        buffer_height = lines.size() * mText.lineheight * mText.fontsize;
    }
    if (!buffer_width)
    {
        for (const auto& line : lines)
        {
            unsigned line_width = 0;
            for (const auto char_ : line)
            {
                const auto* font_glyph = font->FindGlyph(char_);
                const float px_height = font_glyph ? font_glyph->px_height : font->GetFontHeight();
                const float px_width  = font_glyph ? font_glyph->px_width  : font->GetFontWidth();
                const float glyph_scaler = mText.fontsize / px_height;
                const float glyph_width  = px_width * glyph_scaler;
                line_width += glyph_width;
            }
            buffer_width = std::max(buffer_width, line_width);
        }
    }

    float ypos = 0.0f;
    if (mVerticalAlign == VerticalAlignment::AlignCenter)
    {
        const float text_height = lines.size() * mText.lineheight * mText.fontsize;
        ypos = ((float)buffer_height - text_height) / 2.0f;
    }
    else if (mVerticalAlign == VerticalAlignment::AlignBottom)
    {
        const float text_height = lines.size() * mText.lineheight * mText.fontsize;
        ypos = ((float)buffer_height - text_height);
    }

    for (const auto& line : lines)
    {
        float xpos = 0;
        auto index = glyphs.size();
        for (const auto char_ : line)
        {
            const auto* font_glyph = font->FindGlyph(char_);
            const float px_height = font_glyph ? font_glyph->px_height : font->GetFontHeight();
            const float px_width  = font_glyph ? font_glyph->px_width  : font->GetFontWidth();
            const float glyph_scaler = mText.fontsize / px_height;
            const float glyph_width  = px_width * glyph_scaler;
            const float glyph_height = px_height * glyph_scaler;
            if (font_glyph == nullptr)
            {
                xpos += glyph_width;
                continue;
            }
            Glyph glyph;
            glyph.xpos   = xpos;
            glyph.ypos   = ypos;
            glyph.width  = glyph_width;
            glyph.height = glyph_height;
            glyph.texture_xpos   = font_glyph->xpos;
            glyph.texture_ypos   = font_glyph->ypos;
            glyph.texture_width  = font_glyph->width;
            glyph.texture_height = font_glyph->height;
            glyphs.push_back(glyph);
            xpos += glyph_width;
        }

        if (mHorizontalAlign == HorizontalAlignment::AlignCenter)
        {
            float delta = ((float)buffer_width - xpos) / 2.0f;
            for (;index < glyphs.size(); ++index)
            {
                glyphs[index].xpos += delta;
            }
        }
        else if (mHorizontalAlign == HorizontalAlignment::AlignRight)
        {
            float delta = (float)buffer_width - xpos;
            for (; index<glyphs.size(); ++index)
            {
                glyphs[index].xpos += delta;
            }
        }
        ypos += mText.lineheight * mText.fontsize;
    }

    result_texture->Allocate(buffer_width, buffer_height, gfx::Texture::Format::sRGBA);

    auto* fbo = device.FindFramebuffer("BitmapFontCompositeFBO");
    if (fbo == nullptr)
    {
        // when setting the FBO configuration the width/height
        // don't matter since this FBO will only have a color buffer
        // render target.
        gfx::Framebuffer::Config conf;
        conf.format = gfx::Framebuffer::Format::ColorRGBA8;
        conf.width  = 0;
        conf.height = 0;
        fbo = device.MakeFramebuffer("BitmapFontCompositeFBO");
        fbo->SetConfig(conf);
    }
constexpr auto* fragment_src = R"(
#version 100
precision highp float;
uniform sampler2D kGlyphMap;
varying vec2 vTexCoord;
void main() {
  gl_FragColor = texture2D(kGlyphMap, vTexCoord);
}
    )";
constexpr auto* vertex_src = R"(
#version 100
attribute vec2 aPosition;
attribute vec2 aTexCoord;

varying vec2 vTexCoord;
void main() {
    vTexCoord   = aTexCoord;
    gl_Position = vec4(aPosition.x, aPosition.y, 0.0, 1.0);
}
    )";
    auto* program = device.FindProgram("BitmapFontCompositeProgram");
    if (program == nullptr)
    {
        program  = device.MakeProgram("BitmapFontCompositeProgram");
        program->SetName("BitmapFontCompositeProgram");
        auto* fs = device.MakeShader("BitmapFontCompositeFragmentShader");
        auto* vs = device.MakeShader("BitmapFontCompositeVertexShader");
        fs->SetName("BitmapFontCompositeFragmentShader");
        if (!fs->CompileSource(fragment_src))
            return nullptr;
        vs->SetName("BitmapFontCompositeVertexShader");
        if (!vs->CompileSource(vertex_src))
            return nullptr;
        if (!program->Build(fs, vs))
            return nullptr;
    }

    auto* geometry = device.FindGeometry("BitmapFontTextGeometry");
    if (geometry == nullptr)
    {
        geometry = device.MakeGeometry("BitmapFontTextGeometry");
    }


    auto ortho = glm::ortho(0.0f, (float)buffer_width, (float)buffer_height, 0.0f);
    Quad quad;
    quad.top_left     = {0.0f,  0.0f, 0.0f, 1.0f};
    quad.bottom_left  = {0.0f,  1.0f, 0.0f, 1.0f};
    quad.bottom_right = {1.0f,  1.0f, 0.0f, 1.0f};
    quad.top_right    = {1.0f,  0.0f, 0.0f, 1.0f};

    std::vector<Vertex2D> verts;
    for (const auto& glyph : glyphs)
    {
        gfx::Transform t;
        t.Scale(glyph.width, glyph.height);
        t.Translate(glyph.xpos, glyph.ypos);

        const auto& q = TransformQuad(quad, ortho * t.GetAsMatrix());

        Vertex2D v0, v1, v2, v3;
        v0.aPosition = Vec2 {q.top_left.x, q.top_left.y };
        v0.aTexCoord = Vec2 { glyph.texture_xpos, glyph.texture_ypos };

        v1.aPosition = Vec2 {q.bottom_left.x, q.bottom_left.y };
        v1.aTexCoord = Vec2 { glyph.texture_xpos, glyph.texture_ypos + glyph.texture_height };

        v2.aPosition = Vec2 {q.bottom_right.x, q.bottom_right.y };
        v2.aTexCoord = Vec2 { glyph.texture_xpos + glyph.texture_width, glyph.texture_ypos + glyph.texture_height };

        v3.aPosition = Vec2 {q.top_right.x, q.top_right.y };
        v3.aTexCoord = Vec2 { glyph.texture_xpos + glyph.texture_width, glyph.texture_ypos };

        base::AppendVector(verts, {v0, v1, v2});
        base::AppendVector(verts, {v0, v2, v3});
    }
    geometry->ClearDraws();
    geometry->SetVertexBuffer(verts, Geometry::Usage::Stream);
    geometry->AddDrawCmd(Geometry::DrawType::Triangles);

    program->SetTexture("kGlyphMap", 0, *font_texture);
    program->SetTextureCount(1);

    fbo->SetColorTarget(result_texture);

    device.ClearColor(gfx::Color4f(0.0f, 0.0f, 0.0f, 0.0f), fbo);

    Device::State state;
    state.bWriteColor = true;
    state.blending    = Device::State::BlendOp::Transparent;
    state.culling     = Device::State::Culling::Back;
    state.depth_test  = Device::State::DepthTest::Disabled;
    state.premulalpha = false; // todo:
    state.scissor     = IRect(); // disabled
    state.viewport    = IRect(0, 0, buffer_width, buffer_height);
    state.stencil_func = Device::State::StencilFunc::Disabled;
    device.Draw(*program, *geometry, state, fbo);
    return result_texture;
}

bool TextBuffer::ComputeTextMetrics(unsigned int* width, unsigned int* height) const
{
    // todo: implement better, using font metrics and not using rasterization
    auto buffer = RasterizeBitmap();
    if (!buffer)
        return false;
    *width  = buffer->GetWidth();
    *height = buffer->GetHeight();
    return true;
}

std::size_t TextBuffer::GetHash() const
{
    std::size_t hash = 0;
    hash = base::hash_combine(hash, mBufferWidth);
    hash = base::hash_combine(hash, mBufferHeight);
    hash = base::hash_combine(hash, mVerticalAlign);
    hash = base::hash_combine(hash, mHorizontalAlign);
    hash = base::hash_combine(hash, mText.text);
    hash = base::hash_combine(hash, mText.font);
    hash = base::hash_combine(hash, mText.lineheight);
    hash = base::hash_combine(hash, mText.fontsize);
    hash = base::hash_combine(hash, mText.underline);
    return hash;
}

void TextBuffer::IntoJson(data::Writer& data) const
{
    data.Write("width", mBufferWidth);
    data.Write("height", mBufferHeight);
    data.Write("horizontal_alignment", mHorizontalAlign);
    data.Write("vertical_alignment", mVerticalAlign);
    // texts used to be an array before. but this simplification
    // removes the array and only has one chunk
    auto chunk = data.NewWriteChunk();
    chunk->Write("string",      mText.text);
    chunk->Write("font_file",   mText.font);
    chunk->Write("font_size",   mText.fontsize);
    chunk->Write("line_height", mText.lineheight);
    chunk->Write("underline",   mText.underline);
    data.AppendChunk("texts", std::move(chunk));
}

bool TextBuffer::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("width",                &mBufferWidth);
    ok &= data.Read("height",               &mBufferHeight);
    ok &= data.Read("horizontal_alignment", &mHorizontalAlign);
    ok &= data.Read("vertical_alignment",   &mVerticalAlign);

    if (data.GetNumChunks("texts"))
    {
        // texts used to be an array before. but this simplification
        // removes the array and only has one chunk
        const auto& chunk = data.GetReadChunk("texts", 0);
        Text text;
        ok &= chunk->Read("string",      &text.text);
        ok &= chunk->Read("font_file",   &text.font);
        ok &= chunk->Read("font_size",   &text.fontsize);
        ok &= chunk->Read("line_height", &text.lineheight);
        ok &= chunk->Read("underline",   &text.underline);
        mText = std::move(text);
    }
    return ok;
}

}  //namespace
