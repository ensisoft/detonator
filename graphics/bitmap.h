// Copyright (c) 2010-2019 Sami Väisänen, Ensisoft
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

#include <limits>
#include <vector>
#include <cassert>
#include <cstring>
#include <type_traits>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <functional>

#include "base/assert.h"
#include "color4f.h"
#include "types.h"

// macro poisoning
#undef RGB

namespace gfx
{
    using u8 = std::uint8_t;

    struct RGB;
    struct RGBA;

    struct Grayscale {
        u8 r = 0;
        Grayscale(u8 r) : r(r)
        {}
        Grayscale() {}
        Grayscale(const RGB& rgb); // defined after RGB
        Grayscale(const RGBA& rgba); // defined after RGBA
    };

    struct RGB {
        u8 r = 0;
        u8 g = 0;
        u8 b = 0;
        RGB() {}
        RGB(u8 r, u8 g, u8 b) : r(r), g(g), b(b)
        {}
        RGB(Color c)
        {
            switch (c)
            {
                case Color::White:
                    r = g = b = 255;
                    break;
                case Color::Black:
                    break;
                case Color::Red:
                   r = 255;
                   break;
                case Color::DarkRed:
                   r = 127;
                   break;
                case Color::Green:
                   g = 255;
                   break;
                case Color::DarkGreen:
                   g = 127;
                   break;
                case Color::Blue:
                   b = 255;
                   break;
                case Color::DarkBlue:
                   b = 127;
                   break;
                case Color::Cyan:
                   g = b = 255;
                   break;
                case Color::DarkCyan:
                   g = b = 127;
                   break;
                case Color::Magenta:
                   r = b = 255;
                   break;
                case Color::DarkMagenta:
                   r = b = 127;
                   break;
                case Color::Yellow:
                   r = g = 255;
                   break;
                case Color::DarkYellow:
                   r = g = 127;
                   break;
                case Color::Gray:
                   r = g = b = 158;
                   break;
                case Color::DarkGray:
                   r = g = b = 127;
                   break;
                case Color::LightGray:
                   r = g = b = 192;
                   break;
            }
        } // ctor
        RGB(const Grayscale& grayscale)
        {
            r = g = b = grayscale.r;
        }
        RGB(const RGBA& rgba);
    };

    struct RGBA {
        u8 r = 0;
        u8 g = 0;
        u8 b = 0;
        u8 a = 255;

        RGBA() {}
        RGBA(u8 r, u8 g, u8 b, u8 a) : r(r), g(g), b(b), a(a)
        {}
        RGBA(Color c)
        {
            RGB rgb(c);
            r = rgb.r;
            g = rgb.g;
            b = rgb.b;
            a = 255;
        }
        RGBA(const Grayscale& grayscale)
        {
            r = g = b = grayscale.r;
            a = 255;
        }
        RGBA(const RGB& rgb)
        {
            r = rgb.r;
            g = rgb.g;
            b = rgb.b;
        }
    };
    inline bool operator==(const RGB& lhs, const RGB& rhs)
    {
        return lhs.r == rhs.r &&
               lhs.g == rhs.g &&
               lhs.b == rhs.b;
    }
    inline bool operator!=(const RGB& lhs, const RGB& rhs)
    {
        return !(lhs == rhs);
    }
    inline bool operator==(const RGBA& lhs, const RGBA& rhs)
    {
        return lhs.r == rhs.r &&
               lhs.g == rhs.g &&
               lhs.b == rhs.b &&
               lhs.a == rhs.a;
    }
    inline bool operator!=(const RGBA& lhs, const RGBA& rhs)
    {
        return !(lhs == rhs);
    }

    // RGB methods that depend on RGBA
    inline RGB::RGB(const RGBA& rgba)
    {
        const float a = rgba.a / 255.0f;
        // bake the alpha in the color channels
        r = rgba.r * a;
        g = rgba.g * a;
        b = rgba.b * a;
    }

