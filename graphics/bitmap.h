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
#include "data/fwd.h"
#include "graphics/color4f.h"
#include "graphics/types.h"

// macro poisoning
#undef RGB

namespace gfx
{
    using u8 = std::uint8_t;

    // About this monochromatic 8-bits per pixel type.
    //
    // This is called grayscale here in the type system but is typically
    // used to actually represent alpha masks instead of true grayscale images.
    // Bitwise representation is the same for the both, i.e. both have just
    // one 8 bit channel but the meaning is completely different. With alpha
    // masks the channel represents linear opacity from 0.0f/0x00 to 1.0f/0xff,
    // 0.0f being fully transparent and 1.0f being fully opaque.
    // True grayscale images use the 8bit channel to represent the luminance
    // of the image with some encoding such as sRGB.
    struct Grayscale {
        u8 r = 0;
        Grayscale(u8 value=0) : r(value)
        {}
    };
    bool operator==(const Grayscale& lhs, const Grayscale& rhs);
    bool operator!=(const Grayscale& lhs, const Grayscale& rhs);
    Grayscale operator & (const Grayscale& lhs, const Grayscale& rhs);
    Grayscale operator | (const Grayscale& lhs, const Grayscale& rhs);
    Grayscale operator >> (const Grayscale& lhs, unsigned bits);

    // RGB represents a pixel in the RGB color model but doesn't
    // require any specific color model/encoding. The actual channel
    // values can thus represent color values either in sRGB, linear
    // or some other 8bit encoding.
    struct RGB {
        u8 r = 0;
        u8 g = 0;
        u8 b = 0;
        RGB(u8 red=0, u8 green=0, u8 blue=0)
          : r(red), g(green), b(blue)
        {}
        // Set the RGB value based on a color name.
        // The result is sRGB encoded RGB triplet.
        RGB(Color name);
    };

    bool operator==(const RGB& lhs, const RGB& rhs);
    bool operator!=(const RGB& lhs, const RGB& rhs);
    RGB operator & (const RGB& lhs, const RGB& rhs);
    RGB operator | (const RGB& lhs, const RGB& rhs);
    RGB operator >> (const RGB& lhs, unsigned bits);

    // RGBA represents a pixel in the RGB color model but doesn't
    // require any actual color model/encoding. The actual channel
    // color values can thus represent color values either in sRGB,
    // linear or some other 8bit encoding.
    // Note that even when using sRGB the alpha value is not sRGB
    // encoded but represents pixel's transparency on a linear scale.
    // In addition, the alpha value can be either straight or pre-multiplied.
    struct RGBA {
        u8 r = 0;
        u8 g = 0;
        u8 b = 0;
        u8 a = 255;
        RGBA(u8 red=0, u8 green=0, u8 blue=0, u8 alpha = 0xff)
          : r(red), g(green), b(blue), a(alpha)
        {}
        // Set the RGB value based on a color name.
        // The result is sRGB encoded RGB triplet.
        RGBA(Color name, u8 alpha=255);
    };

    bool operator==(const RGBA& lhs, const RGBA& rhs);
    bool operator!=(const RGBA& lhs, const RGBA& rhs);
    RGBA operator & (const RGBA& lhs, const RGBA& rhs);
    RGBA operator | (const RGBA& lhs, const RGBA& rhs);
    RGBA operator >> (const RGBA& lhs, unsigned bits);

    struct fRGBA {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 0.0f;
    };
    fRGBA operator + (const fRGBA& lhs, const fRGBA& rhs);
    fRGBA operator * (const fRGBA& lhs, float scaler);
    fRGBA operator * (float scaler, const fRGBA& rhs);

    struct fRGB {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
    };
    fRGB operator + (const fRGB& lhs, const fRGB& rhs);
    fRGB operator * (const fRGB& lhs, float scaler);
    fRGB operator * (float scaler, const fRGB& rhs);

    struct fGrayscale {
        float r = 0.0f;
    };
    fGrayscale operator + (const fGrayscale& lhs, const fGrayscale& rhs);
    fGrayscale operator * (const fGrayscale& lhs, float scaler);
    fGrayscale operator * (float scaler, const fGrayscale& rhs);

