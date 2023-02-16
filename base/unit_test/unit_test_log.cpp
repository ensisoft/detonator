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

#include "config.h"

#include <thread>
#include <iostream>

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "base/logging.h"

#include "base/assert.cpp"
#include "base/logging.cpp"

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