    // Grayscale methods that depend on RGB/A come here.
    inline Grayscale::Grayscale(const RGB& rgb)
    {
        // assume linear values here.
        // https://en.wikipedia.org/wiki/Grayscale
        // Colorimetric_(perceptual_luminance-preserving)_conversion_to_grayscale
        const float y =
            0.2126f * rgb.r +
            0.7252f * rgb.g +
            0.0722  * rgb.b;
        r = y / 255.0f;
    }
    inline Grayscale::Grayscale(const RGBA& rgba)
    {
        // assume linear values
        // https://en.wikipedia.org/wiki/Grayscale
        // Colorimetric_(perceptual_luminance-preserving)_conversion_to_grayscale
        const float a = rgba.a / 255.0f;
        const float y =
            0.2126f * rgba.r +
            0.7252f * rgba.g +
            0.0722  * rgba.b;
        r = (y * a) / 255.0f;
    }

    static_assert(sizeof(Grayscale) == 1,
        "Unexpected size of Grayscale pixel struct type.");
    static_assert(sizeof(RGB) == 3,
        "Unexpected size of RGB pixel struct type.");
    static_assert(sizeof(RGBA) == 4,
        "Unexpected size of RGBA pixel struct type.");

    // Bitmap interface. Mostly designed so that it's possible
    // to keep bitmap objects around as generic bitmaps
    // regardless of their actual underlying pixel representation.
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
        // Get whether bitmap is valid or not (has been allocated)
        virtual bool IsValid() const = 0;
        // Resize the bitmap. Previous are undefined.
        virtual void Resize(unsigned width, unsigned height) = 0;
        // Get pointer to underlying data. Can be nullptr if the bitmap
        // is not currently valid, i.e no pixel storage has been allocated.
        // Access to the data through this pointer is potentially
        // unsafe since the type of the pixel (i.e. the memory layout)
        // information is lost. However in some cases the generic
        // access is useful for example when passing the pointer to some
        // other APIs
        virtual const void* GetDataPtr() const = 0;
        // Flip the rows of the bitmap around horizontal center line/row.
        // Transforms the bitmap as follows:
        // aaaa           cccc
        // bbbb  becomes  bbbb
        // cccc           aaaa        
        virtual void FlipHorizontally() = 0;
        // Get unique hash value based on the contents of the bitmap.
        virtual std::size_t GetHash() const = 0;
        // Here we could have methods for dynamically getting
        // And setting pixels. I.e. there'd be a method for set/get
        // for each type of pixel and when the format would not match
        // there'd be a conversion. 
        // This is potentially doing something unexpected and there's
        // no current use case for this API so this is omitted
        // and instead one must use the statically typed methods 
        // provided by the actual implementation (Bitmap template)
                
