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

#include <vector>
#include <string>
#include <stdexcept>

#include "base/platform.h"
#include "base/bitflag.h"

namespace test {
// Interface for wrapping multiple "test_main" functions behind an interface.
// This allows a test executable to be built with multiple tests bundled in
// together into a single binary and each test_main is turned into a method
// call instead of being a global function.
// See the macro UNIT_TEST_USE_MAIN_BUNDLE
struct TestBundle
{
    virtual int test_main(int argc, char* argv[]) = 0;

    std::string name;
};

// Note that this class does *not* derive from std::exception
// on purpose so that the test code that would catch std::exceptions
// won't catch this.
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

enum class Type {
    Performance, Feature, Other
};

enum class Color {
    Error, Warning, Success, Message, Info
};

extern base::bitflag<Type> EnabledTestTypes;
extern std::vector<std::string> EnabledTestNames;
extern std::vector<TestBundle*> TestBundles;
extern unsigned ErrorCount;

// Produce printed message into some output device such as stdout.
void Print(Color color, const char* fmt, ...);
// Extract simple bundle name from the filename provided by the __FILE__ macro.
std::string GetBundleName(const char* source_file_name);
// Extract simple filename from the filename provided by the __FILE__ macro.
const char* GetFileName(const char* source_file_name);
// Extract a simple function name from the function name provided by the __PRETTY_FUNCTION__ macro.
const char* GetTestName(const char* function_name);
// Process a testing failure.
void BlurpFailure(const char* expression, const char* file, const char* function, int line, bool fatality);

bool IsEnabledByName(const std::string& name);
bool IsEnabledByType(Type type);
void AddBundle(TestBundle* bundle);

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
        const auto enabled_by_type = IsEnabledByType(mType);
        const auto enabled_by_name = IsEnabledByName(mName);
        // printing all this information here so that the non-fatal
        // test failures print their message *before* the name and result
        // of the test execution.
        test::Print(test::Color::Message, "Running ");
        test::Print(test::Color::Info, "%-50s", mName);
        if (enabled_by_type && enabled_by_name)
        {
            if (mErrors == ErrorCount)
                test::Print(test::Color::Success, "OK\n");
            else test::Print(test::Color::Warning, "Fail\n");
        }
        else
        {
            test::Print(test::Color::Message, "Skipped\n");
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
    : (test::BlurpFailure(#expr, __FILE__, __FUNCTION__, __LINE__, false))

#define TEST_REQUIRE(expr) \
    (expr) \
    ? ((void)0) \
    : (test::BlurpFailure(#expr, __FILE__, __FUNCTION__, __LINE__, true))

#define TEST_MESSAGE(msg, ...) \
    test::Print(test::Color::Message, "%s (%d): '" msg "'\n", __FUNCTION__, __LINE__, ## __VA_ARGS__); \

#define TEST_EXCEPTION(expr) \
    try { \
        expr; \
        TEST_REQUIRE(!"Exception was expected"); \
    } \
    catch (const std::exception& e) \
    {}

#define TEST_CASE(type) \
    test::TestCaseReporter test_case_reporter(__FILE__, __PRETTY_FUNCTION__, type); \
    if (!test::IsEnabledByType(type))                                               \
        return;                                                                     \
    if (!test::IsEnabledByName(test::GetTestName(__PRETTY_FUNCTION__)))             \
        return;

// When using a test bundle the test_main is no longer a global function
// but instead becomes a member function that implements/overrides the
// TestBundle::test_main method. A single object is then automatically
// created and appended to the global list of test bundles to be run.
#if defined(UNIT_TEST_BUNDLE)
  #define EXPORT_TEST_MAIN(main)                                                    \
      namespace {                                                                   \
        struct PrivateTestBundle : public test::TestBundle {                        \
          main                                                                      \
          PrivateTestBundle() {                                                     \
            test::AddBundle(this);                                                  \
            name = test::GetBundleName(__FILE__);                                   \
          }                                                                         \
        } test_bundle;                                                              \
      } // namespace
#else
  #define EXPORT_TEST_MAIN(main) main
  // automatically include the definitions for keeping the compilation
  // of existing unit tests simple and without having to explicitly
  // add the source file in the build rules.
  #include "base/test_minimal.cpp"
#endif