    float sRGB_decode(float value);
    float sRGB_encode(float value);
    fRGBA sRGB_decode(const fRGBA& value);
    fRGBA sRGB_encode(const fRGBA& value);
    fRGB sRGB_decode(const fRGB& value);
    fRGB sRGB_encode(const fRGB& value);

    // transform between unsigned integer and floating
    // point pixel representations.
    fRGBA Pixel_to_floats(const RGBA& value);
    fRGB Pixel_to_floats(const RGB& value);
    fGrayscale Pixel_to_floats(const Grayscale& value);
    RGBA Pixel_to_uints(const fRGBA& value);
    RGB Pixel_to_uints(const fRGB& value);
    Grayscale Pixel_to_uints(const fGrayscale& value);

    fRGBA sRGBA_from_color(Color name);
    fRGB sRGB_from_color(Color name);
    fGrayscale sRGB_grayscale_from_color(Color name);

    static_assert(sizeof(Grayscale) == 1,
        "Unexpected size of Grayscale pixel struct type.");
    static_assert(sizeof(RGB) == 3,
        "Unexpected size of RGB pixel struct type.");
    static_assert(sizeof(RGBA) == 4,
        "Unexpected size of RGBA pixel struct type.");

    template<typename Pixel>
    inline Pixel RasterOp_SourceOver(const Pixel& dst, const Pixel& src)
    { return src; }

    template<typename Pixel>
    inline Pixel RasterOp_BitwiseAnd(const Pixel& dst, const Pixel& src)
    { return dst & src; }

    template<typename Pixel>
    inline Pixel RasterOp_BitwiseOr(const Pixel& dst, const Pixel& src)
    { return dst | src; }


    class IBitmapReadView
    {
    public:
        virtual ~IBitmapReadView() = default;
        // Get the width of the bitmap
        virtual unsigned GetWidth() const = 0;
        // Get the height of the bitmap
        virtual unsigned GetHeight() const = 0;
        // Get the depth of the bitmap
        virtual unsigned GetDepthBits() const = 0;
        // Get pointer to the underlying raw pixel data.
        virtual const void* GetReadPtr() const = 0;
        // Get whether bitmap is valid or not (has been allocated)
        virtual bool IsValid() const = 0;
        // Read bitwise pixel data from the underlying bitmap
        // object. These functions don't do any color space etc.
        // conversion but only consider bits. The number of bytes
        // copied into output pixel are truncated to actual size.
        virtual void ReadPixel(unsigned row, unsigned col, RGBA* pixel) const = 0;
        virtual void ReadPixel(unsigned row, unsigned col, RGB* pixel) const = 0;
        virtual void ReadPixel(unsigned row, unsigned col, Grayscale* pixel) const = 0;

        template<typename Pixel>
        inline void ReadPixel(const UPoint& point, Pixel* pixel) const
        {
            const auto row = point.GetY();
            const auto col = point.GetX();
            return ReadPixel(row, col, pixel);
        }
        template<typename Pixel>
        inline void ReadPixel(const IPoint& point, Pixel* pixel) const
        {
            const auto row = point.GetY();
            const auto col = point.GetX();
            ASSERT(row >= 0 && col >= 0);
            return ReadPixel(row, col, pixel);
        }

        template<typename Pixel>
        inline Pixel ReadPixel(unsigned row, unsigned col) const
        {
            Pixel p;
            ReadPixel(row, col, &p);
            return p;
        }
        template<typename Pixel>
        inline Pixel ReadPixel(const UPoint& point) const
        {
            const auto row = point.GetY();
            const auto col = point.GetX();
            return ReadPixel<Pixel>(row, col);
        }
        template<typename Pixel>
        inline Pixel ReadPixel(const IPoint& point) const
        {
            const auto row = point.GetY();
            const auto col = point.GetX();
            ASSERT(row >= 0 && col >= 0);
            return ReadPixel<Pixel>(row, col);
        }
    protected:
        // no public dynamic copying or assignment
        IBitmapReadView() = default;
        IBitmapReadView(IBitmapReadView&&) = default;
        IBitmapReadView(const IBitmapReadView&) = default;
        IBitmapReadView& operator=(const IBitmapReadView&) = default;
        IBitmapReadView& operator=(IBitmapReadView&&) = default;
    private:
    };

