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

#include <sndfile.h>
#include <fstream>
#include <type_traits>

#include "base/assert.h"
#include "base/utility.h"
#include "base/math.h"
#include "base/logging.h"
#include "audio/element.h"

namespace audio
{

ChannelSplitter::ChannelSplitter(const std::string& name)
  : mName(name)
  , mId(base::RandomString(10))
  , mIn("in")
  , mOutLeft("left")
  , mOutRight("right")
{}

bool ChannelSplitter::Prepare()
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
        DEBUG("ChannelSplitter output set to %1", out);
        return true;
    }
    ERROR("ChannelSplitter input format (%1) is not stereo input.", format);
    return false;
}

void ChannelSplitter::Process(unsigned milliseconds)
{
    BufferHandle buffer;
    if (!mIn.HasBuffers())
    {
        WARN("No Buffer available on port %1:%2", mName, mIn.GetName());
        return;
    }
    mIn.PullBuffer(buffer);
    const auto& format = mIn.GetFormat();
    if (format.sample_type == SampleType::Int32)
        Split<int>(buffer);
    else if (format.sample_type == SampleType::Float32)
        Split<float>(buffer);
    else if (format.sample_type == SampleType::Int16)
        Split<short>(buffer);
    else WARN("Unsupported format %1", format.sample_type);
}

