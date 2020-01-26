// Copyright (c) 2014 Sami Väisänen, Ensisoft
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

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#if defined(_MSC_VER)
#  include <Windows.h> // for DebugBreak
#endif

namespace test {

static
void blurb_failure(const char* expression,
    const char* file,
    const char* function,
    int line, bool fatality)
{
    std::printf("\n%s(%d): %s failed in function: '%s'\n", file, line, expression, function);
    if (fatality)
    {
#if defined(_MSC_VER)
        DebugBreak();
#elif defined(__GNUG__)
        __builtin_trap();
#endif
        std::abort();
    }

}

} // test

#define TEST_CHECK(expr) \
    (expr) \
    ? ((void)0) \
    : (test::blurb_failure(#expr, __FILE__, __FUNCTION__, __LINE__, false))

#define TEST_REQUIRE(expr) \
    (expr) \
    ? ((void)0) \
    : (test::blurb_failure(#expr, __FILE__, __FUNCTION__, __LINE__, true))


#define TEST_MESSAGE(msg, ...) \
    std::printf("%s (%d): '" msg "'\n", __FUNCTION__, __LINE__, ## __VA_ARGS__); \
    std::fflush(stdout)

#define TEST_EXCEPTION(expr) \
    try { \
        expr; \
        TEST_REQUIRE(!"Exception was expected"); \
    } \
    catch (const std::exception& e) \
    {}


int test_main(int argc, char* argv[]);

int main(int argc, char* argv[])
{
    try
    {
        test_main(argc, argv);
        std::printf("\nSuccess!\n");
    }
    catch (const std::exception & e)
    {
        std::printf("Tests didn't run to completion because an exception occurred!\n\n");
        std::printf("%s\n", e.what());
        return 1;
    }
    return 0;
}

#define TEST_BARK() \
    std::printf("%s", __FUNCTION__); \
    std::fflush(stdout)


#define TEST_CUSTOM