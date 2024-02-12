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

#include <vector>
#include <string>
#include <cstdio>
#include <exception>
#include <mutex>

#include "base/platform.h"
#include "base/assert.h"

namespace base
{
    struct TraceEntry {
        const char* name     = nullptr;
        unsigned start_time  = 0;
        unsigned finish_time = 0;
        unsigned level       = 0;
        unsigned tid         = 0;
        std::vector<std::string> markers;
        std::string comment;
    };

    struct TraceEvent {
        std::string name;
        unsigned time = 0;
        unsigned tid  = 0;
    };

    class TraceWriter
    {
    public:
        virtual ~TraceWriter() = default;
        virtual void Write(const TraceEntry& entry) = 0;
        virtual void Write(const TraceEvent& event) = 0;
        virtual void Flush() = 0;
    private:
    };

    class Trace
    {
    public:
        virtual ~Trace() = default;
        virtual void Start() = 0;
        virtual void Write(TraceWriter& writer) const = 0;
        virtual unsigned BeginScope(const char* name) = 0;
        virtual void EndScope(unsigned index) = 0;
        virtual void Marker(std::string marker, unsigned index) = 0;
        virtual void Comment(std::string comment, unsigned index) = 0;
        virtual void Event(std::string name) = 0;

        virtual unsigned GetCurrentTraceIndex() const = 0;

        virtual const char* StoreString(std::string str) = 0;

        inline void Marker(std::string marker)
        {
            const auto index = GetCurrentTraceIndex();

            Marker(std::move(marker), index);
        }
        inline void Comment(std::string comment)
        {
            const auto index = GetCurrentTraceIndex();

            Comment(std::move(comment), index);
        }
    private:
    };

    class TraceLog : public Trace
    {
    public:
        // Some known threadIDs so that the actual thread ID can be
        // for example AudioThread+1, AudioThread+2 so each component/
        // subsystem can have their own thread ids inside some range
        // without stomping on each other's IDs
        enum ThreadId {
            MainThread = 0,
            AudioThread = 100,
            RenderTread = 200,
            TaskThread  = 300
        };

        explicit TraceLog(size_t capacity, size_t threadId = 0) noexcept
        {
            mCallTrace.resize(capacity);
            mThreadId   = threadId;
        }
        virtual void Start() override
        {
            mTraceIndex = 0;
            mStackDepth = 0;
            mDynamicStrings.clear();
            mTraceEvents.clear();
        }
        virtual void Write(TraceWriter& writer) const override
        {
            for (size_t i=0; i<mTraceIndex; ++i)
                writer.Write(mCallTrace[i]);

            for (const auto& event : mTraceEvents)
                writer.Write(event);
        }
        virtual unsigned BeginScope(const char* name) override
        {
            ASSERT(mTraceIndex < mCallTrace.size());
            TraceEntry entry;
            entry.name        = name;
            entry.tid         = mThreadId;
            entry.level       = mStackDepth++;
            entry.start_time  = GetTime();
            entry.finish_time = 0;
            mCallTrace[mTraceIndex] = std::move(entry);
            return mTraceIndex++;
        }
        virtual void EndScope(unsigned index) override
        {
            ASSERT(index < mCallTrace.size());
            ASSERT(mStackDepth);
            mCallTrace[index].finish_time = GetTime();
            mStackDepth--;
        }
        virtual void Marker(std::string str, unsigned index) override
        {
            ASSERT(index < mTraceIndex);
            mCallTrace[index].markers.push_back(std::move(str));
        }

        virtual void Comment(std::string str, unsigned index) override
        {
            ASSERT(index < mTraceIndex);
            mCallTrace[index].comment = std::move(str);
        }
        virtual void Event(std::string name) override
        {
            TraceEvent event;
            event.name = std::move(name);
            event.time = GetTime();
            event.tid  = mThreadId;
            mTraceEvents.push_back(std::move(event));
        }

        virtual unsigned GetCurrentTraceIndex() const override
        {
            ASSERT(mTraceIndex > 0 && mTraceIndex < mCallTrace.size());
            return mTraceIndex - 1;
        }

        virtual const char* StoreString(std::string str) override
        {
            mDynamicStrings.push_back(std::move(str));
            return mDynamicStrings.back().c_str();
        }

        inline void RenameBlock(const char* name, unsigned index) noexcept
        {
            ASSERT(index < mTraceIndex);
            mCallTrace[index].name = name;
        }

        inline std::size_t GetNumEntries() const noexcept
        { return mTraceIndex; }
        inline const TraceEntry& GetEntry(size_t index) const noexcept
        { return mCallTrace[index]; }

    private:
        static unsigned GetTime();
    private:
        std::vector<TraceEntry> mCallTrace;
        std::size_t mTraceIndex = 0;
        std::size_t mStackDepth = 0;
        std::size_t mThreadId   = 0;
        std::vector<std::string> mDynamicStrings;
        std::vector<TraceEvent> mTraceEvents;
    };