template<typename Type>
void ChannelSplitter::Split(BufferHandle buffer)
{
    using StereoFrameType = StereoFrame<Type>;
    using MonoFrameType   = MonoFrame<Type>;

    const auto frame_size  = sizeof(StereoFrameType);
    const auto buffer_size = buffer->GetByteSize();
    const auto num_frames  = buffer_size / frame_size;

    const auto* in = static_cast<StereoFrameType*>(buffer->GetPtr());

    auto left  = std::make_shared<VectorBuffer<>>();
    auto right = std::make_shared<VectorBuffer<>>();
    left->AllocateBytes(buffer_size / 2);
    right->AllocateBytes(buffer_size / 2);
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

Mixer::Mixer(const std::string& name, unsigned int num_srcs)
  : mName(name)
  , mId(base::RandomString(10))
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

bool Mixer::Prepare()
{
    // all input ports should have the same format,
    // otherwise no deal! the user can use a re-sampler
    // to resample/convert the streams in order to make
    // sure that they all have matching format spec.
    const auto& master_format = mSrcs[0].GetFormat();

    for (const auto& src : mSrcs)
    {
        const auto& format = src.GetFormat();
        if (format != master_format)
        {
            ERROR("Mixer src '%1' format mismatch. %2 vs. %3", src.GetName(), format, master_format);
            return false;
        }
    }
    DEBUG("Mixer '%1' prepared with %2 input port(s).", mName, mSrcs.size());
    DEBUG("Mixer '%1' output format set to %2", mName, master_format);
    mOut.SetFormat(master_format);
    return true;
}

void Mixer::Process(unsigned int milliseconds)
{
    const auto& format = mSrcs[0].GetFormat();
    if (format.sample_type == SampleType::Int32)
        format.channel_count == 1 ? MixSources<int, 1>() : MixSources<int, 2>();
    else if (format.sample_type == SampleType::Float32)
        format.channel_count == 1 ? MixSources<float, 1>() : MixSources<float, 2>();
    else if (format.sample_type == SampleType::Int16)
        format.channel_count == 1 ? MixSources<short, 1>() : MixSources<short, 2>();
    else WARN("Unsupported format '%1'", format.sample_type);
}

template<typename DataType, unsigned ChannelCount>
void Mixer::MixSources()
{
    using AudioFrame = Frame<DataType, ChannelCount>;

    std::vector<BufferHandle> buffers;
    std::vector<AudioFrame*> ins;
    for (auto& port :mSrcs)
    {
        if (!port.HasBuffers())
        {
            WARN("No buffer available on port %1:%2", mName, port.GetName());
            return;
        }
        BufferHandle buffer;
        port.PullBuffer(buffer);
        buffers.push_back(buffer);
        auto* ptr = static_cast<AudioFrame*>(buffer->GetPtr());
        ins.push_back(ptr);
    }

    // make things simple now and require that each buffer
    // has the same format and same amount of data. this should
    // work as long as the formats are exactly the same and every
    // source produces the same amount of data, i.e. adheres to the
    // millisecond output.
    for (size_t i=1; i<buffers.size(); ++i)
    {
        if (buffers[i]->GetByteSize() != buffers[0]->GetByteSize())
        {
            WARN("Can't mix buffers with irregular number of audio frames.");
            return;
        }
    }

    const auto frame_size  = sizeof(AudioFrame);
    const auto buffer_size = buffers[0]->GetByteSize();
    const auto num_frames  = buffer_size / frame_size;

    auto* out = ins[0];

    for (unsigned i=0; i<num_frames; ++i)
    {
        MixFrames(&ins[0], ins.size(), out);

        for (auto& in : ins)
            ++in;

        ++out;
    }

    mOut.PushBuffer(buffers[0]);
}

template<unsigned ChannelCount>
void Mixer::MixFrames(Frame<float, ChannelCount>** srcs, unsigned count,
                      Frame<float, ChannelCount>* out) const
{
    const float src_gain = 1.0f/count;

    for (unsigned i=0; i<ChannelCount; ++i)
    {
        float channel_value = 0.0f;

        for (unsigned j=0; j<count; ++j)
        {
            channel_value += (src_gain * srcs[j]->channels[i]);
        }
        out->channels[i] = channel_value;
    }
}

template<typename Type, unsigned ChannelCount>
void Mixer::MixFrames(Frame<Type, ChannelCount>** srcs, unsigned count,
                      Frame<Type, ChannelCount>* out) const
{
    static_assert(std::is_integral<Type>::value);

    const float src_gain = 1.0f/count;

    for (unsigned i=0; i<ChannelCount; ++i)
    {
        std::int64_t channel_value = 0;
        for (unsigned j=0; j<count; ++j)
        {
            // todo: this could still wrap around.
            channel_value += (src_gain * srcs[j]->channels[i]);
        }
        constexpr std::int64_t min = SampleBits<Type>::Bits * -1;
        constexpr std::int64_t max = SampleBits<Type>::Bits;
        out->channels[i] = math::clamp(min, max, channel_value);
    }
}

Gain::Gain(const std::string& name, float gain)
  : mName(name)
  , mId(base::RandomString(10))
  , mIn("in")
  , mOut("out")
  , mGain(gain)
{}

bool Gain::Prepare()
{
    mOut.SetFormat(mIn.GetFormat());
    DEBUG("Gain output set to %1", mIn.GetFormat());
    return true;
}

void Gain::Process(unsigned int milliseconds)
{
    const auto& format = mIn.GetFormat();
    if (format.sample_type == SampleType::Int32)
        format.channel_count == 1 ? AdjustGain<int, 1>() : AdjustGain<int, 2>();
    else if (format.sample_type == SampleType::Float32)
        format.channel_count == 1 ? AdjustGain<float, 1>() : AdjustGain<float, 2>();
    else if (format.sample_type == SampleType::Int16)
        format.channel_count == 1 ? AdjustGain<short, 1>() : AdjustGain<short, 2>();
    else WARN("Unsupported format %1", format.sample_type);
}

template<typename DataType, unsigned ChannelCount>
void Gain::AdjustGain()
{
    BufferHandle buffer;
    if (!mIn.HasBuffers())
    {
        WARN("No buffer available on port %1:%2", mName, mIn.GetName());
        return;
    }
    mIn.PullBuffer(buffer);

    using AudioFrame = Frame<DataType, ChannelCount>;
    const auto frame_size  = sizeof(AudioFrame);
    const auto buffer_size = buffer->GetByteSize();
    const auto num_frames  = buffer_size / frame_size;
    
    ASSERT((buffer_size % frame_size) == 0);

    auto* ptr = static_cast<AudioFrame*>(buffer->GetPtr());
    for (unsigned i=0; i<num_frames; ++i)
    {
        AdjustGain(&ptr[i]);
    }
    mOut.PushBuffer(buffer);
}
template<unsigned ChannelCount>
void Gain::AdjustGain(Frame<float, ChannelCount>* frame) const
{
    // float's can exceed the -1.0f - 1.0f range.
    for (unsigned i=0; i<ChannelCount; ++i)
    {
        frame->channels[i] *= mGain;
    }
}
template<typename Type, unsigned ChannelCount>
void Gain::AdjustGain(Frame<Type, ChannelCount>* frame) const
{
    static_assert(std::is_integral<Type>::value);
    // for integer formats clamp the values in order to avoid
    // undefined overflow / wrapping over.
    for (unsigned i=0; i<ChannelCount; ++i)
    {
        const std::int64_t sample = frame->channels[i] * mGain;
        const std::int64_t min = SampleBits<Type>::Bits * -1;
        const std::int64_t max = SampleBits<Type>::Bits;
        frame->channels[i] = math::clamp(min, max, sample);
    }
}


FileSource::FileSource(const std::string& name, const std::string& file)
  : mName(name)
  , mId(base::RandomString(10))
  , mFile(file)
  , mPort("out")
{
    mFormat.sample_type = SampleType::Int16;
}

FileSource::FileSource(FileSource&& other)
  : mName(other.mName)
  , mId(other.mId)
  , mFile(other.mFile)
  , mDevice(std::move(other.mDevice))
  , mPort(std::move(other.mPort))
  , mFormat(other.mFormat)
  , mFramesRead(other.mFramesRead)
{}

FileSource::~FileSource() = default;

bool FileSource::Prepare()
{
    auto stream = std::make_unique<SndFileInputStream>();
    if (!stream->OpenFile(mFile))
        return false;

    auto device = std::make_unique<SndFileVirtualDevice>(std::move(stream));
    Format format;
    format.channel_count = device->GetNumChannels();
    format.sample_rate   = device->GetSampleRate();
    format.sample_type   = mFormat.sample_type;
    DEBUG("Opened '%1' for reading (%2 frames, %3). %4 channels @ %5 Hz.", mFile,
          device->GetNumFrames(),
          format.sample_type,
          format.channel_count,
          format.sample_rate);
    mDevice = std::move(device);
    mPort.SetFormat(format);
    mFormat = format;
    return true;
}

void FileSource::Process(unsigned milliseconds)
{
    const auto frame_size = GetFrameSizeInBytes(mFormat);
    const auto frames_per_ms = mFormat.sample_rate / 1000;
    const auto frames_wanted = (unsigned)(frames_per_ms * milliseconds);
    const auto frames_available = mDevice->GetNumFrames();
    const auto frames = std::min(frames_available - mFramesRead, frames_wanted);

    auto buffer = std::make_shared<VectorBuffer<>>();
    buffer->SetFormat(mFormat);
    buffer->AllocateBytes(frame_size * frames);
    void* buff = buffer->GetPtr();

    size_t ret = 0;
    if (mFormat.sample_type == SampleType::Float32)
        ret = mDevice->ReadFrames((float*)buff, frames);
    else if (mFormat.sample_type == SampleType::Int32)
        ret = mDevice->ReadFrames((int*)buff, frames);
    else if (mFormat.sample_type == SampleType::Int16)
        ret = mDevice->ReadFrames((short*)buff, frames);

    if (ret != frames)
        WARN("Unexpected number of audio frames. %1 read vs. %2 expected.", ret, frames);

    mFramesRead += ret;
    if (mFramesRead == frames_available)
        DEBUG("File source is done. %1 frames read.", mFramesRead);

    mPort.PushBuffer(buffer);
}

void FileSource::Shutdown()
{
    mDevice.reset();
}

bool FileSource::IsDone() const
{
    return mFramesRead == mDevice->GetNumFrames();
}

#ifdef AUDIO_ENABLE_TEST_SOUND
SineSource::SineSource(const std::string& name, unsigned int frequency)
  : mName(name)
  , mId(base::RandomString(10))
  , mFrequency(frequency)
  , mPort("out")
{
    mFormat.channel_count = 1;
    mFormat.sample_rate   = 44100;
    mFormat.sample_type   = SampleType::Float32;
    mPort.SetFormat(mFormat);
}

bool SineSource::Prepare()
{
    mPort.SetFormat(mFormat);
    DEBUG("Sine '%1' output format set to %2", mName, mFormat);
    return true;
}

void SineSource::Process(unsigned int milliseconds)
{
    const auto frame_size = GetFrameSizeInBytes(mFormat);
    const auto frames_in_millisec = mFormat.sample_rate/1000;
    const auto frames = frames_in_millisec * milliseconds;
    const auto radial_velocity = math::Pi * 2.0 * mFrequency;
    const auto sample_increment = 1.0/mFormat.sample_rate * radial_velocity;
    const auto bytes = frames * frame_size;

    auto buffer = std::make_shared<VectorBuffer<>>();
    buffer->SetFormat(mFormat);
    buffer->AllocateBytes(bytes);

    void* buff = buffer->GetPtr();

    for (unsigned i=0; i<frames; ++i)
    {
        // http://blog.bjornroche.com/2009/12/int-float-int-its-jungle-out-there.html
        const float sample = std::sin(mSampleCount++ * sample_increment);
        if (mFormat.sample_type == SampleType::Float32)
            ((float*)buff)[i] = sample;
        else if (mFormat.sample_type  == SampleType::Int32)
            ((int*)buff)[i] = 0x7fffffff * sample;
        else if (mFormat.sample_type  == SampleType::Int16)
            ((short*)buff)[i] = 0x7fff * sample;
    }
    mPort.PushBuffer(buffer);
}
#endif

} // namespace

