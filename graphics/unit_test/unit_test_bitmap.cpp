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

#include "config.h"

#include <cmath>
#include <iostream>

#include "base/utility.h"
#include "base/format.h"
#include "base/test_minimal.h"
#include "base/test_float.h"
#include "base/test_help.h"
#include "data/json.h"
#include "graphics/color4f.h"
#include "graphics/bitmap.h"
#include "graphics/bitmap_noise.h"
#include "graphics/bitmap_algo.h"

void print_mse()
{
    struct Entry {
        unsigned block_width;
        unsigned block_height;
        gfx::Color first_color;
        gfx::Color second_color;
        std::string name;
    };
    Entry mse_table[] = {
        {1, 1, gfx::Color::White,     gfx::Color::White,     "White-White       1x1px"},
        {1, 1, gfx::Color::White,     gfx::Color::Black,     "White-Black       1x1px"},
        {1, 1, gfx::Color::White,     gfx::Color::Red,       "White-Red         1x1px"},
        {1, 1, gfx::Color::Gray,      gfx::Color::DarkGray,  "Gray-DarkGray     1x1px"},
        {1, 1, gfx::Color::DarkCyan,  gfx::Color::DarkBlue,  "DarkCyan-DarkBlue 1x1px"},

        {4, 4, gfx::Color::White,     gfx::Color::White,     "White-White       4px"},
        {4, 4, gfx::Color::White,     gfx::Color::Black,     "White-Black       4px"},
        {4, 4, gfx::Color::White,     gfx::Color::Red,       "White-Red         4px"},
        {4, 4, gfx::Color::Gray,      gfx::Color::DarkGray,  "Gray-DarkGray     4px"},
        {4, 4, gfx::Color::DarkCyan,  gfx::Color::DarkBlue,  "DarkCyan-DarkBlue 4px"},

        {8, 8, gfx::Color::White,     gfx::Color::White,     "White-White       8x8px"},
        {8, 8, gfx::Color::White,     gfx::Color::Black,     "White-Black       8x8px"},
        {8, 8, gfx::Color::White,     gfx::Color::Red,       "White-Red         8x8px"},
        {8, 8, gfx::Color::Gray,      gfx::Color::DarkGray,  "Gray-DarkGray     8x8px"},
        {8, 8, gfx::Color::DarkCyan,  gfx::Color::DarkBlue,  "DarkCyan-DarkBlue 8x8px"},

        {16, 16, gfx::Color::White,     gfx::Color::White,     "White-White       16x16px"},
        {16, 16, gfx::Color::White,     gfx::Color::Black,     "White-Black       16x16px"},
        {16, 16, gfx::Color::White,     gfx::Color::Red,       "White-Red         16x16px"},
        {16, 16, gfx::Color::Gray,      gfx::Color::DarkGray,  "Gray-DarkGray     16x16px"},
        {16, 16, gfx::Color::DarkCyan,  gfx::Color::DarkBlue,  "DarkCyan-DarkBlue 16x16px"},
    };


    auto SetPixel = [](gfx::Pixel_RGB_Array& array, gfx::Color color) {
        for (auto& pixel : array)
            pixel = gfx::Pixel_RGB(color);
    };

    for (const auto& test : mse_table)
    {
        const auto width = test.block_width;
        const auto height = test.block_height;

        for (size_t i = 0; i < width * height; ++i)
        {
            gfx::Pixel_RGB_Array white;
            white.resize(width * height);
            SetPixel(white, test.first_color);

            gfx::Pixel_RGB_Array red;
            red.resize(width * height);
            SetPixel(red, test.first_color);

            for (size_t j = 0; j <= i; ++j)
                red[j] = gfx::Pixel_RGB(test.second_color);

            const auto mse = gfx::Pixel_MSE(white, red);
            std::printf("%s (%u/%u) pixel diff MSE = %f\n", test.name.c_str(), i + 1, width*height, mse);
        }
    }
}