    class IBitmapWriteView
    {
    public:
        virtual ~IBitmapWriteView() = default;
        // Get the width of the bitmap
        virtual unsigned GetWidth() const = 0;
        // Get the height of the bitmap
        virtual unsigned GetHeight() const = 0;
        // Get the depth of the bitmap
        virtual unsigned GetDepthBits() const = 0;
        // Get pointer to the underlying raw pixel data.
        virtual void* GetWritePtr() const = 0;
        // Get whether bitmap is valid or not (has been allocated)
        virtual bool IsValid() const = 0;
        // Read bitwise pixel data from the underlying bitmap
        // object. These functions don't do any color space etc.
        // conversion but only consider bits. The number of bytes
        // copied into destination bitmap are truncated to actual size.
        virtual void WritePixel(unsigned row, unsigned col, const RGBA& pixel) const = 0;
        virtual void WritePixel(unsigned row, unsigned col, const RGB& pixel) const = 0;
        virtual void WritePixel(unsigned row, unsigned col, const Grayscale& pixel) const = 0;

        template<typename Pixel>
        inline void WritePixel(const UPoint& point, const Pixel& pixel) const
        {
            const auto row = point.GetY();
            const auto col = point.GetX();
            WritePixel(row, col, pixel);
        }
        template<typename Pixel>
        inline void WritePixel(const IPoint& point, const Pixel& pixel) const
        {
            const auto row = point.GetY();
            const auto col = point.GetX();
            ASSERT(row >= 0 && col >= 0);
            WritePixel(row, col, pixel);
        }
    protected:
        // no public dynamic copying or assignment
        IBitmapWriteView() = default;
        IBitmapWriteView(IBitmapWriteView&&) = default;
        IBitmapWriteView(const IBitmapWriteView&) = default;
        IBitmapWriteView& operator=(const IBitmapWriteView&) = default;
        IBitmapWriteView& operator=(IBitmapWriteView&&) = default;
    private:
    };

    class IBitmapReadWriteView : public virtual IBitmapReadView,
                                 public virtual IBitmapWriteView
    {
    public:
        virtual ~IBitmapReadWriteView() = default;
    protected:
        // no public dynamic copying or assignment
        IBitmapReadWriteView() = default;
        IBitmapReadWriteView(IBitmapReadWriteView&&) = default;
        IBitmapReadWriteView(const IBitmapReadWriteView&) = default;
        IBitmapReadWriteView& operator=(const IBitmapReadWriteView&) = default;
        IBitmapReadWriteView& operator=(IBitmapReadWriteView&&) = default;
    private:
    };


    template<typename Pixel>
    class BitmapReadView : public IBitmapReadView
    {
    public:
        using PixelType = Pixel;
        BitmapReadView(const Pixel* data, unsigned width, unsigned height)
          : mPixels(data)
          , mWidth(width)
          , mHeight(height)
        {}
        virtual unsigned GetWidth() const override
        { return mWidth; }
        virtual unsigned GetHeight() const override
        { return mHeight; }
        virtual unsigned GetDepthBits() const override
        { return sizeof(Pixel) * 8; }
        virtual const void* GetReadPtr() const override
        { return mPixels; }
        virtual bool IsValid() const override
        { return mPixels  && mWidth && mHeight; }
        virtual void ReadPixel(unsigned row, unsigned col, RGBA* pixel) const override
        { read_pixel(row, col, pixel); }
        virtual void ReadPixel(unsigned row, unsigned col, RGB* pixel) const override
        { read_pixel(row, col, pixel); }
        virtual void ReadPixel(unsigned row, unsigned col, Grayscale* pixel) const override
        { read_pixel(row, col, pixel); }

        using IBitmapReadView::ReadPixel;
    private:
        template<typename T>
        void read_pixel(unsigned row, unsigned col, T* pout) const
        {
            ASSERT(row < mHeight);
            ASSERT(col < mWidth);
            const auto index = row * mWidth + col;
            constexpr auto bytes  = std::min(sizeof(T), sizeof(Pixel));
            std::memcpy(pout, &mPixels[index], bytes);
        }
    private:
        const Pixel* mPixels  = nullptr;
        const unsigned mWidth  = 0;
        const unsigned mHeight = 0;
    };

