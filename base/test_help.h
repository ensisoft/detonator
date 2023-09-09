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

// this file should only be included in unit test files.

#include "warnpush.h"
#if defined(BASE_TEST_HELP_SUPPORT_GLM)
#  include <glm/vec2.hpp>
#endif
#include "warnpop.h"

#include <cstdio>
#include <cstdarg>

#include <vector>
#include <algorithm>
#include <limits>
#include <functional>
#include <variant>

#include "base/test_float.h"
#include "base/test_minimal.h"
#include "base/color4f.h"
#include "base/types.h"
#include "base/utility.h"

#include "base/snafu.h"

#if defined(BASE_TEST_HELP_SUPPORT_GLM)
namespace glm {
static bool operator==(const glm::vec2& lhs, const glm::vec2& rhs)
{ return real::equals(lhs.x, rhs.x) && real::equals(lhs.y, rhs.y); }
static bool operator!=(const glm::vec2& lhs, const glm::vec2& rhs)
{ return !(lhs == rhs); }
} // glm
#endif

namespace base {
static bool operator==(const Color4f& lhs, const Color4f& rhs)
{
    return real::equals(lhs.Red(), rhs.Red()) &&
           real::equals(lhs.Green(), rhs.Green()) &&
           real::equals(lhs.Blue(), rhs.Blue()) &&
           real::equals(lhs.Alpha(), rhs.Alpha());
}
static bool operator!=(const Color4f& lhs, const Color4f& rhs)
{ return !(lhs == rhs); }

static bool operator==(const FRect& lhs, const FRect& rhs)
{
    return real::equals(lhs.GetX(), rhs.GetX()) &&
           real::equals(lhs.GetY(), rhs.GetY()) &&
           real::equals(lhs.GetWidth(), rhs.GetWidth()) &&
           real::equals(lhs.GetHeight(), rhs.GetHeight());
}
static bool operator!=(const FRect& lhs, const FRect& rhs)
{ return !(lhs == rhs); }

static bool operator==(const FSize& lhs, const FSize& rhs)
{
    return real::equals(lhs.GetWidth(), rhs.GetWidth()) &&
           real::equals(lhs.GetHeight(), rhs.GetHeight());
}
static bool operator!=(const FSize& lhs, const FSize& rhs)
{ return !(lhs == rhs); }

static bool operator==(const FPoint& lhs, const FPoint& rhs)
{
    return real::equals(lhs.GetX(), rhs.GetX()) &&
           real::equals(lhs.GetY(), rhs.GetY());
}
static bool operator!=(const FPoint& lhs, const FPoint& rhs)
{ return !(lhs == rhs); }

} // base

template<typename... Args>
static bool operator==(const std::variant<Args...>& variant, const real::float32& val)
{
    TEST_REQUIRE(std::holds_alternative<float>(variant));
    return std::get<float>(variant) == val;
}
template<typename... Args>
static bool operator!=(const std::variant<Args...>& variant, const real::float32& val)
{
    TEST_REQUIRE(std::holds_alternative<float>(variant));
    return std::get<float>(variant) == val;
}

template<typename T, typename... Args>
static bool operator==(const std::variant<Args...>& variant, const T& val)
{
    TEST_REQUIRE(std::holds_alternative<T>(variant));
    return std::get<T>(variant) == val;
}
template<typename T, typename... Args>
static bool operator!=(const std::variant<Args...>& variant, const T& val)
{
    TEST_REQUIRE(std::holds_alternative<T>(variant));
    return std::get<T>(variant) != val;
}

namespace test {
struct TestTimes {
    unsigned iterations = 0;
    double average = 0.0f;
    double minimum = 0.0f;
    double maximum = 0.0f;
    double median  = 0.0f;
    double total   = 0.0f;
};

template<typename TestCallable>
static TestTimes TimedTest(unsigned iterations, TestCallable function)
{
    std::vector<double> times;
    times.resize(iterations);
    for (unsigned i=0; i<iterations; ++i)
    {
        base::ElapsedTimer timer;
        timer.Start();
        function();
        times[i] = timer.SinceStart();
    }
    auto min_time = std::numeric_limits<double>::max();
    auto max_time = std::numeric_limits<double>::min();
    auto sum_time = 0.0f;
    for (auto time : times)
    {
        min_time = std::min(min_time, time);
        max_time = std::max(max_time, time);
        sum_time += time;
    }
    std::sort(times.begin(), times.end());
    TestTimes ret;
    ret.iterations = iterations;
    ret.average    = sum_time / times.size();
    ret.minimum    = min_time;
    ret.maximum    = max_time;
    ret.total      = sum_time;

    const auto index = iterations/2;

    if (iterations % 2)
        ret.median = times[index];
    else ret.median = (times[index-1] + times[index]) / 2.0;
    return ret;
}

static void PrintTestTimes(const char* name, const TestTimes& times)
{
    test::Print(test::Color::Info, "\n");
    test::Print(test::Color::Info, " %s\n", name);
    test::Print(test::Color::Info, " -------------------------------------------\n");
    test::Print(test::Color::Info, "  loops=%u\n\n", times.iterations);
    test::Print(test::Color::Info, "  total  = %.6f s %6u ms  %6u us\n", times.total,   unsigned(times.total   * 1000u), unsigned(times.total   * 1000u*1000u));
    test::Print(test::Color::Info, "  min    = %.6f s %6u ms  %6u us\n", times.minimum, unsigned(times.minimum * 1000u), unsigned(times.minimum * 1000u*1000u));
    test::Print(test::Color::Info, "  max    = %.6f s %6u ms  %6u us\n", times.maximum, unsigned(times.maximum * 1000u), unsigned(times.maximum * 1000u*1000u));
    test::Print(test::Color::Info, "  avg    = %.6f s %6u ms  %6u us\n", times.average, unsigned(times.average * 1000u), unsigned(times.average * 1000u*1000u));
    test::Print(test::Color::Info, "  median = %.6f s %6u ms  %6u us\n", times.median,  unsigned(times.median  * 1000u), unsigned(times.median  * 1000u*1000u));
    test::Print(test::Color::Info, "\n");
}

static void DevNull(const char* fmt, ...)
{
    va_list args;
    va_start (args, fmt);

#if defined(LINUX_OS)
    static FILE* dev_null = std::fopen("/dev/null", "w");
#elif defined(WINDOWS_OS)
    static FILE* dev_null = std::fopen("nul", "w");
#else
#  warning unimplemented
    static FILE* dev_null = nullptr;
    return;
#endif
    std::vfprintf(dev_null, fmt, args);
    va_end(args);
}

} // namespace



