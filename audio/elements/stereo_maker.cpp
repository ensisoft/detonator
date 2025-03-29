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
#include "audio/elements/stereo_maker.h"

namespace audio
{

StereoMaker::StereoMaker(std::string name, std::string id, Channel which)
  : mName(std::move(name))
  , mId(std::move(id))
  , mChannel(which)
  , mOut("out")
  , mIn("in")
{}

StereoMaker::StereoMaker(std::string name, Channel which)
  : StereoMaker(std::move(name), base::RandomString(10), which)
{}

bool StereoMaker::Prepare(const Loader& loader, const PrepareParams& params)
{
    auto format = mIn.GetFormat();
    format.channel_count = 2;
    mOut.SetFormat(format);
    DEBUG("Audio stereo maker prepared successfully. [elem=%1, output=%2]", mName, format);
    return true;
}

void StereoMaker::Process(Allocator& allocator, EventQueue& events, unsigned milliseconds)
{
    TRACE_SCOPE("StereoMaker");

    BufferHandle buffer;
    if (!mIn.PullBuffer(buffer))
        return;

    const auto& format = mIn.GetFormat();
    if (format.channel_count == 2)
    {
        mOut.PushBuffer(buffer);
        return;
    }
    if (format.sample_type == SampleType::Int32)
        CopyMono<int>(allocator, buffer);
    else if (format.sample_type == SampleType::Float32)
        CopyMono<float>(allocator, buffer);
    else if (format.sample_type == SampleType::Int16)
        CopyMono<short>(allocator, buffer);
    else WARN("Audio stereo maker input buffer has unsupported format. [elem=%1, format=%2]", mName, format.sample_type);
}

template<typename Type>
void StereoMaker::CopyMono(BufferAllocator& allocator, BufferHandle buffer)
{
    using StereoFrameType = StereoFrame<Type>;
    using MonoFrameType   = MonoFrame<Type>;

    const auto frame_size  = sizeof(MonoFrameType);
    const auto buffer_size = buffer->GetByteSize();
    const auto num_frames  = buffer_size / frame_size;

    auto stereo = allocator.Allocate(buffer_size * 2);
    stereo->SetByteSize(buffer_size * 2);
    stereo->SetFormat(mOut.GetFormat());
    Buffer::CopyInfoTags(*buffer, *stereo);

    auto* out = static_cast<StereoFrameType*>(stereo->GetPtr());

    const auto* in = static_cast<const MonoFrameType*>(buffer->GetPtr());

    for (unsigned i=0; i<num_frames; ++i, ++out, ++in)
    {
        if (mChannel == Channel::Both)
        {
            out->channels[0] = in->channels[0];
            out->channels[1] = in->channels[0];
        } else out->channels[static_cast<int>(mChannel)] = in->channels[0];
    }
    mOut.PushBuffer(stereo);
}


} // namespace