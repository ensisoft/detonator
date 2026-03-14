// Copyright (C) 2020-2026 Sami Väisänen
// Copyright (C) 2020-2026 Ensisoft http://www.ensisoft.com
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

#include <tuple>
#include <algorithm>

#include "base/color.h"
#include "base/math.h"

#include "base/snafu.h"

namespace base
{
    namespace detail {
        // HSV to RGB and RGB to HSV based on wikipedia
        // https://en.wikipedia.org/wiki/HSL_and_HSV

        inline auto HSV_from_RGB(const float r, const float g, const float b) noexcept
        {
            const float rgb[] = {r, g, b};

            constexpr auto R = 0;
            constexpr auto G = 1;
            constexpr auto B = 2;

            unsigned min_index = 0;
            unsigned max_index = 0;
            for (unsigned i=1; i<3; ++i) {
                if (rgb[i] > rgb[max_index])
                    max_index = i;
                if (rgb[i] < rgb[min_index])
                    min_index = i;
            }
            const auto& max = rgb[max_index];
            const auto& min = rgb[min_index];

            const auto V = max;
            const auto C = max - min;

            // this is the lightness value for HSL. Leaving it here in case there's
            // a need to extend this to HSL formula as well.
            // const auto L = (max + min) / 2.0f;

            float H = 0.0f;
            if (C == 0.0f || (max_index == min_index)) {
                H = 0.0f;
            } else if (max_index == R) {
                H = 60.0f * std::fmodf((g - b)/C, 6.0f);
            } else if (max_index == G) {
                H = 60.0f * ((b - r)/C + 2.0f);
            } else if (max_index == B) {
                H = 60.0f * ((r - g)/C + 4.0f);
            }
            if (H < 0.0f)
                H += 360.0f;
            const float S = V > 0.0f ? C/V : 0.0f;
            return std::make_tuple(H, S, V);
        }
        inline auto RGB_from_HSV(const float h, const float s, const float v) noexcept
        {
            const auto V = v;
            const auto C = v * s;
            const auto H = h / 60.0f;
            const auto X = C * (1.0f - std::abs(std::fmodf(H, 2.0f) - 1.0f));

            std::tuple<float, float, float> RGB;
            if (0.0f <= H && H < 1)
                RGB = std::make_tuple(C, X, 0.0f);
            else if (1.0f <= H && H < 2)
                RGB = std::make_tuple(X, C, 0.0f);
            else if (2.0f <= H && H < 3)
                RGB = std::make_tuple(0.0f, C, X);
            else if (3.0f <= H && H < 4)
                RGB = std::make_tuple(0.0f, X, C);
            else if (4.0f <= H && H < 5)
                RGB = std::make_tuple(X, 0.0f, C);
            else if (5.0f <= H && H < 6)
                RGB = std::make_tuple(C, 0.0f, X);

            const auto m = V - C;
            const auto [r, g, b] = RGB;
            return std::make_tuple(r+m, g+m, b+m);
        }
    } // detail

    class ColorHSV
    {
    public:
        ColorHSV() = default;
        // not explicit on purpose in order to allow convenient implicit
        // conversion from Color to ColorHSV
        ColorHSV(const Color color) noexcept
        {
            const auto weights = detail::RGBColorWeights(color);
            const auto [h, s, v] = detail::HSV_from_RGB(weights.r, weights.g, weights.b);
            mH = math::clamp(0.0f, 360.0f, h);
            mS = math::clamp(0.0f, 1.0f, s);
            mV = math::clamp(0.0f, 1.0f, v);
        }
        ColorHSV(const float hue, const float saturation, const float value)
        {
            mH = math::clamp(0.0f, 360.0f, hue);
            mS = math::clamp(0.0f, 1.0f, saturation);
            mV = math::clamp(0.0f, 1.0f, value);
        }

        auto GetHue() const noexcept
        { return mH; }
        auto GetSaturation() const noexcept
        { return mS; }
        auto GetValue() const noexcept
        { return mV; }

        void SetHue(float hue) noexcept
        { mH = math::clamp(0.0f, 360.0f, hue); }
        void SetSaturation(float saturation) noexcept
        { mS = math::clamp(0.0f, 1.0f, saturation); }
        void SetValue(float value) noexcept
        { mV = math::clamp(0.0f, 1.0f, value); }

        void MultiplyBrightness(float coefficient) noexcept
        { mV = math::clamp(0.0f, 1.0f, mV * coefficient); }
        void AddBrightness(float delta) noexcept
        { mV = math::clamp(0.0f, 1.0f, mV + delta); }

        auto GetHSV() const noexcept
        {
            return std::make_tuple(mH, mS, mV);
        }
    private:
        // hue, saturation, value
        float mH = 0.0f;
        float mS = 0.0f;
        float mV = 0.0f;
    };
} // namespace