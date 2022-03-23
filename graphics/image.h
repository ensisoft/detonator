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

#pragma once

#include "config.h"

#include "warnpush.h"
#include "warnpop.h"

#include <string>

#include "base/assert.h"
#include "graphics/bitmap.h"

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
        Image(const std::string& URI);
        Image(const Image&) = delete;
       ~Image();

        // Try to load an image file identified by the given file
        // resource identifier. The identifier can be an encoded
        // identifier such as "app://foo/bar/image.png" or a "raw"
        // path such as /home/user/image.png. If the file is a URI
        // it is resolved through the ResourceLoader.
        // On error returns false and the image object remains
        // unchanged.
        bool Load(const std::string& URI);

        // Copy (and optionally convert) the pixel contents of the
        // image into a specific type of a bitmap object.
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
        // Get a view to mutable bitmap data.
        // Important, the returned object may not be accessed
        // after the image has ceased to exist. These views should
        // only be used for a short term while accessing the contents.
        std::unique_ptr<IMutableBitmapView> GetWriteView();

        // Get view to immutable bitmap data.
        // Important, the returned object may not be accessed
        // after the image has ceased to exist. These views should
        // only be used for a short term while accessing the contents.
        std::unique_ptr<IConstBitmapView> GetReadView() const;

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
