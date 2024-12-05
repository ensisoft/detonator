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

#include "base/logging.h"
#include "data/reader.h"
#include "data/writer.h"
#include "graphics/algo.h"
#include "graphics/bitmap.h"
#include "graphics/device.h"
#include "graphics/texture.h"
#include "graphics/texture_text_buffer_source.h"

namespace gfx
{

std::shared_ptr<IBitmap> TextureTextBufferSource::GetData() const
{
    // since this interface is returning a CPU side bitmap object
    // there's no way to use a texture based (bitmap) font here.
    if (mTextBuffer.GetRasterFormat() == TextBuffer::RasterFormat::Bitmap)
        return mTextBuffer.RasterizeBitmap();
    return nullptr;
}

Texture* TextureTextBufferSource::Upload(const Environment& env, Device& device) const
{
    auto* texture = device.FindTexture(GetGpuId());
    if (texture && !env.dynamic_content)
        return texture;

    size_t content_hash = 0;
    if (env.dynamic_content)
    {
        content_hash = base::hash_combine(content_hash, mTextBuffer.GetHash());
        content_hash = base::hash_combine(content_hash, mEffects);
        if (texture && texture->GetContentHash() == content_hash)
            return texture;
    }
    const auto format = mTextBuffer.GetRasterFormat();
    if (format == TextBuffer::RasterFormat::Bitmap)
    {
        if (!texture)
        {
            texture = device.MakeTexture(mId);
            texture->SetName(mName);
        }
        if (const auto& mask = mTextBuffer.RasterizeBitmap())
        {
            texture->SetContentHash(content_hash);
            texture->Upload(mask->GetDataPtr(), mask->GetWidth(), mask->GetHeight(), Texture::Format::AlphaMask, false);
            texture->SetFilter(Texture::MinFilter::Linear);
            texture->SetFilter(Texture::MagFilter::Linear);
            texture->SetContentHash(content_hash);

            // create a logical alpha texture with RGBA format.
            if (mEffects.any_bit())
                algo::ColorTextureFromAlpha(mId, texture, &device);

            // then apply effects
            if (mEffects.test(Effect::Edges))
                algo::DetectSpriteEdges(mId, texture, &device);
            if (mEffects.test(Effect::Blur))
                algo::ApplyBlur(mId, texture, &device);

            texture->GenerateMips();
        }
    }
    else if (format == TextBuffer::RasterFormat::Texture)
    {
        if (texture = mTextBuffer.RasterizeTexture(mId, mName, device))
        {
            texture->SetName(mName);
            texture->SetFilter(Texture::MinFilter::Linear);
            texture->SetFilter(Texture::MagFilter::Linear);
            texture->SetContentHash(content_hash);

            ASSERT(texture->GetFormat() == Texture::Format::RGBA ||
                   texture->GetFormat() == Texture::Format::sRGBA);

            // The frame buffer render produces a texture that doesn't play nice with
            // model space texture coordinates right now. Simplest solution for now is
            // to simply flip it horizontally...
            algo::FlipTexture(mId, texture, &device, algo::FlipDirection::Horizontal);

            if (mEffects.test(Effect::Edges))
                algo::DetectSpriteEdges(mId, texture, &device);
            if (mEffects.test(Effect::Blur))
                algo::ApplyBlur(mId, texture, &device);

            texture->GenerateMips();
        }
    }
    if (texture)
    {
        DEBUG("Uploaded new text texture. [name='%1', effects=%2]", mName, mEffects);
        return texture;
    }
    ERROR("Failed to rasterize text texture. [name='%1']", mName);
    return nullptr;
}

std::size_t TextureTextBufferSource::GetHash() const
{
    auto hash = mTextBuffer.GetHash();
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mEffects);
    return hash;
}

void TextureTextBufferSource::IntoJson(data::Writer& data) const
{
    auto chunk = data.NewWriteChunk();
    mTextBuffer.IntoJson(*chunk);
    data.Write("id", mId);
    data.Write("name", mName);
    data.Write("effects", mEffects);
    data.Write("buffer", std::move(chunk));
}
bool TextureTextBufferSource::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("name", &mName);
    ok &= data.Read("id",   &mId);
    if (data.HasValue("effects"))
        ok &= data.Read("effects", &mEffects);

    const auto& chunk = data.GetReadChunk("buffer");
    if (!chunk)
        return false;

    ok &= mTextBuffer.FromJson(*chunk);
    return ok;
}
} // namespace