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

#include "base/color4f.h"
#include "base/color_hsv.h"

namespace base
{
    inline ColorHSV HSV_from_RGB(const Color4f& color) noexcept
    {
        const auto [r, g, b] = color.GetRGB();
        const auto [h, s, v] = detail::HSV_from_RGB(r, g, b);
        return ColorHSV(h, s, v);
    }

    inline Color4f RGB_from_HSV(const ColorHSV& color, const float alpha = 1.0f) noexcept
    {
        const auto [h, s, v] = color.GetHSV();
        const auto [r, g, b] = detail::RGB_from_HSV(h, s, v);
        return Color4f(r, g, b, alpha);
    }

    inline Color4f MultiplyBrightness(const Color4f& color, const float coefficient) noexcept
    {
        auto hsv = HSV_from_RGB(color);
        hsv.MultiplyBrightness(coefficient);
        return RGB_from_HSV(hsv, color.Alpha());
    }

    inline Color4f AddBrightness(const Color4f& color, const float coefficient) noexcept
    {
        auto hsv = HSV_from_RGB(color);
        hsv.AddBrightness(coefficient);
        return RGB_from_HSV(hsv, color.Alpha());
    }

} // namespace