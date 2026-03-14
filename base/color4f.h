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

#pragma once

#include "config.h"

#include <cmath>
#include <tuple>

#include "base/color.h"
#include "base/math.h"

namespace base
{
    // normalized floating point color using RGBA color model
    // The color space is left undefined since that really depends
    // on the context but most likely it's sRGB. For example when
    // initializing the color with "DarkGray" all RGB channels are
    // set to 0.5, which will appear to be "perceptually mid dark gray"
    // to the human eyeball iff the display system assumes that the
    // value is sRGB encoded and decodes it.
    class Color4f
    {
    public:
        Color4f() = default;

        // construct a Color4f object from floating point
        // channel values in the range of [0.0f, 1.0f]
        Color4f(const float red, const float green, const float blue, const float alpha = 1.0f) noexcept
        {
            mRed   = math::clamp(0.0f, 1.0f, red);
            mGreen = math::clamp(0.0f, 1.0f, green);
            mBlue  = math::clamp(0.0f, 1.0f, blue);
            mAlpha = math::clamp(0.0f, 1.0f, alpha);
        }

        // construct a new color object from integers
        // each integer gets clamped to [0, 255] range
        Color4f(const int red, const int green, const int blue, const int alpha = 255) noexcept
        {
            // note: we take integers (as opposed to some
            // type unsigned) so that the simple syntax of
            // Color4f(10, 20, 200, 255) works without tricks.
            // Otherwise, the conversion with the floats would
            // be ambiguous but the ints are a perfect match.
            mRed   = math::clamp(0, 255, red) / 255.0f;
            mGreen = math::clamp(0, 255, green) / 255.0f;
            mBlue  = math::clamp(0, 255, blue) / 255.0f;
            mAlpha = math::clamp(0, 255, alpha) / 255.0f;
        }

        explicit Color4f(const int rgb, const int alpha = 255)  noexcept
           : Color4f(rgb, rgb , rgb, alpha)
        {}

        explicit Color4f(const float rgb, const float alpha = 1.0f) noexcept
          : Color4f(rgb, rgb, rgb, alpha)
        {}

        // not explicit on purpose to allow for implicit
        // conversion from Color enum.
        Color4f(const Color c, const float alpha = 1.0f) noexcept
        {
            const auto weights = detail::RGBColorWeights(c);
            mRed   = weights.r;
            mGreen = weights.g;
            mBlue  = weights.b;
            mAlpha = c == Color::Transparent ? 0.0f : math::clamp(0.0f, 1.0f, alpha);
        }

        float Red() const noexcept
        { return mRed; }
        float Green() const noexcept
        { return mGreen; }
        float Blue() const noexcept
        { return mBlue; }
        float Alpha() const noexcept
        { return mAlpha; }
        void SetRed(const float red) noexcept
        { mRed = math::clamp(0.0f, 1.0f, red); }
        void SetRed(const int red) noexcept
        { mRed = math::clamp(0, 255, red) / 255.0f; }
        void SetBlue(const float blue) noexcept
        { mBlue = math::clamp(0.0f, 1.0f, blue); }
        void SetBlue(const int blue) noexcept
        { mBlue = math::clamp(0, 255, blue) / 255.0f; }
        void SetGreen(const float green) noexcept
        { mGreen = math::clamp(0.0f, 1.0f,  green); }
        void SetGreen(const int green) noexcept
        { mGreen = math::clamp(0, 255, green) / 255.0f; }
        void SetAlpha(const float alpha) noexcept
        { mAlpha = math::clamp(0.0f, 1.0f, alpha); }
        void SetAlpha(const int alpha) noexcept
        { mAlpha = math::clamp(0, 255, alpha) / 255.0f; }

        Color4f& operator+=(const Color4f& other) noexcept
        {
            mRed   = math::clamp(0.0f, 1.0f, mRed + other.mRed);
            mGreen = math::clamp(0.0f, 1.0f, mGreen + other.mGreen);
            mBlue  = math::clamp(0.0f, 1.0f, mBlue + other.mBlue);
            mAlpha = math::clamp(0.0f, 1.0f, mAlpha + other.mAlpha);
            return *this;
        }

