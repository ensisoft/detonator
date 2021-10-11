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
        const char* name = nullptr;
        unsigned start_time  = 0;
        unsigned finish_time = 0;
        unsigned level = 0;
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
        virtual void Clear() = 0;
        virtual void Start() = 0;
        virtual void Write(TraceWriter& writer) const = 0;
        virtual unsigned BeginScope(const char* name) = 0;
        virtual void EndScope(unsigned index) = 0;
    private:
    };

    class TraceLog : public Trace
    {
    public:
        TraceLog(size_t capacity)
        {
            mCallTrace.resize(capacity);
        }
        virtual void Clear() override
        {
            mTraceIndex = 0;
        }
        virtual void Start() override
        {
            mTraceIndex = 0;
            mStackDepth = 0;
            mStartTime  = std::chrono::high_resolution_clock::now();
        }
        virtual void Write(TraceWriter& writer) const override
        {
            for (size_t i=0; i<mTraceIndex; ++i)
                writer.Write(mCallTrace[i]);
        }
        virtual unsigned BeginScope(const char* name) override
        {
            ASSERT(mTraceIndex < mCallTrace.size());
            TraceEntry entry;
            entry.name  = name;
            entry.level = mStackDepth++;
            entry.start_time = GetTime();
            mCallTrace[mTraceIndex] =  entry;
            return mTraceIndex++;
        }
        virtual void EndScope(unsigned index) override
        {
            ASSERT(index < mCallTrace.size());
            ASSERT(mStackDepth);
            mCallTrace[index].finish_time = GetTime();
            mStackDepth--;
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

    class FileTraceWriter : public TraceWriter
    {
    public:
        FileTraceWriter(const std::string& file);
       ~FileTraceWriter() noexcept;
        FileTraceWriter() = default;
        virtual void Write(const TraceEntry& entry) override;
        virtual void Flush() override;
        FileTraceWriter& operator=(const FileTraceWriter&) = delete;
    private:
        FILE* mFile = nullptr;
    };

    void SetThreadTrace(Trace* trace);
    void TraceClear();
    void TraceStart();
    void TraceWrite(TraceWriter& writer);
    bool IsTracingEnabled();

    unsigned TraceBeginScope(const char* name);
    void TraceEndScope(unsigned index);

    struct AutoTracingScope {
        AutoTracingScope(const char* name)
        { index = TraceBeginScope(name); }
       ~AutoTracingScope()
        { TraceEndScope(index); }
    private:
        unsigned index = 0;
    };

    struct ManualTracingScope {
        ManualTracingScope(const char* name)
        { index = TraceBeginScope(name); }
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

} // namespace

#define TRACE_START() base::TraceStart()
#define TRACE_SCOPE(name) base::AutoTracingScope _trace(name)
#define TRACE_ENTER(name) base::ManualTracingScope foo_##name(#name)
#define TRACE_LEAVE(name) foo_##name.EndScope()
#define TRACE_CALL(call, name)             \
do {                                       \
   base::AutoTracingScope _trace(name);    \
   call;                                   \
} while (0)
