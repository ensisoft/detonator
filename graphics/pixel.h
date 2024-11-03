// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#include <cstdint>
#include <vector>

#include "graphics/color4f.h"

namespace gfx
{

    using u8 = std::uint8_t;

    // Pixel_A represents linear opacity from 0.0f/0x00 to 1.0f/0xff,
    // 0.0f being fully transparent and 1.0f being fully opaque.
    struct Pixel_A {
        u8 r = 0;
        Pixel_A(u8 value=0) : r(value)
        {}
    };
    bool operator==(const Pixel_A& lhs, const Pixel_A& rhs);
    bool operator!=(const Pixel_A& lhs, const Pixel_A& rhs);
    Pixel_A operator & (const Pixel_A& lhs, const Pixel_A& rhs);
    Pixel_A operator | (const Pixel_A& lhs, const Pixel_A& rhs);
    Pixel_A operator >> (const Pixel_A& lhs, unsigned bits);

    // Pixel_RGB represents a pixel in the Pixel_RGB color model but doesn't
    // require any specific color model/encoding. The actual channel
    // values can thus represent color values either in sRGB, linear
    // or some other 8bit encoding.
    struct Pixel_RGB {
        u8 r = 0;
        u8 g = 0;
        u8 b = 0;
        Pixel_RGB(u8 red=0, u8 green=0, u8 blue=0)
          : r(red), g(green), b(blue)
        {}
        // Set the Pixel_RGB value based on a color name.
        // The result is sRGB encoded Pixel_RGB triplet.
        Pixel_RGB(Color name);
    };

    bool operator==(const Pixel_RGB& lhs, const Pixel_RGB& rhs);
    bool operator!=(const Pixel_RGB& lhs, const Pixel_RGB& rhs);
    Pixel_RGB operator & (const Pixel_RGB& lhs, const Pixel_RGB& rhs);
    Pixel_RGB operator | (const Pixel_RGB& lhs, const Pixel_RGB& rhs);
    Pixel_RGB operator >> (const Pixel_RGB& lhs, unsigned bits);

    // Pixel_RGBA represents a pixel in the Pixel_RGB color model but doesn't
    // require any actual color model/encoding. The actual channel
    // color values can thus represent color values either in sRGB,
    // linear or some other 8bit encoding.
    // Note that even when using sRGB the alpha value is not sRGB
    // encoded but represents pixel's transparency on a linear scale.
    // In addition, the alpha value can be either straight or pre-multiplied.
    struct Pixel_RGBA {
        u8 r = 0;
        u8 g = 0;
        u8 b = 0;
        u8 a = 255;
        Pixel_RGBA(u8 red=0, u8 green=0, u8 blue=0, u8 alpha = 0xff)
          : r(red), g(green), b(blue), a(alpha)
        {}
        // Set the Pixel_RGB value based on a color name.
        // The result is sRGB encoded Pixel_RGB triplet.
        Pixel_RGBA(Color name, u8 alpha=255);
    };

    bool operator==(const Pixel_RGBA& lhs, const Pixel_RGBA& rhs);
    bool operator!=(const Pixel_RGBA& lhs, const Pixel_RGBA& rhs);
    Pixel_RGBA operator & (const Pixel_RGBA& lhs, const Pixel_RGBA& rhs);
    Pixel_RGBA operator | (const Pixel_RGBA& lhs, const Pixel_RGBA& rhs);
    Pixel_RGBA operator >> (const Pixel_RGBA& lhs, unsigned bits);

    struct Pixel_RGBAf {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 0.0f;
    };
    Pixel_RGBAf operator + (const Pixel_RGBAf& lhs, const Pixel_RGBAf& rhs);
    Pixel_RGBAf operator * (const Pixel_RGBAf& lhs, float scaler);
    Pixel_RGBAf operator * (float scaler, const Pixel_RGBAf& rhs);

    struct Pixel_RGBf {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
    };
    Pixel_RGBf operator + (const Pixel_RGBf& lhs, const Pixel_RGBf& rhs);
    Pixel_RGBf operator * (const Pixel_RGBf& lhs, float scaler);
    Pixel_RGBf operator * (float scaler, const Pixel_RGBf& rhs);

