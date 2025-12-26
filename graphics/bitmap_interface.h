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
#include "graphics/types.h"

namespace gfx
{
    struct Pixel_A;
    struct Pixel_RGB;
    struct Pixel_RGBA;

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
        virtual void ReadPixel(unsigned row, unsigned col, Pixel_RGBA* pixel) const = 0;
        virtual void ReadPixel(unsigned row, unsigned col, Pixel_RGB* pixel) const = 0;
        virtual void ReadPixel(unsigned row, unsigned col, Pixel_A* pixel) const = 0;

        template<typename Pixel>
        void ReadPixel(const UPoint& point, Pixel* pixel) const
        {
            const auto row = point.GetY();
            const auto col = point.GetX();
            return ReadPixel(row, col, pixel);
        }
        template<typename Pixel>
        void ReadPixel(const IPoint& point, Pixel* pixel) const
        {
            const auto row = point.GetY();
            const auto col = point.GetX();
            ASSERT(row >= 0 && col >= 0);
            return ReadPixel(row, col, pixel);
        }

        template<typename Pixel>
        Pixel ReadPixel(unsigned row, unsigned col) const
        {
            Pixel p;
            ReadPixel(row, col, &p);
            return p;
        }
        template<typename Pixel>
        Pixel ReadPixel(const UPoint& point) const
        {
            const auto row = point.GetY();
            const auto col = point.GetX();
            return ReadPixel<Pixel>(row, col);
        }
        template<typename Pixel>
        Pixel ReadPixel(const IPoint& point) const
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
        virtual void WritePixel(unsigned row, unsigned col, const Pixel_RGBA& pixel) const = 0;
        virtual void WritePixel(unsigned row, unsigned col, const Pixel_RGB& pixel) const = 0;
        virtual void WritePixel(unsigned row, unsigned col, const Pixel_A& pixel) const = 0;

        template<typename Pixel>
        void WritePixel(const UPoint& point, const Pixel& pixel) const
        {
            const auto row = point.GetY();
            const auto col = point.GetX();
            WritePixel(row, col, pixel);
        }
        template<typename Pixel>
        void WritePixel(const IPoint& point, const Pixel& pixel) const
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
        ~IBitmapReadWriteView() override = default;
    protected:
        // no public dynamic copying or assignment
        IBitmapReadWriteView() = default;
        IBitmapReadWriteView(IBitmapReadWriteView&&) = default;
        IBitmapReadWriteView(const IBitmapReadWriteView&) = default;
        IBitmapReadWriteView& operator=(const IBitmapReadWriteView&) = default;
        IBitmapReadWriteView& operator=(IBitmapReadWriteView&&) = default;
    private:
    };


} // namespace