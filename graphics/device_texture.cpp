// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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
#include "device/graphics.h"
#include "graphics/device_texture.h"

namespace gfx {

DeviceTexture::~DeviceTexture()
{
    if (mTexture.IsValid())
    {
        mDevice->DeleteTexture(mTexture);
        if (!IsTransient())
            DEBUG("Deleted texture object. [name='%1']", mName);
    }
}

void DeviceTexture::Upload(const void* bytes, unsigned width, unsigned height, Format format)
{
    if (mTexture.IsValid())
    {
        mDevice->DeleteTexture(mTexture);
    }

    if (bytes)
    {
        mTexture = mDevice->UploadTexture2D(bytes, width, height, format);
        if (!IsTransient())
        {
            DEBUG("Uploaded new texture object. [name='%1', size=%2x%3]", mName, width, height);
        }
    }
    else
    {
        mTexture = mDevice->AllocateTexture2D(width, height, format);
        if (!IsTransient())
        {
            DEBUG("Allocated new texture object. [name='%1', size=%2x%3]", mName, width, height);
        }
    }
    mWidth  = width;
    mHeight = height;
    mFormat = format;
    mArraySize = 0;
    mHasMips = false;
}

void DeviceTexture::Allocate(unsigned width, unsigned height, Format format)
{
    if (mTexture.IsValid())
    {
        mDevice->DeleteTexture(mTexture);
    }
    mTexture = mDevice->AllocateTexture2D(width, height, format);
    if (!IsTransient())
    {
        DEBUG("Allocated new texture object. [name='%1', size=%2x%3]", mName, width, height);
    }
    mWidth  = width;
    mHeight = height;
    mFormat = format;
    mArraySize = 0;
    mHasMips = false;
}

void DeviceTexture::AllocateArray(unsigned width, unsigned height, unsigned array_size, Format format)
{
    if (mTexture.IsValid())
    {
        mDevice->DeleteTexture(mTexture);
    }
    mTexture = mDevice->AllocateTexture2DArray(width, height, array_size, format);
    if (!IsTransient())
    {
        DEBUG("Allocated new texture array object. [name='%1', size=%2x%3,%4]", mName, width, height, array_size);
    }

    mWidth  = width;
    mHeight = height;
    mFormat = format;
    mArraySize = array_size;
    mHasMips = false;
}

bool DeviceTexture::GenerateMips()
{
    ASSERT(mTexture.IsValid());
    ASSERT(mTexture.texture_width && mTexture.texture_height);

    if (mHasMips)
        return true;

    const auto ret = mDevice->GenerateMipmaps(mTexture);
    if (ret == dev::GraphicsDevice::MipStatus::UnsupportedSize)
        WARN("Unsupported texture size for mipmap generation. [name='%1]", mName);
    else if (ret == dev::GraphicsDevice::MipStatus::UnsupportedFormat)
        WARN("Unsupported texture format for mipmap generation. [name='%1']", mName);
    else if (ret == dev::GraphicsDevice::MipStatus::Error)
        WARN("Failed to generate mips on texture. [name='%1']", mName);
    else if (ret == dev::GraphicsDevice::MipStatus::Success)
    {
        if (!IsTransient())
            DEBUG("Successfully generated texture mips. [name='%1']", mName);
    }
    else BUG("Bug on mipmap status.");

    mHasMips = ret == dev::GraphicsDevice::MipStatus::Success;
    return mHasMips;
}


} // namespace