void unit_test_basic()
{
    TEST_CASE(test::Type::Feature)

    // test empty bitmap for "emptiness"
    {
        gfx::Bitmap<gfx::Pixel_RGB> bmp;
        TEST_REQUIRE(bmp.GetWidth() == 0);
        TEST_REQUIRE(bmp.GetHeight() == 0);
        TEST_REQUIRE(bmp.GetDataPtr() == nullptr);
        TEST_REQUIRE(bmp.IsValid() == false);
    }

    // test initialized bitmap for "non emptiness"
    {

        gfx::Bitmap<gfx::Pixel_RGB> bmp(2, 2);
        TEST_REQUIRE(bmp.GetWidth() == 2);
        TEST_REQUIRE(bmp.GetHeight() == 2);
        TEST_REQUIRE(bmp.GetDataPtr() != nullptr);
        TEST_REQUIRE(bmp.IsValid() == true);
    }

    // test pixel set/get
    {
        gfx::Bitmap<gfx::Pixel_RGB> bmp(2, 2);
        bmp.SetPixel(0, 0, gfx::Color::White);
        bmp.SetPixel(0, 1, gfx::Color::Red);
        bmp.SetPixel(1, 0, gfx::Color::Green);
        bmp.SetPixel(1, 1, gfx::Color::Yellow);
        TEST_REQUIRE(bmp.GetPixel(0, 0) == gfx::Color::White);
        TEST_REQUIRE(bmp.GetPixel(0, 1) == gfx::Color::Red);
        TEST_REQUIRE(bmp.GetPixel(1, 0) == gfx::Color::Green);
        TEST_REQUIRE(bmp.GetPixel(1, 1) == gfx::Color::Yellow);
    }
}

void unit_test_filling()
{
    TEST_CASE(test::Type::Feature)

    // test bitmap filling
    {
        {
            gfx::Bitmap<gfx::Pixel_RGB> bmp(2, 2);
            TEST_REQUIRE(bmp.GetPixel(0, 0) == gfx::Color::Black);
            TEST_REQUIRE(bmp.GetPixel(1, 0) == gfx::Color::Black);
            TEST_REQUIRE(bmp.GetPixel(1, 1) == gfx::Color::Black);
            TEST_REQUIRE(bmp.GetPixel(0, 1) == gfx::Color::Black);

            bmp.Fill(gfx::Color::White);
            TEST_REQUIRE(bmp.GetPixel(0, 0) == gfx::Color::White);
            TEST_REQUIRE(bmp.GetPixel(1, 0) == gfx::Color::White);
            TEST_REQUIRE(bmp.GetPixel(1, 1) == gfx::Color::White);
            TEST_REQUIRE(bmp.GetPixel(0, 1) == gfx::Color::White);

            bmp.Fill(gfx::URect(0, 0, 6, 6), gfx::Color::Red);
            TEST_REQUIRE(bmp.GetPixel(0, 0) == gfx::Color::Red);
            TEST_REQUIRE(bmp.GetPixel(1, 0) == gfx::Color::Red);
            TEST_REQUIRE(bmp.GetPixel(1, 1) == gfx::Color::Red);
            TEST_REQUIRE(bmp.GetPixel(0, 1) == gfx::Color::Red);

            bmp.Fill(gfx::URect(0, 0, 1, 1), gfx::Color::Green);
            TEST_REQUIRE(bmp.GetPixel(0, 0) == gfx::Color::Green);
            TEST_REQUIRE(bmp.GetPixel(1, 0) == gfx::Color::Red);
            TEST_REQUIRE(bmp.GetPixel(1, 1) == gfx::Color::Red);
            TEST_REQUIRE(bmp.GetPixel(0, 1) == gfx::Color::Red);

            bmp.Fill(gfx::URect(1, 1, 1, 1), gfx::Color::Green);
            TEST_REQUIRE(bmp.GetPixel(0, 0) == gfx::Color::Green);
            TEST_REQUIRE(bmp.GetPixel(1, 0) == gfx::Color::Red);
            TEST_REQUIRE(bmp.GetPixel(1, 1) == gfx::Color::Green);
            TEST_REQUIRE(bmp.GetPixel(0, 1) == gfx::Color::Red);
        }

        gfx::Bitmap<gfx::Pixel_RGB> bmp(100, 100);
        struct Rect {
            int x, y;
            int w, h;
        } test_rects[] = {
                {0, 0, 10, 10},
                {0, 0, 1, 100},
                {0, 0, 1, 120},
                {0, 5, 1, 96},
                {0, 0, 100, 1},
                {0, 99, 100, 1},
                {99, 0, 1, 100},
                {40, 40, 40, 40},
                {0, 0, 100, 100},
                {0, 0, 200, 200},
        };
        for (const auto& r : test_rects)
        {
            bmp.Fill(gfx::Color::White);
            bmp.Fill(gfx::URect(r.x, r.y, r.w, r.h), gfx::Color::Green);
            for (int y=0; y<100; ++y)
            {
                for (int x=0; x<100; ++x)
                {
                    const bool y_inside = y >= r.y && y < r.y + r.h;
                    const bool x_inside = x >= r.x && x < r.x + r.w;

                    const auto expected_color = y_inside && x_inside ? gfx::Color::Green :
                                                gfx::Color::White;
                    // get pixel is already a tested primitive
                    // that we can build upon
                    TEST_REQUIRE(bmp.GetPixel(y, x) == expected_color);
                }
            }
            static int i = 0;
            WritePPM(bmp, "test_fill_" + std::to_string(i++) + ".ppm");
        }
    }
}

