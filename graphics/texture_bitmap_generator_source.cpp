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

#include "base/logging.h"
#include "data/reader.h"
#include "data/writer.h"
#include "graphics/device_algo.h"
#include "graphics/device.h"
#include "graphics/texture.h"
#include "graphics/texture_bitmap_generator_source.h"

namespace gfx
{

Texture* TextureBitmapGeneratorSource::Upload(const Environment& env, Device& device) const
{
    auto* texture = device.FindTexture(GetGpuId());
    if (texture && !env.dynamic_content)
        return texture;

    size_t content_hash = 0;
    if (env.dynamic_content)
    {
        content_hash = mGenerator->GetHash();
        content_hash = base::hash_combine(content_hash, mEffects);
        if (texture && texture->GetContentHash() == content_hash)
            return texture;
    }
    if (!texture)
    {
        texture = device.MakeTexture(mId);
        texture->SetName(mName);
    }

    // todo: assuming linear color space now
    constexpr auto sRGB = false;
    constexpr auto generate_mips = true;

    if (const auto& bitmap = mGenerator->Generate())
    {
        texture->SetContentHash(content_hash);
        texture->Upload(bitmap->GetDataPtr(), bitmap->GetWidth(), bitmap->GetHeight(),
            Texture::DepthToFormat(bitmap->GetDepthBits(), sRGB));
        texture->SetFilter(Texture::MinFilter::Linear);
        texture->SetFilter(Texture::MagFilter::Linear);

        const auto format = texture->GetFormat();
        if (mEffects.any_bit() && format == Texture::Format::AlphaMask)
            algo::ColorTextureFromAlpha(mId, texture, &device);

        if (mEffects.test(Effect::Edges))
            algo::DetectSpriteEdges(mId, texture, &device);
        if (mEffects.test(Effect::Blur))
            algo::ApplyBlur(mId, texture, &device);

        texture->GenerateMips();
        DEBUG("Uploaded bitmap generator texture. [name='%1', effects=%2]", mName, mEffects);
        return texture;
    } else ERROR("Failed to generate bitmap generator texture. [name='%1']", mName);
    return nullptr;
}

void TextureBitmapGeneratorSource::IntoJson(data::Writer& data) const
{
    auto chunk = data.NewWriteChunk();
    mGenerator->IntoJson(*chunk);
    data.Write("id", mId);
    data.Write("name", mName);
    data.Write("function", mGenerator->GetFunction());
    data.Write("generator", std::move(chunk));
    data.Write("effects", mEffects);
}
bool TextureBitmapGeneratorSource::FromJson(const data::Reader& data)
{
    IBitmapGenerator::Function function;
    bool ok = true;
    ok &= data.Read("id",       &mId);
    ok &= data.Read("name",     &mName);
    if (!data.Read("function", &function))
        return false;
    if (data.HasValue("effects"))
        ok &= data.Read("effects", &mEffects);

    if (function == IBitmapGenerator::Function::Noise)
        mGenerator = std::make_unique<NoiseBitmapGenerator>();
    else BUG("Unhandled bitmap generator type.");

    const auto& chunk = data.GetReadChunk("generator");
    if (!chunk || !mGenerator->FromJson(*chunk))
        return false;

    return ok;
}

} // namespace