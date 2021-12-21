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

#include "base/assert.h"
#include "base/trace.h"

namespace {
static thread_local base::Trace* thread_tracer;
} // namespace

namespace base
{

void SetThreadTrace(Trace* trace)
{
    thread_tracer = trace;
}
void TraceStart()
{
    if (thread_tracer)
        thread_tracer->Start();
}

void TraceWrite(TraceWriter& writer)
{
    if (thread_tracer)
        thread_tracer->Write(writer);
}

unsigned TraceBeginScope(const char* name, std::string comment)
{
    if (thread_tracer)
        return thread_tracer->BeginScope(name, comment);
    return 0;
}
void TraceEndScope(unsigned index)
{
    if (thread_tracer)
        thread_tracer->EndScope(index);
}
void TraceMarker(const std::string& str)
{
    if (thread_tracer)
        thread_tracer->Marker(str);
}
void TraceMarker(const std::string& str, unsigned index)
{
    if (thread_tracer)
        thread_tracer->Marker(str, index);
}

bool IsTracingEnabled()
{ return thread_tracer != nullptr; }

TextFileTraceWriter::TextFileTraceWriter(const std::string& file)
{
    mFile = std::fopen(file.c_str(), "w");
    if (mFile == nullptr)
        throw std::runtime_error("failed to open trace file: " + file);
}
TextFileTraceWriter::~TextFileTraceWriter() noexcept
{
    std::fclose(mFile);
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
    std::fprintf(mFile, "] }\n");
    std::fflush(mFile);
    std::fclose(mFile);
}

void ChromiumTraceJsonWriter::Write(const TraceEntry& entry)
{
    const auto duration = entry.finish_time - entry.start_time;
    const auto start    = entry.start_time;

    std::string markers;
    for (const auto& m : entry.markers)
    {
        markers += m;
        markers.push_back(' ');
    }
    markers.pop_back();

constexpr static auto* JsonString =
  R"(%c { "pid":0, "tid":0, "ph":"X", "ts":%u, "dur":%u, "name":"%s", "args": { "markers": "%s", "comment": "%s" } }
)";

    std::fprintf(mFile, JsonString, mCommaNeeded ? ',' : ' ', start, duration,
                 entry.name, markers.c_str(), entry.comment.c_str());

    mCommaNeeded = true;
}
void ChromiumTraceJsonWriter::Flush()
{
    std::fflush(mFile);
}

} // namespace