    template<typename Pixel>
    class BitmapWriteView : public IBitmapWriteView
    {
    public:
        using PixelType = Pixel;
        BitmapWriteView(Pixel* data, unsigned width, unsigned height)
          : mPixels(data)
          , mWidth(width)
          , mHeight(height)
        {}
        virtual unsigned GetWidth() const override
        { return mWidth; }
        virtual unsigned GetHeight() const override
        { return mHeight; }
        virtual unsigned GetDepthBits() const override
        { return sizeof(Pixel) * 8; }
        virtual void* GetWritePtr() const override
        { return mPixels; }
        virtual bool IsValid() const override
        { return mPixels  && mWidth && mHeight; }
        virtual void WritePixel(unsigned row, unsigned col, const RGBA& pixel) const override
        { write_pixel(row, col, pixel); }
        virtual void WritePixel(unsigned row, unsigned col, const RGB& pixel) const override
        { write_pixel(row, col, pixel); }
        virtual void WritePixel(unsigned row, unsigned col, const Grayscale& pixel) const override
        { write_pixel(row, col, pixel); }

        using IBitmapWriteView::WritePixel;
    private:
        template<typename T>
        void write_pixel(unsigned row, unsigned col, const T& value) const
        {
            ASSERT(row < mHeight);
            ASSERT(col < mWidth);
            const auto index = row * mWidth + col;
            constexpr auto bytes = std::min(sizeof(T), sizeof(Pixel));
            std::memcpy(&mPixels[index], &value, bytes);
        }
    private:
        mutable Pixel* mPixels  = nullptr;
        const unsigned mWidth  = 0;
        const unsigned mHeight = 0;
    };

    template<typename Pixel>
    class BitmapReadWriteView : public IBitmapReadWriteView
    {
    public:
        using PixelType = Pixel;
        BitmapReadWriteView(Pixel* data, unsigned width, unsigned height)
            : mPixels(data)
            , mWidth(width)
            , mHeight(height)
        {}
        virtual unsigned GetWidth() const override
        { return mWidth; }
        virtual unsigned GetHeight() const override
        { return mHeight; }
        virtual unsigned GetDepthBits() const override
        { return sizeof(Pixel) * 8; }
        virtual const void* GetReadPtr() const override
        { return mPixels; }
        virtual void* GetWritePtr() const override
        { return mPixels; }
        virtual bool IsValid() const override
        { return mPixels  && mWidth && mHeight; }
        virtual void WritePixel(unsigned row, unsigned col, const RGBA& pixel) const override
        { write_pixel(row, col, pixel); }
        virtual void WritePixel(unsigned row, unsigned col, const RGB& pixel) const override
        { write_pixel(row, col, pixel); }
        virtual void WritePixel(unsigned row, unsigned col, const Grayscale& pixel) const override
        { write_pixel(row, col, pixel); }
        virtual void ReadPixel(unsigned row, unsigned col, RGBA* pixel) const override
        { read_pixel(row, col, pixel); }
        virtual void ReadPixel(unsigned row, unsigned col, RGB* pixel) const override
        { read_pixel(row, col, pixel); }
        virtual void ReadPixel(unsigned row, unsigned col, Grayscale* pixel) const override
        { read_pixel(row, col, pixel); }

        using IBitmapWriteView::WritePixel;
        using IBitmapReadView::ReadPixel;
    private:
        template<typename T>
        void write_pixel(unsigned row, unsigned col, const T& value) const
        {
            ASSERT(row < mHeight);
            ASSERT(col < mWidth);
            const auto index = row * mWidth + col;
            constexpr auto bytes = std::min(sizeof(T), sizeof(Pixel));
            std::memcpy(&mPixels[index], &value, bytes);
        }
        template<typename T>
        void read_pixel(unsigned row, unsigned col, T* pout) const
        {
            ASSERT(row < mHeight);
            ASSERT(col < mWidth);
            const auto index = row * mWidth + col;
            constexpr auto bytes = std::min(sizeof(T), sizeof(Pixel));
            std::memcpy(pout, &mPixels[index], bytes);
        }
    private:
        mutable Pixel* mPixels  = nullptr;
        const unsigned mWidth  = 0;
        const unsigned mHeight = 0;
    };


