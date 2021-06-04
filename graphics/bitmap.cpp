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
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include "base/json.h"
#include "base/hash.h"
#include "graphics/bitmap.h"

namespace gfx
{

nlohmann::json NoiseBitmapGenerator::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "width", mWidth);
    base::JsonWrite(json, "height", mHeight);
    for (const auto& layer : mLayers)
    {
        nlohmann::json js;
        base::JsonWrite(js, "prime0", layer.prime0);
        base::JsonWrite(js, "prime1", layer.prime1);
        base::JsonWrite(js, "prime2", layer.prime2);
        base::JsonWrite(js, "frequency", layer.frequency);
        base::JsonWrite(js, "amplitude", layer.amplitude);
        json["layers"].push_back(std::move(js));
    }
    return json;
}

bool NoiseBitmapGenerator::FromJson(const nlohmann::json& json)
{
    if (!base::JsonReadSafe(json, "width", &mWidth) ||
        !base::JsonReadSafe(json, "height", &mHeight))
        return false;
    if (!json.contains("layers"))
        return true;

    for (const auto& js : json["layers"].items())
    {
        const auto& obj = js.value();
        Layer layer;
        if (!base::JsonReadSafe(obj, "prime0", &layer.prime0) ||
            !base::JsonReadSafe(obj, "prime1", &layer.prime1) ||
            !base::JsonReadSafe(obj, "prime2", &layer.prime2) ||
            !base::JsonReadSafe(obj, "frequency", &layer.frequency) ||
            !base::JsonReadSafe(obj, "amplitude", &layer.amplitude))
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