    struct Pixel_Af {
        float r = 0.0f;
    };
    Pixel_Af operator + (const Pixel_Af& lhs, const Pixel_Af& rhs);
    Pixel_Af operator * (const Pixel_Af& lhs, float scaler);
    Pixel_Af operator * (float scaler, const Pixel_Af& rhs);

    float sRGB_decode(float value);
    float sRGB_encode(float value);
    Pixel_RGBAf sRGB_decode(const Pixel_RGBAf& value);
    Pixel_RGBAf sRGB_encode(const Pixel_RGBAf& value);
    Pixel_RGBf sRGB_decode(const Pixel_RGBf& value);
    Pixel_RGBf sRGB_encode(const Pixel_RGBf& value);

    // transform between unsigned integer and floating
    // point pixel representations.
    Pixel_RGBAf Pixel_to_floats(const Pixel_RGBA& value);
    Pixel_RGBf Pixel_to_floats(const Pixel_RGB& value);
    Pixel_Af Pixel_to_floats(const Pixel_A& value);
    Pixel_RGBA Pixel_to_uints(const Pixel_RGBAf& value);
    Pixel_RGB Pixel_to_uints(const Pixel_RGBf& value);
    Pixel_A Pixel_to_uints(const Pixel_Af& value);

    Pixel_RGBAf sRGBA_from_color(Color name);
    Pixel_RGBf sRGB_from_color(Color name);
    Pixel_RGBAf RGBA_premul_alpha(const Pixel_RGBAf& rgba);

    double Pixel_MSE(const Pixel_A& lhs, const Pixel_A& rhs) noexcept;
    double Pixel_MSE(const Pixel_RGB& lhs, const Pixel_RGB& rhs) noexcept;
    double Pixel_MSE(const Pixel_RGBA& lhs, const Pixel_RGBA& rhs) noexcept;

    using Pixel_A_Array = std::vector<Pixel_A>;
    using Pixel_RGB_Array = std::vector<Pixel_RGB>;
    using Pixel_RGBA_Array = std::vector<Pixel_RGBA>;

    double Pixel_MSE(const Pixel_A_Array& lhs, const Pixel_A_Array& rhs) noexcept;
    double Pixel_MSE(const Pixel_RGB_Array& lhs, const Pixel_RGB_Array& rhs) noexcept;
    double Pixel_MSE(const Pixel_RGBA_Array& lhs, const Pixel_RGBA_Array& rhs) noexcept;

    static_assert(sizeof(Pixel_A) == 1,
                  "Unexpected size of Pixel_A pixel struct type.");
    static_assert(sizeof(Pixel_RGB) == 3,
                  "Unexpected size of Pixel_RGB pixel struct type.");
    static_assert(sizeof(Pixel_RGBA) == 4,
                  "Unexpected size of Pixel_RGBA pixel struct type.");

    template<typename Pixel>
    inline Pixel RasterOp_SourceOver(const Pixel& dst, const Pixel& src)
    { return src; }

    template<typename Pixel>
    inline Pixel RasterOp_BitwiseAnd(const Pixel& dst, const Pixel& src)
    { return dst & src; }

    template<typename Pixel>
    inline Pixel RasterOp_BitwiseOr(const Pixel& dst, const Pixel& src)
    { return dst | src; }

    namespace PixelEquality {
        struct PixelPrecision {
            template<typename Pixel>
            bool operator()(const Pixel& lhs, const Pixel& rhs) const
            { return lhs == rhs; }
        };

        // mean-squared-error
        struct ThresholdPrecision {
            ThresholdPrecision() = default;
            explicit ThresholdPrecision(double max_mse) noexcept
              : max_mse(max_mse)
            {}

            template<typename Pixel>
            bool operator()(const Pixel& lhs, const Pixel& rhs) const
            {
                const auto mse = Pixel_MSE(lhs, rhs);
                return mse <= max_mse;
            }
            void SetErrorThreshold(double se)
            {
                max_mse = se * se;
            }

            static constexpr double ZeroTolerance = 0.0;
            static constexpr double LowTolerance  = 500.0;
            static constexpr double HighTolerance = 1000.0;
            double max_mse = ZeroTolerance;
        };
    } // PixelEquality

} // namespace