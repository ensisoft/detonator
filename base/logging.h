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
  #define ERROR_RETURN(ret, fmt, ...) \
      do { \
          WriteLog(base::LogEvent::Error, __FILE__, __LINE__, fmt, ## __VA_ARGS__); \
          return ret; \
     } while (0)
#else
  #define DEBUG(...) while(false)
  #define WARN(...)  while(false)
  #define INFO(...)  while(false)
  #define ERROR(...) while(false)
  #define ERROR_RETURN(...) while(false)
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
        OStreamLogger() = default;
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
        std::ostream* m_out = nullptr;
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

#if defined(__EMSCRIPTEN__)
    class EmscriptenLogger : public Logger
    {
    public:
        virtual void Write(LogEvent type, const char* file, int line, const char* msg) override;
        virtual void Write(LogEvent type, const char* msg) override;
        virtual void Flush() override;
        virtual base::bitflag<WriteType> GetWriteMask() const override
        { return WriteType::WriteFormatted; }
    private:
    };
#endif


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

    bool IsLogEventEnabled(LogEvent type);

    void EnableLogEvent(LogEvent type, bool on_off);

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
        if (!IsLogEventEnabled(type))
            return;

        // format the message in the log statement.
        WriteLogMessage(type, file, line, FormatString(fmt, args...));
    }

} // base
