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

#include "data/writer.h"
#include "data/reader.h"
#include "graphics/device.h"
#include "graphics/texture_texture_source.h"

namespace gfx
{

Texture* TextureTextureSource::Upload(const Environment& env, Device& device) const
{
    if (!mTexture)
        mTexture = device.FindTexture(mGpuId);

    return mTexture;
}

void TextureTextureSource::IntoJson(data::Writer& data) const
{
    data.Write("id",     mId);
    data.Write("name",   mName);
    data.Write("gpu_id", mGpuId);
}

bool TextureTextureSource::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",     &mId);
    ok &= data.Read("name",   &mName);
    ok &= data.Read("gpu_id", &mGpuId);
    return ok;
}

} // namespace