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

#include "base/math.h"
#include "types.h"

namespace gfx
{
    // Linear floating point color represenation.
    // All values are clamped to 0-1 range.
    class Color4f
    {
    public:
        Color4f() {}
        // construct a Color4f object from floating point
        // channel values in the range of [0.0f, 1.0f]
        Color4f(float red, float green, float blue, float alpha)
        {
            mRed   = math::clamp(0.0f, 1.0f, red);
            mGreen = math::clamp(0.0f, 1.0f, green);
            mBlue  = math::clamp(0.0f, 1.0f, blue);
            mAlpha = math::clamp(0.0f, 1.0f, alpha);
        }

        // construct a new color object from integers
        // each integer gets clamped to [0, 255] range
        Color4f(int red, int green, int blue, int alpha)
        {
            // note: we take integers (as opposed to some
            // type unsinged) so that the simple syntax of
            // Color4f(10, 20, 200, 255) works without tricks.
            // Otherwise the conversion with the floats would
            // be ambiguous but the ints are a perfect match.
            mRed   = math::clamp(0, 255, red) / 255.0f;
            mGreen = math::clamp(0, 255, green) / 255.0f;
            mBlue  = math::clamp(0, 255, blue) / 255.0f;
            mAlpha = math::clamp(0, 255, alpha) / 255.0f;
        }

        Color4f(Color c)
          : mRed(0.0f)
          , mGreen(0.0f)
          , mBlue(0.0f)
          , mAlpha(1.0f)
        {
            switch (c)
            {
                case Color::White:
                    mRed = mGreen = mBlue = 1.0f;
                    break;
                case Color::Black:
                    break;
                case Color::Red:
                    mRed = 1.0f;
                    break;
                case Color::DarkRed:
                    mRed = 0.5f;
                    break;
                case Color::Green:
                    mGreen = 1.0f;
                    break;
                case Color::DarkGreen:
                    mGreen = 0.5f;
                    break;
                case Color::Blue:
                    mBlue = 1.0f;
                    break;
                case Color::DarkBlue:
                    mBlue = 0.5f;
                    break;
                case Color::Cyan:
                    mGreen = mBlue = 1.0f;
                    break;
                case Color::DarkCyan:
                    mGreen = mBlue = 0.5f;
                    break;
                case Color::Magenta:
                    mRed = mBlue = 1.0f;
                    break;
                case Color::DarkMagenta:
                    mRed = mBlue = 0.5f;
                    break;
                case Color::Yellow:
                    mRed = mGreen = 1.0f;
                    break;
                case Color::DarkYellow:
                    mRed = mGreen = 0.5f;
                    break;
                case Color::Gray:
                    mRed = mGreen = mBlue = 0.62;
                    break;
                case Color::DarkGray:
                    mRed = mGreen = mBlue = 0.5;
                    break;
                case Color::LightGray:
                    mRed = mGreen = mBlue = 0.75;
                    break;
            }
        }

        float Red() const
        { return mRed; }

        float Green() const
        { return mGreen; }

        float Blue() const
        { return mBlue; }

        float Alpha() const
        { return mAlpha; }

        void SetRed(float red)
        { mRed = math::clamp(0.0f, 1.0f, red); }
        void SetRed(int red)
        { mRed = math::clamp(0, 255, red) / 255.0f; }

        void setBlue(float blue)
        { mBlue = math::clamp(0.0f, 1.0f, blue); }
        void SetBlue(int blue)
        { mBlue = math::clamp(0, 255, blue) / 255.0f; }

        void SetGreen(float green)
        { mGreen = math::clamp(0.0f, 1.0f,  green); }
        void SetGreen(int green)
        { mGreen = math::clamp(0, 255, green) / 255.0f; }

        void SetAlpha(float alpha)
        { mAlpha = math::clamp(0.0f, 1.0f, alpha); }
        void SetAlpha(int alpha)
        { mAlpha = math::clamp(0, 255, alpha) / 255.0f; }

    private:
        float mRed   = 1.0f;
        float mGreen = 1.0f;
        float mBlue  = 1.0f;
        float mAlpha = 1.0f;
    };

    inline
    Color4f operator * (const Color4f& color, float scalar)
    {
        const auto r = color.Red();
        const auto g = color.Green();
        const auto b = color.Blue();
        const auto a = color.Alpha();
        return Color4f(r * scalar, g * scalar, b * scalar, a * scalar);
    }



} // namespace
