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

#include <samplerate.h>

#include "base/assert.h"
#include "base/utility.h"
#include "base/math.h"
#include "base/logging.h"
#include "audio/element.h"

namespace audio
{

Joiner::Joiner(const std::string& name)
  : mName(name)
  , mId(base::RandomString(10))
  , mOut("out")
  , mInLeft("left")
  , mInRight("right")
{}

bool Joiner::Prepare()
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
        DEBUG("Joiner '%1' output set to %1", mName, out);
        return true;
    }
    ERROR("Joiner '%1' input formats (%1, %2) are not compatible mono streams.", mName, left, right);
    return false;
}

void Joiner::Process(unsigned int milliseconds)
{
    BufferHandle left;
    BufferHandle right;
    if (!mInLeft.HasBuffers())
    {
        WARN("No buffer available on Joiner '%1' left channel port.", mName);
        return;
    }
    if (!mInRight.HasBuffers())
    {
        WARN("No buffer available on Joiner '%1' right channel port.", mName);
        return;
    }
    mInLeft.PullBuffer(left);
    mInRight.PullBuffer(right);
    if (left->GetByteSize() != right->GetByteSize())
    {
        WARN("Can't join buffers with irregular number of audio frames.");
        return;
    }

    const auto& format = mInLeft.GetFormat();
    if (format.sample_type == SampleType::Int32)
        Join<int>(left, right);
    else if (format.sample_type == SampleType::Float32)
        Join<float>(left, right);
    else if (format.sample_type == SampleType::Int16)
        Join<short>(left, right);
    else WARN("Unsupported format %1", format.sample_type);
}

template<typename Type>
void Joiner::Join(BufferHandle left, BufferHandle right)
{
    using StereoFrameType = StereoFrame<Type>;
    using MonoFrameType   = MonoFrame<Type>;

    const auto frame_size  = sizeof(MonoFrameType);
    const auto buffer_size = left->GetByteSize();
    const auto num_frames  = buffer_size / frame_size;

    auto stereo = std::make_shared<VectorBuffer>();
    stereo->AllocateBytes(buffer_size * 2);

    auto* out = static_cast<StereoFrameType*>(stereo->GetPtr());

    const auto* L = static_cast<const MonoFrameType*>(left->GetPtr());
    const auto* R = static_cast<const MonoFrameType*>(right->GetPtr());
    for (unsigned i=0; i<num_frames; ++i, ++out, ++L, ++R)
    {
        out->channels[0] = L->channels[0];
        out->channels[1] = R->channels[1];
    }
    mOut.PushBuffer(stereo);
}

Splitter::Splitter(const std::string& name)
  : mName(name)
  , mId(base::RandomString(10))
  , mIn("in")
  , mOutLeft("left")
  , mOutRight("right")
{}

bool Splitter::Prepare()
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
        DEBUG("Splitter '%1' output set to %1", mName, out);
        return true;
    }
    ERROR("Splitter '%1' input format (%1) is not stereo input.", mName, format);
    return false;
}

void Splitter::Process(unsigned milliseconds)
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
void Splitter::Split(BufferHandle buffer)
{
    using StereoFrameType = StereoFrame<Type>;
    using MonoFrameType   = MonoFrame<Type>;

    const auto frame_size  = sizeof(StereoFrameType);
    const auto buffer_size = buffer->GetByteSize();
    const auto num_frames  = buffer_size / frame_size;

    const auto* in = static_cast<StereoFrameType*>(buffer->GetPtr());

    auto left  = std::make_shared<VectorBuffer>();
    auto right = std::make_shared<VectorBuffer>();
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

    std::vector<BufferHandle> src_buffers;
    std::vector<AudioFrame*> src_ptrs;
    for (auto& port :mSrcs)
    {
        if (port.HasBuffers())
        {
            BufferHandle buffer;
            port.PullBuffer(buffer);
            src_buffers.push_back(buffer);
            auto* ptr = static_cast<AudioFrame*>(buffer->GetPtr());
            src_ptrs.push_back(ptr);
        }
    }

    BufferHandle out_buffer;
    // find the maximum buffer for computing how many frames must be processed.
    // also the biggest buffer can be used for the output (mixing in place)
    unsigned max_buffer_size = 0;
    for (const auto& buffer : src_buffers)
    {
        if (buffer->GetByteSize() > max_buffer_size)
        {
            max_buffer_size = buffer->GetByteSize();
            out_buffer = buffer;
        }
    }

    const auto frame_size  = sizeof(AudioFrame);
    const auto max_num_frames = max_buffer_size / frame_size;

    auto* out = static_cast<AudioFrame*>(out_buffer->GetPtr());

    for (unsigned frame=0; frame<max_num_frames; ++frame, ++out)
    {
        MixFrames(&src_ptrs[0], src_ptrs.size(), out);

        ASSERT(src_buffers.size() == src_ptrs.size());
        for (size_t i=0; i<src_buffers.size();)
        {
            const auto& buffer = src_buffers[i];
            const auto buffer_size = buffer->GetByteSize();
            const auto buffer_frames = buffer_size / frame_size;
            if (buffer_frames == frame+1)
            {
                const auto end = src_buffers.size() - 1;
                std::swap(src_buffers[i], src_buffers[end]);
                std::swap(src_ptrs[i], src_ptrs[end]);
                src_buffers.pop_back();
                src_ptrs.pop_back();
            }
            else
            {
                ++src_ptrs[i++];
            }
        }
    }

    mOut.PushBuffer(out_buffer);
}

