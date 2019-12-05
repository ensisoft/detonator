// Copyright (c) 2010-2018 Sami Väisänen, Ensisoft
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

#pragma once

#include "config.h"

// simple type definitions

namespace gfx
{
    // Predefined color enum.
    enum class Color {
        Black,   White,
        Red,     DarkRed,
        Green,   DarkGreen,
        Blue,    DarkBlue,
        Cyan,    DarkCyan,
        Magenta, DarkMagenta,
        Yellow,  DarkYellow,
        Gray,    DarkGray, LightGray
    };

    // simple rectangle definition
    class Rect
    {
    public:
        Rect() {}
        Rect(unsigned x, unsigned y, unsigned w, unsigned h)
          : mX(x)
          , mY(y)
          , mWidth(w)
          , mHeight(h)
        {}
        unsigned GetHeight() const
        { return mHeight; }
        unsigned GetWidth() const
        { return mWidth; }
        unsigned GetX() const
        { return mX; }
        unsigned GetY() const
        { return mY; }
        void Resize(unsigned width, unsigned height)
        {
            mWidth = width;
            mHeight = height;
        }
        void Move(unsigned x, unsigned y)
        {
            mX = x;
            mY = y;
        }
    private:
        unsigned mX = 0;
        unsigned mY = 0;
        unsigned mWidth = 0;
        unsigned mHeight = 0;
    };

} // namespace
