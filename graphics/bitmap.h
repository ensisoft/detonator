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

#include <limits>
#include <vector>
#include <cstring>
#include <type_traits>
#include <string>
#include <algorithm>
#include <functional>
#include <memory>

#include "base/assert.h"
#include "base/hash.h"
#include "graphics/pixel.h"
#include "graphics/types.h"
#include "graphics/bitmap_interface.h"
#include "graphics/bitmap_view.h"
#include "graphics/bitmap_algo.h"

#include "base/snafu.h"

namespace gfx
{

    // Bitmap interface. Mostly designed so that it's possible
    // to keep bitmap objects around as generic bitmaps
    // regardless of their actual underlying pixel representation.
    // Extends the read only IBitmapReadView with functionality that
    // mutates the actual bitmap object.
    class IBitmap
    {
    public:
        virtual ~IBitmap() = default;
        // Get the width of the bitmap
        virtual unsigned GetWidth() const = 0;
        // Get the height of the bitmap
        virtual unsigned GetHeight() const = 0;
        // Get the depth of the bitmap
        virtual unsigned GetDepthBits() const = 0;
        // Get pointer to underlying data. Can be nullptr if the bitmap
        // is not currently valid, i.e. no pixel storage has been allocated.
        // Access to the data through this pointer is potentially
        // unsafe since the type of the pixel (i.e. the memory layout)
        // information is lost. However, in some cases the generic
        // access is useful for example when passing the pointer to some
        // other APIs
        virtual const void* GetDataPtr() const = 0;
        virtual       void* GetDataPtr()       = 0;
        // Get whether bitmap is valid or not (has been allocated)
        virtual bool IsValid() const = 0;
        // Resize the bitmap. Previous are undefined.
        virtual void Resize(unsigned width, unsigned height) = 0;
        // Flip the rows of the bitmap around horizontal center line/row.
        // Transforms the bitmap as follows:
        // aaaa           cccc
        // bbbb  becomes  bbbb
        // cccc           aaaa
        virtual void FlipHorizontally() = 0;
        // Get a read only view into the contents of the bitmap.
        virtual std::unique_ptr<IBitmapReadView> GetReadView() const = 0;
        // Get a write only view into the contents of the bitmap.
        virtual std::unique_ptr<IBitmapWriteView> GetWriteView() = 0;
        // Get a read/write view into the contents of the bitmap.
        virtual std::unique_ptr<IBitmapReadWriteView> GetReadWriteView() = 0;
        // Make a clone of this bitmap.
        virtual std::unique_ptr<IBitmap> Clone() const = 0;
        // Get unique hash value based on the contents of the bitmap.
        virtual std::size_t GetHash() const = 0;
    protected:
        // No public dynamic copying or assignment
        IBitmap() = default;
        IBitmap(const IBitmap&) = default;
        IBitmap(IBitmap&&) = default;
        IBitmap& operator=(const IBitmap&) = default;
        IBitmap& operator=(IBitmap&&) = default;
    private:
    };


    template<typename Pixel>
    class Bitmap : public IBitmap
    {
    public:
        using PixelType = Pixel;

        static_assert(std::is_trivially_copyable<Pixel>::value,
            "Pixel should be a POD type and compatible with memcpy.");

        // Create an empty bitmap. This is mostly useful for
        // default constructing and object which can later be
        // assigned to.
        Bitmap() = default;

        // Create a bitmap with the given dimensions.
        // all the pixels are initialized to zero.
        Bitmap(unsigned width, unsigned height)
            : mWidth(width)
            , mHeight(height)
        {
            mPixels.resize(width * height);
        }

        // Create a bitmap from a vector of pixel data. The product of
        // width and height should match the number of pixels in the vector.
        Bitmap(const std::vector<Pixel>& data, unsigned width, unsigned height)
            : mPixels(data)
            , mWidth(width)
            , mHeight(height)
        {
            ASSERT(data.size() == width * height);
        }

