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

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "base/platform.h"
#include "base/bitflag.h"
#include "base/assert.h"
#include "base/utility.h"
#include "base/test_minimal.h"

#if defined(WINDOWS_OS)
#  include <Windows.h> // for DebugBreak
#elif defined(POSIX_OS)
#  include <signal.h> // for raise/signal
#endif

namespace test {

base::bitflag<Type>      EnabledTestTypes;
std::vector<std::string> EnabledTestNames;
std::vector<TestBundle*> TestBundles;

std::string PerformanceRecordFile = "performance-record.txt";

unsigned ErrorCount = 0;

static bool EnableFatality = true;

// maps filenames to bundle names.
std::unordered_map<std::string, std::string> BundleNames;

void Print(Color color, const char* fmt, ...)
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
#else
    std::vprintf(fmt, args);
#endif

    va_end(args);
    std::fflush(stdout);
}

bool ReadYesNo(Color color, const char* prompt)
{
#if defined(__EMSCRIPTEN__)
    return false;
#else
    for (;;)
    {
        Print(color, prompt);
        int ch = getchar();
        if (ch == 'y' || ch == 'Y')
            return true;
        else if (ch == 'n' || ch == 'N')
            return false;
        Print(Color::Error, "\nSorry what?\n");
    }
#endif
}

void SetBundleName(const char* source_file_name, std::string name)
{
    BundleNames[source_file_name] = name;
}

std::string GetBundleName(const char* source_file_name)
{
    auto it = BundleNames.find(source_file_name);
    if (it != BundleNames.end())
        return it->second;

    std::string name(GetFileName(source_file_name));
    if (base::EndsWith(name, ".cpp"))
        name.resize(name.size()-4);
    return name;
}

const char* GetFileName(const char* file)
{
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
    return file;
}

const char* GetTestName(const char* function_name)
{
    return function_name;

    // this for __PRETTY_FUNCTION__
    // skip over the return type in the function name
    //function_name = std::strstr(function_name, " ");
    //function_name++;
    //// skip over calling convention declaration
    //if (std::strstr(function_name, "__cdecl ") == function_name)
    //    function_name += std::strlen("__cdecl ");
    //return function_name;
}

void BlurpFailure(const char* expression, const char* file, const char* function, int line, bool fatality)
{
    ++ErrorCount;

    file = GetFileName(file);

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
    test::Print(Color::Warning, "\n%s(%d): %s failed in function: '%s'\n\n", file, line, expression, function);
}

bool IsEnabledByName(const std::string& name)
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

bool IsEnabledByType(Type type)
{
    return EnabledTestTypes.test(type);
}

void AddBundle(test::TestBundle* bundle)
{
    TestBundles.push_back(bundle);
}

std::string GetPerformanceRecordFileName()
{
    return PerformanceRecordFile;
}

} // namespace

int main(int argc, char* argv[])
{
    test::EnabledTestTypes.set(test::Type::Feature,     true);
    test::EnabledTestTypes.set(test::Type::Performance, true);
    test::EnabledTestTypes.set(test::Type::Other,       true);
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
        else if (!std::strcmp(argv[i], "--disable-other-test") ||
                 !std::strcmp(argv[i], "-dot"))
            test::EnabledTestTypes.set(test::Type::Other, false);
        else if (!std::strcmp(argv[i], "--case") ||
                 !std::strcmp(argv[i], "-c"))
            test::EnabledTestNames.emplace_back(argv[i+1]);
        else if (!std::strcmp(argv[i], "--perf-record"))
            test::PerformanceRecordFile = argv[i+1];
    }
    try
    {
#if defined(UNIT_TEST_BUNDLE)
        for (auto* bundle : test::TestBundles)
        {
            test::Print(test::Color::Info, "Running bundle '%s'\n", bundle->name.c_str());
            test::Print(test::Color::Info, "============================================================\n");
            bundle->test_main(argc, argv);
            test::Print(test::Color::Info, "\n\n");
        }
#else
        extern int test_main(int argc, char* argv[]);
        test_main(argc, argv);
#endif

        if (test::ErrorCount)
            test::Print(test::Color::Warning, "Tests completed with errors.\n");
        else test::Print(test::Color::Success, "Success!\n");
    }
    catch (const test::Fatality& fatality)
    {
        test::Print(test::Color::Error, "\n%s(%d): %s failed in function: '%s'\n",
                    fatality.mFile, fatality.mLine, fatality.mExpression, fatality.mFunc);
        test::Print(test::Color::Warning, "\nTesting finished early on fatality.\n");
        return 1;
    }
    catch (const std::exception & e)
    {
        test::Print(test::Color::Error, "\nTests didn't run to completion because an exception occurred!\n\n");
        test::Print(test::Color::Error, "%s\n", e.what());
        return 1;
    }
    return test::ErrorCount ? 1 : 0;
}