void unit_test_compare()
{

    TEST_CASE(test::Type::Feature)

    // test the compare against pixel
    {
        gfx::Bitmap<gfx::Pixel_RGB> bmp(100, 100);

        const gfx::URect test_rects[] = {
            {0, 0, 10, 10},
            {0, 0, 1, 100},
            {0, 0, 100, 1},
            {0, 99, 100, 1},
            {99, 0, 1, 100},
            {40, 40, 40, 40},
            {0, 0, 100, 100},
            {0, 0, 200, 200},
        };
        for (const auto& rect : test_rects)
        {
            // we've tested the fill operation previously already.
            bmp.Fill(gfx::Color::White);
            bmp.Fill(rect, gfx::Color::Green);

            auto view = bmp.GetPixelReadView();

            TEST_REQUIRE(PixelCompareBitmapRegion(view, rect,
                    gfx::Pixel_RGB(gfx::Color::Green), gfx::PixelEquality::PixelPrecision()));
            TEST_REQUIRE(PixelCompareBitmapRegion(view, rect,
                    gfx::Pixel_RGB(gfx::Color::Green), gfx::PixelEquality::ThresholdPrecision()));
        }
    }

    // test pixel block compare
    {
        // MSE values. (the table was printed by print_mse function)

        // White-Red         4px (1/16) pixel diff MSE = 2709.375000
        // White-Red         4px (2/16) pixel diff MSE = 5418.750000
        // White-Red         4px (3/16) pixel diff MSE = 8128.125000
        // White-Red         4px (4/16) pixel diff MSE = 10837.500000
        // White-Red         4px (5/16) pixel diff MSE = 13546.875000
        // White-Red         4px (6/16) pixel diff MSE = 16256.250000
        // White-Red         4px (7/16) pixel diff MSE = 18965.625000
        // White-Red         4px (8/16) pixel diff MSE = 21675.000000
        // White-Red         4px (9/16) pixel diff MSE = 24384.375000
        // White-Red         4px (10/16) pixel diff MSE = 27093.750000
        // White-Red         4px (11/16) pixel diff MSE = 29803.125000
        // White-Red         4px (12/16) pixel diff MSE = 32512.500000
        // White-Red         4px (13/16) pixel diff MSE = 35221.875000
        // White-Red         4px (14/16) pixel diff MSE = 37931.250000
        // White-Red         4px (15/16) pixel diff MSE = 40640.625000
        // White-Red         4px (16/16) pixel diff MSE = 43350.000000

        gfx::Bitmap<gfx::Pixel_RGB> lhs(16, 16);
        gfx::Bitmap<gfx::Pixel_RGB> rhs(16, 16);

        lhs.Fill(gfx::Color::White);
        rhs.Fill(gfx::Color::White);

        TEST_REQUIRE(PixelBlockCompareBitmaps(lhs.GetPixelReadView(),
                                              rhs.GetPixelReadView(),
                                              gfx::USize(4, 4),
                                              gfx::PixelEquality::ThresholdPrecision(0.0)));

        rhs.SetPixel(0, 0,  gfx::Color::Red);
        TEST_REQUIRE(!PixelBlockCompareBitmaps(lhs.GetPixelReadView(),
                                               rhs.GetPixelReadView(),
                                               gfx::USize(4, 4),
                                               gfx::PixelEquality::ThresholdPrecision(0.0)));

        TEST_REQUIRE(PixelBlockCompareBitmaps(lhs.GetPixelReadView(),
                                               rhs.GetPixelReadView(),
                                               gfx::USize(4, 4),
                                               gfx::PixelEquality::ThresholdPrecision(2800.0)));

        rhs.SetPixel(0, 0, gfx::Color::White);
        rhs.SetPixel(15, 15, gfx::Color::Red);
        TEST_REQUIRE(!PixelBlockCompareBitmaps(lhs.GetPixelReadView(),
                                               rhs.GetPixelReadView(),
                                               gfx::USize(4, 4),
                                               gfx::PixelEquality::ThresholdPrecision(0.0)));

        TEST_REQUIRE(PixelBlockCompareBitmaps(lhs.GetPixelReadView(),
                                              rhs.GetPixelReadView(),
                                              gfx::USize(4, 4),
                                              gfx::PixelEquality::ThresholdPrecision(2800.0)));

    }

}

