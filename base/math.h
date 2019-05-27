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

#include <boost/random/mersenne_twister.hpp>
#include <cmath>

namespace math
{
   const auto Pi = 3.14159265358979323846;

    // GLM is a great vector optimized library.
    // however here the goal is to move away from 3rd party
    // dependencies as much as possible and rely on standard c++ and OpenGL
    // to ultimately port to WebGL and JS through emscripten
    class Rect
    {
    public:
        Rect() {}
        Rect(float x, float y, float width, float height)
          : mX(x)
          , mY(y)
          , mW(width)
          , mH(height)
        {}
        float X() const { return mX; }
        float Y() const { return mY; }
        float Width() const { return mW; }
        float Height() const { return mH; }
    private:
        float mX = 0.0f;
        float mY = 0.0f;
        float mW = 0.0f;
        float mH = 0.0f;
    };


    class Vector2D
    {
    public:
        Vector2D() {}
        Vector2D(float x, float y) : mX(x), mY(y) {}

        float X() const
        { return mX; }

        float Y() const
        { return mY; }

        void Normalize()
        {
            const float len = Length();
            mX /= len;
            mY /= len;
        }
        float Length() const
        {
            return std::sqrt(mX*mX + mY*mY);
        }

        const Vector2D& operator+=(const Vector2D& other)
        {
            mX += other.mX;
            mY += other.mY;
            return *this;
        }
        const Vector2D& operator*=(float scalar)
        {
            mX *= scalar;
            mY *= scalar;
            return *this;
        }
    private:
        friend Vector2D operator+(const Vector2D&, const Vector2D&);
        friend Vector2D operator-(const Vector2D&, const Vector2D&);
        friend Vector2D operator*(const Vector2D&, float scalar);
    private:
        float mX = 0;
        float mY = 0;
    };

    inline
    Vector2D operator+(const Vector2D& lhs, const Vector2D& rhs)
    {
        return Vector2D(lhs.mX + rhs.mX, lhs.mY + rhs.mY);
    }
    inline
    Vector2D operator-(const Vector2D& lhs, const Vector2D& rhs)
    {
        return Vector2D(lhs.mX - rhs.mX, lhs.mY - rhs.mY);
    }
    inline
    Vector2D operator*(const Vector2D& vec, float scalar)
    {
        return Vector2D(vec.mX * scalar, vec.mY * scalar);
    }

    template<typename T> inline
    T wrap(T max, T min, T val)
    {
        if (val > max)
            return min;
        if (val < min)
            return max;
        return val;
    }

    template<typename T> inline
    T clamp(T min, T val, T max)
    {
        if (val < min)
            return min;
        if (val > max)
            return max;
        return val;
    }

    // generate a random number in the range of min max (inclusive)
    template<typename T>
    T rand(T min, T max)
    {
        static boost::random::mt19937 generator;

        const auto range = max - min;
        const auto value = (double)generator() / (double)generator.max();
        return min + (range * value);
    }

} // namespace
