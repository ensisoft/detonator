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
    namespace detail {
        struct RGBColorWeights {
            explicit RGBColorWeights(const Color color) noexcept
            {
                switch (color)
                {
                    case Color::White:
                        r = g = b = 1.0f;
                        break;
                    case Color::Black:
                        break;
                    case Color::Red:
                        r = 1.0f;
                        break;
                    case Color::DarkRed:
                        r = 0.5f;
                        break;
                    case Color::Green:
                        g = 1.0f;
                        break;
                    case Color::DarkGreen:
                        g = 0.5f;
                        break;
                    case Color::Blue:
                        b = 1.0f;
                        break;
                    case Color::DarkBlue:
                        b = 0.5f;
                        break;
                    case Color::Cyan:
                        g = b = 1.0f;
                        break;
                    case Color::DarkCyan:
                        g = b = 0.5f;
                        break;
                    case Color::Magenta:
                        r = b = 1.0f;
                        break;
                    case Color::DarkMagenta:
                        r = b = 0.5f;
                        break;
                    case Color::Yellow:
                        r = g = 1.0f;
                        break;
                    case Color::DarkYellow:
                        r = g = 0.5f;
                        break;
                    case Color::Gray:
                        r = g = b = 0.62f;
                        break;
                    case Color::DarkGray:
                        r = g = b = 0.5f;
                        break;
                    case Color::LightGray:
                        r = g = b = 0.75f;
                        break;
                    case Color::HotPink:
                        r = 1.0f;
                        g = 0.4117f;
                        b = 0.705f;
                        break;
                    case Color::Gold:
                        r = 1.0f;
                        g = 0.84313f;
                        b = 0.0f;
                        break;
                    case Color::Silver:
                        r = 0.752941f;
                        g = 0.752941f;
                        b = 0.752941f;
                        break;
                    case Color::Bronze:
                        r = 0.804f;
                        g = 0.498f;
                        b = 0.196f;
                        break;
                    case Color::Transparent:
                        r = 0.0f;
                        g = 0.0f;
                        b = 0.0f;
                        break;
                }
            }
            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;
        };
    }
} // namespace