void unit_test_copy()
{
    TEST_CASE(test::Type::Feature)

    // test copying of data from a pixel buffer pointer
    {
        gfx::Bitmap<gfx::Pixel_RGB> dst(4, 4);
        dst.Fill(gfx::Color::White);

        const gfx::Pixel_RGB red_data[2 * 2] = {
                gfx::Color::Red, gfx::Color::Green,
                gfx::Color::Yellow, gfx::Color::Blue
        };

        dst.Copy(0, 0, 2, 2, red_data);
        TEST_REQUIRE(dst.GetPixel(0, 0) == gfx::Color::Red);
        TEST_REQUIRE(dst.GetPixel(1, 0) == gfx::Color::Yellow);
        TEST_REQUIRE(dst.GetPixel(0, 1) == gfx::Color::Green);
        TEST_REQUIRE(dst.GetPixel(1, 1) == gfx::Color::Blue);
        TEST_REQUIRE(dst.GetPixel(2, 2) == gfx::Color::White);
        TEST_REQUIRE(dst.GetPixel(3, 2) == gfx::Color::White);
        TEST_REQUIRE(dst.GetPixel(2, 3) == gfx::Color::White);
        TEST_REQUIRE(dst.GetPixel(3, 3) == gfx::Color::White);

        dst.Fill(gfx::Color::White);
        dst.Copy(2, 2, 2, 2, red_data);
        TEST_REQUIRE(dst.GetPixel(0, 0) == gfx::Color::White);
        TEST_REQUIRE(dst.GetPixel(1, 0) == gfx::Color::White);
        TEST_REQUIRE(dst.GetPixel(0, 1) == gfx::Color::White);
        TEST_REQUIRE(dst.GetPixel(1, 1) == gfx::Color::White);
        TEST_REQUIRE(dst.GetPixel(2, 2) == gfx::Color::Red);
        TEST_REQUIRE(dst.GetPixel(2, 3) == gfx::Color::Green);
        TEST_REQUIRE(dst.GetPixel(3, 2) == gfx::Color::Yellow);
        TEST_REQUIRE(dst.GetPixel(3, 3) == gfx::Color::Blue);

        dst.Fill(gfx::Color::White);
        dst.Copy(-1, -1, 2, 2, red_data);
        TEST_REQUIRE(dst.GetPixel(0, 0) == gfx::Color::Blue);
        TEST_REQUIRE(dst.GetPixel(1, 0) == gfx::Color::White);

        dst.Fill(gfx::Color::White);
        dst.Copy(-2, -2, 2, 2, red_data);
        TEST_REQUIRE(dst.GetPixel(0, 0) == gfx::Color::White);
    }


    // test copying of data from a bitmap
    {
        gfx::Bitmap<gfx::Pixel_RGB> dst(4, 4);
        gfx::Bitmap<gfx::Pixel_RGB> src(2, 2);
        src.SetPixel(0, 0, gfx::Color::Red);
        src.SetPixel(0, 1, gfx::Color::Green);
        src.SetPixel(1, 0, gfx::Color::Blue);
        src.SetPixel(1, 1, gfx::Color::Yellow);

        dst.Fill(gfx::Color::White);
        dst.Copy(0, 0, src);
        TEST_REQUIRE(dst.GetPixel(0, 0) == gfx::Color::Red);
        TEST_REQUIRE(dst.GetPixel(1, 0) == gfx::Color::Blue);
        TEST_REQUIRE(dst.GetPixel(0, 1) == gfx::Color::Green);
        TEST_REQUIRE(dst.GetPixel(1, 1) == gfx::Color::Yellow);
        TEST_REQUIRE(dst.GetPixel(2, 2) == gfx::Color::White);

        dst.Fill(gfx::Color::White);
        dst.Copy(2, 2, src);
        TEST_REQUIRE(dst.GetPixel(0, 0) == gfx::Color::White);
        TEST_REQUIRE(dst.GetPixel(1, 0) == gfx::Color::White);
        TEST_REQUIRE(dst.GetPixel(0, 1) == gfx::Color::White);
        TEST_REQUIRE(dst.GetPixel(1, 1) == gfx::Color::White);
        TEST_REQUIRE(dst.GetPixel(2, 2) == gfx::Color::Red);
        TEST_REQUIRE(dst.GetPixel(2, 3) == gfx::Color::Green);
        TEST_REQUIRE(dst.GetPixel(3, 2) == gfx::Color::Blue);
        TEST_REQUIRE(dst.GetPixel(3, 3) == gfx::Color::Yellow);

        dst.Fill(gfx::Color::White);
        dst.Copy(3, 3, src);
        TEST_REQUIRE(dst.GetPixel(0, 0) == gfx::Color::White);
        TEST_REQUIRE(dst.GetPixel(1, 0) == gfx::Color::White);
        TEST_REQUIRE(dst.GetPixel(0, 1) == gfx::Color::White);
        TEST_REQUIRE(dst.GetPixel(1, 1) == gfx::Color::White);
        TEST_REQUIRE(dst.GetPixel(2, 2) == gfx::Color::White);
        TEST_REQUIRE(dst.GetPixel(2, 3) == gfx::Color::White);
        TEST_REQUIRE(dst.GetPixel(3, 2) == gfx::Color::White);
        TEST_REQUIRE(dst.GetPixel(3, 3) == gfx::Color::Red);

        dst.Fill(gfx::Color::White);
        dst.Copy(-1, -1, src);
        TEST_REQUIRE(dst.GetPixel(0, 0) == gfx::Color::Yellow);

        dst.Fill(gfx::Color::White);
        dst.Copy(-2, -2, src);
        TEST_REQUIRE(dst.GetPixel(0, 0) == gfx::Color::White);
    }


    // test copying of data out of a bitmap.
    {
        gfx::Bitmap<gfx::Pixel_RGB> src(4, 4);
        src.Fill(gfx::URect(0, 0, 2, 2), gfx::Color::Red);
        src.Fill(gfx::URect(2, 0, 2, 2), gfx::Color::Green);
        src.Fill(gfx::URect(0, 2, 2, 2), gfx::Color::Blue);
        src.Fill(gfx::URect(2, 2, 2, 2), gfx::Color::Yellow);
        // copy whole bitmap.
        {
            const auto& ret = src.Copy(gfx::URect(0, 0, 4, 4));
            TEST_REQUIRE(PixelCompare(ret, src));
        }
        // copy a sub rectangle
        {
            const auto& ret = src.Copy(gfx::URect(2, 2, 2, 2));
            TEST_REQUIRE(ret.GetHeight() == 2);
            TEST_REQUIRE(ret.GetWidth() == 2);
            TEST_REQUIRE(ret.GetPixel(0, 0) == gfx::Color::Yellow);
            TEST_REQUIRE(ret.GetPixel(1, 0) == gfx::Color::Yellow);
            TEST_REQUIRE(ret.GetPixel(0, 1) == gfx::Color::Yellow);
            TEST_REQUIRE(ret.GetPixel(1, 1) == gfx::Color::Yellow);
        }
        // copy sub rectangle that is larger than source.
        {
            const auto& ret = src.Copy(gfx::URect(2, 2, 3, 3));
            TEST_REQUIRE(ret.GetHeight() == 2);
            TEST_REQUIRE(ret.GetWidth() == 2);
            TEST_REQUIRE(ret.GetPixel(0, 0) == gfx::Color::Yellow);
            TEST_REQUIRE(ret.GetPixel(1, 0) == gfx::Color::Yellow);
            TEST_REQUIRE(ret.GetPixel(0, 1) == gfx::Color::Yellow);
            TEST_REQUIRE(ret.GetPixel(1, 1) == gfx::Color::Yellow);
        }
        src.Resize(5, 3);
        src.Fill(gfx::Color::Green);
        src.Fill(gfx::URect(2, 0, 3, 3), gfx::Color::HotPink);
        {
            const auto& ret = src.Copy(gfx::URect(1, 0, 2, 3));
            TEST_REQUIRE(ret.GetWidth() == 2);
            TEST_REQUIRE(ret.GetHeight() == 3);
            TEST_REQUIRE(ret.GetPixel(0, 0) == gfx::Color::Green);
            TEST_REQUIRE(ret.GetPixel(1, 0) == gfx::Color::Green);
            TEST_REQUIRE(ret.GetPixel(2, 0) == gfx::Color::Green);
            TEST_REQUIRE(ret.GetPixel(0, 1) == gfx::Color::HotPink);
            TEST_REQUIRE(ret.GetPixel(1, 1) == gfx::Color::HotPink);
            TEST_REQUIRE(ret.GetPixel(2, 1) == gfx::Color::HotPink);

        }
    }

}