    class TextFileTraceWriter : public TraceWriter
    {
    public:
        TextFileTraceWriter(const std::string& file);
        TextFileTraceWriter(TextFileTraceWriter&& other)
          : mFile(other.mFile)
        {
            other.mFile = nullptr;
        }
       ~TextFileTraceWriter() noexcept;
        TextFileTraceWriter() = delete;
        virtual void Write(const TraceEntry& entry) override;
        virtual void Write(const TraceEvent& event) override;
        virtual void Flush() override;
        TextFileTraceWriter& operator=(const TextFileTraceWriter&) = delete;
    private:
        FILE* mFile = nullptr;
    };

    class ChromiumTraceJsonWriter : public TraceWriter
    {
    public:
        ChromiumTraceJsonWriter(const std::string& file);
        ChromiumTraceJsonWriter(ChromiumTraceJsonWriter&& other)
          : mFile(other.mFile)
        {
            other.mFile = nullptr;
        }
        ChromiumTraceJsonWriter(const ChromiumTraceJsonWriter&) = delete;
       ~ChromiumTraceJsonWriter() noexcept;
        ChromiumTraceJsonWriter();
        virtual void Write(const TraceEntry& entry) override;
        virtual void Write(const TraceEvent& event) override;
        virtual void Flush() override;
        ChromiumTraceJsonWriter& operator=(const ChromiumTraceJsonWriter&) = delete;
    private:
        FILE* mFile = nullptr;
        bool mCommaNeeded = false;
    };

    template<typename WrappedWriter>
    class LockedTraceWriter : public TraceWriter
    {
    public:
        explicit LockedTraceWriter(WrappedWriter&& writer) noexcept
           : mWriter(std::move(writer))
        {}
        virtual void Write(const TraceEntry& entry) override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mWriter.Write(entry);
        }
        virtual void Write(const TraceEvent& event) override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mWriter.Write(event);
        }
        virtual void Flush() override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mWriter.Flush();
        }
    private:
        std::mutex mMutex;
        WrappedWriter mWriter;
    };

    Trace* GetThreadTrace();
    void SetThreadTrace(Trace* trace);
    void TraceStart();
    void TraceWrite(TraceWriter& writer);
    void TraceMarker(std::string str);
    void TraceMarker(std::string str, unsigned index);

    void TraceComment(std::string str);
    void TraceComment(std::string str, unsigned index);

    void TraceEvent(std::string name);

    void EnableTracing(bool on_off);
    bool IsTracingEnabled();

    unsigned TraceBeginScope(const char* name);
    void TraceEndScope(unsigned index);

    struct AutoTracingScope {
        AutoTracingScope(const char* name, std::string comment) noexcept
        {
            index = TraceBeginScope(name);
            TraceComment(std::move(comment), index);

        }
        explicit AutoTracingScope(const char* name) noexcept
        {
            index = TraceBeginScope(name);
        }
       ~AutoTracingScope() noexcept
        {
            TraceEndScope(index);
        }
    private:
        unsigned index = 0;
    };

    struct ManualTracingScope {
        ManualTracingScope(const char* name, std::string comment) noexcept
        {
            index = TraceBeginScope(name);
            TraceComment(std::move(comment), index);

        }
        explicit ManualTracingScope(const char* name) noexcept
        {
            index = TraceBeginScope(name);
        }
       ~ManualTracingScope() noexcept
        {
            if (std::uncaught_exceptions())
            {
                TraceEndScope(index);
                index = -1;
            }
            ASSERT(index == -1 && "No matching call to TraceEndScope found.");
        }
        void EndScope() noexcept
        {
            TraceEndScope(index);
            index = -1;
        }
    private:
        unsigned index = 0;
    };

    namespace detail {
        template<typename... Args>
        std::string FormatTraceComment(const char* fmt, Args... args)
        {
            static char pray[256] = {0};
            std::snprintf(pray, sizeof(pray),fmt, args...);
            return pray;
        }
        inline std::string FormatTraceComment(const char* fmt)
        { return fmt; }
        inline std::string FormatTraceComment()
        { return ""; }
    } // namespace

} // namespace

#if defined(BASE_TRACING_ENABLE_TRACING)
#  define TRACE_START() base::TraceStart()
#  define TRACE_SCOPE(name, ...) base::AutoTracingScope _trace(name, base::detail::FormatTraceComment(__VA_ARGS__))
#  define TRACE_ENTER(name, ...) base::ManualTracingScope foo_##name(#name, base::detail::FormatTraceComment(__VA_ARGS__))
#  define TRACE_LEAVE(name) foo_##name.EndScope()
#  define TRACE_CALL(name, call, ...)                                                        \
do {                                                                                         \
   base::AutoTracingScope _trace(name, base::detail::FormatTraceComment(__VA_ARGS__));       \
   call;                                                                                     \
} while (0)
#  define TRACE_BLOCK(name, block, ...)                                                      \
do {                                                                                         \
   base::AutoTracingScope _trace(name, base::detail::FormatTraceComment(__VA_ARGS__));       \
   block                                                                                     \
} while (0)
#  define TRACE_EVENT(name) base::TraceEvent(name)
#else
#  define TRACE_START()
#  define TRACE_SCOPE(name, ...)
#  define TRACE_ENTER(name, ...)
#  define TRACE_LEAVE(name)
#  define TRACE_CALL(name, call, ...) \
do {                                  \
  call;                               \
} while(0)
#  define TRACE_BLOCK(name, block, ...) \
do {                                  \
 block;                               \
} while(0)
#  define TRACE_EVENT(name)
#endif



