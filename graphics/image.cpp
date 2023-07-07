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
#  include <stb/stb_image.h>
#include "warnpop.h"

#include <algorithm>
#include "graphics/image.h"
#include "graphics/loader.h"
#include "graphics/resource.h"

namespace gfx
{

Image::Image(const std::string& URI)
{
    const auto& buffer = gfx::LoadResource(URI);
    if (!buffer)
        return;

    int xres  = 0;
    int yres  = 0;
    int depth = 0;
    auto* bmp = stbi_load_from_memory((const stbi_uc*)buffer->GetData(),
                                      (int)buffer->GetByteSize(), &xres, &yres, &depth, 0);
    if (bmp == nullptr)
        return;

    mURI    = URI;
    mWidth  = static_cast<unsigned>(xres);
    mHeight = static_cast<unsigned>(yres);
    mDepth  = static_cast<unsigned>(depth);
    mData   = bmp;
}
Image::Image(const void* data, size_t bytes)
{
    if (!data || !bytes)
        return;
    int xres  = 0;
    int yres  = 0;
    int depth = 0;
    auto* bmp = stbi_load_from_memory((const stbi_uc*)data, bytes, &xres, &yres, &depth, 0);
    if (bmp == nullptr)
        return;

    mWidth  = static_cast<unsigned>(xres);
    mHeight = static_cast<unsigned>(yres);
    mDepth  = static_cast<unsigned>(depth);
    mData   = bmp;
}

Image::~Image()
{
    if (mData)
        stbi_image_free(mData);
}

bool Image::Load(const std::string& URI)
{
    Image temp(URI);
    if (!temp.IsValid())
        return false;
    std::swap(mURI, temp.mURI);
    std::swap(mWidth, temp.mWidth);
    std::swap(mHeight, temp.mHeight);
    std::swap(mDepth, temp.mDepth);
    std::swap(mData, temp.mData);
    return true;
}
bool Image::Load(const void* data, size_t bytes)
{
    Image temp(data, bytes);
    if (!temp.IsValid())
        return false;
    std::swap(mURI, temp.mURI);
    std::swap(mWidth, temp.mWidth);
    std::swap(mHeight, temp.mHeight);
    std::swap(mDepth, temp.mDepth);
    std::swap(mData, temp.mData);
    return true;
}

std::unique_ptr<IBitmap> Image::GetBitmap() const
{
    std::unique_ptr<IBitmap> ret;
    if (mDepth == 1)
        ret.reset(new AlphaMask((Grayscale*)mData, mWidth, mHeight));
    else if (mDepth == 3)
        ret.reset(new Bitmap<RGB>((RGB*)mData, mWidth, mHeight));
    else if (mDepth == 4)
        ret.reset(new Bitmap<RGBA>((RGBA*)mData, mWidth, mHeight));
    return ret;
}

std::unique_ptr<IBitmapWriteView> Image::GetWriteView()
{
    std::unique_ptr<IBitmapWriteView> ret;
    if (mDepth == 1)
        ret.reset(new BitmapWriteView<Grayscale>((Grayscale*)mData, mWidth, mHeight));
    else if (mDepth == 3)
        ret.reset(new BitmapWriteView<RGB>((RGB*)mData, mWidth, mHeight));
    else if (mDepth == 4)
        ret.reset(new BitmapWriteView<RGBA>((RGBA*)mData, mWidth, mHeight));
    return ret;
}

std::unique_ptr<IBitmapReadView> Image::GetReadView() const
{
    std::unique_ptr<IBitmapReadView> ret;
    if (mDepth == 1)
        ret.reset(new BitmapReadView<Grayscale>((const Grayscale*)mData, mWidth, mHeight));
    else if (mDepth == 3)
        ret.reset(new BitmapReadView<RGB>((const RGB*)mData, mWidth, mHeight));
    else if (mDepth == 4)
        ret.reset(new BitmapReadView<RGBA>((const RGBA*)mData, mWidth, mHeight));
    return ret;
}

} // namespace