    template<typename Pixel>
    void FillBitmap(const BitmapWriteView<Pixel>& dst,
                    const IRect& dst_rect, const Pixel& value)
    {
        const auto dst_width  = dst.GetWidth();
        const auto dst_height = dst.GetHeight();
        const auto dst_rect_safe = IRect(0, 0, dst_width, dst_height);
        const auto rect = Intersect(dst_rect_safe, dst_rect);
        for (unsigned y=0; y<rect.GetHeight(); ++y)
        {
            for (unsigned x=0; x<rect.GetWidth(); ++x)
            {
                const auto point = rect.MapToGlobal(x, y);
                dst.WritePixel(point, value);
            }
        }
    }

    template<typename Pixel, typename RasterOp>
    void BlitBitmap(const BitmapReadWriteView<Pixel>& dst,
                    const BitmapReadView<Pixel>& src,
                    const IPoint& dst_pos, const IRect& src_rect, RasterOp raster_op)
    {
        const auto dst_width  = dst.GetWidth();
        const auto dst_height = dst.GetHeight();
        const auto src_width  = src.GetWidth();
        const auto src_height = src.GetHeight();
        const auto src_rect_safe = Intersect(IRect(0, 0, src_width, src_height), src_rect);
        const auto dst_rect = IRect(dst_pos, src_rect_safe.GetSize());
        const auto cpy_rect = Intersect(IRect(0, 0, dst_width, dst_height), dst_rect);

        for (unsigned y=0; y<cpy_rect.GetHeight(); ++y)
        {
            for (unsigned x=0; x<cpy_rect.GetWidth(); ++x)
            {
                const auto& dst_point = cpy_rect.MapToGlobal(x, y);
                const auto& src_point = src_rect_safe.MapToGlobal(dst_rect.MapToLocal(dst_point));
                const auto& src_pixel = src.template ReadPixel<Pixel>(src_point);
                const auto& dst_pixel = dst.template ReadPixel<Pixel>(dst_point);
                const auto& ret_pixel = raster_op(src_pixel, dst_pixel);
                dst.WritePixel(dst_point, ret_pixel);
            }
        }
    }

    template<typename Pixel>
    void CopyBitmap(const BitmapWriteView<Pixel>& dst,
                    const BitmapReadView<Pixel>& src,
                    const IPoint& dst_pos, const IRect& src_rect)
    {
        const auto dst_width  = dst.GetWidth();
        const auto dst_height = dst.GetHeight();
        const auto src_width  = src.GetWidth();
        const auto src_height = src.GetHeight();
        const auto src_rect_safe = Intersect(IRect(0, 0, src_width, src_height), src_rect);
        const auto dst_rect = IRect(dst_pos, src_rect_safe.GetSize());
        const auto cpy_rect = Intersect(IRect(0, 0, dst_width, dst_height), dst_rect);

        for (unsigned y=0; y<cpy_rect.GetHeight(); ++y)
        {
            for (unsigned x=0; x<cpy_rect.GetWidth(); ++x)
            {
                const auto& dst_point = cpy_rect.MapToGlobal(x, y);
                const auto& src_point = src_rect_safe.MapToGlobal(dst_rect.MapToLocal(dst_point));
                const auto& src_pixel = src.template ReadPixel<Pixel>(src_point);
                dst.WritePixel(dst_point, src_pixel);
            }
        }
    }

    template<typename SrcPixel, typename DstPixel>
    void ReinterpretBitmap(const BitmapWriteView<DstPixel>& dst,
                           const BitmapReadView<SrcPixel>& src)
    {
        const auto src_width  = src.GetWidth();
        const auto src_height = src.GetHeight();
        const auto dst_width = dst.GetWidth();
        const auto dst_height = dst.GetHeight();
        ASSERT(src_width == dst_width);
        ASSERT(src_height == dst_height);
        for (unsigned row=0; row<src_height; ++row)
        {
            for (unsigned col=0; col<src_width; ++col)
            {
                // either read from src as dst pixel type and get
                // conversion on read or read from src as src pixel
                // type and then write as dst pixel type and convert
                // on write.
                DstPixel value;
                src.ReadPixel(row, col, &value);
                dst.WritePixel(row, col, value);
            }
        }
    }