        // Create a bitmap from a vector of pixel data. The product of
        // width and height should match the number of pixels in the vector.
        Bitmap(std::vector<Pixel>&& data, unsigned width, unsigned height)
            : mPixels(std::move(data))
            , mWidth(width)
            , mHeight(height)
        {
            ASSERT(data.size() == width * height);
        }

        // create a bitmap from the given data.
        // stride is the number of *bytes* per scanline.
        // if stride is 0 it will default to width * sizeof(Pixel)
        Bitmap(const Pixel* data, unsigned width, unsigned height, unsigned stride_in_bytes = 0)
            : mWidth(width)
            , mHeight(height)
        {
            mPixels.resize(width * height);
            // if the bitmap is being constructed with 0 width or 0 height
            // early return here because accessing mPixels[0] would then be invalid.
            if (mPixels.empty())
                return;

            const auto num_rows_to_copy = stride_in_bytes ? height : 1;
            const auto num_bytes_per_row = stride_in_bytes ? stride_in_bytes : sizeof(Pixel) * width * height;
            const auto ptr = (const u8*)data;
            for (std::size_t y=0; y<num_rows_to_copy; ++y)
            {
                const auto src = y * stride_in_bytes;
                const auto dst = y * width;
                std::memcpy(&mPixels[dst], &ptr[src], num_bytes_per_row);
            }
        }
        // IBitmap interface implementation

        // Get the width of the bitmap.
        unsigned GetWidth() const override
        { return mWidth; }
        // Get the height of the bitmap.
        unsigned GetHeight() const override
        { return mHeight; }
        // Get the depth of the bitmap
        unsigned GetDepthBits() const override
        { return sizeof(Pixel) * 8; }
        // Get whether the bitmap is valid or not (has been allocated)
        bool IsValid() const override
        { return mWidth != 0 && mHeight != 0; }
        // Resize the bitmap with the given dimensions.
        void Resize(unsigned width, unsigned height) override
        {
            Bitmap b(width, height);
            mPixels = std::move(b.mPixels);
            mWidth  = width;
            mHeight = height;
        }
        // Get the unsafe data pointer.
        const void* GetDataPtr() const override
        {
            return mPixels.empty() ? nullptr : (const void*)&mPixels[0];
        }
        void* GetDataPtr() override
        {
            return mPixels.empty() ? nullptr : (void*)&mPixels[0];
        }
        // Flip the rows of the bitmap around horizontal axis (the middle row)
        void FlipHorizontally() override
        {
            for (unsigned y=0; y<mHeight/2; ++y)
            {
                const auto top = y;
                const auto bot = mHeight - 1 - y;
                auto* src = &mPixels[top * mWidth];
                auto* dst = &mPixels[bot * mWidth];
                for (unsigned x=0; x<mWidth; ++x) {
                    std::swap(src[x], dst[x]);
                }
            }
        }
        std::size_t GetHash() const override
        {
            std::size_t hash_value = 0;
            if (mPixels.empty())
                return hash_value;

            const auto ptr = (const uint8_t*)&mPixels[0];
            const auto len  = mPixels.size() * sizeof(Pixel);
            for (size_t i=0; i<len; ++i)
                hash_value = base::hash_combine(hash_value, ptr[i]);
            return hash_value;
        }
        std::unique_ptr<IBitmapReadView> GetReadView() const override
        { return std::make_unique<BitmapReadView<Pixel>>((const Pixel*)GetDataPtr(), mWidth, mHeight); }
        std::unique_ptr<IBitmapWriteView> GetWriteView() override
        { return std::make_unique<BitmapWriteView<Pixel>>((Pixel*)GetDataPtr(), mWidth, mHeight); }
        std::unique_ptr<IBitmapReadWriteView> GetReadWriteView() override
        { return std::make_unique<BitmapReadWriteView<Pixel>>((Pixel*)GetDataPtr(), mWidth, mHeight); }
        std::unique_ptr<IBitmap> Clone() const override
        { return std::make_unique<Bitmap>(*this); }

