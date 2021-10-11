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
void TraceClear()
{
    if (thread_tracer)
        thread_tracer->Clear();
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

unsigned TraceBeginScope(const char* name)
{
    if (thread_tracer)
        return thread_tracer->BeginScope(name);
    return 0;
}
void TraceEndScope(unsigned index)
{
    if (thread_tracer)
        thread_tracer->EndScope(index);
}

bool IsTracingEnabled()
{ return thread_tracer != nullptr; }

FileTraceWriter::FileTraceWriter(const std::string& file)
{
    mFile = std::fopen(file.c_str(), "w");
    if (mFile == nullptr)
        throw std::runtime_error("failed to open trace file: " + file);
}
FileTraceWriter::~FileTraceWriter() noexcept
{
    std::fclose(mFile);
}

void FileTraceWriter::Write(const TraceEntry& entry)
{
    // https://stackoverflow.com/questions/293438/left-pad-printf-with-spaces
    // %*s < print spaces
    std::fprintf(mFile, "%*s%s %fms\n", entry.level, " ", entry.name,
                 (entry.finish_time - entry.start_time) / 1000.0f);
}

void FileTraceWriter::Flush()
{
    std::fprintf(mFile, "\n");
    std::fflush(mFile);
}

} // namespace
