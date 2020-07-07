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

#include "config.h"

#include "base/test_minimal.h"
#include "base/math.h"

#include "graphics/bitmap.h"
#include "graphics/types.h"
#include "graphics/image_packing.h"

int test_main(int argc, char* argv[])
{

    // single image
    {
        std::vector<gfx::pack::NamedImage> images;
        images.push_back({1, 1, 64, 64, 0});

        const auto ret = gfx::pack::PackImages(images);
        TEST_REQUIRE(ret.height == 64);
        TEST_REQUIRE(ret.width == 64);
        TEST_REQUIRE(images[0].xpos == 0);
        TEST_REQUIRE(images[0].ypos == 0);
    }

    // generate randomly sized rectangles and use a distinct color
    // to fill the target bitmap with each of these rectangles as
    // indicating by the result of the packing operation and then
    // finally scan the resulting image to make sure that for each
    // rectangle we find the expected number of pixels with that color.
    {
        std::vector<gfx::pack::NamedImage> images;
        for (int i=1; i<=static_cast<int>(gfx::Color::LightGray); ++i)
        {
            gfx::pack::NamedImage img;
            img.width  = math::rand(10, 150);
            img.height = math::rand(10, 150);
            img.name   = i;
            images.push_back(img);
        }
        const auto ret = gfx::pack::PackImages(images);
        gfx::Bitmap<gfx::RGB> bmp(ret.width, ret.height);
        bmp.Fill(gfx::Color::Black);
        for (size_t i=0; i<images.size(); ++i)
        {
            const auto& img = images[i];
            const gfx::Rect rc(img.xpos, img.ypos, img.width, img.height);
            // this are of the bitmap should be black i.e.
            // nothing has been placed there yet.
            const auto is_non_occupied = bmp.Compare(rc, gfx::Color::Black);
            TEST_REQUIRE(is_non_occupied);
            bmp.Fill(rc, static_cast<gfx::Color>(img.name));
        }

        WritePNG(bmp, "packed_image_test.png");

        for (const auto& img : images)
        {
            const auto expected_pixels = img.width * img.height;
            unsigned matching_pixels = 0;
            for (size_t y=0; y<bmp.GetHeight(); ++y)
            {
                for (size_t x=0; x<bmp.GetWidth(); ++x)
                {
                    const auto& p = bmp.GetPixel(y, x);
                    if (p == static_cast<gfx::Color>(img.name))
                        ++matching_pixels;
                }
            }
            TEST_REQUIRE(matching_pixels == expected_pixels);
        }
    }
    return 0;
}