void unit_test_flip()
{

    TEST_CASE(test::Type::Feature)

    // test flip
    {
        gfx::Bitmap<gfx::Pixel_RGB> bmp(4, 5);
        bmp.Fill(gfx::Color::White);
        bmp.SetPixel(0, 0, gfx::Color::Red);
        bmp.SetPixel(0, 1, gfx::Color::Red);
        bmp.SetPixel(0, 2, gfx::Color::Red);
        bmp.SetPixel(0, 3, gfx::Color::Red);

        bmp.FlipHorizontally();
        TEST_REQUIRE(bmp.GetPixel(0, 0) == gfx::Color::White);
        TEST_REQUIRE(bmp.GetPixel(0, 1) == gfx::Color::White);
        TEST_REQUIRE(bmp.GetPixel(0, 2) == gfx::Color::White);
        TEST_REQUIRE(bmp.GetPixel(0, 3) == gfx::Color::White);

        TEST_REQUIRE(bmp.GetPixel(4, 0) == gfx::Color::Red);
        TEST_REQUIRE(bmp.GetPixel(4, 1) == gfx::Color::Red);
        TEST_REQUIRE(bmp.GetPixel(4, 2) == gfx::Color::Red);
        TEST_REQUIRE(bmp.GetPixel(4, 3) == gfx::Color::Red);
    }

}