    template<typename Pixel, typename CompareFunc>
    bool CompareBitmaps(const BitmapReadView<Pixel>& src,
                        const BitmapReadView<Pixel>& dst,
                        const URect& src_rect,
                        const URect& dst_rect,
                        CompareFunc comparer)
    {
        const auto src_width  = src.GetWidth();
        const auto src_height = src.GetHeight();
        const auto dst_width  = dst.GetWidth();
        const auto dst_height = dst.GetHeight();
        const auto dst_rect_safe = Intersect(URect(0, 0, dst_width, dst_height), dst_rect);
        const auto src_rect_safe = Intersect(URect(0, 0, src_width, src_height), src_rect);

        const auto width  = std::min(dst_rect_safe.GetWidth(), src_rect_safe.GetWidth());
        const auto height = std::min(dst_rect_safe.GetHeight(), dst_rect_safe.GetHeight());
        for (unsigned row=0; row<height; ++row)
        {
            for (unsigned col=0; col<width; ++col)
            {
                const auto& dst_point = dst_rect_safe.MapToGlobal(col, row);
                const auto& src_point = src_rect_safe.MapToGlobal(col, row);
                const auto& dst_pixel = dst.template ReadPixel<Pixel>(dst_point);
                const auto& src_pixel = src.template ReadPixel<Pixel>(src_point);
                if (!comparer(dst_pixel, src_pixel))
                    return false;
            }
        }
        return true;
    }

    template<typename Pixel, typename CompareFunc>
    bool CompareBitmapRegion(const BitmapReadView<Pixel>& bmp,
                             const URect& area,
                             const Pixel& reference,
                             CompareFunc comparer)
    {
        const auto bmp_width  = bmp.GetWidth();
        const auto bmp_height = bmp.GetHeight();
        const auto safe_rect  = Intersect(URect(0, 0, bmp_width, bmp_height), area);
        for (unsigned row=0; row<safe_rect.GetHeight(); ++row)
        {
            for (unsigned col=0; col<safe_rect.GetWidth(); ++col)
            {
                const auto& bmp_point = safe_rect.MapToGlobal(col, row);
                const auto& bmp_pixel = bmp.template ReadPixel<Pixel>(bmp_point);
                if (!comparer(bmp_pixel, reference))
                    return false;
            }
        }
        return true;
    }

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

        // pixel to pixel
        struct Pixel2Pixel {
            bool operator()(const Pixel& lhs, const Pixel& rhs) const
            { return lhs == rhs; }
        };

        // mean-squared-error
        struct MSE {
            bool operator()(const Grayscale& lhs, const Grayscale& rhs) const
            {
                const auto r = (int)lhs.r - (int)rhs.r;
                const auto sum = r*r;
                const auto mse = sum / 1.0;
                return mse < max_mse;
            }

            bool operator()(const RGB& lhs, const RGB& rhs) const
            {
                const auto r = (int)lhs.r - (int)rhs.r;
                const auto g = (int)lhs.g - (int)rhs.g;
                const auto b = (int)lhs.b - (int)rhs.b;

                const auto sum = r*r + g*g + b*b;
                const auto mse = sum / 3;
                return mse < max_mse;
            }
            bool operator()(const RGBA& lhs, const RGBA& rhs) const
            {
                const auto r = (int)lhs.r - (int)rhs.r;
                const auto g = (int)lhs.g - (int)rhs.g;
                const auto b = (int)lhs.b - (int)rhs.b;
                const auto a = (int)lhs.a - (int)rhs.a;

                const auto sum = r*r + g*g + b*b + a*a;
                const auto mse = sum / 4.0;
                return mse < max_mse;
            }
            void SetErrorTreshold(double se)
            {
                max_mse = se * se;
            }
            double max_mse;
        };

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
        virtual unsigned GetWidth() const override
        { return mWidth; }
        // Get the height of the bitmap.
        virtual unsigned GetHeight() const override
        { return mHeight; }
        // Get the depth of the bitmap
        virtual unsigned GetDepthBits() const override
        { return sizeof(Pixel) * 8; }
        // Get whether the bitmap is valid or not (has been allocated)
        virtual bool IsValid() const override
        { return mWidth != 0 && mHeight != 0; }
        // Resize the bitmap with the given dimensions.
        virtual void Resize(unsigned width, unsigned height) override
        {
            Bitmap b(width, height);
            mPixels = std::move(b.mPixels);
            mWidth  = width;
            mHeight = height;
        }
        // Get the unsafe data pointer.
        virtual const void* GetDataPtr() const override
        {
            return mPixels.empty() ? nullptr : (const void*)&mPixels[0];
        }
        virtual void* GetDataPtr() override
        {
            return mPixels.empty() ? nullptr : (void*)&mPixels[0];
        }
        // Flip the rows of the bitmap around horizontal axis (the middle row)
        virtual void FlipHorizontally() override
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
        virtual std::size_t GetHash() const override
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
        virtual std::unique_ptr<IBitmapReadView> GetReadView() const override
        { return std::make_unique<BitmapReadView<Pixel>>((const Pixel*)GetDataPtr(), mWidth, mHeight); }
        virtual std::unique_ptr<IBitmapWriteView> GetWriteView() override
        { return std::make_unique<BitmapWriteView<Pixel>>((Pixel*)GetDataPtr(), mWidth, mHeight); }
        virtual std::unique_ptr<IBitmapReadWriteView> GetReadWriteView() override
        { return std::make_unique<BitmapReadWriteView<Pixel>>((Pixel*)GetDataPtr(), mWidth, mHeight); }
        virtual std::unique_ptr<IBitmap> Clone() const override
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
        bool Compare(const URect& rc, const Pixel& reference, CompareFunctor comparer) const
        {
            return CompareBitmapRegion(GetPixelReadView(),
                       rc, reference, std::move(comparer));
        }

