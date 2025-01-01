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
#  include <stb/stb_image_write.h>
#include "warnpop.h"

#include <algorithm>
#include <fstream>
#include <cmath>
#include <set>

#include "base/hash.h"
#include "base/math.h"
#include "data/writer.h"
#include "data/reader.h"
#include "graphics/bitmap.h"

namespace {
void PremultiplyPixel_lRGB(const gfx::Pixel_RGBA& src, gfx::Pixel_RGBA* dst)
{
    gfx::Pixel_RGBAf norm;
    norm = gfx::Pixel_to_floats(src);
    norm = gfx::RGBA_premul_alpha(norm);
    *dst = gfx::Pixel_to_uints(norm);
}
void PremultiplyPixel_sRGB(const gfx::Pixel_RGBA& src, gfx::Pixel_RGBA* dst)
{
    gfx::Pixel_RGBAf norm;
    norm = gfx::Pixel_to_floats(src);
    norm = gfx::sRGB_decode(norm);
    norm = gfx::RGBA_premul_alpha(norm);
    norm = gfx::sRGB_encode(norm);
    *dst = gfx::Pixel_to_uints(norm);
}

template<typename T_u8, typename T_float>
std::unique_ptr<gfx::Bitmap<T_u8>> ConvertToLinear(const gfx::IBitmapReadView& src)
{
    ASSERT(src.IsValid());

    const auto width  = src.GetWidth();
    const auto height = src.GetHeight();

    using Bitmap = gfx::Bitmap<T_u8>;

    auto ret = std::make_unique<Bitmap>(width, height);
    auto dst = ret->GetWriteView();

    for (unsigned row=0; row<height; ++row)
    {
        for (unsigned col=0; col<width; ++col)
        {
            T_u8 value;
            T_float norm;
            src.ReadPixel(row, col, &value);
            norm  = gfx::Pixel_to_floats(value);
            norm  = gfx::sRGB_decode(norm);
            value = gfx::Pixel_to_uints(norm);
            dst->WritePixel(row, col, value);
        }
    }
    return ret;
}

template<typename T_u8, typename T_float, bool support_srgb, bool alpha_channel>
std::unique_ptr<gfx::Bitmap<T_u8>> BoxFilter(const gfx::IBitmapReadView& src, bool srgb, bool premul_alpha)
{
    ASSERT(src.IsValid());

    const auto src_width  = src.GetWidth();
    const auto src_height = src.GetHeight();
    if (src_width == 1 && src_height == 1)
        return nullptr;

    const auto dst_width  = std::max(1u, src_width / 2);
    const auto dst_height = std::max(1u, src_height / 2);

    const auto src_height_max = src_height & ~0x1;
    const auto src_width_max  = src_width & ~0x1;

    using Bitmap = gfx::Bitmap<T_u8>;

    auto ret = std::make_unique<Bitmap>(dst_width, dst_height);
    auto dst = ret->GetWriteView();

    for (unsigned dst_row=0, src_row=0; src_row<src_height_max; src_row+=2, dst_row++)
    {
        for (unsigned dst_col=0, src_col=0; src_col<src_width_max; src_col+=2, dst_col++)
        {
            // read 2x2 pixels from the source image
            T_u8 values[4];
            src.ReadPixel(std::min(src_row+0, src_height-1), std::min(src_col+0, src_width-1), &values[0]);
            src.ReadPixel(std::min(src_row+0, src_height-1), std::min(src_col+1, src_width-1), &values[1]);
            src.ReadPixel(std::min(src_row+1, src_height-1), std::min(src_col+0, src_width-1), &values[2]);
            src.ReadPixel(std::min(src_row+1, src_height-1), std::min(src_col+1, src_width-1), &values[3]);

            // normalize the values for better precision. alternative would
            // be to use bit shift (right shift by two) on the integer type
            // but that would lose some precision.
            T_float values_norm[4];
            values_norm[0] = gfx::Pixel_to_floats(values[0]);
            values_norm[1] = gfx::Pixel_to_floats(values[1]);
            values_norm[2] = gfx::Pixel_to_floats(values[2]);
            values_norm[3] = gfx::Pixel_to_floats(values[3]);

            // if we're dealing with sRGB color space then the values need first
            // be converted into linear before averaging. this is only supported
            // for RGBA and RGB formats.
            if constexpr (support_srgb)
            {
                if (srgb)
                {
                    values_norm[0] = gfx::sRGB_decode(values_norm[0]);
                    values_norm[1] = gfx::sRGB_decode(values_norm[1]);
                    values_norm[2] = gfx::sRGB_decode(values_norm[2]);
                    values_norm[3] = gfx::sRGB_decode(values_norm[3]);
                }
            }

            // premultiply alpha into RGB
            if constexpr (alpha_channel)
            {
                if (premul_alpha)
                {
                    for (auto& rgba : values_norm)
                    {
                        rgba = gfx::RGBA_premul_alpha(rgba);
                    }
                }
            }

            // compute the average of the original 4 pixels.
            T_float value = values_norm[0] * 0.25f +
                            values_norm[1] * 0.25f +
                            values_norm[2] * 0.25f +
                            values_norm[3] * 0.25f;
            // encode linear in sRGB if needed.
            if constexpr (support_srgb)
            {
                if (srgb)
                    value = gfx::sRGB_encode(value);
            }

            // write the new pixel value out.
            dst->WritePixel(dst_row, dst_col, gfx::Pixel_to_uints(value));
        }
    }
    return ret;
}
} // namespace