void unit_test_ppm()
{
    TEST_CASE(test::Type::Feature)

    // test PPM writing
    {
        gfx::Bitmap<gfx::Pixel_RGB> bmp(256, 256);
        bmp.Fill(gfx::Color::Green);

        for (int y=0; y<256; ++y)
        {
            for (int x=0; x<256; ++x)
            {
                int xp = x - 128;
                int yp = y - 128;
                const int radius = 100;
                if (xp*xp + yp*yp <= radius*radius)
                    bmp.SetPixel(y, x, gfx::Color::Red);
            }
        }
        WritePPM(bmp, "bitmap.ppm");
        WritePNG(bmp, "bitmap.png");
    }
}

void unit_test_mipmap()
{
    TEST_CASE(test::Type::Feature)

    // test mip map generation with box filter
    {
        gfx::Bitmap<gfx::Pixel_RGB> src(256, 256);
        src.Fill(gfx::Color::Green);

        for (int y=0; y<256; ++y)
        {
            for (int x=0; x<256; ++x)
            {
                int xp = x - 128;
                int yp = y - 128;
                const int radius = 100;
                if (xp*xp + yp*yp <= radius*radius)
                    src.SetPixel(y, x, gfx::Color::Red);
            }
        }

        struct TestCase {
            unsigned width  = 0;
            unsigned height = 0;
        } cases[] = {
                {256, 256},
                {257, 256},
                {256, 257},
                {300, 256},
                {256, 300}
        };

        for (auto test : cases)
        {
            gfx::Bitmap<gfx::Pixel_RGB> bmp;
            bmp.Resize(test.width, test.height);
            bmp.Fill(gfx::Color::White);
            bmp.Copy(0, 0, src);

            WritePNG(bmp, base::FormatString("%1x%2_level_0.png", test.width, test.height));
            unsigned level = 1;

            auto mip = gfx::GenerateNextMipmap(bmp, false);
            while (mip)
            {
                WritePNG(*mip, base::FormatString("%1x%2_level_%3.png", test.width, test.height, level));

                mip = gfx::GenerateNextMipmap(*mip, false);
                ++level;
            }
        }
    }

}

