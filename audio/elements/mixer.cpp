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
#include "audio/algo.h"
#include "audio/elements/mixer.h"

namespace audio
{

Mixer::Mixer(std::string name, std::string id, unsigned int num_srcs)
  : mName(std::move(name))
  , mId(std::move(id))
  , mOut("out")
{
    // mixer requires at least 1 src port.
    ASSERT(num_srcs);
    for (unsigned i=0; i<num_srcs; ++i)
    {
        SingleSlotPort p(base::FormatString("in%1", i));
        mSrcs.push_back(std::move(p));
    }
}

Mixer::Mixer(std::string name, unsigned num_srcs)
  : Mixer(std::move(name), base::RandomString(10), num_srcs)
{}

Mixer::Mixer(std::string name, std::string id, const std::vector<PortDesc>& srcs)
  : mName(std::move(name))
  , mId(std::move(id))
  , mOut("out")
{
    for (const auto& desc : srcs)
    {
        SingleSlotPort p(desc.name);
        mSrcs.push_back(std::move(p));
    }
}

bool Mixer::Prepare(const Loader& loader, const PrepareParams& params)
{
    // all input ports should have the same format,
    // otherwise no deal! the user can use a re-sampler
    // to resample/convert the streams in order to make
    // sure that they all have matching format spec.
    const auto& master_format = mSrcs[0].GetFormat();
    if (!IsValid(master_format))
    {
        ERROR("Audio mixer input port format is invalid. [elem=%1, port=%2]", mName, mSrcs[0].GetName());
        return false;
    }

    for (const auto& src : mSrcs)
    {
        const auto& format = src.GetFormat();
        const auto& name   = src.GetName();
        if (format != master_format)
        {
            ERROR("Audio mixer input port is incompatible with other ports. [elem=%1, port=%2, format=%3]", mName, name, format);
            return false;
        }
    }
    DEBUG("Audio mixer prepared successfully. [elem=%1, srcs=%2, output=%3]", mName, mSrcs.size(), master_format);
    mOut.SetFormat(master_format);
    return true;
}

void Mixer::Process(Allocator& allocator, EventQueue& events, unsigned milliseconds)
{
    TRACE_SCOPE("Mixer");

    // The mixing here only looks at incoming buffers and combines them together
    // in chunks of whole buffers. No buffer splitting or queueing is supported.
    // This works only as long as every incoming buffer contains an equal amount
    // of PMC data (as measured in milliseconds).
    // So for example when mixing together src0 and src1 which both produce audio
    // buffers with 20ms worth of PCM data the mixer will then produce a single
    // output buffer with 20ms worth of data and everything will work fine.
    // If however src0 produced 20ms audio buffers and src1 produced 5ms audio
    // buffers the mixer *could not* produce 20ms output but only 5ms and would have
    // to keep the rest of the unmixed data around until the next Process call.
    // Mixing 20ms and 5ms together into 20ms combination would produce 15ms gaps
    // in src1's output.
    // The only exception to the above rule is when a source element is winding
    // down and is producing its *last* audio buffer which can then contain less
    // than the requested milliseconds. Since there's no next buffer from this
    // source there'll be no audio gap either.

    const float src_gain = 1.0f/mSrcs.size();

    std::vector<BufferHandle> src_buffers;
    for (auto& port :mSrcs)
    {
        BufferHandle buffer;
        if (port.PullBuffer(buffer))
            src_buffers.push_back(buffer);
    }
    if (src_buffers.size() == 0)
        return;

    BufferHandle ret;

    const auto& format = mSrcs[0].GetFormat();
    if (format.sample_type == SampleType::Int32)
        ret = format.channel_count == 1 ? MixBuffers<int, 1>(src_buffers, src_gain)
                                        : MixBuffers<int, 2>(src_buffers, src_gain);
    else if (format.sample_type == SampleType::Float32)
        ret = format.channel_count == 1 ? MixBuffers<float, 1>(src_buffers,src_gain)
                                        : MixBuffers<float, 2>(src_buffers, src_gain);
    else if (format.sample_type == SampleType::Int16)
        ret = format.channel_count == 1 ? MixBuffers<short, 1>(src_buffers, src_gain)
                                        : MixBuffers<short, 2>(src_buffers, src_gain);
    else WARN("Audio mixer input buffer has unsupported format. [elem=%1, format=%2]", mName, format.sample_type);
    mOut.PushBuffer(ret);
}

} // namespace