        BitmapReadView<Pixel> GetPixelReadView() const
        { return BitmapReadView<Pixel>((const Pixel*)GetDataPtr(), mWidth, mHeight); }
        BitmapWriteView<Pixel> GetPixelWriteView()
        { return BitmapWriteView<Pixel>((Pixel*)GetDataPtr(), mWidth, mHeight); }
        BitmapReadWriteView<Pixel> GetPixelReadWriteView()
        { return BitmapReadWriteView<Pixel>((Pixel*)GetDataPtr(), mWidth, mHeight); }

        // Get a pixel from within the bitmap.
        // The pixel must be within this bitmap's width and height.
        const Pixel& GetPixel(unsigned row, unsigned col) const
        {
            ASSERT(row < mHeight);
            ASSERT(col < mWidth);
            const auto index = row * mWidth + col;
            return mPixels[index];
        }
        const Pixel& GetPixel(const UPoint& p) const
        {
            return GetPixel(p.GetY(), p.GetX());
        }

        // Set a pixel within the bitmap to a new pixel value.
        // The pixel's coordinates must be within this bitmap's
        // width and height.
        void SetPixel(unsigned row, unsigned col, const Pixel& value)
        {
            ASSERT(row < mHeight);
            ASSERT(col < mWidth);
            const auto index = row * mWidth + col;
            mPixels[index] = value;
        }
        void SetPixel(const UPoint& point, const Pixel& value)
        {
            SetPixel(point.GetY(), point.GetX(), value);
        }

        // Compare the pixels in this bitmap within the given rectangle
        // against the given reference pixel value with the given compare functor.
        // Returns false if any of the pixels compare to not equal as determined
        // by the compare functor.
        // Otherwise, if all pixels compare equal returns true.
        // This is mostly useful as a testing utility.
        template<typename CompareFunctor>
        bool PixelCompare(const URect& rc, const Pixel& reference, CompareFunctor comparer) const
        {
            return PixelCompareBitmapRegion(GetPixelReadView(),
                       rc, reference, std::move(comparer));
        }

        // Compare the pixels in this bitmap within the given
        // rectangle against the given reference pixel and expect
        // pixel perfect matching.
        bool PixelCompare(const URect& rc, const Pixel& reference) const
        {
            return PixelCompareBitmapRegion(GetPixelReadView(),
                       rc, reference, PixelEquality::PixelPrecision());
        }

        // Compare all pixels in this bitmap against the given
        // reference pixel for equality using the given compare functor.
        // Returns false if the compare functor identifies non-equality
        // for any pixel otherwise true.
        template<typename CompareFunctor>
        bool PixelCompare(const Pixel& reference, CompareFunctor comparer) const
        {
            return PixelCompareBitmapRegion(GetPixelReadView(),
                       URect(0, 0, mWidth, mHeight), reference, std::move(comparer));
        }

        // Compare all pixels in this bitmap against the given
        // reference pixel and expect pixel perfect matching.
        // Returns false if any pixel doesn't match the reference
        // otherwise true.
        bool PixelCompare(const Pixel& reference) const
        {
            return PixelCompareBitmapRegion(GetPixelReadView(),
                       URect(0, 0, mWidth, mHeight), reference, PixelEquality::PixelPrecision());
        }

        // Fill the area defined by the rectangle rc with the given pixel value.
        // The rectangle is clipped to the pixmap borders.
        void Fill(const IRect& rect, const Pixel& value)
        {
            FillBitmap(GetPixelWriteView(), IRect(rect), value);
        }
        void Fill(const URect& rect, const Pixel& value)
        {
            FillBitmap(GetPixelWriteView(), IRect(rect), value);
        }

        // Fill the entire bitmap with the given pixel value.
        void Fill(const Pixel& value)
        {
            if (sizeof(Pixel) == 1)
            {
                std::memset(&mPixels[0], value.r, mPixels.size());
            }
            else
            {
                FillBitmap(GetPixelWriteView(), IRect(0, 0, mWidth, mHeight), value);
            }
        }

