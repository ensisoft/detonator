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