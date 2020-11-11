// Copyright (c) 2010-2014 Sami Väisänen, Ensisoft
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

#ifdef BASE_LOGGING_ENABLE_CURSES
#  include <curses.h>
#endif
#include <cassert>
#include <iostream>
#include <atomic>
#include <chrono>
#include <cstdio>
#include "logging.h"

namespace {
// a thread specific logger object.
thread_local base::Logger* threadLogger;

// global logger
static base::Logger* globalLogger;

// atomic flag to globally specify whether debug
// logs are allowed to go through or not
static std::atomic<bool> isGlobalDebugLogEnabled(false);

} // namespace

namespace base
{

const char* ToString(base::LogEvent e)
{
    switch (e)
    {
        case LogEvent::Debug:
           return "Debug";
        case LogEvent::Info:
           return "Info";
        case LogEvent::Warning:
           return "Warn";
        case LogEvent::Error:
            return "Error";
    }
    assert(!"wat");
    return "";
}

OStreamLogger::OStreamLogger(std::ostream& out) : m_out(out)
{}

void OStreamLogger::Write(LogEvent type, const char* msg)
{
    m_out << msg;
}

void OStreamLogger::Flush()
{
    m_out.flush();
}

CursesLogger::CursesLogger()
{
#ifdef BASE_LOGGING_ENABLE_CURSES
    initscr();
    start_color();
    use_default_colors();
    scrollok(stdscr, TRUE);

    // init some colors
    init_pair(1+(short)LogEvent::Debug, COLOR_CYAN, -1);
    init_pair(1+(short)LogEvent::Info,  COLOR_WHITE, -1);
    init_pair(1+(short)LogEvent::Warning, COLOR_YELLOW, -1);
    init_pair(1+(short)LogEvent::Error, COLOR_RED, -1);
#endif
}

CursesLogger::~CursesLogger()
{
#ifdef BASE_LOGGING_ENABLE_CURSES
    endwin();
#endif
}

void CursesLogger::Write(LogEvent type, const char* msg)
{
 #ifdef BASE_LOGGING_ENABLE_CURSES
    attron(COLOR_PAIR(1 + (short)type));
    printw(msg);
    refresh();
    attroff(COLOR_PAIR(1 + (short)type));

    int x, y;
    getyx(stdscr, y, x);

    int mx, my;
    getmaxyx(stdscr, my, mx);
    if (y == my)
        addch('\n');

#else
    std::printf(msg);
#endif
}

Logger* SetGlobalLog(Logger* log)
{
    auto ret = globalLogger;
    globalLogger = log;
    return ret;
}

Logger* GetGlobalLog()
{
    return globalLogger;
}

Logger* GetThreadLog()
{
    return threadLogger;
}

Logger* SetThreadLog(Logger* log)
{
    auto* ret = threadLogger;
    threadLogger = log;
    return ret;
}

void FlushThreadLog()
{
    auto* ret = threadLogger;
    if (!ret)
        return;
    ret->Flush();
}

void FlushGlobalLog()
{
    if (!globalLogger)
        return;
    globalLogger->Flush();
}

bool IsDebugLogEnabled()
{
    return isGlobalDebugLogEnabled;
}

void EnableDebugLog(bool on_off)
{
    isGlobalDebugLogEnabled = on_off;
}

void WriteLogMessage(LogEvent type, const char* file, int line, const std::string& message)
{
    using steady_clock = std::chrono::steady_clock;
    // magic static is thread safe.
    static const auto first_event_time = steady_clock::now();
    const auto current_event_time = steady_clock::now();
    const auto elapsed = current_event_time - first_event_time;
    const double seconds = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() / 1000.0;

    // format the whole log string.
    // todo: fix this potential buffer issue here.
    char formatted_log_message[512] = {0};

    auto* thread_log = GetThreadLog();
    auto* global_log = GetGlobalLog();
    if ((thread_log && thread_log->TestWriteMask(Logger::WriteType::WriteFormatted)) ||
        (global_log && global_log->TestWriteMask(Logger::WriteType::WriteFormatted)))
    {
        std::snprintf(formatted_log_message, sizeof(formatted_log_message) - 1,
                      "[%f] %s: %s:%d \"%s\"\n",
                      seconds, ToString(type), file, line, message.c_str());
    }
    if (thread_log)
    {
        if (thread_log->TestWriteMask(Logger::WriteType::WriteRaw))
            thread_log->Write(type, file, line, message.c_str());
        if (thread_log->TestWriteMask(Logger::WriteType::WriteFormatted))
            thread_log->Write(type, formatted_log_message);
        return;
    }

    // acquire access to the global logger
    if (!global_log)
        return;

    if (global_log->TestWriteMask(Logger::WriteType::WriteRaw))
        global_log->Write(type, file, line, message.c_str());
    if (global_log->TestWriteMask(Logger::WriteType::WriteFormatted))
        global_log->Write(type, formatted_log_message);
}

} // base
