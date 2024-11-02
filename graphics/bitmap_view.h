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

#include "base/assert.h"
#include "graphics/bitmap_interface.h"

namespace gfx
{
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
        BitmapReadView() = default;
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
        virtual void ReadPixel(unsigned row, unsigned col, Pixel_RGBA* pixel) const override
        { read_pixel(row, col, pixel); }
        virtual void ReadPixel(unsigned row, unsigned col, Pixel_RGB* pixel) const override
        { read_pixel(row, col, pixel); }
        virtual void ReadPixel(unsigned row, unsigned col, Pixel_A* pixel) const override
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
        BitmapWriteView() = default;
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
        virtual void WritePixel(unsigned row, unsigned col, const Pixel_RGBA& pixel) const override
        { write_pixel(row, col, pixel); }
        virtual void WritePixel(unsigned row, unsigned col, const Pixel_RGB& pixel) const override
        { write_pixel(row, col, pixel); }
        virtual void WritePixel(unsigned row, unsigned col, const Pixel_A& pixel) const override
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
        virtual void WritePixel(unsigned row, unsigned col, const Pixel_RGBA& pixel) const override
        { write_pixel(row, col, pixel); }
        virtual void WritePixel(unsigned row, unsigned col, const Pixel_RGB& pixel) const override
        { write_pixel(row, col, pixel); }
        virtual void WritePixel(unsigned row, unsigned col, const Pixel_A& pixel) const override
        { write_pixel(row, col, pixel); }
        virtual void ReadPixel(unsigned row, unsigned col, Pixel_RGBA* pixel) const override
        { read_pixel(row, col, pixel); }
        virtual void ReadPixel(unsigned row, unsigned col, Pixel_RGB* pixel) const override
        { read_pixel(row, col, pixel); }
        virtual void ReadPixel(unsigned row, unsigned col, Pixel_A* pixel) const override
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


} // namespace