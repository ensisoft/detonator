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

#include "warnpush.h"
#  include <boost/random/mersenne_twister.hpp>
#include "warnpop.h"
#include <cmath>

namespace math
{
    const auto Pi = 3.14159265358979323846;

    template<typename T> inline
    T wrap(T min, T max, T val)
    {
        if (val > max)
            return min;
        if (val < min)
            return max;
        return val;
    }

    template<typename T> inline
    T clamp(T min, T max, T val)
    {
        if (val < min)
            return min;
        if (val > max)
            return max;
        return val;
    }

    // generate a random number in the range of min max (inclusive)
    template<typename T>
    T rand(T min, T max)
    {
        static boost::random::mt19937 generator;

        const auto range = max - min;
        const auto value = (double)generator() / (double)generator.max();
        return static_cast<T>(min + (range * value));
    }

} // namespace
