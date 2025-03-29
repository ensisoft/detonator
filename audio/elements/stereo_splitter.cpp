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
#include "audio/elements/stereo_splitter.h"

namespace audio
{

StereoSplitter::StereoSplitter(const std::string& name, const std::string& id)
  : mName(name)
  , mId(id)
  , mIn("in")
  , mOutLeft("left")
  , mOutRight("right")
{}
StereoSplitter::StereoSplitter(const std::string& name)
  : StereoSplitter(name, base::RandomString(10))
{}

bool StereoSplitter::Prepare(const Loader& loader, const PrepareParams& params)
{
    const auto format = mIn.GetFormat();
    if (format.channel_count == 2)
    {
        Format out;
        out.channel_count = 1;
        out.sample_rate = format.sample_rate;
        out.sample_type = format.sample_type;
        mOutRight.SetFormat(out);
        mOutLeft.SetFormat(out);
        DEBUG("Audio splitter prepared successfully. [elem=%1, output=%2]", mName, out);
        return true;
    }
    ERROR("Audio splitter input format is not stereo. [elem=%1]", mName);
    return false;
}

void StereoSplitter::Process(Allocator& allocator, EventQueue& events, unsigned milliseconds)
{
    TRACE_SCOPE("StereoSplitter");

    BufferHandle buffer;
    if (!mIn.HasBuffers())
        return;

    mIn.PullBuffer(buffer);
    const auto& format = mIn.GetFormat();
    if (format.sample_type == SampleType::Int32)
        Split<int>(allocator, buffer);
    else if (format.sample_type == SampleType::Float32)
        Split<float>(allocator, buffer);
    else if (format.sample_type == SampleType::Int16)
        Split<short>(allocator, buffer);
    else WARN("Audio splitter input buffer has unsupported format. [elem=%1, format=%2]", mName, format);
}

template<typename Type>
void StereoSplitter::Split(Allocator& allocator, BufferHandle buffer)
{
    using StereoFrameType = StereoFrame<Type>;
    using MonoFrameType   = MonoFrame<Type>;

    const auto frame_size  = sizeof(StereoFrameType);
    const auto buffer_size = buffer->GetByteSize();
    const auto num_frames  = buffer_size / frame_size;

    const auto* in = static_cast<StereoFrameType*>(buffer->GetPtr());

    auto left  = allocator.Allocate(buffer_size / 2);
    auto right = allocator.Allocate(buffer_size / 2);
    left->SetByteSize(buffer_size / 2);
    left->SetFormat(mOutLeft.GetFormat());
    right->SetByteSize(buffer_size / 2);
    right->SetFormat(mOutRight.GetFormat());
    Buffer::CopyInfoTags(*buffer, *left);
    Buffer::CopyInfoTags(*buffer, *right);
    auto* L = static_cast<MonoFrameType *>(left->GetPtr());
    auto* R = static_cast<MonoFrameType *>(right->GetPtr());

    for (unsigned i=0; i<num_frames; ++i, ++in, ++L, ++R)
    {
        L->channels[0] = in->channels[0];
        R->channels[0] = in->channels[1];
    }
    mOutLeft.PushBuffer(left);
    mOutRight.PushBuffer(right);
}


} // namespace