        // Compare the pixels in this bitmap within the given
        // rectangle against the given reference pixel and expect
        // pixel perfect matching.
        bool Compare(const URect& rc, const Pixel& reference) const
        {
            return CompareBitmapRegion(GetPixelReadView(),
                       rc, reference, Pixel2Pixel());
        }

        // Compare all pixels in this bitmap against the given
        // reference pixel for equality using the given compare functor.
        // Returns false if the compare functor identifies non-equality
        // for any pixel otherwise true.
        template<typename CompareFunctor>
        bool Compare(const Pixel& reference, CompareFunctor comparer) const
        {
            return CompareBitmapRegion(GetPixelReadView(),
                       URect(0, 0, mWidth, mHeight), reference, std::move(comparer));
        }

        // Compare all pixels in this bitmap against the given
        // reference pixel and expect pixel perfect matching.
        // Returns false if any pixel doesn't match the reference
        // otherwise true.
        bool Compare(const Pixel& reference) const
        {
            return CompareBitmapRegion(GetPixelReadView(),
                       URect(0, 0, mWidth, mHeight), reference, Pixel2Pixel());
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

    using GrayscaleBitmap = Bitmap<Grayscale>;
    using AlphaMask  = Bitmap<Grayscale>;
    using RgbBitmap  = Bitmap<RGB>;
    using RgbaBitmap = Bitmap<RGBA>;

    // Compare the pixels between two pixmaps within the given rectangle using
    // the given compare functor.
    // The rectangle is clipped to stay within the bounds of both bitmaps.
    // Returns false if the compare functor identifies non-equality for any pixel.
    // Otherwise returns true the pixels between the two bitmaps compare equal
    // as determined by the compare functor.
    template<typename CompareF, typename PixelT>
    bool Compare(const Bitmap<PixelT>& lhs, const URect& rc, const Bitmap<PixelT>& rhs, CompareF comparer)
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
        return CompareBitmaps(lhs.GetPixelReadView(),
                              rhs.GetPixelReadView(),
                              min_rect, min_rect, std::move(comparer));
    }

    template<typename PixelT>
    bool Compare(const Bitmap<PixelT>& lhs, const URect& rc, const Bitmap<PixelT>& rhs)
    {
        using ComparerF = typename Bitmap<PixelT>::Pixel2Pixel;
        return Compare(lhs, rc, rhs, ComparerF());
    }

    // Compare two bitmaps for equality. The bitmaps are equal
    // if they have equal dimensions and the given comparison
    // functor considers each pixel value to be equal.
    template<typename CompareF, typename PixelT>
    bool Compare(const Bitmap<PixelT>& lhs, const Bitmap<PixelT>& rhs, CompareF comparer)
    {
        if (lhs.GetHeight() != rhs.GetHeight())
            return false;
        if (lhs.GetWidth() != rhs.GetWidth())
            return false;
        const URect rc (0, 0, lhs.GetWidth(), lhs.GetHeight());

        return Compare(lhs, rc, rhs, comparer);
    }

