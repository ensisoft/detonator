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

#include <samplerate.h>

#include "base/assert.h"
#include "base/logging.h"
#include "base/trace.h"
#include "audio/elements/resampler.h"

namespace audio
{

Resampler::Resampler(const std::string& name, unsigned sample_rate)
  : mName(name)
  , mId(base::RandomString(10))
  , mSampleRate(sample_rate)
  , mIn("in")
  , mOut("out")
{}

Resampler::Resampler(const std::string& name, const std::string& id,  unsigned sample_rate)
  : mName(name)
  , mId(id)
  , mSampleRate(sample_rate)
  , mIn("in")
  , mOut("out")
{}

Resampler::~Resampler()
{
    if (mState)
        ::src_delete(mState);
}

bool Resampler::Prepare(const Loader& loader, const PrepareParams& params)
{
    const auto& in = mIn.GetFormat();
    if (in.sample_type != SampleType::Float32)
    {
        ERROR("Audio re-sampler requires float32 input. [elem=%1, input=%2]", mName, in.sample_type);
        return false;
    }

    int error = 0;
    mState = ::src_new(SRC_SINC_BEST_QUALITY, in.channel_count, &error);
    if (mState == NULL)
    {
        ERROR("Audio re-sampler prepare error. [elem=%1, error=%2, what='%3']", mName, error, ::src_strerror(error));
        return false;
    }

    Format format;
    format.sample_type   = SampleType::Float32;
    format.channel_count = in.channel_count;
    format.sample_rate   = mSampleRate;
    mOut.SetFormat(format);
    DEBUG("Audio re-sampler prepared successfully. [elem=%1, output=%2]", mName, format);
    return true;
}

void Resampler::Process(Allocator& allocator, EventQueue& events, unsigned milliseconds)
{
    TRACE_SCOPE("Resampler");

    BufferHandle src_buffer;
    if (!mIn.PullBuffer(src_buffer))
        return;

    const auto& src_format = mIn.GetFormat();
    const auto& out_format = mOut.GetFormat();
    if (src_format == out_format)
    {
        mOut.PushBuffer(src_buffer);
        return;
    }

    const auto src_buffer_size = src_buffer->GetByteSize();
    const auto src_frame_size  = sizeof(float) * src_format.channel_count;
    const auto src_frame_count = src_buffer_size / src_frame_size;
    const auto max_frame_count = milliseconds * (out_format.sample_rate / 1000);
    const auto out_frame_count = std::min((unsigned)max_frame_count, (unsigned)src_frame_count);
    ASSERT((src_buffer_size % src_frame_size) == 0);

    auto out_buffer = allocator.Allocate(out_frame_count * src_frame_size);
    out_buffer->SetFormat(out_format);
    Buffer::CopyInfoTags(*src_buffer, *out_buffer);

    SRC_DATA data;
    data.data_in       = static_cast<const float*>(src_buffer->GetPtr());
    data.input_frames  = src_frame_count;
    data.data_out      = static_cast<float*>(out_buffer->GetPtr());
    data.output_frames = out_frame_count; // maximum number of frames for output.
    data.src_ratio     = double(out_format.sample_rate) / double(src_format.sample_rate);
    data.end_of_input = 0; // more coming 1 = end of input. todo: how to deal with this properly?
    const auto ret = ::src_process(mState, &data);
    if (ret != 0)
    {
        ERROR("Audio re-sampler resample error. [elem=%1, error=%2, what='%3']", mName, ret, ::src_strerror(ret));
        return;
    }

    // todo: deal with the case when not all input is consumed.
    if (data.input_frames_used != data.input_frames)
    {
        const auto pending = data.input_frames - data.input_frames_used;
        WARN("Audio re-sampler discarding input frames. [elem=%1, frames=%2]", mName, pending);
    }

    out_buffer->SetByteSize(src_frame_size * data.output_frames_gen);
    mOut.PushBuffer(out_buffer);
}

} // namespace