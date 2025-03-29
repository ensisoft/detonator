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
#include "audio/elements/sine_source.h"

namespace audio
{

SineSource::SineSource(std::string name,
                       std::string id,
                       const Format& format,
                       unsigned frequency,
                       unsigned millisecs)
  : mName(std::move(name))
  , mId(std::move(id))
  , mDuration(millisecs)
  , mFrequency(frequency)
  , mPort("out")
{
    mFormat = format;
    mPort.SetFormat(mFormat);
}

SineSource::SineSource(std::string name,
                       const Format& format,
                       unsigned frequency,
                       unsigned millisecs)
  : SineSource(std::move(name), base::RandomString(10), format, frequency, millisecs)
{}

bool SineSource::Prepare(const Loader& loader, const PrepareParams& params)
{
    mPort.SetFormat(mFormat);
    DEBUG("Audio sine source prepared successfully. [elem=%1, output=%2]", mName, mFormat);
    return true;
}

void SineSource::Process(Allocator& allocator, EventQueue& events, unsigned milliseconds)
{
    TRACE_SCOPE("SineSource");

    if (mDuration)
    {
        ASSERT(mDuration > mMilliSecs);
        milliseconds = std::min(milliseconds, mDuration - mMilliSecs);
    }

    const auto frame_size = GetFrameSizeInBytes(mFormat);
    const auto frames_in_millisec = mFormat.sample_rate/1000;
    const auto frames = frames_in_millisec * milliseconds;
    const auto bytes  = frames * frame_size;

    auto buffer = allocator.Allocate(bytes);
    buffer->SetFormat(mFormat);
    buffer->SetByteSize(bytes);

    if (mFormat.sample_type == SampleType::Int32)
        mFormat.channel_count == 1 ? Generate<int, 1>(buffer, frames)
                                   : Generate<int, 2>(buffer, frames);
    else if (mFormat.sample_type == SampleType::Float32)
        mFormat.channel_count == 1 ? Generate<float, 1>(buffer, frames)
                                   : Generate<float, 2>(buffer, frames);
    else if (mFormat.sample_type == SampleType::Int16)
        mFormat.channel_count == 1 ? Generate<short, 1>(buffer, frames)
                                   : Generate<short, 2>(buffer, frames);

    mPort.PushBuffer(buffer);
    mMilliSecs += milliseconds;
}

template<typename DataType, unsigned ChannelCount>
void SineSource::Generate(BufferHandle buffer, unsigned frames)
{
    using AudioFrame = Frame<DataType, ChannelCount>;

    const auto radial_velocity  = math::Pi * 2.0 * mFrequency;
    const auto sample_increment = 1.0/mFormat.sample_rate * radial_velocity;
    auto* ptr = static_cast<AudioFrame*>(buffer->GetPtr());

    for (unsigned  i=0; i<frames; ++i, ++ptr)
    {
        const float sample = std::sin(mSampleCount++ * sample_increment);
        GenerateFrame(ptr, sample);
    }
}

template<unsigned ChannelCount>
void SineSource::GenerateFrame(Frame<float, ChannelCount>* frame, float value)
{
    for (unsigned i=0; i<ChannelCount; ++i)
    {
        frame->channels[i] = value;
    }
}

template<typename Type, unsigned ChannelCount>
void SineSource::GenerateFrame(Frame<Type, ChannelCount>* frame, float value)
{
    static_assert(std::is_integral<Type>::value);

    for (unsigned i=0; i<ChannelCount; ++i)
    {
        frame->channels[i] = SampleBits<Type>::Bits * value;
    }
}

} // namespace