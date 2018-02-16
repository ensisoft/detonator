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
#include <iostream>
#include "logging.h"

// todo: make this a TLS object

base::Logger* threadLogger;

bool debugLog = false;

namespace base
{

const char* str(base::LogEvent e)
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

void OStreamLogger::Write(LogEvent type, const std::string& msg, const char* file, int line)
{
    const char* s = str(type);
    const char* m = msg.c_str();
    std::printf("%s: %s:%d \"%s\"\n", s, file, line, m);
}

CursesLogger::CursesLogger()
{
#ifdef BASE_LOGGING_ENABLE_CURSES
    initscr();

    start_color();

    use_default_colors();

    scrollok(stdscr, TRUE);

    // init some colors
    init_pair(1+(short)LogEvent::Debug, COLOR_YELLOW, -1);
    init_pair(1+(short)LogEvent::Info,  COLOR_GREEN, -1);
    init_pair(1+(short)LogEvent::Warning, COLOR_CYAN, -1);
    init_pair(1+(short)LogEvent::Error, COLOR_RED, -1);
#endif
}

CursesLogger::~CursesLogger()
{
#ifdef BASE_LOGGING_ENABLE_CURSES
    endwin();
#endif
}

void CursesLogger::Write(LogEvent type, const std::string& msg, const char* file, int line)
{
    const char* s = str(type);
    const char* m = msg.c_str();

#ifdef BASE_LOGGING_ENABLE_CURSES
    attron(COLOR_PAIR(1 + (short)type));
    printw("%s: %s:%d \"%s\"\n", s, file, line, m);
    refresh();
    attroff(COLOR_PAIR(1 + (short)type));

    int x, y;
    getyx(stdscr, y, x);

    int mx, my;
    getmaxyx(stdscr, my, mx);
    if (y == my)
        addch('\n');

#else
    std::printf("%s: %s:%d \"%s\"\n", s, file, line, m);
#endif
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

bool IsDebugLogEnabled()
{
    return debugLog;
}

void EnableDebugLog(bool on_off)
{
    debugLog = on_off;
}


} // base