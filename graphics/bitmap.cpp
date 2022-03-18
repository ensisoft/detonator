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

#include <fstream>

#include "base/hash.h"
#include "base/math.h"
#include "data/writer.h"
#include "data/reader.h"
#include "graphics/bitmap.h"

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

void WritePPM(const IBitmapView& bmp, const std::string& filename)
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
            tmp.push_back(bmp.ReadPixel(row, col));
        }
    }
    out << "P6 " << width << " " << height << " 255\n";
    out.write((const char*)&tmp[0], bytes);
    out.close();
}

void WritePNG(const IBitmapView& bmp, const std::string& filename)
{
    const auto w = bmp.GetWidth();
    const auto h = bmp.GetHeight();
    const auto d = bmp.GetDepthBits() / 8;
    if (!stbi_write_png(filename.c_str(), w, h, d, bmp.GetDataPtr(), d * w))
        throw std::runtime_error(std::string("failed to write png: " + filename));
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
