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

#pragma once

#include "config.h"

#include <iosfwd>
#include <string>
#include <mutex>
#include <chrono>
#include <cstdio>

#include "format.h"

// note that wingdi.h also has ERROR macro. On Windows Qt headers incluce
// windows.h which includes wingdi.h which redefines the ERROR macro which
// causes macro collision. This is a hack to already include wingdi.h 
// (has to be done through windows.h) and then undefine ERROR which allows
// the code below to hijack the macro name. The problem obviously is that
// any code that would then try to use WinGDI with the ERROR macro would 
// produce garbage errors. 
#ifdef _WIN32
#  include <Windows.h>
#  undef ERROR
#endif

#ifdef BASE_LOGGING_ENABLE_LOG
  #define DEBUG(fmt, ...) WriteLog(base::LogEvent::Debug,   __FILE__, __LINE__, fmt, ## __VA_ARGS__)
  #define WARN(fmt, ...)  WriteLog(base::LogEvent::Warning, __FILE__, __LINE__, fmt, ## __VA_ARGS__)
  #define INFO(fmt, ...)  WriteLog(base::LogEvent::Info,    __FILE__, __LINE__, fmt, ## __VA_ARGS__)
  #define ERROR(fmt, ...) WriteLog(base::LogEvent::Error,   __FILE__, __LINE__, fmt, ## __VA_ARGS__)
#else
  #define DEBUG(...) while(false)
  #define WARN(...)  while(false)
  #define INFO(...)  while(false)
  #define ERROR(...) while(false)
#endif

// minimalistic logging interface to get some diagnostics/information.
// using this system is optional.
// resource acquisition errors are indicated through exceptions
// programmer errors will terminate the process through a core dump.

namespace base
{
    // type of logging event.
    enum class LogEvent
    {
        // Debug relevance only
        Debug,
        // Generic information about some event
        Info,
        // Warning about not being able to do something,
        // typical scenario is when some input data coming
        // from outside the system is bad and it's rejected.
        Warning,
        // Error about failing to do something, some system/resource
        // allocation has failed. No further processing is currently
        // possible. Examples are file write failing, socket connection
        // dying (broken pipe, timeout) etc. similar (system) errors.
        Error
    };

    // Get human readable log event string.
    const char* ToString(LogEvent e);

    // Logger interface for writing data out.
    class Logger
    {
    public:
        virtual ~Logger() = default;

        // Write a not-formatted log message to the log.
        virtual void Write(LogEvent type, const char* file, int line, const char* msg)  = 0;

        // Write a preformatted log event to the log.
        // The log message has information such as the source file/line
        // and timestamp baked in.
        virtual void Write(LogEvent type, const char* msg)  = 0;
        // Flush the log.
        virtual void Flush() = 0;
    protected:
    private:
    };

    // Standard ostream logger
    class OStreamLogger : public Logger
    {
    public:
        OStreamLogger(std::ostream& out);

        virtual void Write(LogEvent type, const char* file, int line, const char* msg) override
        { /* not supported */ }
        virtual void Write(LogEvent type, const char* msg) override;
        virtual void Flush() override;
    private:
        std::ostream& m_out;
    };

    // Similar to OStreamLogger except uses
    // curses (when available) for fancier output.
    class CursesLogger : public Logger
    {
    public:
        CursesLogger();
       ~CursesLogger();

        virtual void Write(LogEvent type, const char* file, int line, const char* msg) override
        { /* not supported */ }
        virtual void Write(LogEvent type, const char* msg) override;
        virtual void Flush() override
        { /* no op */ }
    private:
    };


    template<typename WrappedLogger>
    class LockedLogger : public Logger
    {
    public:
        LockedLogger(WrappedLogger&& other)
          : mLogger(std::move(other))
        {}
        LockedLogger() = default;
        virtual void Write(LogEvent type, const char* file, int line, const char* msg) override
        {
            std::unique_lock<std::mutex> lock(mMutex);
            mLogger.Write(type, file, line, msg);
        }
        virtual void Write(LogEvent type, const char* msg) override
        {
            std::unique_lock<std::mutex> lock(mMutex);
            mLogger.Write(type, msg);
        }
        virtual void Flush() override
        {
            std::unique_lock<std::mutex> lock(mMutex);
            mLogger.Flush();
        }
        const WrappedLogger& GetLogger() const 
        { return mLogger; }
        WrappedLogger& GetLogger()
        { return mLogger; }
    private:
        WrappedLogger mLogger;
        std::mutex mMutex;
    };


    // Set the logger object for all threads to use. Each thread can override
    // this setting by setting a thread specific log. If a thread specific
    // logger is set that will the precedence over global logger.
    // In the precense of threads the logger object should be thread safe.
    Logger* SetGlobalLog(Logger* log);

    // Get access to the global logger object (if any).
    Logger* GetGlobalLog();

    // Get the calling thread's current logger object (if any)
    Logger* GetThreadLog();

    // Set the logger object for the calling thread. nullptr is a
    // valid value and turns off the thread specific logging.
    // Returns previous logger object (if any).
    Logger* SetThreadLog(Logger* log);

    // Flush the thread specific logger (if any). If no logger then a no-op
    void FlushThreadLog();

    // Flush the global logger (if any). If no logger then a no-op.
    // Thread safe.
    void FlushGlobalLog();

    // Check if the global (pertains to all threads) setting for runtime
    // debug logging is on or off
    bool IsDebugLogEnabled();

    // Toggle the global (pertains to all threads) setting for runtime
    // debug logging on or off
    void EnableDebugLog(bool on_off);

    // Interface for writing a variable argument message to the
    // calling thread's logger or the global logger.
    template<typename... Args>
    void WriteLog(LogEvent type, const char* file, int line, const std::string& fmt, const Args&... args)
    {
        if (type == LogEvent::Debug)
        {
            if (!IsDebugLogEnabled())
                return;
        }

        using steady_clock = std::chrono::steady_clock;
        // magic static is thread safe.
        static const auto first_event_time = steady_clock::now();
        const auto current_event_time = steady_clock::now();
        const auto elapsed = current_event_time - first_event_time;
        const double seconds = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() / 1000.0;

        // format the message in the log statement.
        const auto& msg = FormatString(fmt, args...);

        // format the whole log string.
        // todo: fix this potential buffer issue here.
        char formatted_log_message[512] = {0};

        std::snprintf(formatted_log_message, sizeof(formatted_log_message)-1,
            "[%f] %s: %s:%d \"%s\"\n",
            seconds, ToString(type), file, line, msg.c_str());

        auto* thread_log = GetThreadLog();
        if (thread_log)
        {
            thread_log->Write(type, file, line, msg.c_str());
            thread_log->Write(type, formatted_log_message);
            return;
        }

        // acquire access to the global logger
        auto* global_log = GetGlobalLog();
        if (!global_log)
            return;

        global_log->Write(type, file, line, msg.c_str());
        global_log->Write(type, formatted_log_message);
    }

} // base
