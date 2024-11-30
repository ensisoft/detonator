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

#include <atomic>
#include <chrono>

#include "base/assert.h"
#include "base/trace.h"
#include "base/logging.h"
#include "base/memory.h"

namespace {
static thread_local base::Trace* thread_tracer = nullptr;
static thread_local bool enable_tracing = false;
} // namespace

namespace base
{

unsigned TraceLog::BeginScope(const char* name)
{
    if (mTraceIndex == mCallTrace.size())
    {
        if (!mMaxStackSizeExceededWarning)
        {
            WARN("Tracing scopes exceed maximum trace stack size. [max='%1']", mCallTrace.size());
            WARN("Your tracing will be incomplete!!");
            WARN("You must increase the maximum trace entry count in order receive complete trace.");
            WARN("This message is printed once per run.");
            mMaxStackSizeExceededWarning = true;
        }
        return mTraceIndex;
    }

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

void TraceLog::EndScope(unsigned int index)
{
    ASSERT(index <= mCallTrace.size());
    ASSERT(mStackDepth);

    // if the index is the maximum we are ignoring this
    // stack pop.
    if (index == mTraceIndex)
        return;

    mCallTrace[index].finish_time = GetTime();
    mStackDepth--;
}

Trace* GetThreadTrace()
{
    return thread_tracer;
}

void SetThreadTrace(Trace* trace)
{
    thread_tracer = trace;
}
void TraceStart()
{
    if (thread_tracer && enable_tracing)
        thread_tracer->Start();
}

void TraceWrite(TraceWriter& writer)
{
    if (thread_tracer && enable_tracing)
        thread_tracer->Write(writer);
}

unsigned TraceBeginScope(const char* name)
{
    if (thread_tracer && enable_tracing)
        return thread_tracer->BeginScope(name);
    return 0;
}

void TraceEndScope(unsigned index)
{
    if (thread_tracer && enable_tracing)
        thread_tracer->EndScope(index);
}

void TraceMarker(std::string str)
{
    if (thread_tracer && enable_tracing)
        thread_tracer->Marker(std::move(str));
}

void TraceMarker(std::string str, unsigned index)
{
    if (thread_tracer && enable_tracing)
        thread_tracer->Marker(std::move(str), index);
}

void TraceComment(std::string str)
{
    if (thread_tracer && enable_tracing)
        thread_tracer->Comment(std::move(str));
}

void TraceComment(std::string str, unsigned index)
{
    if (thread_tracer && enable_tracing)
        thread_tracer->Comment(std::move(str), index);
}

void TraceEvent(std::string name)
{
    if (thread_tracer && enable_tracing)
        thread_tracer->Event(std::move(name));
}

bool IsTracingEnabled()
{
    return enable_tracing;
}

void EnableTracing(bool on_off)
{
    enable_tracing = on_off;
}

// static
unsigned int TraceLog::GetTime()
{
    using clock = std::chrono::high_resolution_clock;

    static auto start_time = clock::now();

    const auto now = clock::now();
    const auto gone = now - start_time;
    return std::chrono::duration_cast<std::chrono::microseconds>(gone).count();
}


TextFileTraceWriter::TextFileTraceWriter(const std::string& file)
{
    mFile = std::fopen(file.c_str(), "w");
    if (mFile == nullptr)
        throw std::runtime_error("failed to open trace file: " + file);
}
TextFileTraceWriter::~TextFileTraceWriter() noexcept
{
    // could be moved.
    if (mFile)
    {
        std::fclose(mFile);
    }
}

void TextFileTraceWriter::Write(const TraceEntry& entry)
{
    // https://stackoverflow.com/questions/293438/left-pad-printf-with-spaces
    // %*s < print spaces
    std::fprintf(mFile, "%*s%s %fms, '%s'", entry.level+1, " ", entry.name,
                 (entry.finish_time - entry.start_time) / 1000.0f, entry.comment.c_str());
    for (const auto& m : entry.markers)
        std::fprintf(mFile, " %s ", m.c_str());
    std::fprintf(mFile, "\n\n");
}

void TextFileTraceWriter::Write(const struct TraceEvent& event)
{
    // todo:
}

void TextFileTraceWriter::Flush()
{
    std::fflush(mFile);
}

ChromiumTraceJsonWriter::ChromiumTraceJsonWriter(const std::string& file)
{
    mFile = std::fopen(file.c_str(), "w");
    if (mFile == nullptr)
        throw std::runtime_error("failed to open trace file: " + file);
    std::fprintf(mFile, "{\"traceEvents\":[\n");
}

ChromiumTraceJsonWriter::~ChromiumTraceJsonWriter() noexcept
{
    // could be moved
    if (mFile)
    {
        std::fprintf(mFile, "] }\n");
        std::fflush(mFile);
        std::fclose(mFile);
    }
}

void ChromiumTraceJsonWriter::Write(const TraceEntry& entry)
{
    const auto duration = entry.finish_time - entry.start_time;
    const auto start    = entry.start_time;
    const auto threadId = entry.tid;

    std::string markers;
    for (const auto& m : entry.markers)
    {
        markers += m;
        markers.push_back(' ');
    }
    if (!markers.empty())
        markers.pop_back();

    // ph = type
    // ph = X -> complete event
    // ph = i -> instant event


constexpr static auto* JsonFormat =
  R"(%c { "pid":0, "tid":%u, "ph":"X", "ts":%u, "dur":%u, "name":"%s", "args": { "markers": "%s", "comment": "%s" } }
)";
    std::fprintf(mFile, JsonFormat, mCommaNeeded ? ',' : ' ',
                 (unsigned)threadId,
                 (unsigned)start,
                 (unsigned)duration,
                 entry.name, markers.c_str(), entry.comment.c_str());

    mCommaNeeded = true;
}

void ChromiumTraceJsonWriter::Write(const struct TraceEvent& event)
{
    constexpr static auto* JsonFormat =
R"(%c { "pid":0, "tid":%u, "ph":"i", "ts":%u, "s":"g", "name":"%s"  }
)";
    std::fprintf(mFile, JsonFormat, mCommaNeeded ? ',': ' ',
                 (unsigned)event.tid,
                 (unsigned)event.time,
                 event.name.c_str());

    mCommaNeeded = true;
}

void ChromiumTraceJsonWriter::Flush()
{
    std::fflush(mFile);
}

} // namespace