void unit_test_noise()
{
    TEST_CASE(test::Type::Feature)

    // random noise bitmap generation
    {
        gfx::NoiseBitmapGenerator gen;
        gen.SetWidth(256);
        gen.SetHeight(256);
        gfx::NoiseBitmapGenerator::Layer layers[] = {
                {2399,23346353,458912449, 4.0f, 200.0f},
                {2963, 29297533, 458913047, 8.0f, 64.0f},
                {5689, 88124567, 458912471, 128.0f, 4.0}
        };
        gen.AddLayer(layers[0]);
        gen.AddLayer(layers[1]);
        gen.AddLayer(layers[2]);
        auto bitmap = gen.Generate();
        WritePNG(*bitmap, "noise.png");

        data::JsonObject json;
        gen.IntoJson(json);
        gfx::NoiseBitmapGenerator other;
        other.FromJson(json);
        TEST_REQUIRE(other.GetWidth() == 256);
        TEST_REQUIRE(other.GetHeight() == 256);
        TEST_REQUIRE(other.GetNumLayers() == 3);
        TEST_REQUIRE(other.GetLayer(0).prime0 == 2399);
        TEST_REQUIRE(other.GetLayer(0).prime1 == 23346353);
        TEST_REQUIRE(other.GetLayer(0).prime2 == 458912449);
        TEST_REQUIRE(other.GetLayer(0).frequency == real::float32(4.0f));
        TEST_REQUIRE(other.GetLayer(0).amplitude == real::float32(200.0f));
    }
}

