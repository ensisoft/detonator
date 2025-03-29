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
#include "audio/elements/playlist.h"

namespace audio
{

Playlist::Playlist(const std::string& name, const std::string& id, const std::vector<PortDesc>& srcs)
  : mName(name)
  , mId(id)
  , mOut("out")
{
    for (const auto& desc : srcs)
    {
        SingleSlotPort p(desc.name);
        mSrcs.push_back(std::move(p));
    }
}
Playlist::Playlist(const std::string& name, const std::vector<PortDesc>& srcs)
  : Playlist(name, base::RandomString(10), srcs)
{}

bool Playlist::Prepare(const Loader& loader, const PrepareParams& params)
{
    // all input ports should have the same format,
    // otherwise no deal! the user can use a re-sampler
    // to resample/convert the streams in order to make
    // sure that they all have matching format spec.
    const auto& master_format = mSrcs[0].GetFormat();
    if (!IsValid(master_format))
    {
        ERROR("Audio playlist input port format is invalid. [elem=%1, port=%2]", mName, mSrcs[0].GetName());
        return false;
    }

    for (const auto& src : mSrcs)
    {
        const auto& format = src.GetFormat();
        const auto& name   = src.GetName();
        if (format != master_format)
        {
            ERROR("Audio playlist port is incompatible with other ports. [elem=%1, port=%2, format=%3]", mName, name, format);
            return false;
        }
    }
    DEBUG("Audio playlist prepared successfully. [elem=%1, srcs=%2, output=%3]", mName, mSrcs.size(), master_format);
    mOut.SetFormat(master_format);
    return true;
}

void Playlist::Process(Allocator& allocator, EventQueue& events, unsigned milliseconds)
{
    TRACE_SCOPE("Playlist");

    if (mSrcIndex == mSrcs.size())
        return;
    BufferHandle buffer;
    if (!mSrcs[mSrcIndex].PullBuffer(buffer))
        return;

    // if all the sources of audio buffers on the current
    // input port are done based on the buffer info tags
    // then we move onto to next source port.
    bool all_sources_done = true;
    for (size_t i=0; i<buffer->GetNumInfoTags(); ++i)
    {
        const auto& tag = buffer->GetInfoTag(i);
        if (!tag.element.source)
            continue;
        if (!tag.element.source_done)
            all_sources_done = false;
    }
    if (all_sources_done)
        mSrcIndex++;

    mOut.PushBuffer(buffer);
}

} // namespace