    // Compare two bitmaps for equality. The bitmaps are equal
    // if the have equal dimensions and all the pixels
    // have equal pixel values.
    template<typename PixelT>
    bool Compare(const Bitmap<PixelT>& lhs, const Bitmap<PixelT>& rhs)
    {
        using ComparerF = typename Bitmap<PixelT>::Pixel2Pixel;
        return Compare(lhs, rhs, ComparerF());
    }


    template<typename Pixel>
    bool operator==(const Bitmap<Pixel>& lhs, const Bitmap<Pixel>& rhs)
    {
        return Compare(lhs, rhs);
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

    // Interface for accessing / generating bitmaps procedurally.
    // Each implementation implements some procedural method for
    // creating a bitmap and generating its content.
    class IBitmapGenerator
    {
    public:
        // Get the function type of the implementation.
        enum class Function {
            Noise
        };
        // Destructor.
        virtual ~IBitmapGenerator() = default;
        // Get the generation function.
        virtual Function GetFunction() const = 0;
        // Generate a new bitmap using the algorithm.
        virtual std::unique_ptr<IBitmap> Generate() const = 0;
        // Clone this generator.
        virtual std::unique_ptr<IBitmapGenerator> Clone() const = 0;
        // Get the hash value of the generator.
        virtual std::size_t GetHash() const = 0;
        // Serialize the generator into JSON.
        virtual void IntoJson(data::Writer& data) const = 0;
        // Load the generator's state from the given JSON object.
        // Returns true if successful otherwise false.
        virtual bool FromJson(const data::Reader& data) = 0;
        // Get the width of the bitmaps that will be generated (in pixels).
        virtual unsigned GetWidth() const = 0;
        // Get the height of the bitmaps that will be generated (in pixels).
        virtual unsigned GetHeight() const = 0;
        // Set the width of the bitmaps that will be generated (in pixels).
        virtual void SetWidth(unsigned width) = 0;
        // Set the height of the bitmaps that will be generated (in pixels).
        virtual void SetHeight(unsigned height) = 0;
    private:
    };

    // Implement bitmap generation using a noise function.
    // Each layer of noise is defined as a function with prime
    // number seeds and frequency and amplitude. The bitmap
    // is filled by sampling all the noise layers / functions
    // and summing their signals.
    class NoiseBitmapGenerator : public IBitmapGenerator
    {
    public:
        struct Layer {
            std::uint32_t prime0 = 7;
            std::uint32_t prime1 = 743;
            std::uint32_t prime2 = 7873;
            float frequency = 1.0f;
            float amplitude = 1.0f;
        };
        NoiseBitmapGenerator() = default;
        NoiseBitmapGenerator(unsigned width, unsigned height)
            : mWidth(width)
            , mHeight(height)
        {}
        size_t GetNumLayers() const
        { return mLayers.size(); }
        void AddLayer(const Layer& layer)
        { mLayers.push_back(layer); }
        const Layer& GetLayer(size_t index) const
        { return mLayers[index]; }
        Layer& GetLayer(size_t index)
        { return mLayers[index]; }
        void DelLayer(size_t index)
        {
            auto it = mLayers.begin();
            std::advance(it, index);
            mLayers.erase(it);
        }
        bool HasLayers() const
        { return !mLayers.empty(); }
        // Create random generator settings.
        void Randomize(unsigned min_prime_index,
                       unsigned max_prime_index,
                       unsigned layers=1);

        virtual Function GetFunction() const override
        { return Function::Noise; }
        virtual std::unique_ptr<IBitmapGenerator> Clone() const override
        { return std::make_unique<NoiseBitmapGenerator>(*this); }
        virtual unsigned GetWidth() const override
        { return mWidth; }
        virtual unsigned GetHeight() const override
        { return mHeight; }
        virtual void SetHeight(unsigned height) override
        { mHeight = height; }
        virtual void SetWidth(unsigned width) override
        { mWidth = width; }
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;
        virtual std::unique_ptr<IBitmap> Generate() const override;
        virtual std::size_t GetHash() const override;
    private:
        unsigned mWidth  = 0;
        unsigned mHeight = 0;
        std::vector<Layer> mLayers;
    };

} // namespace