        template<typename RasterOp>
        void Blit(int x, int y, const Bitmap& src, RasterOp raster_op)
        {
            auto& dst = *this;
            BlitBitmap(dst.GetPixelReadWriteView(),
                       src.GetPixelReadView(),
                       IPoint(x, y),
                       IRect(0, 0, src.GetWidth(), src.GetHeight()), raster_op);
        }
        template<typename RasterOp>
        void Blit(int x, int y, const BitmapReadView<Pixel>& src, RasterOp raster_op)
        {
            auto& dst = *this;
            BlitBitmap(dst.GetPixelReadWriteView(),
                       src,
                       IPoint(x, y),
                       IRect(0, 0, src.GetWidth(), src.GetHeight()), raster_op);
        }
        template<typename RasterOp>
        void Blit(int x, int y, unsigned src_width, unsigned src_height, const Pixel* src, RasterOp raster_op)
        {
            auto& dst = *this;
            BlitBitmap(dst.GetPixelReadWriteView(),
                       BitmapReadView<Pixel>(src, src_width, src_height),
                       IPoint(x, y),
                       IRect(0, 0, src_width, src_height), raster_op);
        }

        // Copy data from the given other bitmap into this bitmap.
        // The destination position can be negative.
        // Any pixel that is not within the bounds of this bitmap will be clipped.
        // This will perform a direct bitwise pixel transfer without any regard
        // for the underlying color space.
        void Copy(int x, int y, const Bitmap& src)
        {
            auto& dst = *this;
            CopyBitmap(dst.GetPixelWriteView(),
                       src.GetPixelReadView(),
                       IPoint(x, y),
                       IRect(0, 0, src.mWidth, src.mHeight));
        }
        void Copy(int x, int y, const BitmapReadView<Pixel>& src)
        {
            auto& dst = *this;
            CopyBitmap(dst.GetPixelWriteView(),
                       src,
                       IPoint(x, y),
                       IRect(0, 0, src.GetWidth(), src.GetHeight()));
        }
        void Copy(int x, int y, unsigned src_width, unsigned src_height, const Pixel* src)
        {
            auto& dst = *this;
            CopyBitmap(dst.GetPixelWriteView(),
                       BitmapReadView<Pixel>(src, src_width, src_height),
                       IPoint(x, y),
                       IRect(0, 0, src_width, src_height));
        }

        // Copy a region of pixels from this bitmap into a new bitmap.
        Bitmap Copy(const URect& rect) const
        {
            const auto& rc = Intersect(URect(0, 0, mWidth, mHeight), rect);

            Bitmap ret;
            ret.Resize(rc.GetWidth(), rc.GetHeight());

            auto& src = *this;
            CopyBitmap(ret.GetPixelWriteView(),
                       src.GetPixelReadView(),
                       IPoint(0, 0), IRect(rc));

            return ret;
        }

        URect GetRect() const
        { return URect(0u, 0u, mWidth, mHeight); }
        USize GetSize() const
        { return USize(mWidth, mHeight); }
    private:
        std::vector<Pixel> mPixels;
        unsigned mWidth  = 0;
        unsigned mHeight = 0;
    private:
        template<typename T> friend bool operator==(const Bitmap<T>& lhs, const Bitmap<T>& rhs);
        template<typename T> friend bool operator!=(const Bitmap<T>& lhs, const Bitmap<T>& rhs);
    };

    using AlphaMask  = Bitmap<Pixel_A>;
    using RgbBitmap  = Bitmap<Pixel_RGB>;
    using RgbaBitmap = Bitmap<Pixel_RGBA>;

