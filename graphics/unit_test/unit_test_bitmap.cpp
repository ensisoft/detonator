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

#include <cmath>

#include "base/test_minimal.h"
#include "../bitmap.h"


int test_main(int argc, char* argv[])
{
    // test empty bitmap for "emptyness"
    {
        gfx::Bitmap<gfx::RGB> bmp;
        TEST_REQUIRE(bmp.GetWidth() == 0);
        TEST_REQUIRE(bmp.GetHeight() == 0);
        TEST_REQUIRE(bmp.GetData() == nullptr);
        TEST_REQUIRE(bmp.IsValid() == false);
    }

    // test initialized bitmap for "non emptyness"
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
            WritePPM(bmp, "/tmp/test_fill_" + std::to_string(i++) + ".ppm");
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
        dst.Fill(gfx::Color::White);
        dst.Copy(-2, -2, src);                                   
        TEST_REQUIRE(dst.GetPixel(0, 0) == gfx::Color::White);
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
        WritePPM(bmp, "/tmp/bitmap.ppm");
    }

    return 0;
}