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
#  include <boost/math/special_functions/prime.hpp>
#  include <stb/stb_image_write.h>
#include "warnpop.h"

#include <algorithm>
#include <fstream>
#include <cmath>

#include "base/hash.h"
#include "base/math.h"
#include "data/writer.h"
#include "data/reader.h"
#include "graphics/bitmap.h"

namespace {
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
            norm  = gfx::RGB_u8_to_float(value);
            norm  = gfx::sRGB_decode(norm);
            value = gfx::RGB_u8_from_float(norm);
            dst->WritePixel(row, col, value);
        }
    }
    return ret;
}

template<typename T_u8, typename T_float, bool srgb>
std::unique_ptr<gfx::Bitmap<T_u8>> BoxFilter(const gfx::IBitmapReadView& src)
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
            values_norm[0] = gfx::RGB_u8_to_float(values[0]);
            values_norm[1] = gfx::RGB_u8_to_float(values[1]);
            values_norm[2] = gfx::RGB_u8_to_float(values[2]);
            values_norm[3] = gfx::RGB_u8_to_float(values[3]);

            // if we're dealing with sRGB color space then the values need first
            // be converted into linear before averaging. this is only supported
            // for RGBA and RGB formats.
            if constexpr (srgb)
            {
                values_norm[0] = gfx::sRGB_decode(values_norm[0]);
                values_norm[1] = gfx::sRGB_decode(values_norm[1]);
                values_norm[2] = gfx::sRGB_decode(values_norm[2]);
                values_norm[3] = gfx::sRGB_decode(values_norm[3]);
            }

            // compute the average of the original 4 pixels.
            T_float value = values_norm[0] * 0.25f +
                            values_norm[1] * 0.25f +
                            values_norm[2] * 0.25f +
                            values_norm[3] * 0.25f;
            // encode linear in sRGB if needed.
            if constexpr (srgb)
            {
                value = gfx::sRGB_encode(value);
            }

            // write the new pixel value out.
            dst->WritePixel(dst_row, dst_col, gfx::RGB_u8_from_float(value));
        }
    }
    return ret;
}
} // namespace

