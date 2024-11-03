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

#include "graphics/pixel.h"

namespace gfx
{

Pixel_RGB::Pixel_RGB(Color c)
{
    switch (c)
    {
        case Color::White:
            r = g = b = 255;
            break;
        case Color::Black:
            break;
        case Color::Red:
            r = 255;
            break;
        case Color::DarkRed:
            r = 127;
            break;
        case Color::Green:
            g = 255;
            break;
        case Color::DarkGreen:
            g = 127;
            break;
        case Color::Blue:
            b = 255;
            break;
        case Color::DarkBlue:
            b = 127;
            break;
        case Color::Cyan:
            g = b = 255;
            break;
        case Color::DarkCyan:
            g = b = 127;
            break;
        case Color::Magenta:
            r = b = 255;
            break;
        case Color::DarkMagenta:
            r = b = 127;
            break;
        case Color::Yellow:
            r = g = 255;
            break;
        case Color::DarkYellow:
            r = g = 127;
            break;
        case Color::Gray:
            r = g = b = 158;
            break;
        case Color::DarkGray:
            r = g = b = 127;
            break;
        case Color::LightGray:
            r = g = b = 192;
            break;
        case Color::HotPink:
            r = 255;
            g = 105;
            b = 180;
            break;
        case Color::Gold:
            r = 255;
            g = 215;
            b = 0;
            break;
        case Color::Silver:
            r = 192;
            g = 192;
            b = 192;
            break;
        case Color::Bronze:
            r = 205;
            g = 127;
            b = 50;
            break;
        case Color::Transparent:
            r = 0;
            g = 0;
            b = 0;
            break;
    }

} // ctor

Pixel_RGBA::Pixel_RGBA(Color name, u8 alpha)
{
    Pixel_RGB tmp(name);
    r = tmp.r;
    g = tmp.g;
    b = tmp.b;
    a = name == Color::Transparent ? 0 : alpha;
}

bool operator==(const Pixel_A& lhs, const Pixel_A& rhs)
{
    return lhs.r == rhs.r;
}

bool operator!=(const Pixel_A& lhs, const Pixel_A& rhs)
{
    return lhs.r != rhs.r;
}

Pixel_A operator & (const Pixel_A& lhs, const Pixel_A& rhs)
{
    Pixel_A ret;
    ret.r = lhs.r & rhs.r;
    return ret;
}

Pixel_A operator | (const Pixel_A& lhs, const Pixel_A& rhs)
{
    Pixel_A ret;
    ret.r = lhs.r | rhs.r;
    return ret;
}

Pixel_A operator >> (const Pixel_A& lhs, unsigned bits)
{
    Pixel_A ret;
    ret.r = lhs.r >> bits;
    return ret;
}

bool operator==(const Pixel_RGB& lhs, const Pixel_RGB& rhs)
{
    return lhs.r == rhs.r &&
           lhs.g == rhs.g &&
           lhs.b == rhs.b;
}

bool operator!=(const Pixel_RGB& lhs, const Pixel_RGB& rhs)
{
    return !(lhs == rhs);
}

Pixel_RGB operator & (const Pixel_RGB& lhs, const Pixel_RGB& rhs)
{
    Pixel_RGB ret;
    ret.r = lhs.r & rhs.r;
    ret.g = lhs.g & rhs.g;
    ret.b = lhs.b & rhs.b;
    return ret;
}

Pixel_RGB operator | (const Pixel_RGB& lhs, const Pixel_RGB& rhs)
{
    Pixel_RGB ret;
    ret.r = lhs.r | rhs.r;
    ret.g = lhs.g | rhs.g;
    ret.b = lhs.b | rhs.b;
    return ret;
}

Pixel_RGB operator >> (const Pixel_RGB& lhs, unsigned bits)
{
    Pixel_RGB ret;
    ret.r = lhs.r >> bits;
    ret.g = lhs.g >> bits;
    ret.b = lhs.b >> bits;
    return ret;
}

bool operator==(const Pixel_RGBA& lhs, const Pixel_RGBA& rhs)
{
    return lhs.r == rhs.r &&
           lhs.g == rhs.g &&
           lhs.b == rhs.b &&
           lhs.a == rhs.a;
}

bool operator!=(const Pixel_RGBA& lhs, const Pixel_RGBA& rhs)
{
    return !(lhs == rhs);
}

Pixel_RGBA operator & (const Pixel_RGBA& lhs, const Pixel_RGBA& rhs)
{
    Pixel_RGBA ret;
    ret.r = lhs.r & rhs.r;
    ret.g = lhs.g & rhs.g;
    ret.b = lhs.b & rhs.b;
    ret.a = lhs.a & rhs.a;
    return ret;
}

Pixel_RGBA operator | (const Pixel_RGBA& lhs, const Pixel_RGBA& rhs)
{
    Pixel_RGBA ret;
    ret.r = lhs.r | rhs.r;
    ret.g = lhs.g | rhs.g;
    ret.b = lhs.b | rhs.b;
    ret.a = lhs.a | rhs.a;
    return ret;
}

Pixel_RGBA operator >> (const Pixel_RGBA& lhs, unsigned bits)
{
    Pixel_RGBA ret;
    ret.r = lhs.r >> bits;
    ret.g = lhs.g >> bits;
    ret.b = lhs.b >> bits;
    ret.a = lhs.a >> bits;
    return ret;
}

Pixel_RGBAf operator + (const Pixel_RGBAf& lhs, const Pixel_RGBAf& rhs)
{
    Pixel_RGBAf ret;
    ret.r = lhs.r + rhs.r;
    ret.g = lhs.g + rhs.g;
    ret.b = lhs.b + rhs.b;
    ret.a = lhs.a + rhs.a;
    return ret;
}

Pixel_RGBAf operator * (const Pixel_RGBAf& lhs, float scaler)
{
    Pixel_RGBAf ret;
    ret.r = lhs.r * scaler;
    ret.g = lhs.g * scaler;
    ret.b = lhs.b * scaler;
    ret.a = lhs.a * scaler;
    return ret;
}

Pixel_RGBAf operator * (float scaler, const Pixel_RGBAf& rhs)
{
    Pixel_RGBAf ret;
    ret.r = rhs.r * scaler;
    ret.g = rhs.g * scaler;
    ret.b = rhs.b * scaler;
    ret.a = rhs.a * scaler;
    return ret;
}

Pixel_RGBf operator + (const Pixel_RGBf& lhs, const Pixel_RGBf& rhs)
{
    Pixel_RGBf ret;
    ret.r = lhs.r + rhs.r;
    ret.g = lhs.g + rhs.g;
    ret.b = lhs.b + rhs.b;
    return ret;
}

Pixel_RGBf operator * (const Pixel_RGBf& lhs, float scaler)
{
    Pixel_RGBf ret;
    ret.r = lhs.r * scaler;
    ret.g = lhs.g * scaler;
    ret.b = lhs.b * scaler;
    return ret;
}

Pixel_RGBf operator * (float scaler, const Pixel_RGBf& rhs)
{
    Pixel_RGBf ret;
    ret.r = rhs.r * scaler;
    ret.g = rhs.g * scaler;
    ret.b = rhs.b * scaler;
    return ret;
}

Pixel_Af operator + (const Pixel_Af& lhs, const Pixel_Af& rhs)
{
    Pixel_Af ret;
    ret.r = lhs.r + rhs.r;
    return ret;
}

Pixel_Af operator * (const Pixel_Af& lhs, float scaler)
{
    Pixel_Af ret;
    ret.r = lhs.r * scaler;
    return ret;
}

Pixel_Af operator * (float scaler, const Pixel_Af& rhs)
{
    Pixel_Af ret;
    ret.r = rhs.r * scaler;
    return ret;
}

float sRGB_decode(float value)
{
    return value <= 0.04045f
           ? value / 12.92f
           : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

float sRGB_encode(float value)
{
    return value <= 0.0031308f
           ? value * 12.92f
           : std::pow(value, 1.0f/2.4f) * 1.055f - 0.055f;
}

Pixel_RGBAf sRGB_decode(const Pixel_RGBAf& value)
{
    Pixel_RGBAf ret;
    ret.r = sRGB_decode(value.r);
    ret.g = sRGB_decode(value.g);
    ret.b = sRGB_decode(value.b);
    // alpha is not sRGB encoded.
    ret.a = value.a;
    return ret;
}

Pixel_RGBf sRGB_decode(const Pixel_RGBf& value)
{
    Pixel_RGBf ret;
    ret.r = sRGB_decode(value.r);
    ret.g = sRGB_decode(value.g);
    ret.b = sRGB_decode(value.b);
    return ret;
}

Pixel_RGBAf sRGB_encode(const Pixel_RGBAf& value)
{
    Pixel_RGBAf ret;
    ret.r = sRGB_encode(value.r);
    ret.g = sRGB_encode(value.g);
    ret.b = sRGB_encode(value.b);
    // alpha is not sRGB encoded.
    ret.a = value.a;
    return ret;
}

Pixel_RGBf sRGB_encode(const Pixel_RGBf& value)
{
    Pixel_RGBf ret;
    ret.r = sRGB_encode(value.r);
    ret.g = sRGB_encode(value.g);
    ret.b = sRGB_encode(value.b);
    return ret;
}

Pixel_RGBAf Pixel_to_floats(const Pixel_RGBA& value)
{
    Pixel_RGBAf ret;
    ret.r = value.r / 255.0f;
    ret.g = value.g / 255.0f;
    ret.b = value.b / 255.0f;
    ret.a = value.a / 255.0f;
    return ret;
}

Pixel_RGBf Pixel_to_floats(const Pixel_RGB& value)
{
    Pixel_RGBf ret;
    ret.r = value.r / 255.0f;
    ret.g = value.g / 255.0f;
    ret.b = value.b / 255.0f;
    return ret;
}

Pixel_Af Pixel_to_floats(const Pixel_A& value)
{
    Pixel_Af ret;
    ret.r = value.r / 255.0f;
    return ret;
}

Pixel_RGBA Pixel_to_uints(const Pixel_RGBAf& value)
{
    Pixel_RGBA ret;
    ret.r = value.r * 255;
    ret.g = value.g * 255;
    ret.b = value.b * 255;
    ret.a = value.a * 255;
    return ret;
}

Pixel_RGB Pixel_to_uints(const Pixel_RGBf& value)
{
    Pixel_RGB ret;
    ret.r = value.r * 255;
    ret.g = value.g * 255;
    ret.b = value.b * 255;
    return ret;
}

Pixel_A Pixel_to_uints(const Pixel_Af& value)
{
    Pixel_A ret;
    ret.r = value.r * 255;
    return ret;
}

Pixel_RGBAf RGBA_premul_alpha(const Pixel_RGBAf& rgba)
{
    Pixel_RGBAf ret;
    ret.r = rgba.r * rgba.a;
    ret.g = rgba.g * rgba.a;
    ret.b = rgba.b * rgba.a;
    ret.a = rgba.a;
    return ret;
}

Pixel_RGBAf sRGBA_from_color(Color name)
{
    const Color4f color(name);

    Pixel_RGBAf ret;
    ret.r = color.Red();
    ret.g = color.Green();
    ret.b = color.Blue();
    ret.a = 1.0f;
    return ret;
}

Pixel_RGBf sRGB_from_color(Color name)
{
    const Color4f color(name);

    Pixel_RGBf ret;
    ret.r = color.Red();
    ret.g = color.Green();
    ret.b = color.Blue();
    return ret;
}

double Pixel_MSE(const Pixel_A& lhs, const Pixel_A& rhs) noexcept
{
    const auto r = static_cast<int>(lhs.r) - static_cast<int>(rhs.r);
    const auto sum = r*r;
    const auto mse = sum / 1.0;
    return mse;
}

double Pixel_MSE(const Pixel_RGB& lhs, const Pixel_RGB& rhs) noexcept
{
    const auto r = static_cast<int>(lhs.r) - static_cast<int>(rhs.r);
    const auto g = static_cast<int>(lhs.g) - static_cast<int>(rhs.g);
    const auto b = static_cast<int>(lhs.b) - static_cast<int>(rhs.b);

    const auto sum = r*r + g*g + b*b;
    const auto mse = sum / 3.0;
    return mse;
}

double Pixel_MSE(const Pixel_RGBA& lhs, const Pixel_RGBA& rhs) noexcept
{
    const auto r = static_cast<int>(lhs.r) - static_cast<int>(rhs.r);
    const auto g = static_cast<int>(lhs.g) - static_cast<int>(rhs.g);
    const auto b = static_cast<int>(lhs.b) - static_cast<int>(rhs.b);
    const auto a = static_cast<int>(lhs.a) - static_cast<int>(rhs.a);

    const auto sum = r*r + g*g + b*b + a*a;
    const auto mse = sum / 4.0;
    return mse;
}

double Pixel_MSE(const Pixel_A_Array& lhs, const Pixel_A_Array& rhs) noexcept
{
    ASSERT(lhs.size() == rhs.size());
    const auto channels = 1;
    const auto samples = static_cast<double>(lhs.size() * channels);

    double mse = 0.0;

    for (size_t i=0; i<lhs.size(); ++i)
    {
        const auto r = static_cast<int>(lhs[i].r) - static_cast<int>(rhs[i].r);
        mse += ((r * r) / samples);
    }
    return mse;
}
double Pixel_MSE(const Pixel_RGB_Array& lhs, const Pixel_RGB_Array& rhs) noexcept
{
    ASSERT(lhs.size() == rhs.size());
    const auto channels = 3;
    const auto samples = static_cast<double>(lhs.size() * channels);

    double mse = 0.0;

    for (size_t i=0; i<lhs.size(); ++i)
    {
        const auto r = static_cast<int>(lhs[i].r) - static_cast<int>(rhs[i].r);
        const auto g = static_cast<int>(lhs[i].g) - static_cast<int>(rhs[i].g);
        const auto b = static_cast<int>(lhs[i].b) - static_cast<int>(rhs[i].b);

        mse += ((r * r) / samples);
        mse += ((g * g) / samples);
        mse += ((b * b) / samples);
    }
    return mse;
}
double Pixel_MSE(const Pixel_RGBA_Array& lhs, const Pixel_RGBA_Array& rhs) noexcept
{
    ASSERT(lhs.size() == rhs.size());
    const auto channels = 4;
    const auto samples = static_cast<double>(lhs.size() * channels);

    double mse = 0.0;

    for (size_t i=0; i<lhs.size(); ++i)
    {
        const auto r = static_cast<int>(lhs[i].r) - static_cast<int>(rhs[i].r);
        const auto g = static_cast<int>(lhs[i].g) - static_cast<int>(rhs[i].g);
        const auto b = static_cast<int>(lhs[i].b) - static_cast<int>(rhs[i].b);
        const auto a = static_cast<int>(lhs[i].a) - static_cast<int>(rhs[i].a);

        mse += ((r * r) / samples);
        mse += ((g * g) / samples);
        mse += ((b * b) / samples);
        mse += ((a * a) / samples);
    }
    return mse;
}


} // namespace
