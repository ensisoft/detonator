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

#include "warnpush.h"
#  include <base64/base64.h>
#include "warnpop.h"

#include "base/logging.h"
#include "data/reader.h"
#include "data/writer.h"
#include "graphics/device.h"
#include "graphics/texture.h"
#include "graphics/texture_bitmap_buffer_source.h"

namespace gfx
{

std::size_t TextureBitmapBufferSource::GetHash() const
{
    size_t hash = mBitmap->GetHash();
    hash = base::hash_combine(hash, mBitmap->GetWidth());
    hash = base::hash_combine(hash, mBitmap->GetHeight());
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mEffects);
    hash = base::hash_combine(hash, mColorSpace);
    return hash;
}

Texture* TextureBitmapBufferSource::Upload(const Environment& env, Device& device) const
{
    auto* texture = device.FindTexture(GetGpuId());
    if (texture && !env.dynamic_content)
        return texture;

    size_t content_hash = 0;
    if (env.dynamic_content)
    {
        content_hash = base::hash_combine(content_hash, mBitmap->GetWidth());
        content_hash = base::hash_combine(content_hash, mBitmap->GetHeight());
        content_hash = base::hash_combine(content_hash, mBitmap->GetHash());
        content_hash = base::hash_combine(content_hash, mEffects);
        content_hash = base::hash_combine(content_hash, mColorSpace);
        if (texture && texture->GetContentHash() == content_hash)
            return texture;
    }
    if (!texture)
    {
        texture = device.MakeTexture(mId);
        texture->SetName(mName);
    }

    const auto sRGB = mColorSpace == ColorSpace::sRGB;
    constexpr auto generate_mips = true;

    texture->SetName(mName);
    texture->SetContentHash(content_hash);
    texture->Upload(mBitmap->GetDataPtr(), mBitmap->GetWidth(), mBitmap->GetHeight(),
        Texture::DepthToFormat(mBitmap->GetDepthBits(), sRGB), generate_mips);
    return texture;
}

void TextureBitmapBufferSource::IntoJson(data::Writer& data) const
{
    const auto depth = mBitmap->GetDepthBits() / 8;
    const auto width = mBitmap->GetWidth();
    const auto height = mBitmap->GetHeight();
    const auto bytes = width * height * depth;
    data.Write("id",     mId);
    data.Write("name",   mName);
    data.Write("width",  width);
    data.Write("height", height);
    data.Write("depth",  depth);
    data.Write("data", base64::Encode((const unsigned char*)mBitmap->GetDataPtr(), bytes));
    data.Write("effects", mEffects);
    data.Write("colorspace", mColorSpace);
}
bool TextureBitmapBufferSource::FromJson(const data::Reader& data)
{
    bool ok = true;
    unsigned width = 0;
    unsigned height = 0;
    unsigned depth = 0;
    std::string base64;
    ok &= data.Read("id",     &mId);
    ok &= data.Read("name",   &mName);
    ok &= data.Read("width",  &width);
    ok &= data.Read("height", &height);
    ok &= data.Read("depth",  &depth);
    ok &= data.Read("data",   &base64);
    if (data.HasValue("effects"))
        ok &= data.Read("effects", &mEffects);
    if (data.HasValue("colorspace"))
        ok &= data.Read("colorspace", &mColorSpace);

    const auto& bits = base64::Decode(base64);
    if (depth == 1 && bits.size() == width*height)
        mBitmap = std::make_shared<AlphaMask>((const Pixel_A*) &bits[0], width, height);
    else if (depth == 3 && bits.size() == width*height*3)
        mBitmap = std::make_shared<RgbBitmap>((const Pixel_RGB*) &bits[0], width, height);
    else if (depth == 4 && bits.size() == width*height*4)
        mBitmap = std::make_shared<RgbaBitmap>((const Pixel_RGBA*) &bits[0], width, height);
    else return false;
    return ok;
}


} // namespace