namespace gfx
{
RGB::RGB(Color c)
{
    switch (c)
    {
        case Color::White:
            r = g = b = 255;
            break;
        case Color::Black:
            break;
        case Color::Red:
            r = 255;
            break;
        case Color::DarkRed:
            r = 127;
            break;
        case Color::Green:
            g = 255;
            break;
        case Color::DarkGreen:
            g = 127;
            break;
        case Color::Blue:
            b = 255;
            break;
        case Color::DarkBlue:
            b = 127;
            break;
        case Color::Cyan:
            g = b = 255;
            break;
        case Color::DarkCyan:
            g = b = 127;
            break;
        case Color::Magenta:
            r = b = 255;
            break;
        case Color::DarkMagenta:
            r = b = 127;
            break;
        case Color::Yellow:
            r = g = 255;
            break;
        case Color::DarkYellow:
            r = g = 127;
            break;
        case Color::Gray:
            r = g = b = 158;
            break;
        case Color::DarkGray:
            r = g = b = 127;
            break;
        case Color::LightGray:
            r = g = b = 192;
            break;
        case Color::HotPink:
            r = 255;
            g = 105;
            b = 180;
            break;
        case Color::Gold:
            r = 255;
            g = 215;
            b = 0;
            break;
        case Color::Silver:
            r = 192;
            g = 192;
            b = 192;
            break;
        case Color::Bronze:
            r = 205;
            g = 127;
            b = 50;
            break;
    }
} // ctor

bool operator==(const Grayscale& lhs, const Grayscale& rhs)
{
    return lhs.r == rhs.r;
}
bool operator!=(const Grayscale& lhs, const Grayscale& rhs)
{
    return lhs.r != rhs.r;
}
Grayscale operator & (const Grayscale& lhs, const Grayscale& rhs)
{
    return Grayscale(lhs.r & rhs.r);
}
Grayscale operator | (const Grayscale& lhs, const Grayscale& rhs)
{
    return Grayscale(lhs.r | rhs.r);
}

Grayscale operator >> (const Grayscale& lhs, unsigned bits)
{
    return Grayscale (lhs.r >> bits);
}

bool operator==(const RGB& lhs, const RGB& rhs)
{
    return lhs.r == rhs.r &&
           lhs.g == rhs.g &&
           lhs.b == rhs.b;
}
bool operator!=(const RGB& lhs, const RGB& rhs)
{ return !(lhs == rhs); }

RGB operator & (const RGB& lhs, const RGB& rhs)
{
    RGB ret;
    ret.r = lhs.r & rhs.r;
    ret.g = lhs.g & rhs.g;
    ret.b = lhs.b & rhs.b;
    return ret;
}
RGB operator | (const RGB& lhs, const RGB& rhs)
{
    RGB ret;
    ret.r = lhs.r | rhs.r;
    ret.g = lhs.g | rhs.g;
    ret.b = lhs.b | rhs.b;
    return ret;
}

RGB operator >> (const RGB& lhs, unsigned bits)
{
    RGB ret;
    ret.r = lhs.r >> bits;
    ret.g = lhs.g >> bits;
    ret.b = lhs.b >> bits;
    return ret;
}

bool operator==(const RGBA& lhs, const RGBA& rhs)
{
    return lhs.r == rhs.r &&
           lhs.g == rhs.g &&
           lhs.b == rhs.b &&
           lhs.a == rhs.a;
}
bool operator!=(const RGBA& lhs, const RGBA& rhs)
{
    return !(lhs == rhs);
}
RGBA operator & (const RGBA& lhs, const RGBA& rhs)
{
    RGBA ret;
    ret.r = lhs.r & rhs.r;
    ret.g = lhs.g & rhs.g;
    ret.b = lhs.b & rhs.b;
    ret.a = lhs.a & rhs.a;
    return ret;
}
RGBA operator | (const RGBA& lhs, const RGBA& rhs)
{
    RGBA ret;
    ret.r = lhs.r | rhs.r;
    ret.g = lhs.g | rhs.g;
    ret.b = lhs.b | rhs.b;
    ret.a = lhs.a | rhs.a;
    return ret;
}

RGBA operator >> (const RGBA& lhs, unsigned bits)
{
    RGBA ret;
    ret.r = lhs.r >> bits;
    ret.g = lhs.g >> bits;
    ret.b = lhs.b >> bits;
    ret.a = lhs.a >> bits;
    return ret;
}

fRGBA operator + (const fRGBA& lhs, const fRGBA& rhs)
{
    fRGBA ret;
    ret.r = lhs.r + rhs.r;
    ret.g = lhs.g + rhs.g;
    ret.b = lhs.b + rhs.b;
    ret.a = lhs.a + rhs.a;
    return ret;
}
fRGBA operator * (const fRGBA& lhs, float scaler)
{
    fRGBA ret;
    ret.r = lhs.r * scaler;
    ret.g = lhs.g * scaler;
    ret.b = lhs.b * scaler;
    ret.a = lhs.a * scaler;
    return ret;
}
fRGBA operator * (float scaler, const fRGBA& rhs)
{
    fRGBA ret;
    ret.r = rhs.r * scaler;
    ret.g = rhs.g * scaler;
    ret.b = rhs.b * scaler;
    ret.a = rhs.a * scaler;
    return ret;
}

fRGB operator + (const fRGB& lhs, const fRGB& rhs)
{
    fRGB ret;
    ret.r = lhs.r + rhs.r;
    ret.g = lhs.g + rhs.g;
    ret.b = lhs.b + rhs.b;
    return ret;
}
fRGB operator * (const fRGB& lhs, float scaler)
{
    fRGB ret;
    ret.r = lhs.r * scaler;
    ret.g = lhs.g * scaler;
    ret.b = lhs.b * scaler;
    return ret;
}
fRGB operator * (float scaler, const fRGB& rhs)
{
    fRGB ret;
    ret.r = rhs.r * scaler;
    ret.g = rhs.g * scaler;
    ret.b = rhs.b * scaler;
    return ret;
}

fGrayscale operator + (const fGrayscale& lhs, const fGrayscale& rhs)
{
    fGrayscale ret;
    ret.r = lhs.r + rhs.r;
    return ret;
}
fGrayscale operator * (const fGrayscale& lhs, float scaler)
{
    fGrayscale ret;
    ret.r = lhs.r * scaler;
    return ret;
}
fGrayscale operator * (float scaler, const fGrayscale& rhs)
{
    fGrayscale ret;
    ret.r = rhs.r * scaler;
    return ret;
}

float sRGB_decode(float value)
{
    return value <= 0.04045f
           ? value / 12.92f
           : std::pow((value + 0.055f) / 1.055f, 2.4f);
}
float sRGB_encode(float value)
{
    return value <= 0.0031308f
           ? value * 12.92f
           : std::pow(value, 1.0f/2.4f) * 1.055f - 0.055f;
}

fRGBA sRGB_decode(const fRGBA& value)
{
    fRGBA ret;
    ret.r = sRGB_decode(value.r);
    ret.g = sRGB_decode(value.g);
    ret.b = sRGB_decode(value.b);
    // alpha is not sRGB encoded.
    ret.a = value.a;
    return ret;
}
fRGB sRGB_decode(const fRGB& value)
{
    fRGB ret;
    ret.r = sRGB_decode(value.r);
    ret.g = sRGB_decode(value.g);
    ret.b = sRGB_decode(value.b);
    return ret;
}
fRGBA sRGB_encode(const fRGBA& value)
{
    fRGBA ret;
    ret.r = sRGB_encode(value.r);
    ret.g = sRGB_encode(value.g);
    ret.b = sRGB_encode(value.b);
    // alpha is not sRGB encoded.
    ret.a = value.a;
    return ret;
}
fRGB sRGB_encode(const fRGB& value)
{
    fRGB ret;
    ret.r = sRGB_encode(value.r);
    ret.g = sRGB_encode(value.g);
    ret.b = sRGB_encode(value.b);
    return ret;
}

fRGBA RGB_u8_to_float(const RGBA& value)
{
    fRGBA ret;
    ret.r = value.r / 255.0f;
    ret.g = value.g / 255.0f;
    ret.b = value.b / 255.0f;
    ret.a = value.a / 255.0f;
    return ret;
}
fRGB RGB_u8_to_float(const RGB& value)
{
    fRGB ret;
    ret.r = value.r / 255.0f;
    ret.g = value.g / 255.0f;
    ret.b = value.b / 255.0f;
    return ret;
}
fGrayscale RGB_u8_to_float(const Grayscale& value)
{
    fGrayscale ret;
    ret.r = value.r / 255.0f;
    return ret;
}

RGBA RGB_u8_from_float(const fRGBA& value)
{
    RGBA ret;
    ret.r = value.r * 255;
    ret.g = value.g * 255;
    ret.b = value.b * 255;
    ret.a = value.a * 255;
    return ret;
}
RGB RGB_u8_from_float(const fRGB& value)
{
    RGB ret;
    ret.r = value.r * 255;
    ret.g = value.g * 255;
    ret.b = value.b * 255;
    return ret;
}
Grayscale RGB_u8_from_float(const fGrayscale& value)
{
    Grayscale ret;
    ret.r = value.r * 255;
    return ret;
}

void WritePPM(const IBitmapReadView& bmp, const std::string& filename)
{
    static_assert(sizeof(RGB) == 3,
        "Padding bytes found. Cannot copy RGB data as a byte stream.");

    std::ofstream out(filename, std::ios::binary);
    if (!out.is_open())
        throw std::runtime_error("failed to open file: " + filename);

    const auto width  = bmp.GetWidth();
    const auto height = bmp.GetHeight();
    const auto depth  = bmp.GetDepthBits() / 8;
    const auto bytes  = width * height * sizeof(RGB);

    std::vector<RGB> tmp;
    tmp.reserve(width * height);
    for (unsigned row=0; row<height; ++row)
    {
        for (unsigned col=0; col<width; ++col)
        {
            RGB value;
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
    if (src.GetDepthBits() == 32) {
        if (srgb) return ::BoxFilter<RGBA, fRGBA, true>(src);
        return ::BoxFilter<RGBA, fRGBA, false>(src);
    } else if (src.GetDepthBits() == 24) {
        if (srgb) return ::BoxFilter<RGB, fRGB, true> (src);
        return ::BoxFilter<RGB, fRGB, false>(src);
    } else if (src.GetDepthBits() == 8)
        return ::BoxFilter<Grayscale, fGrayscale, false>(src);
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
        return ::ConvertToLinear<RGBA, fRGBA>(src);
    else if (src.GetDepthBits() == 24)
        return ::ConvertToLinear<RGB, fRGB>(src);
    return nullptr;
}
std::unique_ptr<IBitmap> ConvertToLinear(const IBitmap& src)
{
    auto view = src.GetReadView();
    return ConvertToLinear(*view);
}

void NoiseBitmapGenerator::Randomize(unsigned min_prime_index, unsigned max_prime_index, unsigned layers)
{
    mLayers.clear();
    for (unsigned i=0; i<layers; ++i)
    {
        const auto prime_index = math::rand(min_prime_index, max_prime_index);
        const auto prime = boost::math::prime(prime_index);
        Layer layer;
        layer.prime0 = prime;
        layer.frequency = math::rand(1.0f, 100.0f);
        layer.amplitude = math::rand(1.0f, 255.0f);
        mLayers.push_back(layer);
    }
}

void NoiseBitmapGenerator::IntoJson(data::Writer& data) const
{
    data.Write("width", mWidth);
    data.Write("height", mHeight);
    for (const auto& layer : mLayers)
    {
        auto chunk = data.NewWriteChunk();
        chunk->Write("prime0", layer.prime0);
        chunk->Write("prime1", layer.prime1);
        chunk->Write("prime2", layer.prime2);
        chunk->Write("frequency", layer.frequency);
        chunk->Write("amplitude", layer.amplitude);
        data.AppendChunk("layers", std::move(chunk));
    }
}

bool NoiseBitmapGenerator::FromJson(const data::Reader& data)
{
    if (!data.Read("width", &mWidth) ||
        !data.Read("height", &mHeight))
        return false;
    for (unsigned i=0; i<data.GetNumChunks("layers"); ++i)
    {
        const auto& chunk = data.GetReadChunk("layers", i);
        Layer layer;
        if (!chunk->Read("prime0", &layer.prime0) ||
            !chunk->Read("prime1", &layer.prime1) ||
            !chunk->Read("prime2", &layer.prime2) ||
            !chunk->Read("frequency", &layer.frequency) ||
            !chunk->Read("amplitude", &layer.amplitude))
            return false;
        mLayers.push_back(std::move(layer));
    }
    return true;
}

std::unique_ptr<IBitmap> NoiseBitmapGenerator::Generate() const
{
    auto ret = std::make_unique<GrayscaleBitmap>();
    ret->Resize(mWidth, mHeight);
    const float w = mWidth;
    const float h = mHeight;
    for (unsigned y = 0; y < mHeight; ++y)
    {
        for (unsigned x = 0; x < mWidth; ++x)
        {
            float pixel = 0.0f;
            for (const auto& layer : mLayers)
            {
                const math::NoiseGenerator gen(layer.frequency, layer.prime0, layer.prime1, layer.prime2);
                const auto amplitude = math::clamp(0.0f, 255.0f, layer.amplitude);
                const auto sample = gen.GetSample(x / w, y / h);
                pixel += (sample * amplitude);
            }
            Grayscale px;
            px.r = math::clamp(0u, 255u, (unsigned) pixel);
            ret->SetPixel(y, x, px);
        }
    }
    return ret;
}
 size_t NoiseBitmapGenerator::GetHash() const
 {
     size_t hash = 0;
     hash = base::hash_combine(hash, mWidth);
     hash = base::hash_combine(hash, mHeight);
     for (const auto& layer : mLayers)
     {
         hash = base::hash_combine(hash, layer.prime0);
         hash = base::hash_combine(hash, layer.prime1);
         hash = base::hash_combine(hash, layer.prime2);
         hash = base::hash_combine(hash, layer.amplitude);
         hash = base::hash_combine(hash, layer.frequency);
     }
     return hash;
 }

} // namespace
