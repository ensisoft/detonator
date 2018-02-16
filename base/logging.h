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

#include "format.h"

// wingdi defines ERROR
#ifdef _WIN32
  #ifdef ERROR
    #undef ERROR
  #endif
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
        Debug, Info, Warning, Error
    };


    // logger interface for writing data out.
    class Logger
    {
    public:
        virtual ~Logger() = default;

        // write the event.
        virtual void Write(LogEvent type, const std::string& msg, const char* file, int line) = 0;

    protected:
    private:
    };

    // standard ostream logger
    class OStreamLogger : public Logger
    {
    public:
        OStreamLogger(std::ostream& out);

        virtual void Write(LogEvent type, const std::string& msg, const char* file, int line) override;
    private:
        std::ostream& m_out;
    };

    // similar to OStreamLogger except uses
    // curses (when available) for fancier output.
    class CursesLogger : public Logger
    {
    public:
        CursesLogger();
       ~CursesLogger();

       virtual void Write(LogEvent type, const std::string& msg, const char* file, int line) override;
    private:
    };


    // get the current thread's current logger object (if any)
    Logger* GetThreadLog();

    // set the current thread's logger object. nullptr is a valid value
    // and turns off the logging.
    // returns previous logger object.
    Logger* SetThreadLog(Logger* log);

    bool IsDebugLogEnabled();

    void EnableDebugLog(bool on_off);

    // interface for writing a variable argument message to the
    // current thread's current logger.
    template<typename... Args>
    void WriteLog(LogEvent type, const char* file, int line, std::string fmt, const Args&... args)
    {
        if (type == LogEvent::Debug)
        {
            if (!IsDebugLogEnabled())
                return;
        }

        auto* logger = GetThreadLog();
        if (logger == nullptr)
            return;

        const auto& msg = FormatString(std::move(fmt), args...);
        logger->Write(type, msg, file, line);
    }

} // base