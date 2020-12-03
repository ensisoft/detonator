// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#include "config.h"

#include "warnpush.h"
#  include <stb/stb_image.h>
#include "warnpop.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

#include "base/logging.h"
#include "base/utility.h"
#include "graphics/image.h"
#include "graphics/resource.h"

namespace gfx
{

Image::Image(const std::string& URI, ResourceLoader::ResourceType hint)
{
    // resolve file first.
    const auto& buffer = LoadResource(hint, URI);
    if (!buffer)
    {
        ERROR("Failed to load image: '%1' buffer", URI);
        return;
    }
    int xres  = 0;
    int yres  = 0;
    int depth = 0;
    auto* bmp = stbi_load_from_memory((const stbi_uc*)buffer->GetData(),
                                      (int)buffer->GetSize(), &xres, &yres, &depth, 0);
    if (bmp == nullptr)
    {
        ERROR("Decompressing image file '%1' failed.", URI);
        return;
    }
    mURI    = URI;
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

bool Image::Load(const std::string& URI, ResourceLoader::ResourceType hint)
{
    Image temp(URI, hint);
    if (!temp.IsValid())
        return false;
    std::swap(mURI, temp.mURI);
    std::swap(mWidth, temp.mWidth);
    std::swap(mHeight, temp.mHeight);
    std::swap(mDepth, temp.mDepth);
    std::swap(mData, temp.mData);
    return true;
}

} // namespace
