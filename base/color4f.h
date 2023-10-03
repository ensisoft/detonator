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

#include "base/math.h"

namespace base
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
        Gray,    DarkGray, LightGray,
        // some special colors
        HotPink, Transparent,
        Gold,
        Silver,
        Bronze
    };


    // Linear floating point color representation
    // All values are clamped to 0-1 range.
    class Color4f
    {
    public:
        Color4f() = default;
        // construct a Color4f object from floating point
        // channel values in the range of [0.0f, 1.0f]
        Color4f(float red, float green, float blue, float alpha) noexcept
        {
            mRed   = math::clamp(0.0f, 1.0f, red);
            mGreen = math::clamp(0.0f, 1.0f, green);
            mBlue  = math::clamp(0.0f, 1.0f, blue);
            mAlpha = math::clamp(0.0f, 1.0f, alpha);
        }

        // construct a new color object from integers
        // each integer gets clamped to [0, 255] range
        Color4f(int red, int green, int blue, int alpha) noexcept
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

        Color4f(Color c, float alpha = 1.0f) noexcept
                : mRed(0.0f)
                , mGreen(0.0f)
                , mBlue(0.0f)
        {
            mAlpha = math::clamp(0.0f, 1.0f, alpha);
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
                    mRed = mGreen = mBlue = 0.62f;
                    break;
                case Color::DarkGray:
                    mRed = mGreen = mBlue = 0.5f;
                    break;
                case Color::LightGray:
                    mRed = mGreen = mBlue = 0.75f;
                    break;
                case Color::HotPink:
                    mRed   = 1.0f;
                    mGreen = 0.4117f;
                    mBlue  = 0.705f;
                    break;
                case Color::Gold:
                    mRed = 1.0f;
                    mGreen = 0.84313f;
                    mBlue  = 0.0f;
                    break;
                case Color::Silver:
                    mRed   = 0.752941f;
                    mGreen = 0.752941f;
                    mBlue  = 0.752941f;
                    break;
                case Color::Bronze:
                    mRed   = 0.804f;
                    mGreen = 0.498f;
                    mBlue  = 0.196f;
                    break;
                case Color::Transparent:
                    mRed   = 0.0f;
                    mGreen = 0.0f;
                    mBlue  = 0.0f;
                    mAlpha = 0.0f;
            }
        }

        inline float Red() const noexcept
        { return mRed; }
        inline float Green() const noexcept
        { return mGreen; }
        inline float Blue() const noexcept
        { return mBlue; }
        inline float Alpha() const noexcept
        { return mAlpha; }
        inline void SetRed(float red) noexcept
        { mRed = math::clamp(0.0f, 1.0f, red); }
        inline void SetRed(int red) noexcept
        { mRed = math::clamp(0, 255, red) / 255.0f; }
        inline void SetBlue(float blue) noexcept
        { mBlue = math::clamp(0.0f, 1.0f, blue); }
        inline void SetBlue(int blue) noexcept
        { mBlue = math::clamp(0, 255, blue) / 255.0f; }
        inline void SetGreen(float green) noexcept
        { mGreen = math::clamp(0.0f, 1.0f,  green); }
        inline void SetGreen(int green) noexcept
        { mGreen = math::clamp(0, 255, green) / 255.0f; }
        inline void SetAlpha(float alpha) noexcept
        { mAlpha = math::clamp(0.0f, 1.0f, alpha); }
        inline void SetAlpha(int alpha) noexcept
        { mAlpha = math::clamp(0, 255, alpha) / 255.0f; }
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
    inline Color4f operator*(float scalar, const Color4f& color) noexcept
    {
        const auto r = color.Red();
        const auto g = color.Green();
        const auto b = color.Blue();
        const auto a = color.Alpha();
        return Color4f(r * scalar, g * scalar, b * scalar, a * scalar);
    }
    inline Color4f operator+(const Color4f& lhs, const Color4f& rhs) noexcept
    {
        return Color4f(lhs.Red()    + rhs.Red(),
                       lhs.Green() + rhs.Green(),
                       lhs.Blue()   + rhs.Blue(),
                       lhs.Alpha() + rhs.Alpha());

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
