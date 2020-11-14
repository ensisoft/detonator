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

#include "base/format.h"
#include "base/bitflag.h"

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

        enum class WriteType {
            WriteRaw,
            WriteFormatted
        };
        virtual base::bitflag<WriteType> GetWriteMask() const
        {
            base::bitflag<WriteType> writes;
            writes.set(WriteType::WriteRaw);
            writes.set(WriteType::WriteFormatted);
            return writes;
        }
        inline bool TestWriteMask(WriteType bit) const
        {
            const auto& bits = GetWriteMask();
            return bits.test(bit);
        }
    protected:
    private:
    };

    class NullLogger : public Logger
    {
    public:
        virtual void Write(LogEvent type, const char* file, int line, const char* msg) override
        { }
        virtual void Write(LogEvent type, const char* msg) override
        {}
        virtual void Flush() override
        {}
        virtual base::bitflag<WriteType> GetWriteMask() const override
        { return base::bitflag<WriteType>(); }
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
        virtual base::bitflag<WriteType> GetWriteMask() const override
        { return WriteType::WriteFormatted; }
        void EnableTerminalColors(bool on_off)
        { mTerminalColors = on_off; }
    private:
        std::ostream& m_out;
        bool mTerminalColors = true;
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
        virtual base::bitflag<WriteType> GetWriteMask() const override
        { return WriteType::WriteFormatted; }
    private:
    };

    // Protect access to a non-thread safe logger object
    // by wrapping it inside a locked logger for exclusive
    // and locked access.
    template<typename WrappedLogger>
    class LockedLogger : public Logger
    {
    public:
        LockedLogger()
        {
            mWrites.set(WriteType::WriteFormatted);
            mWrites.set(WriteType::WriteRaw);
        }
        LockedLogger(WrappedLogger&& other) : LockedLogger()
        { mLogger = std::move(other); }

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
        virtual bitflag<WriteType> GetWriteMask() const override
        { return mWrites; }
        class LoggerAccess
        {
        public:
            LoggerAccess(std::unique_lock<std::mutex> lock,  WrappedLogger& logger)
                : mLock(std::move(lock))
                , mLogger(logger)
            {}
            WrappedLogger* operator->()
            { return &mLogger; }
            WrappedLogger& GetLogger()
            { return mLogger; }
        private:
            std::unique_lock<std::mutex> mLock;
            WrappedLogger& mLogger;
        };
        LoggerAccess GetLoggerSafe()
        {
            std::unique_lock<std::mutex> lock(mMutex);
            return LoggerAccess(std::move(lock), mLogger);
        }
        WrappedLogger& GetLoggerUnsafe()
        { return mLogger; }

        void EnableWrite(WriteType type, bool on_off)
        { mWrites.set(type, on_off); }
    private:
        WrappedLogger mLogger;
        std::mutex mMutex;
        bitflag<WriteType> mWrites;
    };

    // Insert log messages into an intermediate buffer 
    // until flushed into the wrapped logger. This is
    // convenient when combined with the LockedLogger
    // when the *real* logger has thread affinity. 
    // I.e you can do LockedLogger<BufferLogger<MyLogger>> logger;
    // and multiple threads can log safely by pushing into
    // the log buffer from which the events can then be dispatched
    // by a single thread into the actual logger.
    template<typename WrappedLogger>
    class BufferLogger : public Logger
    {
    public:
        struct LogMessage {
            LogEvent type = LogEvent::Debug;
            std::string file;
            std::string msg;
            int line = 0;
        };
        BufferLogger()
        {
            mWrites.set(WriteType::WriteFormatted);
            mWrites.set(WriteType::WriteRaw);
        }
        BufferLogger(WrappedLogger&& other) : BufferLogger()
        {  mLogger = std::move(other); }

        virtual void Write(LogEvent type, const char* file, int line, const char* msg) override
        {
            LogMessage log;
            log.type = type;
            log.file = file;
            log.line = line;
            log.msg  = msg;
            mBuffer.push_back(std::move(log));
        }
        virtual void Write(LogEvent type, const char* msg) override
        {
            LogMessage log;
            log.type = type;
            log.msg  = msg;
            mBuffer.push_back(std::move(log));
        }
        virtual void Flush() override
        {
            // not implemented because of potentially
            // using this buffer logger with the locked logger.
        }
        virtual bitflag<WriteType> GetWriteMask() const override
        { return mWrites; }

        // Dispatch the buffered log messages to the actual
        // logger object. If using multiple threads you should
        // probably only use a single thread to call this,
        // i.e. the thread that "owns" the WrappedLogger
        // when there's thread affinity.
        void Dispatch()
        {
            for (const auto& msg : mBuffer)
            {
                if (msg.file.empty()) {
                    mLogger.Write(msg.type, msg.msg.c_str());
                } else {
                    mLogger.Write(msg.type, msg.file.c_str(), msg.line, msg.msg.c_str());
                }
            }
            mBuffer.clear();
        }
        size_t GetBufferMsgCount() const
        { return mBuffer.size(); }
        const LogMessage& GetMessage(size_t i) const
        { return mBuffer[i]; }
        const WrappedLogger& GetLogger() const
        { return mLogger; }
        WrappedLogger& GetLogger()
        { return mLogger; }
        void EnableWrite(WriteType type, bool on_off)
        { mWrites.set(type, on_off); }
    private:
        std::vector<LogMessage> mBuffer;
        WrappedLogger mLogger;
        bitflag<WriteType> mWrites;
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

    // Write new log message in the loggers.
    void WriteLogMessage(LogEvent type, const char* file, int line, const std::string& message);

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
        // format the message in the log statement.
        WriteLogMessage(type, file, line, FormatString(fmt, args...));
    }

} // base