        auto GetRGB() const noexcept
        {
            return std::make_tuple(mRed, mGreen, mBlue);
        }
        auto GetRGBA() const noexcept
        {
            return std::make_tuple(mRed, mGreen, mBlue, mAlpha);
        }

    private:
        float mRed   = 1.0f;
        float mGreen = 1.0f;
        float mBlue  = 1.0f;
        float mAlpha = 1.0f;
    };

    inline Color4f operator*(const Color4f& color, float scalar) noexcept
    {
        const auto r = color.Red();
        const auto g = color.Green();
        const auto b = color.Blue();
        const auto a = color.Alpha();
        return Color4f(r * scalar, g * scalar, b * scalar, a * scalar);
    }
    inline Color4f operator/(const Color4f& color, float scalar) noexcept
    {
        const auto r = color.Red();
        const auto g = color.Green();
        const auto b = color.Blue();
        const auto a = color.Alpha();

        scalar = 1.0f / scalar;
        return Color4f(r * scalar, g * scalar, b * scalar, a * scalar);
    }

    inline Color4f operator*(float scalar, const Color4f& color) noexcept
    {
        const auto r = color.Red();
        const auto g = color.Green();
        const auto b = color.Blue();
        const auto a = color.Alpha();
        return Color4f(r * scalar, g * scalar, b * scalar, a * scalar);
    }
    inline Color4f operator/(float scalar, const Color4f& color) noexcept
    {
        const auto r = color.Red();
        const auto g = color.Green();
        const auto b = color.Blue();
        const auto a = color.Alpha();

        scalar = 1.0f/ scalar;
        return Color4f(r * scalar, g * scalar, b * scalar, a * scalar);
    }

    inline Color4f operator+(const Color4f& lhs, const Color4f& rhs) noexcept
    {
        return Color4f(lhs.Red() + rhs.Red(),
                       lhs.Green() + rhs.Green(),
                       lhs.Blue() + rhs.Blue(),
                       lhs.Alpha() + rhs.Alpha());

    }
    inline Color4f operator/(const Color4f& lhs, const Color4f& rhs) noexcept
    {
        return Color4f(lhs.Red() / rhs.Red(),
                       lhs.Green() / rhs.Green(),
                       lhs.Blue() / rhs.Blue(),
                       lhs.Alpha() / rhs.Alpha());

    }

    inline bool Equals(const Color4f& lhs, const Color4f& rhs, float epsilon = 0.0001) noexcept
    {
        if (math::equals(lhs.Red(), rhs.Red()) &&
            math::equals(lhs.Green(), rhs.Green()) &&
            math::equals(lhs.Blue(), rhs.Blue()) &&
            math::equals(lhs.Alpha(), rhs.Alpha()))
            return true;
        return false;
    }

    inline float sRGB_Decode(float value) noexcept
    {
        return value <= 0.04045f
               ? value / 12.92f
               : std::pow((value + 0.055f) / 1.055f, 2.4f);
    }

    inline float sRGB_Encode(float value) noexcept
    {
        return value <= 0.0031308f
               ? value * 12.92f
               : std::pow(value, 1.0f/2.4f) * 1.055f - 0.055f;
    }

    // Encode a linear color value into sRGB encoded color.
    inline Color4f sRGB_Encode(const Color4f& color) noexcept
    {
        return Color4f(sRGB_Encode(color.Red()),
                       sRGB_Encode(color.Green()),
                       sRGB_Encode(color.Blue()),
                       color.Alpha());
    }

    // Decode a sRGB color value into a linear color.
    inline Color4f sRGB_Decode(const Color4f& color) noexcept
    {
        return Color4f(sRGB_Decode(color.Red()),
                       sRGB_Decode(color.Green()),
                       sRGB_Decode(color.Blue()),
                       color.Alpha());
    }


} // namespace
