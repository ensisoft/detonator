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

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <optional>

#include "base/math.h"
#include "base/utility.h"

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
        Gray,    DarkGray, LightGray,
        // some special colors
        HotPink,
        Gold,
        Silver,
        Bronze
    };


    // Linear floating point color represenation.
    // All values are clamped to 0-1 range.
    class Color4f
    {
    public:
        Color4f() = default;
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

        Color4f(Color c, float alpha = 1.0f)
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
                    mRed = 0.752941;
                    mGreen = 0.752941;
                    mBlue = 0.752941;
                    break;
                case Color::Bronze:
                    mRed = 0.804;
                    mGreen = 0.498;
                    mBlue = 0.196;
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

        nlohmann::json ToJson() const
        {
            nlohmann::json json;
            base::JsonWrite(json, "r", mRed);
            base::JsonWrite(json, "g", mGreen);
            base::JsonWrite(json, "b", mBlue);
            base::JsonWrite(json, "a", mAlpha);
            return json;
        }
        static std::optional<Color4f> FromJson(const nlohmann::json& object)
        {
            Color4f ret;
            if (!base::JsonReadSafe(object, "r", &ret.mRed) ||
                !base::JsonReadSafe(object, "g", &ret.mGreen) ||
                !base::JsonReadSafe(object, "b", &ret.mBlue) ||
                !base::JsonReadSafe(object, "a", &ret.mAlpha))
                return std::nullopt;
            return ret;
        }

        size_t GetHash() const
        {
            size_t hash = 0;
            hash = base::hash_combine(hash, mRed);
            hash = base::hash_combine(hash, mGreen);
            hash = base::hash_combine(hash, mBlue);
            hash = base::hash_combine(hash, mAlpha);
            return hash;
        }

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
