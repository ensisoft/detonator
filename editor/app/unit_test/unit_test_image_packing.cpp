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

#include "editor/app/packing.h"
#include "graphics/bitmap.h"
#include "graphics/types.h"

void unit_test_unbounded()
{
    // single image
    {
        std::vector<app::PackingRectangle> images;
        images.push_back({1, 1, 64, 64});

        const auto ret = app::PackRectangles(images);
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
        std::vector<app::PackingRectangle> images;
        for (int i=1; i<=static_cast<int>(gfx::Color::LightGray); ++i)
        {
            app::PackingRectangle img;
            img.width  = math::rand(10, 150);
            img.height = math::rand(10, 150);
            img.index   = i;
            images.push_back(img);
        }
        const auto ret = app::PackRectangles(images);
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
            bmp.Fill(rc, static_cast<gfx::Color>(img.index));
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
                    if (p == static_cast<gfx::Color>(img.index))
                        ++matching_pixels;
                }
            }
            TEST_REQUIRE(matching_pixels == expected_pixels);
        }
    }
}

void unit_test_bounded()
{
    // single image
    {
        std::vector<app::PackingRectangle> list;
        list.push_back({1, 1, 64, 64});

        TEST_REQUIRE(app::PackRectangles(app::RectanglePackSize{10, 10}, list) == false);
        TEST_REQUIRE(app::PackRectangles(app::RectanglePackSize{64, 64}, list) == true);
        TEST_REQUIRE(list[0].success);
        TEST_REQUIRE(list[0].xpos == 0);
        TEST_REQUIRE(list[0].ypos == 0);
    }

    {
        std::vector<app::PackingRectangle> list;
        list.push_back({0, 0, 64, 64});
        list.push_back({0, 0, 32, 32});
        list.push_back({0, 0, 16, 16});
        list[0].cookie = "64";
        list[1].cookie = "32";
        list[2].cookie = "16";
        TEST_REQUIRE(app::PackRectangles({96, 96}, list) == true);
        TEST_REQUIRE(list[0].cookie == "64");
        TEST_REQUIRE(list[0].xpos == 0);
        TEST_REQUIRE(list[0].ypos == 0);
        TEST_REQUIRE(list[1].cookie == "32");
        TEST_REQUIRE(list[1].xpos == 64);
        TEST_REQUIRE(list[1].ypos == 0);
        TEST_REQUIRE(list[2].cookie == "16");
        TEST_REQUIRE(list[2].xpos == 64);
        TEST_REQUIRE(list[2].ypos == 32);
    }

    {
        std::vector<app::PackingRectangle> list;
        list.push_back({0, 0, 64, 64});
        list.push_back({0, 0, 32, 32});
        list.push_back({0, 0, 32, 32});
        list.push_back({0, 0, 96, 32});
        list[0].cookie = "64";
        list[1].cookie = "32_1";
        list[2].cookie = "32_2";
        list[3].cookie = "96";
        TEST_REQUIRE(app::PackRectangles({96, 96}, list) == true);
    }

    {
        std::vector<app::PackingRectangle> list;
        list.push_back({0, 0, 64, 64});
        list.push_back({0, 0, 32, 32});
        list.push_back({0, 0, 32, 32});
        list.push_back({0, 0, 96, 32});
        list.push_back({0, 0, 16, 16});
        list[0].cookie = "64";
        list[1].cookie = "32_1";
        list[2].cookie = "32_2";
        list[3].cookie = "96";
        list[4].cookie = "16";
        TEST_REQUIRE(app::PackRectangles({96, 96}, list) == false);
    }
}

int test_main(int argc, char* argv[])
{
    unit_test_unbounded();
    unit_test_bounded();
    return 0;
}
