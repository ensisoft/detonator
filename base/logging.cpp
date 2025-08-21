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

#ifdef __EMSCRIPTEN__
#  include <emscripten/emscripten.h>
#  include <emscripten/html5.h>
#endif

#include <iostream>
#include <iomanip>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>

#include "base/assert.h"
#include "base/logging.h"

namespace {
// a thread specific logger object.
thread_local base::Logger* threadLogger;

// global logger
base::Logger* globalLogger;
std::mutex globalLoggerMutex;

// flags to globally specify which log events
// are enabled or not.
bool isGlobalVerboseLogEnabled = false;
bool isGlobalDebugLogEnabled   = false;
bool isGlobalWarnLogEnabled    = true;
bool isGlobalInfoLogEnabled    = true;
bool isGlobalErrorLogEnabled   = true;

} // namespace

namespace base
{

const char* ToString(base::LogEvent e)
{
    switch (e)
    {
        case LogEvent::Verbose:
            return "Verbose";
        case LogEvent::Debug:
           return "Debug";
        case LogEvent::Info:
           return "Info";
        case LogEvent::Warning:
           return "Warning";
        case LogEvent::Error:
            return "Error";
    }
    BUG("wat");
    return "";
}

OStreamLogger::OStreamLogger(std::ostream& out) : m_out(&out)
{}

void OStreamLogger::Write(LogEvent type, const char* file, int line, const char* msg, double time)
{
#if defined(LINUX_OS) || defined(WINDOWS_OS)
    if (mStyle == Style::FancyColor)
    {
        auto& out = *m_out;
        std::ios old_state(nullptr);
        old_state.copyfmt(out);

        out << "\033[" << 2 << "m"
            << "["
            << std::fixed
            << std::setprecision(3) // after the radix point
            //<< std::setw(10)
            //<< std::setfill('0')
            << time
            << "]  "
            << "\033[m";

        out << std::setfill(' ');

        out << "\033[" << 1 << "m"
            << std::left
            << std::setw(7)
            << ToString(type) << " "
            << "\033[m";

        std::stringstream  ss;
        std::string file_and_line;
        ss << file << ":" << line;
        ss >> file_and_line;
        if (file_and_line.size() > 25)
        {
            const auto count = file_and_line.size() - 25;
            file_and_line = file_and_line.substr(count);
        }

        out << "\033[" << 3 << "m"
            << std::right
            << std::setw(25)
            << file_and_line
            << "  "
            << "\033[m";

        if (type == LogEvent::Error)
        {
            out << "\033[" << 1 << "m";
            out << "\033[" << 91 << "m";
        }
        else if (type == LogEvent::Warning)
        {
            out << "\033[" << 1 << "m";
            out << "\033[" << 93 << "m";
        }
        else if (type == LogEvent::Info)
        {
            //out << "\033[" << 1 << "m";
            out << "\033[" << 97 << "m";
        }
        else if (type == LogEvent::Debug || type == LogEvent::Verbose)
        {
            //out << "\033[" << 90 << "m";
        }
        out << msg << "\033[m";
        out << "\n";

        out.copyfmt(old_state);
        return;
    }
#endif

    // format the whole log string.
    // todo: fix this potential buffer issue here.
    char formatted_log_message[512] = {0};
    std::snprintf(formatted_log_message, sizeof(formatted_log_message) - 1,
                  "[%f] %s: %s:%d \"%s\"\n",
                  time, ToString(type), file, line, msg);
    Write(type, formatted_log_message);
}

void OStreamLogger::Write(LogEvent type, const char* msg)
{
    if (mStyle == Style::Basic)
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
#if defined(LINUX_OS)
    // output terminal escape code to change text color.
    if (type == LogEvent::Error)
        *m_out << "\033[" << 31 << "m";
    else if (type == LogEvent::Warning)
        *m_out << "\033[" << 33 << "m";
    else if (type == LogEvent::Info)
        *m_out << "\033[" << 36 << "m";

    // print the actual message
    *m_out << msg;

    // reset terminal color.
    *m_out << "\033[m";
#elif defined(WINDOWS_OS)
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO console = {0};
    GetConsoleScreenBufferInfo(out, &console);
    if (type == LogEvent::Info)
        SetConsoleTextAttribute(out, FOREGROUND_GREEN | FOREGROUND_BLUE);
    else if (type == LogEvent::Error)
        SetConsoleTextAttribute(out, FOREGROUND_RED);
    else if (type == LogEvent::Warning)
        SetConsoleTextAttribute(out, FOREGROUND_RED | FOREGROUND_GREEN);
    //else if (type == LogEvent::Info)
    //    SetConsoleTextAttribute(out, FOREGROUND_GREEN);

    *m_out << msg;
    SetConsoleTextAttribute(out, console.wAttributes);
#else
#  warning Unimplemented function
    BUG("Unimplemented function called.");
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
    std::printf("%s", msg);
#endif
}

#if defined(__EMSCRIPTEN__)
void EmscriptenLogger::Write(LogEvent type, const char* file, int line, const char* msg, double time)
{
    // not implemented
}

void EmscriptenLogger::Write(LogEvent type, const char* msg)
{
    if (type == LogEvent::Warning)
        emscripten_log(EM_LOG_CONSOLE | EM_LOG_WARN,  "%s", msg);
    else if (type == LogEvent::Debug)
        emscripten_log(EM_LOG_CONSOLE | EM_LOG_DEBUG, "%s", msg);
    else if (type == LogEvent::Error)
        emscripten_log(EM_LOG_CONSOLE | EM_LOG_ERROR, "%s", msg);
    else if (type == LogEvent::Info)
        emscripten_log(EM_LOG_CONSOLE | EM_LOG_INFO, "%s", msg);
}
void EmscriptenLogger::Flush()
{
    // intentionally empty.
}
#endif // __EMSCRIPTEN__

Logger* SetGlobalLog(Logger* log)
{
    std::lock_guard<std::mutex> lock(globalLoggerMutex);

    auto ret = globalLogger;
    globalLogger = log;
    return ret;
}

GlobalLogger GetGlobalLog()
{
    globalLoggerMutex.lock();

    return { globalLogger, &globalLoggerMutex };
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
    auto logger = GetGlobalLog();
    if (logger)
        logger->Flush();
}

bool IsDebugLogEnabled()
{
    std::lock_guard<std::mutex> lock(globalLoggerMutex);

    return isGlobalDebugLogEnabled;
}

bool IsLogEventEnabled(LogEvent type)
{
    std::lock_guard<std::mutex> lock(globalLoggerMutex);

    if (type == LogEvent::Verbose)
        return isGlobalVerboseLogEnabled;
    else if (type == LogEvent::Debug)
        return isGlobalDebugLogEnabled;
    else if (type == LogEvent::Warning)
        return isGlobalWarnLogEnabled;
    else if (type == LogEvent::Error)
        return isGlobalWarnLogEnabled;
    else if (type == LogEvent::Info)
        return isGlobalInfoLogEnabled;
    else BUG("No such log event.");
    return false;
}

void EnableLogEvent(LogEvent type, bool on_off)
{
    std::lock_guard<std::mutex> lock(globalLoggerMutex);

    if (type == LogEvent::Verbose)
        isGlobalVerboseLogEnabled = on_off;
    else if (type == LogEvent::Debug)
        isGlobalDebugLogEnabled = on_off;
    else if (type == LogEvent::Warning)
        isGlobalWarnLogEnabled = on_off;
    else if (type == LogEvent::Error)
        isGlobalWarnLogEnabled = on_off;
    else if (type == LogEvent::Info)
        isGlobalInfoLogEnabled = on_off;
    else BUG("No such log event.");
}

void EnableDebugLog(bool on_off)
{
    std::lock_guard<std::mutex> lock(globalLoggerMutex);

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
    auto global_log = GetGlobalLog();
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
            thread_log->Write(type, file, line, message.c_str(), seconds);
        if (thread_log->TestWriteMask(Logger::WriteType::WriteFormatted))
            thread_log->Write(type, formatted_log_message);
        return;
    }

    // acquire access to the global logger
    if (!global_log)
        return;

    if (global_log->TestWriteMask(Logger::WriteType::WriteRaw))
        global_log->Write(type, file, line, message.c_str(), seconds);
    if (global_log->TestWriteMask(Logger::WriteType::WriteFormatted))
        global_log->Write(type, formatted_log_message);
}

} // base

extern "C" {
base::Logger* base_AcquireGlobalLog()
{
    globalLoggerMutex.lock();

    if (globalLogger == nullptr)
        globalLoggerMutex.unlock();

    return globalLogger;
}
void base_ReleaseGlobalLog(base::Logger* logger)
{
    if (logger)
    {
        globalLoggerMutex.unlock();
    }
}


base::Logger* base_GetThreadLog()
{
    return base::GetThreadLog();
}
} // extern C
