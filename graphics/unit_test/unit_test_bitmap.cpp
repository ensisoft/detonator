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
#include "data/json.h"
#include "graphics/color4f.h"
#include "graphics/bitmap.h"

int test_main(int argc, char* argv[])
{
    // test empty bitmap for "emptiness"
    {
        gfx::Bitmap<gfx::RGB> bmp;
        TEST_REQUIRE(bmp.GetWidth() == 0);
        TEST_REQUIRE(bmp.GetHeight() == 0);
        TEST_REQUIRE(bmp.GetData() == nullptr);
        TEST_REQUIRE(bmp.IsValid() == false);
    }

    // test initialized bitmap for "non emptiness"
    {

        gfx::Bitmap<gfx::RGB> bmp(2, 2);
        TEST_REQUIRE(bmp.GetWidth() == 2);
        TEST_REQUIRE(bmp.GetHeight() == 2);
        TEST_REQUIRE(bmp.GetData() != nullptr);        
        TEST_REQUIRE(bmp.IsValid() == true);
    }

    // test pixel set/get
    {
        gfx::Bitmap<gfx::RGB> bmp(2, 2);
        bmp.SetPixel(0, 0, gfx::Color::White);
        bmp.SetPixel(0, 1, gfx::Color::Red);        
        bmp.SetPixel(1, 0, gfx::Color::Green);        
        bmp.SetPixel(1, 1, gfx::Color::Yellow);                
        TEST_REQUIRE(bmp.GetPixel(0, 0) == gfx::Color::White);
        TEST_REQUIRE(bmp.GetPixel(0, 1) == gfx::Color::Red);
        TEST_REQUIRE(bmp.GetPixel(1, 0) == gfx::Color::Green);
        TEST_REQUIRE(bmp.GetPixel(1, 1) == gfx::Color::Yellow);        
    }

    // test bitmap filling
    {
        {
            gfx::Bitmap<gfx::RGB> bmp(2, 2);
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

        gfx::Bitmap<gfx::RGB> bmp(100, 100);
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

    // test the compare against pixel
    {
        gfx::Bitmap<gfx::RGB> bmp(100, 100);
        struct Rect {
            int x, y; 
            int w, h;
        } test_rects[] = {
            {0, 0, 10, 10},
            {0, 0, 1, 100},
            {0, 0, 100, 1},
            {0, 99, 100, 1},
            {99, 0, 1, 100},
            {40, 40, 40, 40},
            {0, 0, 100, 100},
            {0, 0, 200, 200},
        };
        for (const auto& r : test_rects) 
        {
            const auto& rc = gfx::URect(r.x, r.y, r.w, r.h);
            // we've tested the fill operation previously already.
            bmp.Fill(gfx::Color::White);
            bmp.Fill(rc, gfx::Color::Green);
            TEST_REQUIRE(bmp.Compare(rc, gfx::Color::Green));
        }        
        
    }


    // test copying of data from a pixel buffer pointer
    {
        gfx::Bitmap<gfx::RGB> dst(4, 4);
        dst.Fill(gfx::Color::White);

        const gfx::RGB red_data[2*2] = {
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
        gfx::Bitmap<gfx::RGB> dst(4, 4);
        gfx::Bitmap<gfx::RGB> src(2, 2);
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
        gfx::Bitmap<gfx::RGB> src(4, 4);
        src.Fill(gfx::URect(0, 0, 2, 2), gfx::Color::Red);
        src.Fill(gfx::URect(2, 0, 2, 2), gfx::Color::Green);
        src.Fill(gfx::URect(0, 2, 2, 2), gfx::Color::Blue);
        src.Fill(gfx::URect(2, 2, 2, 2), gfx::Color::Yellow);
        // copy whole bitmap.
        {
            const auto& ret = src.Copy(gfx::URect(0, 0, 4, 4));
            TEST_REQUIRE(Compare(ret, src));
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

    // test flip
    {
        gfx::Bitmap<gfx::RGB> bmp(4, 5);
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

    // test PPM writing
    {
        gfx::Bitmap<gfx::RGB> bmp(256, 256);
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

    // test mip map generation with box filter
    {
        gfx::Bitmap<gfx::RGB> src(256, 256);
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
            gfx::Bitmap<gfx::RGB> bmp;
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
    return 0;
}