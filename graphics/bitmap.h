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

#include "base/assert.h"
#include "types.h"

#include <vector>
#include <cassert>
#include <cstring>
#include <type_traits>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>

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

    template<typename Pixel>
    class Bitmap
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

        // Resize the bitmap with the given dimensions.
        void Resize(unsigned width, unsigned height)
        {
            Bitmap b(width, height);
            mPixels = std::move(b.mPixels);
            mWidth  = width;
            mHeight = height;
        }

        // Get a pixel from within the bitmap. 
        // The pixel must be within the this bitmap's width and height.
        Pixel GetPixel(std::size_t row, std::size_t col) const
        {
            ASSERT(row < mHeight);
            ASSERT(col < mWidth);
            const auto src = row * mWidth + col;
            return mPixels[src];
        }

        // Set a pixel within the bitmap to a new pixel value.
        // The pixel's coordinates must be within this bitmap's 
        // width and height. 
        void SetPixel(std::size_t row, std::size_t col, const Pixel& pixel)
        {
            ASSERT(row < mHeight);
            ASSERT(col < mWidth);
            const auto dst = row * mWidth + col;
            mPixels[dst] = pixel;
        }

        // Compare the pixels in this bitmap within the given rectangle
        // against the given reference pixel value using the given
        // compare functor. 
        // returns false if the compare_functor identifies non-equality
        // for any pixel. otherwise returns true and all pixels equal 
        // the reference pixel as determined by the compare functor.
        template<typename Comp>
        bool Compare(const Rect& rc, const Pixel& reference, Comp compare_functor) const
        {
            const auto right  = rc.GetX() + rc.GetWidth();
            const auto bottom = rc.GetY() + rc.GetHeight();
            const auto xend = std::min(right, mWidth);
            const auto yend = std::min(bottom, mHeight);

            for (std::size_t y=rc.GetY(); y<yend; ++y)
            {
                for (std::size_t x=rc.GetX(); x<xend; ++x)
                {
                    const auto& px = GetPixel(y, x);
                    if (!(compare_functor(px, reference)))
                        return false;
                }
            }
            return true;
        }
        bool Compare(const Rect& rc, const Pixel& p) const
        {
            return Compare(rc, p, Pixel2Pixel());
        }

        template<typename Comp>
        bool Compare(const Pixel& val, Comp c) const
        {
            const Rect rc(0, 0, mWidth, mHeight);
            return Compare(rc, val, c);
        }
        bool Compare(const Pixel& p) const
        {
            return Compare(p, Pixel2Pixel());
        }

        // Compare the pixels between two pixmaps within the given rectangle using
        // the given compare functor. 
        // The rectangle is clipped to stay within the bounds of both bitmaps.
        // Returns false if the compare functor identifies non-equality for any pixel.
        // Otherwise returns true the pixels between the two bitmaps compare equal
        // as determined by the compare functor.
        template<typename Comp> static
        bool Compare(const Bitmap& lhs, const Rect& rc, const Bitmap& rhs, Comp compare_functor)
        {
            const auto right = rc.GetX() + rc.GetWidth();
            const auto bottom = rc.GetY() + rc.GetHeight();
            const auto xend = std::min(right, std::min(lhs.mWidth, rhs.mWidth));
            const auto yend = std::min(bottom, std::min(lhs.mHeight, rhs.mHeight));

            for (std::size_t y=rc.GetY(); y<yend; ++y)
            {
                for (std::size_t x=rc.GetX(); x<xend; ++x)
                {
                    const auto& px1  = lhs.GetPixel(y, x);
                    const auto& px2  = rhs.GetPixel(y, x);
                    if (!compare_functor(px1, px2))
                        return false;
                }
            }
            return true;
        }

        static bool Compare(const Bitmap& lhs, const Rect& rc, const Bitmap& rhs)
        {
            return Compare(lhs, rc, rhs, Pixel2Pixel());
        }


        // Compare two bitmaps for equality. The bitmaps are equal
        // if they have equal dimensions and the given comparison
        // functor considers each pixel value to be equal.
        template<typename Comp>
        static bool Compare(const Bitmap& lhs, const Bitmap& rhs, Comp c)
        {
            if (lhs.mHeight != rhs.mHeight)
                return false;
            if (lhs.mWidth != rhs.mWidth)
                return false;
            const Rect rc (0, 0, lhs.mWidth, lhs.mHeight);

            return Compare(lhs, rc, rhs, c);
        }

        // Compare two bitmaps for equality. The bitmaps are equal
        // if the have equal dimensions and all the pixels
        // have equal pixel values.
        static bool Compare(const Bitmap& lhs, const Bitmap& rhs)
        {
            return Compare(lhs, rhs, Pixel2Pixel());
        }


        // Fill the area defined by the rectangle rc with the given pixel value.
        // The rectangle is clipped to the pixmap borders.
        void Fill(const Rect& rc, const Pixel& value)
        {
            const auto right  = rc.GetX() + rc.GetWidth();
            const auto bottom = rc.GetY() + rc.GetHeight();
            const auto xend = std::min(right, mWidth);
            const auto yend = std::min(bottom, mHeight);

            for (std::size_t y=rc.GetY(); y<yend; ++y)
            {
                for (std::size_t x=rc.GetX(); x<xend; ++x)
                {
                    SetPixel(y, x, value);
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
                const Rect rc(0, 0, mWidth, mHeight);
                Fill(rc, value);
            }
        }

        template<typename PixelType>
        void Copy(const Rect& rc, const PixelType* data)
        {
            const auto right  = rc.GetX() + rc.GetWidth();
            const auto bottom = rc.GetY() + rc.GetHeight();
            const auto stride = rc.GetWidth();
            const auto xend = std::min(right, mWidth);
            const auto yend = std::min(bottom, mHeight);
            size_t offset = 0;
            for (size_t y=rc.GetY(); y<yend; ++y)
            {
                for (size_t x=rc.GetX(); x<xend; ++x)
                {
                    SetPixel(y, x, data[offset++]);
                }
            }
        }

        template<typename PixelType>
        void Copy(unsigned x, unsigned y, const Bitmap<PixelType>& bmp)
        {
            const auto right  = x + bmp.GetWidth();
            const auto bottom = y + bmp.GetHeight();
            const auto xend = std::min(right, mWidth);
            const auto yend = std::min(bottom, mHeight);
            for (size_t row=y; row<yend; ++row) 
            {
                for (size_t col=x; col<xend; ++col) 
                {
                    SetPixel(row, col, bmp.GetPixel(row-y, col-x));
                }
            }
        }

        // Get the width of the bitmap.
        unsigned GetWidth() const
        { return mWidth; }

        // Get the height of the bitmap.
        unsigned GetHeight() const
        { return mHeight; }

        // Get pixel pointer to the raw data.
        const Pixel* GetData() const
        { return mPixels.empty() ? nullptr : &mPixels[0]; }

        // Returns true if any dimension is 0
        bool IsEmpty() const 
        { return mWidth == 0 || mHeight == 0; }

        // Flip the rows of the bitmap around horizontal center line/row.
        // Transforms the bitmap as follows:
        // aaaa           cccc
        // bbbb  becomes  bbbb
        // cccc           aaaa
        void FlipHorizontally() 
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

        Bitmap& operator=(Bitmap&& other) 
        {
            if (this == &other)
                return *this;
            mPixels = std::move(other.mPixels);
            mWidth  = other.mWidth;
            mHeight = other.mHeight;
            return *this;
        }

    private:
        std::vector<Pixel> mPixels;
        unsigned mWidth  = 0;
        unsigned mHeight = 0;
    private:
        template<typename T> friend bool operator==(const Bitmap<T>& lhs, const Bitmap<T>& rhs);
        template<typename T> friend bool operator!=(const Bitmap<T>& lhs, const Bitmap<T>& rhs);
    };


    template<typename Pixel>
    bool operator==(const Bitmap<Pixel>& lhs, const Bitmap<Pixel>& rhs)
    {
        return Bitmap<Pixel>::Compare(lhs, rhs);
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
