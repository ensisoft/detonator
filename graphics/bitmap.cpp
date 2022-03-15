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
#include "warnpop.h"

#include "base/hash.h"
#include "base/math.h"
#include "data/writer.h"
#include "data/reader.h"
#include "graphics/bitmap.h"

namespace gfx
{

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
