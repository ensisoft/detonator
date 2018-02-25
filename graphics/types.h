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

#include <string>

#include "geometry.h"

namespace invaders
{
    class Transform
    {
    public:
        Transform() = default;

        Transform(float sx, float sy, float x, float y)
          : mScaleX(sx)
          , mScaleY(sy)
          , mPosX(x)
          , mPosY(y)
        {}

        void MoveTo(float x, float y)
        {
            mPosX = x;
            mPosY = y;
        }

        void Resize(float sx, float sy)
        {
            mScaleX = sx;
            mScaleY = sy;
        }
        float Width() const
        { return mScaleX; }

        float Height() const
        { return mScaleY; }

        float X() const
        { return mPosX; }

        float Y() const
        { return mPosY; }

    private:
        float mScaleX = 1.0f;
        float mScaleY = 1.0f;
        float mPosX = 0.0f;
        float mPosY = 0.0f;

    };


    class Shape
    {
    public:
        virtual ~Shape() = default;
        virtual void Upload(Geometry& geom) const = 0;
    private:
    };

    class Rect : public Shape
    {
    public:
        virtual void Upload(Geometry& geom) const
        {
            const Vertex verts[6] = {
                { 0,  0 },
                { 0, -1 },
                { 1, -1 },

                { 0,  0 },
                { 1, -1 },
                { 1,  0 }
            };
            geom.Update(verts, 6);
        }
    private:
    };

    // linear floating point color
    class Color4f
    {
    public:
        Color4f() {}
        Color4f(float red, float green, float blue, float alpha)
          : mRed(red)
          , mGreen(green)
          , mBlue(blue)
          , mAlpha(alpha)
        {}

        float Red() const
        { return mRed; }

        float Green() const
        { return mGreen; }

        float Blue() const
        { return mBlue; }

        float Alpha() const
        { return mAlpha; }

        void SetRed(float red)
        { mRed = red; }

        void setBlue(float blue)
        { mBlue = blue; }

        void SetGreen(float green)
        { mGreen = green; }

        void SetAlpha(float alpha)
        { mAlpha = alpha;}

    private:
        float mRed   = 1.0f;
        float mGreen = 1.0f;
        float mBlue  = 1.0f;
        float mAlpha = 1.0f;
    };

    class Gradient
    {
    public:
    };

    class Fill
    {
    public:
    private:
    };

    class Texture
    {
    public:
    };

} // namespace