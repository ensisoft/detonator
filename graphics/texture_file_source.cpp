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
#include "data/writer.h"
#include "data/reader.h"
#include "graphics/device_algo.h"
#include "graphics/device.h"
#include "graphics/texture.h"
#include "graphics/image.h"
#include "graphics/texture_file_source.h"
#include "graphics/packer.h"

namespace gfx
{

std::string TextureFileSource::GetGpuId() const
{
    // using the mFile URI is *not* enough to uniquely
    // identify this texture object on the GPU because it's
    // possible that the *same* texture object (same underlying file)
    // is used with *different* flags in another material.
    // in other words, "foo.png with pre-multiplied alpha" must be
    // a different GPU texture object than "foo.png with straight alpha".
    size_t gpu_hash = 0;
    gpu_hash = base::hash_combine(gpu_hash, mFile);
    gpu_hash = base::hash_combine(gpu_hash, mColorSpace);
    gpu_hash = base::hash_combine(gpu_hash, mFlags);
    gpu_hash = base::hash_combine(gpu_hash, mEffects);
    return std::to_string(gpu_hash);
}

size_t TextureFileSource::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mFile);
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mFlags);
    hash = base::hash_combine(hash, mColorSpace);
    hash = base::hash_combine(hash, mEffects);
    return hash;
}

Texture* TextureFileSource::Upload(const Environment& env, Device& device) const
{
    const auto& gpu_id = GetGpuId();
    auto* texture = device.FindTexture(gpu_id);
    if (texture && !env.dynamic_content)
        return texture;

    // compute content hash on first time or every time if dynamic_content
    size_t content_hash = 0;
    if (env.dynamic_content || !texture)
        content_hash = base::hash_combine(0, mFile);

    if (env.dynamic_content)
    {
        // check for dynamic changes.
        if (texture && texture->GetContentHash() == content_hash)
            return texture;
    }

    if (!texture)
    {
        texture = device.MakeTexture(gpu_id);
        texture->SetName(mName.empty() ? mFile : mName);
        texture->SetContentHash(0);
    }
    else if (texture->GetContentHash() == 0)
    {
        // if the texture already exists but hash is zero that means we've been here
        // before and the texture has failed to load. In this case return early with
        // a nullptr and skip subsequent load attempts.
        // Note that in the editor if the user wants to reload a texture that was
        // for example just modified by an image editor tool there's explicitly
        // the reload mechanism that will nuke textures from the device.
        // After that is done, the next Upload will again have to recreate the
        // texture object on the device again.
        return nullptr;
    }

    if (const auto& bitmap = GetData())
    {
        const auto sRGB = mColorSpace == ColorSpace::sRGB;
        texture->SetContentHash(content_hash);
        texture->Upload(bitmap->GetDataPtr(),
                        bitmap->GetWidth(),
                        bitmap->GetHeight(),
                        Texture::DepthToFormat(bitmap->GetDepthBits(), sRGB));

        texture->SetFilter(Texture::MinFilter::Linear);
        texture->SetFilter(Texture::MagFilter::Linear);

        const auto format = texture->GetFormat();
        if (mEffects.any_bit() && format == Texture::Format::AlphaMask)
            algo::ColorTextureFromAlpha(gpu_id, texture, &device);

        if (mEffects.any_bit())
        {
            if (format == Texture::Format::RGBA || format == Texture::Format::sRGBA)
            {
                if (mEffects.test(Effect::Edges))
                    algo::DetectSpriteEdges(gpu_id, texture, &device);
                if (mEffects.test(Effect::Blur))
                    algo::ApplyBlur(gpu_id, texture, &device);
            } else WARN("Texture effects not supported on texture format. [name='%1', format='%2, effects=%3']", mName, format, mEffects);
        }

        texture->GenerateMips();
        DEBUG("Uploaded texture file source texture. [name='%1', file='%2', effects=%3]", mName, mFile, mEffects);
        return texture;
    } else ERROR("Failed to upload texture file source texture. [name='%1', file='%2']", mName, mFile);
    return nullptr;
}

std::shared_ptr<const IBitmap> TextureFileSource::GetData() const
{
    DEBUG("Loading texture file. [file='%1']", mFile);
    Image file(mFile);
    if (!file.IsValid())
    {
        ERROR("Failed to load texture image file. [file='%1']", mFile);
        return nullptr;
    }

    if (file.GetDepthBits() == 8)
        return std::make_shared<AlphaMask>(file.AsBitmap<Pixel_A>());
    else if (file.GetDepthBits() == 24)
        return std::make_shared<RgbBitmap>(file.AsBitmap<Pixel_RGB>());
    else if (file.GetDepthBits() == 32)
    {
        if (!TestFlag(Flags::PremulAlpha))
            return std::make_shared<RgbaBitmap>(file.AsBitmap<Pixel_RGBA>());

        auto view = file.GetPixelReadView<Pixel_RGBA>();
        auto ret = std::make_shared<Bitmap<Pixel_RGBA>>();
        ret->Resize(view.GetWidth(), view.GetHeight());
        DEBUG("Performing alpha pre-multiply on texture. [file='%1']", mFile);
        PremultiplyAlpha(ret->GetPixelWriteView(), view, true /* srgb */);
        return ret;
    }
    else ERROR("Unexpected texture bit depth. [file='%1', depth=%2]", mFile, file.GetDepthBits());

    return nullptr;
}
void TextureFileSource::IntoJson(data::Writer& data) const
{
    data.Write("id",         mId);
    data.Write("file",       mFile);
    data.Write("name",       mName);
    data.Write("flags",      mFlags);
    data.Write("colorspace", mColorSpace);
    data.Write("effects",    mEffects);
}
bool TextureFileSource::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",         &mId);
    ok &= data.Read("file",       &mFile);
    ok &= data.Read("name",       &mName);
    ok &= data.Read("flags",      &mFlags);
    ok &= data.Read("colorspace", &mColorSpace);
    if (data.HasValue("effects"))
        ok &= data.Read("effects", &mEffects);
    return ok;
}

void TextureFileSource::BeginPacking(TexturePacker* packer) const
{
    packer->PackTexture(this, mFile);
    packer->SetTextureFlag(this, TexturePacker::TextureFlags::AllowedToPack,
                           TestFlag(Flags::AllowPacking));
    packer->SetTextureFlag(this, TexturePacker::TextureFlags::AllowedToResize,
                           TestFlag(Flags::AllowResizing));
}
void TextureFileSource::FinishPacking(const TexturePacker* packer)
{
    mFile = packer->GetPackedTextureId(this);
}

} // namespace