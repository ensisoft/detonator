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

#include "config.h"

#include <thread>
#include <iostream>

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "base/logging.h"

void thread_entry(base::Logger* logger)
{
    base::SetThreadLog(logger);
    for (int i=0; i<100; ++i)
    {
        INFO("thread");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int test_main(int argc, char* argv[])
{
    {
        base::NullLogger null;
        base::SetGlobalLog(&null);
        TEST_REQUIRE(base::GetGlobalLog() == &null);
        base::SetGlobalLog(nullptr);
        TEST_REQUIRE(base::GetGlobalLog() == nullptr);
        base::EnableDebugLog(true);
        TEST_REQUIRE(base::IsDebugLogEnabled());
        base::EnableDebugLog(false);
        TEST_REQUIRE(base::IsDebugLogEnabled() == false);
    }

    {
        base::BufferLogger<base::NullLogger> logger;
        logger.EnableWrite(base::Logger::WriteType::WriteFormatted, false);
        base::SetGlobalLog(&logger);
        base::EnableDebugLog(true);

        DEBUG("debug");
        INFO("information");
        WARN("warning");
        ERROR("error");

        TEST_REQUIRE(logger.GetBufferMsgCount() == 4);
        TEST_REQUIRE(logger.GetMessage(0).msg == "debug");
        TEST_REQUIRE(logger.GetMessage(0).line != 0);
        TEST_REQUIRE(logger.GetMessage(0).file.find("unit_test_log.cpp") != std::string::npos);
        TEST_REQUIRE(logger.GetMessage(0).type == base::LogEvent::Debug);

        TEST_REQUIRE(logger.GetMessage(1).msg == "information");
        TEST_REQUIRE(logger.GetMessage(1).line != 0);
        TEST_REQUIRE(logger.GetMessage(1).file.find("unit_test_log.cpp") != std::string::npos);
        TEST_REQUIRE(logger.GetMessage(1).type == base::LogEvent::Info);

        TEST_REQUIRE(logger.GetMessage(2).msg == "warning");
        TEST_REQUIRE(logger.GetMessage(2).line != 0);
        TEST_REQUIRE(logger.GetMessage(2).file.find("unit_test_log.cpp") != std::string::npos);
        TEST_REQUIRE(logger.GetMessage(2).type == base::LogEvent::Warning);

        TEST_REQUIRE(logger.GetMessage(3).msg == "error");
        TEST_REQUIRE(logger.GetMessage(3).line != 0);
        TEST_REQUIRE(logger.GetMessage(3).file.find("unit_test_log.cpp") != std::string::npos);
        TEST_REQUIRE(logger.GetMessage(3).type == base::LogEvent::Error);

        logger.Dispatch();
        TEST_REQUIRE(logger.GetBufferMsgCount() == 0);
        base::SetGlobalLog(nullptr);
    }

    // test thread logs
    {
        base::BufferLogger<base::NullLogger> one;
        base::BufferLogger<base::NullLogger> two;
        one.EnableWrite(base::Logger::WriteType::WriteFormatted, false);
        two.EnableWrite(base::Logger::WriteType::WriteFormatted, false);

        std::thread t0(thread_entry, &one);
        std::thread t1(thread_entry, &two);

        t0.join();
        t1.join();
        TEST_REQUIRE(one.GetBufferMsgCount() == 100);
        TEST_REQUIRE(two.GetBufferMsgCount() == 100);
    }

    // test thread safe log.
    {
        base::LockedLogger<base::BufferLogger<base::NullLogger>> log;
        log.EnableWrite(base::Logger::WriteType::WriteFormatted, false);
        base::SetGlobalLog(&log);

        std::thread t0(thread_entry, &log);
        std::thread t1(thread_entry, &log);
        thread_entry(&log);

        t0.join();
        t1.join();
        TEST_REQUIRE(log.GetLoggerUnsafe().GetBufferMsgCount() == 300);
        base::SetGlobalLog(nullptr);
        base::SetThreadLog(nullptr);
    }

    // test some terminal colors
    {
        base::OStreamLogger logger(std::cout);
        logger.EnableTerminalColors(true);
        base::SetGlobalLog(&logger);
        DEBUG("Hello");
        INFO("Hello");
        WARN("Hello");
        ERROR("Hello");
    }

    return 0;
}
