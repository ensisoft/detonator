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

#if defined(WINDOWS_OS)
#  include <Windows.h>
#endif

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

OStreamLogger::OStreamLogger(std::ostream& out) : m_out(&out)
{}

void OStreamLogger::Write(LogEvent type, const char* msg)
{
    if (!mTerminalColors)
    {
        (*m_out) << msg;
        return;
    }
    // Using raw terminal escape sequences here. This might or might
    // not work depending on the terminal. Also if the ostream
    // is *not* connected to a terminal output (could be a file)
    // then strange garbage will be in the file. In such case
    // one would want to disable the term colors i.e set
    // mTerminalColors to false.
    //
    // But why? Because ncurses sucks monkey balls. Ncurses has
    // problems such as not being able to report whether it's
    // actually been initialized or not. Consider a case you have
    // several independent components that all want to use ncurses.
    // What happens if they all call initsrc or endwin? I can't find
    // any documentation about the behaviour of ncurses in such
    // cases.
    // Additionally ncurses when initialized requires the use of
    // ncurses output functions. Raw stdout output will then be
    // garbled. This doesn't play nice with code that simply does
    // a printf or std::cout call to print something.

    // More information about the terminal escape colors  (and codes)
    // is available at Wikipedia.
    // https://en.wikipedia.org/wiki/ANSI_escape_code#Colors
    //
    // In case wiki gets re-written below is a handy C application
    // to print text with different foreground/background attr
    // combinations.
    //

    /*
    int main(void)
    {
        int i, j, n;

        for (i = 0; i < 11; i++) {
            for (j = 0; j < 10; j++) {
                n = 10*i + j;
                if (n > 108) break;
                //printf("\033[%dm %3d \033[m", n, n);
                std::cout << "\033[" << n << "m";
                std::cout << " " << n << " ";
                std::cout << "\033[m";
            }
            std::cout << std::endl;
            //printf("\n");
        }
        return (0);
    }
    */
#if defined(POSIX_OS)
    // output terminal escape code to change text color.
    if (type == LogEvent::Debug)
        *m_out << "\033[" << 36 << "m";
    else if (type == LogEvent::Error)
        *m_out << "\033[" << 31 << "m";
    else if (type == LogEvent::Warning)
        *m_out << "\033[" << 33 << "m";
    //else if (type == LogEvent::Info)
    //    m_out << "\033[" << 32 << "m";

    // print the actual message
    *m_out << msg;

    // reset terminal color.
    *m_out << "\033[m";
#elif defined(WINDOWS_OS)
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO console = {0};
    GetConsoleScreenBufferInfo(out, &console);
    if (type == LogEvent::Debug)
        SetConsoleTextAttribute(out, FOREGROUND_GREEN | FOREGROUND_BLUE);
    else if (type == LogEvent::Error)
        SetConsoleTextAttribute(out, FOREGROUND_RED);
    else if (type == LogEvent::Warning)
        SetConsoleTextAttribute(out, FOREGROUND_RED | FOREGROUND_GREEN);
    //else if (type == LogEvent::Info)
    //    SetConsoleTextAttribute(out, FOREGROUND_GREEN);

    *m_out << msg;
    SetConsoleTextAttribute(out, console.wAttributes);
#endif
}

void OStreamLogger::Flush()
{
    (*m_out).flush();
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
    init_pair(1+(short)LogEvent::Info,  -1, -1);
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
    // strip the path from the file name.
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
