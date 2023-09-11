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

#include "base/test_minimal.h"
#include "base/math.h"
#include "base/types.h"
#include "graphics/bitmap.h"
#include "editor/app/packing.h"

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
        gfx::Bitmap<gfx::Pixel_RGB> bmp(ret.width, ret.height);
        bmp.Fill(gfx::Color::Black);
        for (size_t i=0; i<images.size(); ++i)
        {
            const auto& img = images[i];
            const base::URect rc(img.xpos, img.ypos, img.width, img.height);
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
