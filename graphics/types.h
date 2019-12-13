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
    template<typename T>
    class TRect
    {
    public:
        TRect() {}
        TRect(T x, T y, T w, T h)
          : mX(x)
          , mY(y)
          , mWidth(w)
          , mHeight(h)
        {}
        T GetHeight() const
        { return mHeight; }
        T GetWidth() const
        { return mWidth; }
        T GetX() const
        { return mX; }
        T GetY() const
        { return mY; }
        T Resize(T width, T height)
        {
            mWidth = width;
            mHeight = height;
        }
        T Move(T x, T y)
        {
            mX = x;
            mY = y;
        }
    private:
        T mX = 0;
        T mY = 0;
        T mWidth = 0;
        T mHeight = 0;
    };

    using Rect  = TRect<unsigned>;
    using RectF = TRect<float>;
    using RectI = TRect<int>;

} // namespace
