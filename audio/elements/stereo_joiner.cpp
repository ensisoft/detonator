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
#include "base/utility.h"
#include "audio/elements/stereo_joiner.h"

namespace audio
{

StereoJoiner::StereoJoiner(std::string name, std::string id)
  : mName(std::move(name))
  , mId(std::move(id))
  , mOut("out")
  , mInLeft("left")
  , mInRight("right")
{}

StereoJoiner::StereoJoiner(std::string name)
  : StereoJoiner(std::move(name), base::RandomString(10))
{}

bool StereoJoiner::Prepare(const Loader& loader, const PrepareParams& params)
{
    const auto left  = mInLeft.GetFormat();
    const auto right = mInRight.GetFormat();
    if (left == right && left.channel_count == 1)
    {
        Format out;
        out.channel_count = 2;
        out.sample_rate = left.sample_rate;
        out.sample_type = left.sample_type;
        mOut.SetFormat(out);
        DEBUG("Audio joiner prepared successfully. [elem=%1, output=%2]", mName, out);
        return true;
    }
    ERROR("Audio joiner input formats are not compatible. [elem=%1, left=%2, right=%3]", mName, left, right);
    return false;
}

void StereoJoiner::Process(Allocator& allocator, EventQueue& events, unsigned milliseconds)
{
    TRACE_SCOPE("StereoJoiner");

    BufferHandle left;
    BufferHandle right;
    if (!mInLeft.HasBuffers())
        return;
    else if (!mInRight.HasBuffers())
        return;

    mInLeft.PullBuffer(left);
    mInRight.PullBuffer(right);
    if (left->GetByteSize() != right->GetByteSize())
    {
        WARN("Audio joiner cannot join buffers with irregular number of audio frames. [elem=%1]", mName);
        return;
    }

    const auto& format = mInLeft.GetFormat();
    if (format.sample_type == SampleType::Int32)
        Join<int>(allocator, left, right);
    else if (format.sample_type == SampleType::Float32)
        Join<float>(allocator, left, right);
    else if (format.sample_type == SampleType::Int16)
        Join<short>(allocator, left, right);
    else WARN("Audio joiner input buffer has unsupported format. [elem=%1, format=%2]", mName, format.sample_type);
}

template<typename Type>
void StereoJoiner::Join(Allocator& allocator, const BufferHandle& left, const BufferHandle& right)
{
    using StereoFrameType = StereoFrame<Type>;
    using MonoFrameType   = MonoFrame<Type>;

    const auto frame_size  = sizeof(MonoFrameType);
    const auto buffer_size = left->GetByteSize();
    const auto num_frames  = buffer_size / frame_size;

    auto stereo = allocator.Allocate(buffer_size * 2);
    stereo->SetByteSize(buffer_size * 2);
    stereo->SetFormat(mOut.GetFormat());
    Buffer::CopyInfoTags(*left, *stereo);
    Buffer::CopyInfoTags(*right, *stereo);

    auto* out = static_cast<StereoFrameType*>(stereo->GetPtr());

    const auto* L = static_cast<const MonoFrameType*>(left->GetPtr());
    const auto* R = static_cast<const MonoFrameType*>(right->GetPtr());
    for (unsigned i=0; i<num_frames; ++i, ++out, ++L, ++R)
    {
        out->channels[0] = L->channels[0];
        out->channels[1] = R->channels[0];
    }
    mOut.PushBuffer(stereo);
}


} // namespace