template<unsigned ChannelCount>
void Mixer::MixFrames(Frame<float, ChannelCount>** srcs, unsigned count,
                      Frame<float, ChannelCount>* out) const
{
    const float src_gain = 1.0f/mSrcs.size();

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

    const float src_gain = 1.0f/mSrcs.size();

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

Resampler::Resampler(const std::string& name, unsigned sample_rate)
  : mName(name)
  , mId(base::RandomString(10))
  , mSampleRate(sample_rate)
  , mIn("in")
  , mOut("out")
{}

Resampler::~Resampler()
{
    if (mState)
        ::src_delete(mState);
}

bool Resampler::Prepare()
{
    const auto& in = mIn.GetFormat();
    if (in.sample_type != SampleType::Float32)
    {
        ERROR("Resampler '%1' requires data in '%2' format vs. %3 offered.", mName,
              SampleType::Float32, in.sample_type);
        return false;
    }

    int error = 0;
    mState = ::src_new(SRC_SINC_BEST_QUALITY, in.channel_count, &error);
    if (mState == NULL)
    {
        ERROR("Resampler '%1' prepare error %2 ('%3').", mName, error, ::src_strerror(error));
        return false;
    }

    Format format;
    format.sample_type   = SampleType::Float32;
    format.channel_count = in.channel_count;
    format.sample_rate   = mSampleRate;
    mOut.SetFormat(format);
    DEBUG("Resampler '%1' output set to '%2'", mName, format);
    return true;
}

void Resampler::Process(unsigned milliseconds)
{
    BufferHandle src_buffer;
    if (!mIn.PullBuffer(src_buffer))
    {
        WARN("No input buffer on port %1:%2", mName, mIn.GetName());
        return;
    }
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

    auto out_buffer = std::make_shared<VectorBuffer>();
    out_buffer->AllocateBytes(out_frame_count * src_frame_size);
    out_buffer->SetFormat(out_format);

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
        ERROR("Resampler '%1' resampling error %2 ('%3').", mName, ret, ::src_strerror(ret));
        return;
    }

    // todo: deal with the case when not all input is consumed.
    if (data.input_frames_used != data.input_frames)
    {
        const auto pending = data.input_frames - data.input_frames_used;
        WARN("Resampler '%1' discarding %2 pending input frames.", mName, pending);
    }

    out_buffer->ResizeBytes(src_frame_size * data.output_frames_gen);
    mOut.PushBuffer(out_buffer);
}

FileSource::FileSource(const std::string& name, const std::string& file, SampleType type)
  : mName(name)
  , mId(base::RandomString(10))
  , mFile(file)
  , mPort("out")
{
    mFormat.sample_type = type;
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

    auto buffer = std::make_shared<VectorBuffer>();
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

bool FileSource::IsSourceDone() const
{
    return mFramesRead == mDevice->GetNumFrames();
}

#ifdef AUDIO_ENABLE_TEST_SOUND
SineSource::SineSource(const std::string& name, unsigned int frequency)
  : mName(name)
  , mId(base::RandomString(10))
  , mFrequency(frequency)
  , mLimitDuration(false)
  , mPort("out")
{
    mFormat.channel_count = 1;
    mFormat.sample_rate   = 44100;
    mFormat.sample_type   = SampleType::Float32;
    mPort.SetFormat(mFormat);
}

SineSource::SineSource(const std::string& name, unsigned int frequency, unsigned int millisecs)
  : mName(name)
  , mId(base::RandomString(10))
  , mFrequency(frequency)
  , mDuration(millisecs)
  , mLimitDuration(true)
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
    if (mLimitDuration)
    {
        ASSERT(mDuration > mMilliSecs);
        milliseconds = std::min(milliseconds, mDuration - mMilliSecs);
    }

    const auto frame_size = GetFrameSizeInBytes(mFormat);
    const auto frames_in_millisec = mFormat.sample_rate/1000;
    const auto frames = frames_in_millisec * milliseconds;
    const auto radial_velocity = math::Pi * 2.0 * mFrequency;
    const auto sample_increment = 1.0/mFormat.sample_rate * radial_velocity;
    const auto bytes = frames * frame_size;

    auto buffer = std::make_shared<VectorBuffer>();
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
    mMilliSecs += milliseconds;
}
#endif

} // namespace

