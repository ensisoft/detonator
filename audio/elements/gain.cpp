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
#include "audio/algo.h"
#include "audio/elements/gain.h"

namespace audio
{

Gain::Gain(const std::string& name, float gain)
  : mName(name)
  , mId(base::RandomString(10))
  , mIn("in")
  , mOut("out")
  , mGain(gain)
{}
Gain::Gain(const std::string& name, const std::string& id, float gain)
  : mName(name)
  , mId(id)
  , mIn("in")
  , mOut("out")
  , mGain(gain)
{}

bool Gain::Prepare(const Loader& loader, const PrepareParams& params)
{
    mOut.SetFormat(mIn.GetFormat());
    DEBUG("Audio gain element prepared successfully. [name=%1, gain=%2, output=%3]", mName, mGain, mIn.GetFormat());
    return true;
}

void Gain::Process(Allocator& allocator, EventQueue& events, unsigned milliseconds)
{
    TRACE_SCOPE("Gain");

    BufferHandle buffer;
    if (!mIn.PullBuffer(buffer))
        return;

    const auto& format = mIn.GetFormat();
    if (format.sample_type == SampleType::Int32)
        format.channel_count == 1 ? AdjustGain<int, 1>(buffer)
                                  : AdjustGain<int, 2>(buffer);
    else if (format.sample_type == SampleType::Float32)
        format.channel_count == 1 ? AdjustGain<float, 1>(buffer)
                                  : AdjustGain<float, 2>(buffer);
    else if (format.sample_type == SampleType::Int16)
        format.channel_count == 1 ? AdjustGain<short, 1>(buffer)
                                  : AdjustGain<short, 2>(buffer);
    else WARN("Unsupported format %1", format.sample_type);
}

void Gain::ReceiveCommand(Command& cmd)
{
    if (auto* ptr = cmd.GetIf<SetGainCmd>())
    {
        mGain = ptr->gain;
        DEBUG("Received audio gain command. [elem=%1, gain=%2]", mName, mGain);
    }
    else BUG("Unexpected command.");
}

template<typename DataType, unsigned ChannelCount>
void Gain::AdjustGain(BufferHandle buffer)
{
    using AudioFrame = Frame<DataType, ChannelCount>;
    const auto frame_size  = sizeof(AudioFrame);
    const auto buffer_size = buffer->GetByteSize();
    const auto num_frames  = buffer_size / frame_size;
    ASSERT((buffer_size % frame_size) == 0);

    auto* ptr = static_cast<AudioFrame*>(buffer->GetPtr());
    for (unsigned i=0; i<num_frames; ++i)
    {
        AdjustFrameGain(&ptr[i], mGain);
    }
    mOut.PushBuffer(buffer);
}

} // namespace