// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
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

#include <cstdio>
#include <string>

#include "graphics/types.h"

namespace gfx
{
// basic formatting functionality for some simple types.
// todo: move the JSON functionality here.
inline std::string ToString(const FRect& rect)
{
    char buff[100];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff),"x:%.2f, y:%.2f, w:%.2f, h:%.2f",
                  rect.GetX(), rect.GetY(), rect.GetWidth(), rect.GetHeight());
    return buff;
}

inline std::string ToString(const FSize& size)
{
    char buff[100];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff), "w:%.2f, h:%.2f",
                  size.GetWidth(), size.GetHeight());
    return buff;
}

inline std::string ToString(const FPoint& point)
{
    char buff[100];
    std::memset(buff, 0, sizeof(buff));
    std::snprintf(buff, sizeof(buff), "x:%.2f, y:%.2f",
                  point.GetX(), point.GetY());
    return buff;
}

} // namespace