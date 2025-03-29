// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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
#include "base/logging.h"
#include "base/trace.h"
#include "audio/elements/zero_source.h"

namespace audio
{

ZeroSource::ZeroSource(const std::string& name, const std::string& id, const Format& format)
  : mName(name)
  , mId(id)
  , mFormat(format)
  , mOut("out")
{
    mOut.SetFormat(format);
}
ZeroSource::ZeroSource(const std::string& name, const Format& format)
  : ZeroSource(name, base::RandomString(10), format)
{}

bool ZeroSource::Prepare(const Loader& loader, const PrepareParams& params)
{
    DEBUG("Audio zero source prepared successfully. [elem=%1, output=%2]", mName, mFormat);
    return true;
}

void ZeroSource::Process(Allocator& allocator, EventQueue& events, unsigned milliseconds)
{
    TRACE_SCOPE("ZeroSource");

    const auto frame_size = GetFrameSizeInBytes(mFormat);
    const auto frames_per_ms = mFormat.sample_rate / 1000;
    const auto frames_wanted = frames_per_ms * milliseconds;

    auto buffer = allocator.Allocate(frame_size * frames_wanted);
    buffer->SetFormat(mFormat);
    buffer->SetByteSize(frame_size * frames_wanted);
    mOut.PushBuffer(buffer);
}

} // namespace