        // No dynamic copying or assignment
        IBitmap(const IBitmap&) = delete;
        IBitmap& operator=(const IBitmap&) = delete;
    protected:
        IBitmap() = default;
    private:
    };


    template<typename Pixel>
    class Bitmap : public IBitmap
    {
    public:
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
        Bitmap()
          : mWidth(0)
          , mHeight(0)
        {}

        // Create a bitmap with the given dimensions.
        // all the pixels are initialized to zero.
        Bitmap(unsigned width, unsigned height)
            : mWidth(width)
            , mHeight(height)
        {
            mPixels.resize(width * height);
        }

        Bitmap(const std::vector<Pixel>& data, unsigned width, unsigned height)
            : mPixels(data)
            , mWidth(width)
            , mHeight(height)
        {
            ASSERT(data.size() == width * height);
        }

        // create a bitmap from the given data.
        // stride is the number of *bytes* per scanline.
        // if stride is 0 it will default to width * sizeof(Pixel)
        Bitmap(const Pixel* data, unsigned width, unsigned height, unsigned stride = 0)
            : mWidth(width)
            , mHeight(height)
        {
            mPixels.resize(width * height);
            // if the bitmap is being constructed with 0 width or 0 height
            // early return here because accessing mPixels[0] would then be invalid.
            if (mPixels.empty())
                return;

            if (!stride)
                stride = sizeof(Pixel) * width;

            if (stride == sizeof(Pixel) * width)
            {
                const auto bytes = width * height * sizeof(Pixel);
                std::memcpy(&mPixels[0], data, bytes);
            }
            else
            {
                const auto ptr = (const u8*)data;
                // do a strided copy
                for (std::size_t y=0; y<height; ++y)
                {
                    const auto src = y * stride;
                    const auto dst = y * width;
                    const auto len = width * sizeof(Pixel);
                    std::memcpy(&mPixels[dst], &ptr[src], len);
                }
            }
        }

        Bitmap(const Bitmap& other)
            : mPixels(other.mPixels)
            , mWidth(other.mWidth)
            , mHeight(other.mHeight)
        {}
        Bitmap(Bitmap&& other)
        {
            mPixels = std::move(other.mPixels);
            mWidth  = other.mWidth;
            mHeight = other.mHeight;
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
                hash_value ^= std::hash<std::uint8_t>()(ptr[i]);
            return hash_value;
        }

        // Get a pixel from within the bitmap.
        // The pixel must be within the this bitmap's width and height.
        Pixel GetPixel(unsigned row, unsigned col) const
        {
            ASSERT(row < mHeight);
            ASSERT(col < mWidth);
            const auto src = row * mWidth + col;
            return mPixels[src];
        }
        Pixel GetPixel(const UPoint& p) const 
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
            const auto dst = row * mWidth + col;
            mPixels[dst] = value;
        }
        void SetPixel(const UPoint& p, const Pixel& value)
        {
            SetPixel(p.GetY(), p.GetX(), value);
        }

        // Compare the pixels in this bitmap within the given rectangle
        // against the given reference pixel value using the given
        // compare functor.
        // returns false if the compare_functor identifies non-equality
        // for any pixel. otherwise returns true and all pixels equal
        // the reference pixel as determined by the compare functor.
        // This is mostly useful as a testing utility.
        template<typename CompareF>
        bool Compare(const URect& rc, const Pixel& reference, CompareF comparer) const
        {
            const auto& dst = Intersect(GetRect(), rc);

            for (unsigned y=0; y<dst.GetHeight(); ++y)
            {
                for (unsigned x=0; x<dst.GetWidth(); ++x)
                {
                    const auto& px = GetPixel(dst.MapToGlobal(x,y));
                    if (!(comparer(px, reference)))
                        return false;
                }
            }
            return true;
        }

        // Compare the pixels in this bitmap within the given
        // rectangle against the given reference pixel and expect
        // pixel perfect matching.
        bool Compare(const URect& rc, const Pixel& reference) const
        {
            return Compare(rc, reference, Pixel2Pixel());
        }

        // Compare all pixels in this bitmap against the given
        // reference pixel for equality using the given comparer functor.
        // Returns false if the compare functor identifies non-equality
        // for any pixel otherwise true.
        template<typename ComparerF>
        bool Compare(const Pixel& reference, ComparerF comparer) const
        {
            const URect rc(0, 0, mWidth, mHeight);
            return Compare(rc, reference, comparer);
        }

        // Compare all pixels in this bitmap against the given
        // reference pixel and expect pixel perfect matching.
        // Returns false if any pixel doesn't match the reference
        // otherwise true.
        bool Compare(const Pixel& reference) const
        {
            return Compare(reference, Pixel2Pixel());
        }

        // Fill the area defined by the rectangle rc with the given pixel value.
        // The rectangle is clipped to the pixmap borders.
        void Fill(const URect& rc, const Pixel& value)
        {
            const auto& dst = Intersect(GetRect(), rc);

            for (unsigned y=0; y<dst.GetHeight(); ++y)
            {
                for (unsigned x=0; x<dst.GetWidth(); ++x)
                {
                    SetPixel(dst.MapToGlobal(x, y), value);
                }
            }
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
                const URect rc(0, 0, mWidth, mHeight);
                Fill(rc, value);
            }
        }

        // Copy data from given pixel array pointer into this bitmap.
        // The data is expected to point to a total of width*height worth of pixels.
        // The offset (position) of where to start copying the data into can be negative.
        // Any pixel that is not within the bounds of this bitmap is silently clipped.
        template<typename PixelType>
        void Copy(int x, int y, unsigned width, unsigned height, const PixelType* data)
        {
            ASSERT(width < std::numeric_limits<int>::max());
            ASSERT(height < std::numeric_limits<int>::max());

            const auto& src = IRect(x, y, width, height);
            const auto& own = IRect(GetRect());
            const auto& dst = Intersect(own, src);
            
            for (unsigned y=0; y<dst.GetHeight(); ++y)
            {
                for (unsigned x=0; x<dst.GetWidth(); ++x)
                {
                    const auto& g = dst.MapToGlobal(x, y);
                    const auto& l = src.MapToLocal(g);
                    SetPixel(UPoint(g), data[l.GetY() * width + l.GetX()]);
                }
            }
        }

        // Copy data from the given other bitmap into this bitmap.
        // The offset (position) of where to start copying the data into can be negative.
        // Any pixel that is not within the bounds of this bitmap is silently clipped.
        template<typename PixelType>
        void Copy(int x, int y, const Bitmap<PixelType>& bmp)
        {
            const auto w = bmp.GetWidth();
            const auto h = bmp.GetHeight();
            ASSERT(w < std::numeric_limits<int>::max());
            ASSERT(h < std::numeric_limits<int>::max());

            const auto& src = IRect(x, y, w, h);
            const auto& own = IRect(GetRect());
            const auto& dst = Intersect(own, src);
            
            for (unsigned y=0; y<dst.GetHeight(); ++y)
            {
                for (unsigned x=0; x<dst.GetWidth(); ++x)
                {
                    const auto& g = dst.MapToGlobal(x, y);
                    const auto& l = src.MapToLocal(g);
                    SetPixel(UPoint(g), bmp.GetPixel(UPoint(l)));
                }
            }
        }

        // Get pixel pointer to the raw data.
        const Pixel* GetData() const
        { return mPixels.empty() ? nullptr : &mPixels[0]; }

        Bitmap& operator=(Bitmap&& other)
        {
            if (this == &other)
                return *this;
            mPixels = std::move(other.mPixels);
            mWidth  = other.mWidth;
            mHeight = other.mHeight;
            return *this;
        }

        URect GetRect() const 
        { return URect(0u, 0u, mWidth, mHeight); }

    private:
        std::vector<Pixel> mPixels;
        unsigned mWidth  = 0;
        unsigned mHeight = 0;
    private:
        template<typename T> friend bool operator==(const Bitmap<T>& lhs, const Bitmap<T>& rhs);
        template<typename T> friend bool operator!=(const Bitmap<T>& lhs, const Bitmap<T>& rhs);
    };

    using GrayscaleBitmap = Bitmap<Grayscale>;
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
        const auto& min_bitmap_rect = Intersect(lhs.GetRect(), rhs.GetRect());
        const auto& min_rect = Intersect(rc, min_bitmap_rect);
        if (min_rect.IsEmpty())
            return false;

        for (unsigned y=0; y<min_rect.GetHeight(); ++y)
        {
            for (unsigned x=0; x<min_rect.GetWidth(); ++x)
            {
                const auto& point = min_rect.MapToGlobal(x, y);
                const auto& px1  = lhs.GetPixel(point);
                const auto& px2  = rhs.GetPixel(point);
                if (!comparer(px1, px2))
                    return false;
            }
        }
        return true;
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
        if (lhs.GetWidth != rhs.GetWidth())
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

    template<typename OutputIt>
    void WritePPM(const Bitmap<RGB>& bmp, OutputIt out)
    {
        const auto w = bmp.GetWidth();
        const auto h = bmp.GetHeight();

        std::stringstream ss;
        ss << "P6 " << w << " " << h << " 255\n";
        std::string header = ss.str();
        std::copy(std::begin(header), std::end(header), out);

        // todo: fix if out is pointer.
        static_assert(sizeof(RGB) == 3,
            "Padding bytes found. Cannot copy RGB data as a byte stream.");

        const auto bytes = w * h * sizeof(RGB);
        const auto* beg  = (const char*)bmp.GetData();
        const auto* end  = (const char*)bmp.GetData() + bytes;
        std::copy(beg, end, out);
    }

    inline
    void WritePPM(const Bitmap<RGB>& bmp, const std::string& filename)
    {
        std::ofstream out(filename, std::ios::binary);
        if (!out.is_open())
            throw std::runtime_error("failed to open file: " + filename);

        WritePPM(bmp, std::ostreambuf_iterator<char>(out));
    }

    inline
    void WritePPM(const Bitmap<RGB>& bmp, const char* filename)
    {
        WritePPM(bmp, std::string(filename));
    }

} // namespace
