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
#include "../image.h"

int test_main(int argc, char* argv[])
{
    // todo: fix the path if the data files can be copied into
    // the current binary output folder.

    {
        gfx::Image img("../graphics/unit_test/4x4_square_rgb.jpg");
        TEST_REQUIRE(img.GetHeight() == 4);
        TEST_REQUIRE(img.GetWidth() == 4);
        TEST_REQUIRE(img.GetDepthBits() == 24);

        gfx::Bitmap<gfx::RGB> bmp = img.AsBitmap<gfx::RGB>(); 
        gfx::Bitmap<gfx::RGB> ref(4, 4);
        
        // top left 2x2
        ref.SetPixel(0, 0, gfx::Color::Red);
        ref.SetPixel(0, 1, gfx::Color::Red);
        ref.SetPixel(1, 0, gfx::Color::Red); 
        ref.SetPixel(1, 1, gfx::Color::Red);

        // top right 2x2
        ref.SetPixel(0, 2, gfx::Color::Green);
        ref.SetPixel(0, 3, gfx::Color::Green);
        ref.SetPixel(1, 2, gfx::Color::Green); 
        ref.SetPixel(1, 3, gfx::Color::Green);        

        // bottom left 2x2
        ref.SetPixel(2, 0, gfx::Color::Blue);
        ref.SetPixel(2, 1, gfx::Color::Blue);
        ref.SetPixel(3, 0, gfx::Color::Blue); 
        ref.SetPixel(3, 1, gfx::Color::Blue);

        // bottom right 2x2
        ref.SetPixel(2, 2, gfx::Color::White);
        ref.SetPixel(2, 3, gfx::Color::White);
        ref.SetPixel(3, 2, gfx::Color::White); 
        ref.SetPixel(3, 3, gfx::Color::White);                

        gfx::Bitmap<gfx::RGB>::MSE mse;
        mse.SetErrorTreshold(10);
        TEST_REQUIRE(Compare(bmp, gfx::URect(0, 0, 4, 4), ref, mse));

    }

    {
        gfx::Image img("../graphics/unit_test/4x4_square_rgba.png");
        TEST_REQUIRE(img.GetHeight() == 4);
        TEST_REQUIRE(img.GetWidth() == 4);
        TEST_REQUIRE(img.GetDepthBits() == 32);

        gfx::Bitmap<gfx::RGBA> bmp = img.AsBitmap<gfx::RGBA>(); 
        gfx::Bitmap<gfx::RGBA> ref(4, 4);
        
        // top left 2x2
        ref.SetPixel(0, 0, gfx::Color::Red);
        ref.SetPixel(0, 1, gfx::Color::Red);
        ref.SetPixel(1, 0, gfx::Color::Red); 
        ref.SetPixel(1, 1, gfx::Color::Red);

        // top right 2x2
        ref.SetPixel(0, 2, gfx::Color::Green);
        ref.SetPixel(0, 3, gfx::Color::Green);
        ref.SetPixel(1, 2, gfx::Color::Green); 
        ref.SetPixel(1, 3, gfx::Color::Green);        

        // bottom left 2x2
        ref.SetPixel(2, 0, gfx::Color::Blue);
        ref.SetPixel(2, 1, gfx::Color::Blue);
        ref.SetPixel(3, 0, gfx::Color::Blue); 
        ref.SetPixel(3, 1, gfx::Color::Blue);

        // bottom right 2x2
        ref.SetPixel(2, 2, gfx::Color::White);
        ref.SetPixel(2, 3, gfx::Color::White);
        ref.SetPixel(3, 2, gfx::Color::White); 
        ref.SetPixel(3, 3, gfx::Color::White);                

        gfx::Bitmap<gfx::RGBA>::MSE mse;
        mse.SetErrorTreshold(10);
        TEST_REQUIRE(Compare(bmp, gfx::URect(0, 0, 4, 4), ref, mse));

    }

    //
    {
        gfx::Image img("../graphics/unit_test/4x4_square_grayscale.jpg");
        TEST_REQUIRE(img.GetHeight() == 4);
        TEST_REQUIRE(img.GetWidth() == 4);
        TEST_REQUIRE(img.GetDepthBits() == 8);

        gfx::Bitmap<gfx::Grayscale> bmp = img.AsBitmap<gfx::Grayscale>();
        
        gfx::Bitmap<gfx::Grayscale> ref(4, 4);
        ref.Fill(0xff);
        ref.SetPixel(0, 0, 0);
        ref.SetPixel(0, 1, 0);
        ref.SetPixel(1, 0, 0); 
        ref.SetPixel(1, 1, 0);
        ref.SetPixel(2, 2, 0x7f);
        ref.SetPixel(2, 3, 0x7f);
        ref.SetPixel(3, 2, 0x7f);
        ref.SetPixel(3, 3, 0x7f);

        gfx::Bitmap<gfx::Grayscale>::MSE mse;
        mse.SetErrorTreshold(10);
        TEST_REQUIRE(Compare(bmp, gfx::URect(0, 0, 4, 4), ref, mse));
    }



    return 0;
}