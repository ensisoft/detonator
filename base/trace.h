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
#include <chrono>
#include <cstdio>

#include "base/platform.h"
#include "base/assert.h"

namespace base
{
    struct TraceEntry {
        const char* name     = nullptr;
        unsigned start_time  = 0;
        unsigned finish_time = 0;
        unsigned level       = 0;
        std::vector<std::string> markers;
        std::string comment;
    };

    class TraceWriter
    {
    public:
        virtual ~TraceWriter() = default;
        virtual void Write(const TraceEntry& entry) = 0;
        virtual void Flush() = 0;
    private:
    };

    class Trace
    {
    public:
        virtual ~Trace() = default;
        virtual void Start() = 0;
        virtual void Write(TraceWriter& writer) const = 0;
        virtual unsigned BeginScope(const char* name, std::string comment) = 0;
        virtual void EndScope(unsigned index) = 0;
        virtual void Marker(const std::string& marker) = 0;
        virtual void Marker(const std::string& marker, unsigned index) = 0;
    private:
    };

    class TraceLog : public Trace
    {
    public:
        TraceLog(size_t capacity)
        {
            mCallTrace.resize(capacity);
            mStartTime  = std::chrono::high_resolution_clock::now();
        }
        virtual void Start() override
        {
            mTraceIndex = 0;
            mStackDepth = 0;
        }
        virtual void Write(TraceWriter& writer) const override
        {
            for (size_t i=0; i<mTraceIndex; ++i)
                writer.Write(mCallTrace[i]);
        }
        virtual unsigned BeginScope(const char* name, std::string comment) override
        {
            ASSERT(mTraceIndex < mCallTrace.size());
            TraceEntry entry;
            entry.name        = name;
            entry.level       = mStackDepth++;
            entry.start_time  = GetTime();
            entry.finish_time = 0;
            entry.comment     = std::move(comment);
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
        virtual void Marker(const std::string& str) override
        {
            ASSERT(mTraceIndex > 0 && mTraceIndex < mCallTrace.size());
            mCallTrace[mTraceIndex-1].markers.push_back(str);
        }
        virtual void Marker(const std::string& str, unsigned index) override
        {
            ASSERT(index < mTraceIndex);
            mCallTrace[index].markers.push_back(str);
        }
        std::size_t GetNumEntries() const
        { return mTraceIndex; }
        const TraceEntry& GetEntry(size_t index) const
        { return mCallTrace[index]; }
    private:
        unsigned GetTime() const
        {
            using clock = std::chrono::high_resolution_clock;
            const auto now = clock::now();
            const auto gone = now - mStartTime;
            return std::chrono::duration_cast<std::chrono::microseconds>(gone).count();
        }
    private:
        std::vector<TraceEntry> mCallTrace;
        std::size_t mTraceIndex = 0;
        std::size_t mStackDepth = 0;
        std::chrono::high_resolution_clock::time_point mStartTime;
    };

    class TextFileTraceWriter : public TraceWriter
    {
    public:
        TextFileTraceWriter(const std::string& file);
       ~TextFileTraceWriter() noexcept;
        TextFileTraceWriter() = default;
        virtual void Write(const TraceEntry& entry) override;
        virtual void Flush() override;
        TextFileTraceWriter& operator=(const TextFileTraceWriter&) = delete;
    private:
        FILE* mFile = nullptr;
    };

    class ChromiumTraceJsonWriter : public TraceWriter
    {
    public:
        ChromiumTraceJsonWriter(const std::string& file);
       ~ChromiumTraceJsonWriter() noexcept;
        ChromiumTraceJsonWriter();
        virtual void Write(const TraceEntry& entry) override;
        virtual void Flush() override;
        ChromiumTraceJsonWriter& operator=(const ChromiumTraceJsonWriter&) = delete;
    private:
        FILE* mFile = nullptr;
        bool mCommaNeeded = false;
    };

    void SetThreadTrace(Trace* trace);
    void TraceStart();
    void TraceWrite(TraceWriter& writer);
    void TraceMarker(const std::string& str, unsigned index);
    void TraceMarker(const std::string& str);
    bool IsTracingEnabled();

    unsigned TraceBeginScope(const char* name, std::string comment);
    void TraceEndScope(unsigned index);

    struct AutoTracingScope {
        AutoTracingScope(const char* name, std::string comment)
        { index = TraceBeginScope(name, std::move(comment)); }
       ~AutoTracingScope()
        { TraceEndScope(index); }
    private:
        unsigned index = 0;
    };

    struct ManualTracingScope {
        ManualTracingScope(const char* name, std::string comment)
        { index = TraceBeginScope(name, std::move(comment)); }
       ~ManualTracingScope()
        {
            ASSERT(index == -1 && "No matching call to TraceEndScope found.");
        }
        void EndScope()
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

#define TRACE_START() base::TraceStart()
#define TRACE_SCOPE(name, ...) base::AutoTracingScope _trace(name, base::detail::FormatTraceComment(__VA_ARGS__))
#define TRACE_ENTER(name, ...) base::ManualTracingScope foo_##name(#name, base::detail::FormatTraceComment(__VA_ARGS__))
#define TRACE_LEAVE(name) foo_##name.EndScope()
#define TRACE_CALL(name, call, ...)                                                          \
do {                                                                                         \
   base::AutoTracingScope _trace(name, base::detail::FormatTraceComment(__VA_ARGS__));       \
   call;                                                                                     \
} while (0)
#define TRACE_BLOCK(name, block, ...)                                                        \
do {                                                                                         \
   base::AutoTracingScope _trace(name, base::detail::FormatTraceComment(__VA_ARGS__));       \
   block                                                                                     \
} while (0)




