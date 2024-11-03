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

#include <vector>

#include "base/assert.h"
#include "graphics/bitmap_view.h"
#include "graphics/types.h"

namespace gfx
{
    template<typename Pixel>
    void ReadBitmapPixels(const BitmapReadView<Pixel>& src,
                          const URect& rect, std::vector<Pixel>* pixels)
    {
        const auto bitmap_width  = src.GetWidth();
        const auto bitmap_height = src.GetHeight();

        ASSERT(Contains(URect(0, 0, bitmap_width, bitmap_height), rect));

        for (unsigned y=0; y<rect.GetHeight(); ++y)
        {
            for (unsigned x=0; x<rect.GetWidth(); ++x)
            {
                const auto src_point = rect.MapToGlobal(x, y);
                const auto src_pixel = src.template ReadPixel<Pixel>(src_point);
                pixels->push_back(src_pixel);
            }
        }
    }

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

    template<typename SrcPixel, typename DstPixel, typename Converter>
    void ConvertBitmap(const BitmapWriteView<DstPixel>& dst,
                       const BitmapReadView<SrcPixel>& src,
                       Converter conversion_op)
    {
        const auto src_width  = src.GetWidth();
        const auto src_height = src.GetHeight();
        const auto dst_width  = dst.GetWidth();
        const auto dst_height = dst.GetHeight();
        ASSERT(src_width == dst_width);
        ASSERT(src_height == dst_height);
        for (unsigned row=0; row<src_height; ++row)
        {
            for (unsigned col=0; col<src_width; ++col)
            {
                SrcPixel src_pixel;
                DstPixel dst_pixel;
                src.ReadPixel(row, col, &src_pixel);
                conversion_op(src_pixel, &dst_pixel);
                dst.WritePixel(row, col, dst_pixel);
            }
        }
    }

    template<typename Pixel, typename CompareFunc>
    bool PixelCompareBitmaps(const BitmapReadView<Pixel>& src,
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
    bool PixelCompareBitmapRegion(const BitmapReadView<Pixel>& bmp,
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

    template<typename Pixel, typename CompareFunc>
    bool PixelBlockCompareBitmaps(const BitmapReadView<Pixel>& lhs,
                                  const BitmapReadView<Pixel>& rhs,
                                  const USize& block_size,
                                  CompareFunc comparator)
    {
        ASSERT(lhs.GetWidth() == rhs.GetWidth());
        ASSERT(lhs.GetHeight() == rhs.GetHeight());

        const auto bitmap_width  = lhs.GetWidth();
        const auto bitmap_height = lhs.GetHeight();
        const auto block_width  = block_size.GetWidth();
        const auto block_height = block_size.GetHeight();
        // right now only allow exact multiples of block size for the image size.
        ASSERT((bitmap_width % block_width) == 0);
        ASSERT((bitmap_height % block_height) == 0);

        const auto rows = bitmap_height / block_height;
        const auto cols = bitmap_width / block_width;

        for (unsigned row=0; row<rows; ++row)
        {
            for (unsigned col=0; col<cols; ++col)
            {
                const auto y = row * block_height;
                const auto x = col * block_width;
                const URect block(x, y, block_width, block_height);

                std::vector<Pixel> lhs_array;
                std::vector<Pixel> rhs_array;
                ReadBitmapPixels(lhs, block, &lhs_array);
                ReadBitmapPixels(rhs, block, &rhs_array);
                if (!comparator(lhs_array, rhs_array))
                    return false;
            }
        }
        return true;
    }

} // namespace