namespace gfx
{

void WritePPM(const IBitmapReadView& bmp, const std::string& filename)
{
    static_assert(sizeof(Pixel_RGB) == 3,
        "Padding bytes found. Cannot copy Pixel_RGB data as a byte stream.");

    std::ofstream out(filename, std::ios::binary);
    if (!out.is_open())
        throw std::runtime_error("failed to open file: " + filename);

    const auto width  = bmp.GetWidth();
    const auto height = bmp.GetHeight();
    const auto depth  = bmp.GetDepthBits() / 8;
    const auto bytes  = width * height * sizeof(Pixel_RGB);

    std::vector<Pixel_RGB> tmp;
    tmp.reserve(width * height);
    for (unsigned row=0; row<height; ++row)
    {
        for (unsigned col=0; col<width; ++col)
        {
            Pixel_RGB value;
            bmp.ReadPixel(row, col, &value);
            tmp.push_back(value);
        }
    }
    out << "P6 " << width << " " << height << " 255\n";
    out.write((const char*)&tmp[0], bytes);
    out.close();
}
void WritePPM(const IBitmap& bmp, const std::string& filename)
{
    auto view = bmp.GetReadView();
    WritePPM(*view, filename);
}

void WritePNG(const IBitmapReadView& bmp, const std::string& filename)
{
    const auto w = bmp.GetWidth();
    const auto h = bmp.GetHeight();
    const auto d = bmp.GetDepthBits() / 8;
    if (!stbi_write_png(filename.c_str(), w, h, d, bmp.GetReadPtr(), d * w))
        throw std::runtime_error(std::string("failed to write png: " + filename));
}
void WritePNG(const IBitmap& bmp, const std::string& filename)
{
    auto view = bmp.GetReadView();
    WritePNG(*view, filename);
}

std::unique_ptr<IBitmap> GenerateNextMipmap(const IBitmapReadView& src, bool srgb)
{
    const auto premul_alpha = false;
    if (src.GetDepthBits() == 32) {
       return ::BoxFilter<Pixel_RGBA, Pixel_RGBAf, true, true>(src, srgb, premul_alpha);
    } else if (src.GetDepthBits() == 24) {
        return ::BoxFilter<Pixel_RGB, Pixel_RGBf, true, false> (src, srgb, false);
    } else if (src.GetDepthBits() == 8)
        return ::BoxFilter<Pixel_A, Pixel_Af, false, false>(src, false, false);
    return nullptr;
}
std::unique_ptr<IBitmap> GenerateNextMipmap(const IBitmap& src, bool srgb)
{
    auto view = src.GetReadView();
    return GenerateNextMipmap(*view, srgb);
}

std::unique_ptr<IBitmap> ConvertToLinear(const IBitmapReadView& src)
{
    if (src.GetDepthBits() == 32)
        return ::ConvertToLinear < Pixel_RGBA, Pixel_RGBAf > (src);
    else if (src.GetDepthBits() == 24)
        return ::ConvertToLinear < Pixel_RGB, Pixel_RGBf > (src);
    return nullptr;
}
std::unique_ptr<IBitmap> ConvertToLinear(const IBitmap& src)
{
    auto view = src.GetReadView();
    return ConvertToLinear(*view);
}

void PremultiplyAlpha(const BitmapWriteView<Pixel_RGBA>& dst,
                      const BitmapReadView<Pixel_RGBA>& src, bool srgb)
{
    auto conversion  = srgb ? &PremultiplyPixel_sRGB
                            : &PremultiplyPixel_lRGB;
    ConvertBitmap(dst, src, conversion);
}

Bitmap<Pixel_RGBA> PremultiplyAlpha(const BitmapReadView<Pixel_RGBA>& src, bool srgb)
{
    const auto src_width  = src.GetWidth();
    const auto src_height = src.GetHeight();

    Bitmap<Pixel_RGBA> ret;
    ret.Resize(src_width, src_height);

    PremultiplyAlpha(ret.GetPixelWriteView(), src, srgb);
    return ret;
}

Bitmap<Pixel_RGBA> PremultiplyAlpha(const Bitmap<Pixel_RGBA>& src, bool srgb)
{
    return PremultiplyAlpha(src.GetPixelReadView(), srgb);
}

URect FindImageRectangle(const IBitmapReadView& img, const IPoint& start)
{
    URect ret;

    if (img.GetDepthBits() != 32)
        return ret;

    using BitmapReadViewRGBA = BitmapReadView<Pixel_RGBA>;

    const auto* rgba_view = dynamic_cast<const BitmapReadViewRGBA*>(&img);
    ASSERT(rgba_view);

    // explorer all adjacent pixels and their adjacent pixels etc
    // with a breadth first search until the alpha value is 0.0.
    struct Compare {
        bool operator ()(const IPoint& lhs, const IPoint& rhs) const
        {
            if (lhs.GetX() < rhs.GetX())
                return true;
            else if (lhs.GetX() == rhs.GetX())
                if (lhs.GetY() < rhs.GetY())
                return true;
            return false;
        }
    };

    auto ReadPixel = [](const BitmapReadViewRGBA& img, const IPoint& point, Pixel_RGBA* pixel) {
        const auto w = img.GetWidth();
        const auto h = img.GetHeight();
        if (point.GetX() >= w || point.GetX() < 0)
            return false;
        else if (point.GetY() >= h || point.GetY() < 0)
            return false;

        img.ReadPixel(point.GetY(), point.GetX(), pixel);
        return true;
    };

    Pixel_RGBA pixel;
    std::set<IPoint, Compare> pixels_visited;
    std::set<IPoint, Compare> pixels_to_explore;
    pixels_to_explore.insert(start);

    int min_x = std::numeric_limits<int>::max();
    int min_y = std::numeric_limits<int>::max();
    int max_x = 0;
    int max_y = 0;

    struct PixelOffset {
        int x, y;
    };
    static PixelOffset neighbors[] = {
        {-1, 0}, // left
        {-1, -1}, // left up
        {0, -1},  // up
        {1, -1},  // up right
        {1, 0}, // right
        {1, 1}, // right down
        {0, 1}, // down
        {-1, 1} // down left
    };

    while (!pixels_to_explore.empty())
    {
        auto beg = pixels_to_explore.begin();
        auto this_pixel = *beg;
        pixels_to_explore.erase(beg);

        if (!ReadPixel(*rgba_view, this_pixel, &pixel))
            continue;

        pixels_visited.insert(this_pixel);

        if (pixel.a == 0)
            continue;

        min_x = std::min(min_x, this_pixel.GetX());
        max_x = std::max(max_x, this_pixel.GetX());
        min_y = std::min(min_y, this_pixel.GetY());
        max_y = std::max(max_y, this_pixel.GetY());

        for (size_t i=0; i<8; ++i)
        {
            const auto neighbor = neighbors[i];
            const auto neighbor_pixel = this_pixel.TranslateCopy(neighbor.x, neighbor.y);
            if (auto it = pixels_visited.find(neighbor_pixel); it != pixels_visited.end())
                continue;

            if (!ReadPixel(*rgba_view, neighbor_pixel, &pixel) || pixel.a == 0)
                continue;

            pixels_to_explore.insert(neighbor_pixel);
        }
    }
    if (min_x == std::numeric_limits<int>::max() || min_y == std::numeric_limits<int>::max())
        return ret;

    const auto found_width  = max_x - min_x + 1;
    const auto found_height = max_y - min_y + 1;

    ASSERT(min_x >= 0 && (min_x + found_width <= img.GetWidth()));
    ASSERT(min_y >= 0 && (min_y + found_height <= img.GetHeight()));
    ret.SetX(min_x);
    ret.SetY(min_y);
    ret.SetWidth(found_width);
    ret.SetHeight(found_height);
    return ret;
}

void SetAlphaToOne(RgbaBitmap& bitmap)
{
    const auto count = bitmap.GetWidth() * bitmap.GetHeight();

    for (unsigned index=0; index<count; ++index)
        bitmap.GetPixel(index).a = 255;

}

} // namespace