    // Compare the pixels between two pixmaps within the given rectangle using
    // the given compare functor.
    // The rectangle is clipped to stay within the bounds of both bitmaps.
    // Returns false if the compare functor identifies non-equality for any pixel.
    // Otherwise returns true the pixels between the two bitmaps compare equal
    // as determined by the compare functor.
    template<typename CompareF, typename PixelT>
    bool PixelCompare(const Bitmap<PixelT>& lhs, const URect& rc, const Bitmap<PixelT>& rhs, CompareF comparer)
    {
        // take the intersection of the bitmaps and then intersection
        // of the minimum bitmap rect and the rect of interest
        const auto lhs_height = lhs.GetHeight();
        const auto rhs_height = rhs.GetHeight();
        const auto lhs_width  = lhs.GetWidth();
        const auto rhs_width  = rhs.GetWidth();
        const auto lhs_rect = URect(0, 0, lhs_width, lhs_height);
        const auto rhs_rect = URect(0, 0, rhs_width, rhs_height);
        const auto& min_bitmap_rect = Intersect(lhs_rect, rhs_rect);
        const auto& min_rect = Intersect(rc, min_bitmap_rect);
        if (min_rect.IsEmpty())
            return false;
        return PixelCompareBitmaps(lhs.GetPixelReadView(),
                                   rhs.GetPixelReadView(),
                                   min_rect, min_rect, std::move(comparer));
    }

    template<typename PixelT>
    bool PixelCompare(const Bitmap<PixelT>& lhs, const URect& rc, const Bitmap<PixelT>& rhs)
    {
        using ComparerF = typename Bitmap<PixelT>::Pixel2Pixel;
        return PixelCompare(lhs, rc, rhs, ComparerF());
    }

    // Compare two bitmaps for equality. The bitmaps are equal
    // if they have equal dimensions and the given comparison
    // functor considers each pixel value to be equal.
    template<typename CompareF, typename PixelT>
    bool PixelCompare(const Bitmap<PixelT>& lhs, const Bitmap<PixelT>& rhs, CompareF comparer)
    {
        if (lhs.GetHeight() != rhs.GetHeight())
            return false;
        if (lhs.GetWidth() != rhs.GetWidth())
            return false;
        const URect rc (0, 0, lhs.GetWidth(), lhs.GetHeight());

        return PixelCompare(lhs, rc, rhs, comparer);
    }

    // Compare two bitmaps for equality. The bitmaps are equal
    // if the have equal dimensions and all the pixels
    // have equal pixel values.
    template<typename PixelT>
    bool PixelCompare(const Bitmap<PixelT>& lhs, const Bitmap<PixelT>& rhs)
    {
        return PixelCompare(lhs, rhs, PixelEquality::PixelPrecision());
    }


    template<typename Pixel>
    bool operator==(const Bitmap<Pixel>& lhs, const Bitmap<Pixel>& rhs)
    {
        return PixelCompare(lhs, rhs);
    }

    template<typename Pixel>
    bool operator!=(const Bitmap<Pixel>& lhs, const Bitmap<Pixel>& rhs)
    {
        return !(lhs == rhs);
    }

    void WritePPM(const IBitmapReadView& bmp, const std::string& filename);
    void WritePNG(const IBitmapReadView& bmp, const std::string& filename);
    void WritePPM(const IBitmap& bmp, const std::string& filename);
    void WritePNG(const IBitmap& bmp, const std::string& filename);

    std::unique_ptr<IBitmap> GenerateNextMipmap(const IBitmapReadView& src, bool srgb);
    std::unique_ptr<IBitmap> GenerateNextMipmap(const IBitmap& src, bool srgb);
    std::unique_ptr<IBitmap> ConvertToLinear(const IBitmapReadView& src);
    std::unique_ptr<IBitmap> ConvertToLinear(const IBitmap& src);

    void PremultiplyAlpha(const BitmapWriteView<Pixel_RGBA>& dst,
                          const BitmapReadView<Pixel_RGBA>& src, bool srgb);
    Bitmap<Pixel_RGBA> PremultiplyAlpha(const BitmapReadView<Pixel_RGBA>& src, bool srgb);
    Bitmap<Pixel_RGBA> PremultiplyAlpha(const Bitmap<Pixel_RGBA>& src, bool srgb);

    URect FindImageRectangle(const IBitmapReadView& src, const IPoint& start);

} // namespace