void unit_test_find_rect()
{
    TEST_CASE(test::Type::Feature)

    {
        gfx::Bitmap<gfx::Pixel_RGBA> bmp(256, 256);
        bmp.Fill(gfx::Color::Transparent);
        bmp.Fill(gfx::URect(20, 20, 3, 3), gfx::Color::Red);

        auto view = bmp.GetReadView();

        auto ret = gfx::FindImageRectangle(*view, gfx::IPoint(21, 21));
        TEST_REQUIRE(ret.GetX() == 20);
        TEST_REQUIRE(ret.GetY() == 20);
        TEST_REQUIRE(ret.GetWidth() == 3);
        TEST_REQUIRE(ret.GetHeight() == 3);

        ret = gfx::FindImageRectangle(*view, gfx::IPoint(20, 20));
        TEST_REQUIRE(ret.GetX() == 20);
        TEST_REQUIRE(ret.GetY() == 20);
        TEST_REQUIRE(ret.GetWidth() == 3);
        TEST_REQUIRE(ret.GetHeight() == 3);

        ret = gfx::FindImageRectangle(*view, gfx::IPoint(19, 19));
        TEST_REQUIRE(ret.IsEmpty());
    }

    {
        gfx::Bitmap<gfx::Pixel_RGBA> bmp(256, 256);
        bmp.Fill(gfx::Color::Transparent);
        bmp.Fill(gfx::URect(0, 0, 3, 3), gfx::Color::Red);

        auto view = bmp.GetReadView();

        auto ret = gfx::FindImageRectangle(*view, gfx::IPoint(1, 1));
        TEST_REQUIRE(ret.GetX() == 0);
        TEST_REQUIRE(ret.GetY() == 0);
        TEST_REQUIRE(ret.GetWidth() == 3);
        TEST_REQUIRE(ret.GetHeight() == 3);
    }

    {
        gfx::Bitmap<gfx::Pixel_RGBA> bmp(256, 256);
        bmp.Fill(gfx::Color::Transparent);
        bmp.Fill(gfx::URect(0, 0, 256, 256), gfx::Color::Red);

        auto view = bmp.GetReadView();

        auto ret = gfx::FindImageRectangle(*view, gfx::IPoint(1, 1));
        TEST_REQUIRE(ret.GetX() == 0);
        TEST_REQUIRE(ret.GetY() == 0);
        TEST_REQUIRE(ret.GetWidth() == 256);
        TEST_REQUIRE(ret.GetHeight() == 256);
    }

    // test on a larger image (unsophisticated perf)
    {
        gfx::Bitmap<gfx::Pixel_RGBA> bmp(2048, 2048);
        bmp.Fill(gfx::Color::Transparent);
        bmp.Fill(gfx::URect(0, 0, 1024, 1024), gfx::Color::Red);

        auto view = bmp.GetReadView();

        auto ret = gfx::FindImageRectangle(*view, gfx::IPoint(1, 1));
        TEST_REQUIRE(ret.GetX() == 0);
        TEST_REQUIRE(ret.GetY() == 0);
        TEST_REQUIRE(ret.GetWidth() == 1024);
        TEST_REQUIRE(ret.GetHeight() == 1024);
    }

}

void unit_test_algo()
{
    TEST_CASE(test::Type::Feature)

    {
        gfx::Bitmap<gfx::Pixel_RGBA> bmp(256, 256);
        bmp.Fill(gfx::Color::Transparent);
        bmp.Fill(gfx::URect(0, 0, 6, 6), gfx::Color::Red);
        bmp.Fill(gfx::URect(250, 250, 6, 6), gfx::Color::Green);

        auto view = bmp.GetPixelReadView();

        std::vector<gfx::Pixel_RGBA> pixels;
        gfx::ReadBitmapPixels(view, gfx::URect(0, 0, 6, 6), &pixels);

        TEST_REQUIRE(pixels.size() == 6 * 6);
        for (const auto& pixel : pixels)
            TEST_REQUIRE(pixel == gfx::Color::Red);

        pixels.clear();
        gfx::ReadBitmapPixels(view, gfx::URect(250, 250, 6, 6), &pixels);
        TEST_REQUIRE(pixels.size() == 6 * 6);
        for (const auto& pixel : pixels)
            TEST_REQUIRE(pixel == gfx::Color::Green);

    }

}

EXPORT_TEST_MAIN(
int test_main(int argc, char* argv[])
{
    bool print_mse_table = false;
    for (int i=1; i<argc; ++i)
    {
        if (!std::strcmp(argv[i], "--print-mse"))
            print_mse_table = true;
    }

    if (print_mse_table)
    {
        print_mse();
        return 0;
    }

    unit_test_basic();
    unit_test_filling();
    unit_test_compare();
    unit_test_copy();
    unit_test_flip();
    unit_test_ppm();
    unit_test_mipmap();
    unit_test_noise();
    unit_test_find_rect();
    unit_test_algo();
    return 0;
}
) // TEST_MAIN