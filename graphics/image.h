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

#pragma once

#include "config.h"

#include "warnpush.h"
#include "warnpop.h"

#include <string>

#include "base/assert.h"
#include "graphics/bitmap.h"
#include "graphics/resource.h"

namespace gfx
{
    // Load compressed images from files such as .jpg or .png into CPU memory
    class Image
    {
    public:
        // Construct an invalid image. (IsValid will be false).
        // You'll need to explicitly call Load after this.
        Image() = default;

        // Construct a new image object and try to load an image
        // immediately using the given file URI.
        // If the image load fails the object will be constructed
        // (i.e. no exception is thrown) but IsValid will be false.
        // The optional resource type is a hint to the resource loader
        // (in case of using encoded file identifier) as to what's the
        // purpose of the data.
        Image(const std::string& URI, ResourceLoader::ResourceType hint = ResourceLoader::ResourceType::Image);
        Image(const Image&) = delete;
       ~Image();

        // Try to load an image file identified by the given file
        // resource identifier. The identifier can be an encoded
        // identifier such as "app://foo/bar/image.png" or a "raw"
        // path such as /home/user/image.png. If the file is a URI
        // it is resolved through the ResourceLoader.
        // On error returns false and the image object remains
        // unchanged.
        bool Load(const std::string& URI, ResourceLoader::ResourceType hint = ResourceLoader::ResourceType::Image);

        // Copy (and optionally convert) the pixel contents of the
        // image into a specific type of bitmap object.
        // The bitmap allows for more fine grained control over
        // the pixel data such as GetPixel/SetPixel if that's what you need.
        // If the image cannot be represented as a bitmap of any type
        // known to the system an invalid bitmap will be returned.
        template<typename PixelT>
        Bitmap<PixelT> AsBitmap() const
        {
            ASSERT(mData);
            if (mDepth == sizeof(PixelT))
                return Bitmap<PixelT>(reinterpret_cast<const PixelT*>(mData), mWidth, mHeight);

            Bitmap<PixelT> ret(mWidth, mHeight);
            if (mDepth == 1)
                ret.Copy(0, 0, mWidth, mHeight, reinterpret_cast<const Grayscale*>(mData));
            else if (mDepth == 3)
                ret.Copy(0, 0, mWidth, mHeight, reinterpret_cast<const RGB*>(mData));
            else if (mDepth == 4)
                ret.Copy(0, 0, mWidth, mHeight, reinterpret_cast<const RGBA*>(mData));

            return ret;
        }
        // Returns true if the image has been loaded, otherwise false.
        bool IsValid() const
        { return mData != nullptr; }
        // Get image width in pixels.
        unsigned GetWidth() const
        { return mWidth; }
        // Get image height in pixels.
        unsigned GetHeight() const
        { return mHeight; }
        // Get the depth of the image in bits.
        unsigned GetDepthBits() const
        { return mDepth * 8; }
        // Get the raw data pointer to the image bits.
        const void* GetData() const
        { return mData; }

        Image& operator=(const Image&) = delete;
    private:
        std::string mURI;
        unsigned mWidth  = 0;
        unsigned mHeight = 0;
        unsigned mDepth  = 0;
        unsigned char* mData = nullptr;
    };
} // namespace
