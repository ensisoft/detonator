// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <stdexcept>
#include <cstring>
#include <functional>
#include <vector>

#include "base/platform.h"
#include "base/assert.h"
#include "base/bitflag.h"

#if defined(WINDOWS_OS)
#  include <Windows.h> // for DebugBreak
#elif defined(POSIX_OS)
#  include <signal.h> // for raise/signal
#endif

namespace test {
// Note that this class does *not* derive from std::exception
// on purpose so that the test code that would catch std::exceptions
// wont catch this.
class Fatality {
public:
    Fatality(const char* expression,
             const char* file,
             const char* function,
             int line)
      : mExpression(expression)
      , mFile(file)
      , mFunc(function)
      , mLine(line)
    {}
public:
    const char* mExpression = nullptr;
    const char* mFile       = nullptr;
    const char* mFunc       = nullptr;
    const int mLine         = 0;
};

static unsigned ErrorCount = 0;
static bool EnableFatality = true;

enum class Type {
    Performance, Feature
};

base::bitflag<Type> EnabledTestTypes;
std::vector<std::string> EnabledTestNames;

enum class Color {
    Error, Warning, Success, Message, Info
};

static void print(Color color, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

#if defined(LINUX_OS)
    if (color == Color::Error)
        std::printf("\033[%dm", 31);
    else if (color == Color::Warning)
        std::printf("\033[%dm", 93);
    else if (color == Color::Success)
        std::printf("\033[%dm", 32);
    else if (color == Color::Info)
        std::printf("\033[%dm", 97);
    std::vprintf(fmt, args);
    std::printf("\033[m");
#elif defined(WINDOWS_OS)
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO console = {0};
    GetConsoleScreenBufferInfo(out, &console);

    if (color == Color::Error)
        SetConsoleTextAttribute(out, FOREGROUND_RED | FOREGROUND_INTENSITY);
    else if (color == Color::Warning)
        SetConsoleTextAttribute(out, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    else if (color == Color::Success)
        SetConsoleTextAttribute(out, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    else if (color == Color::Info)
        SetConsoleTextAttribute(out, FOREGROUND_INTENSITY);

    std::vprintf(fmt, args);
    
    SetConsoleTextAttribute(out, console.wAttributes);
#endif

    va_end(args);

    std::fflush(stdout);
}

static void blurb_failure(const char* expression,
    const char* file,
    const char* function,
    int line, bool fatality)
{
    ++ErrorCount;

    // strip the path from the filename
#if defined(POSIX_OS)
    const char* p = file;
    while (*file) {
        if (*file == '/')
            p = file + 1;
        ++file;
    }
    file = p;
#elif defined(WINDOWS_OS)
    const char* p = file;
    while (*file) {
        if (*file == '\\')
            p = file + 1;
        ++file;
    }
    file = p;
#endif

    if (fatality && EnableFatality)
    {
        if (debug::has_debugger())
        {
#if defined(WINDOWS_OS)
            DebugBreak();
#elif defined(POSIX_OS)
            ::raise(SIGTRAP);
#endif
        }
        // instead of exiting/aborting the process here throw an exception
        // for unwinding the stack back to test_main which can then report
        // the total test case tally. I know.. this won't work if exceptions
        // are disabled (because bla bla bluh bluh) or if there's a catch block
        // that would manage to catch this exception.
        throw Fatality(expression, file, function, line);
    }
    test::print(Color::Warning, "\n%s(%d): %s failed in function: '%s'\n\n", file, line, expression, function);
}

static const char* GetTestName(const char* function_name)
{
    // skip over the return type in the function name
    function_name = std::strstr(function_name, " ");
    function_name++;
    // skip over calling convention declaration
    if (std::strstr(function_name, "__cdecl ") == function_name)
        function_name += std::strlen("__cdecl ");
    return function_name;
}

static bool IsEnabledByName(const std::string& name)
{
    if (EnabledTestNames.empty())
        return true;

    for (const auto& str : EnabledTestNames)
    {
        if (name.find(str) != std::string::npos)
            return true;
    }
    return false;
}

class TestCaseReporter {
public:
    TestCaseReporter(const char* file, const char* func, Type type)
      : mFile(file)
      , mName(GetTestName(func))
      , mType(type)
    {
        // use the current number of errors to realize if the
        // current test case failed or not
        mErrors = ErrorCount;
    }
   ~TestCaseReporter()
    {
        const auto enabled_by_type = EnabledTestTypes.test(mType);
        const auto enabled_by_name = IsEnabledByName(mName);
        // printing all this information here so that the non-fatal
        // test failures print their message *before* the name and result
        // of the test execution.
        test::print(test::Color::Message, "Running ");
        test::print(test::Color::Info, "%-50s", mName);
        if (enabled_by_type && enabled_by_name)
        {
            if (mErrors == ErrorCount)
                test::print(test::Color::Success, "OK\n");
            else test::print(test::Color::Warning, "Fail\n");
        }
        else
        {
            test::print(test::Color::Message, "Skipped\n");
        }
    }
private:
    const char* mFile = nullptr;
    const char* mName = nullptr;
    const Type mType;
    unsigned mErrors = 0;
};

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
    test::print(test::Color::Message, "%s (%d): '" msg "'\n", __FUNCTION__, __LINE__, ## __VA_ARGS__); \

#define TEST_EXCEPTION(expr) \
    try { \
        expr; \
        TEST_REQUIRE(!"Exception was expected"); \
    } \
    catch (const std::exception& e) \
    {}

#define TEST_CASE(type) \
    test::TestCaseReporter test_case_reporter(__FILE__, __PRETTY_FUNCTION__, type); \
    if (!test::EnabledTestTypes.test(type))                                         \
        return;                                                                     \
    if (!test::IsEnabledByName(test::GetTestName(__PRETTY_FUNCTION__)))             \
        return;

int test_main(int argc, char* argv[]);

int main(int argc, char* argv[])
{
    test::EnabledTestTypes.set(test::Type::Feature,     true);
    test::EnabledTestTypes.set(test::Type::Performance, true);
    for (int i=1; i<argc; ++i)
    {
        if (!std::strcmp(argv[i], "--disable-fatality") ||
            !std::strcmp(argv[i], "-df"))
            test::EnableFatality = false;
        else if (!std::strcmp(argv[i], "--disable-perf-test") ||
                 !std::strcmp(argv[i], "-dpt"))
            test::EnabledTestTypes.set(test::Type::Performance, false);
        else if (!std::strcmp(argv[i], "--disable-feature-test") ||
                 !std::strcmp(argv[i], "-dft"))
            test::EnabledTestTypes.set(test::Type::Feature, false);
        else if (!std::strcmp(argv[i], "--case") ||
                 !std::strcmp(argv[i], "-c"))
            test::EnabledTestNames.emplace_back(argv[i+1]);
    }
    try
    {
        test_main(argc, argv);
        if (test::ErrorCount)
            test::print(test::Color::Warning, "Tests completed with errors.\n");
        else test::print(test::Color::Success, "Success!\n");
    }
    catch (const test::Fatality& fatality)
    {
        test::print(test::Color::Error, "\n%s(%d): %s failed in function: '%s'\n",
                    fatality.mFile, fatality.mLine, fatality.mExpression, fatality.mFunc);
        test::print(test::Color::Warning, "\nTesting finished early on fatality.\n");
        return 1;
    }
    catch (const std::exception & e)
    {
        test::print(test::Color::Error, "\nTests didn't run to completion because an exception occurred!\n\n");
        test::print(test::Color::Error, "%s\n", e.what());
        return 1;
    }
    return test::ErrorCount ? 1 : 0;
}
