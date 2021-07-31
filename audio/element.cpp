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
#include <cmath> // for pow

#include <samplerate.h>

#include "base/assert.h"
#include "base/utility.h"
#include "base/math.h"
#include "base/logging.h"
#include "audio/element.h"
#include "audio/sndfile.h"
#include "audio/mpg123.h"
#include "audio/loader.h"

namespace {
using namespace audio;
template<unsigned ChannelCount>
void AdjustFrameGain(Frame<float, ChannelCount>* frame, float gain)
{
    // float's can exceed the -1.0f - 1.0f range.
    for (unsigned i=0; i<ChannelCount; ++i)
    {
        frame->channels[i] *= gain;
    }
}
template<typename Type, unsigned ChannelCount>
void AdjustFrameGain(Frame<Type, ChannelCount>* frame, float gain)
{
    static_assert(std::is_integral<Type>::value);
    // for integer formats clamp the values in order to avoid
    // undefined overflow / wrapping over.
    for (unsigned i=0; i<ChannelCount; ++i)
    {
        const std::int64_t sample = frame->channels[i] * gain;
        const std::int64_t min = SampleBits<Type>::Bits * -1;
        const std::int64_t max = SampleBits<Type>::Bits;
        frame->channels[i] = math::clamp(min, max, sample);
    }
}

template<unsigned ChannelCount>
void MixFrames(Frame<float, ChannelCount>** srcs, unsigned count, float src_gain,
               Frame<float, ChannelCount>* out)
{
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
void MixFrames(Frame<Type, ChannelCount>** srcs, unsigned count, float src_gain,
               Frame<Type, ChannelCount>* out)
{
    static_assert(std::is_integral<Type>::value);

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

template<typename Type, unsigned ChannelCount>
BufferHandle MixBuffers(std::vector<BufferHandle>& src_buffers, float src_gain)
{
    using AudioFrame = Frame<Type, ChannelCount>;

    std::vector<AudioFrame*> src_ptrs;

    BufferHandle out_buffer;
    // find the maximum buffer for computing how many frames must be processed.
    // also the biggest buffer can be used for the output (mixing in place)
    unsigned max_buffer_size = 0;
    for (const auto& buffer : src_buffers)
    {
        auto* ptr = static_cast<AudioFrame*>(buffer->GetPtr());
        src_ptrs.push_back(ptr);
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
        MixFrames(&src_ptrs[0], src_ptrs.size(), src_gain, out);

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
    return out_buffer;
}

} // namespace

namespace audio
{

StereoMaker::StereoMaker(const std::string& name, const std::string& id, Channel which)
  : mName(name)
  , mId(id)
  , mChannel(which)
  , mOut("out")
  , mIn("in")
{}
StereoMaker::StereoMaker(const std::string& name, Channel which)
  : StereoMaker(name, base::RandomString(10), which)
{}

bool StereoMaker::Prepare(const Loader& loader)
{
    auto format = mIn.GetFormat();
    format.channel_count = 2;
    mOut.SetFormat(format);
    DEBUG("MonoToStereo '%1' out format set to '%2'.", mName, format);
    return true;
}

void StereoMaker::Process(EventQueue& events, unsigned milliseconds)
{
    BufferHandle buffer;
    if (!mIn.PullBuffer(buffer))
    {
        WARN("No buffer available on MonoToStereo '%1' input port.", mName);
        return;
    }
    const auto& format = mIn.GetFormat();
    if (format.channel_count == 2)
    {
        mOut.PushBuffer(buffer);
        return;
    }
    if (format.sample_type == SampleType::Int32)
        CopyMono<int>(buffer);
    else if (format.sample_type == SampleType::Float32)
        CopyMono<float>(buffer);
    else if (format.sample_type == SampleType::Int16)
        CopyMono<short>(buffer);
    else WARN("Unsupported format %1", format.sample_type);
}

template<typename Type>
void StereoMaker::CopyMono(BufferHandle buffer)
{
    using StereoFrameType = StereoFrame<Type>;
    using MonoFrameType   = MonoFrame<Type>;

    const auto frame_size  = sizeof(MonoFrameType);
    const auto buffer_size = buffer->GetByteSize();
    const auto num_frames  = buffer_size / frame_size;

    auto stereo = std::make_shared<VectorBuffer>();
    stereo->AllocateBytes(buffer_size * 2);

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

StereoJoiner::StereoJoiner(const std::string& name, const std::string& id)
  : mName(name)
  , mId(id)
  , mOut("out")
  , mInLeft("left")
  , mInRight("right")
{}
StereoJoiner::StereoJoiner(const std::string& name)
  : StereoJoiner(name, base::RandomString(10))
{}

bool StereoJoiner::Prepare(const Loader& loader)
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

void StereoJoiner::Process(EventQueue& events, unsigned milliseconds)
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
void StereoJoiner::Join(BufferHandle left, BufferHandle right)
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

bool StereoSplitter::Prepare(const Loader& loader)
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

void StereoSplitter::Process(EventQueue& events, unsigned milliseconds)
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
void StereoSplitter::Split(BufferHandle buffer)
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

Mixer::Mixer(const std::string& name, const std::string& id, unsigned int num_srcs)
  : mName(name)
  , mId(id)
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
Mixer::Mixer(const std::string& name, unsigned num_srcs)
  : Mixer(name, base::RandomString(10), num_srcs)
{}

bool Mixer::Prepare(const Loader& loader)
{
    // all input ports should have the same format,
    // otherwise no deal! the user can use a re-sampler
    // to resample/convert the streams in order to make
    // sure that they all have matching format spec.
    const auto& master_format = mSrcs[0].GetFormat();
    if (!IsValid(master_format))
    {
        ERROR("Mixer '%1' input port '%2' format not set.", mName, mSrcs[0].GetName());
        return false;
    }

    for (const auto& src : mSrcs)
    {
        const auto& format = src.GetFormat();
        const auto& name   = src.GetName();
        if (format != master_format)
        {
            ERROR("Mixer '%1' port '%2' format (%3) is not compatible with other ports.", mName, name, format);
            return false;
        }
    }
    DEBUG("Mixer '%1' prepared with %2 input port(s).", mName, mSrcs.size());
    DEBUG("Mixer '%1' output format set to %2", mName, master_format);
    mOut.SetFormat(master_format);
    return true;
}

void Mixer::Process(EventQueue& events, unsigned milliseconds)
{
    const float src_gain = 1.0f/mSrcs.size();

    std::vector<BufferHandle> src_buffers;
    for (auto& port :mSrcs)
    {
        BufferHandle buffer;
        if (port.PullBuffer(buffer))
            src_buffers.push_back(buffer);
    }
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
    else WARN("Unsupported format '%1'", format.sample_type);
    mOut.PushBuffer(ret);
}

Fade::Fade(const std::string& name, const std::string& id, float duration, Effect effect)
  : mName(name)
  , mId(id)
  , mIn("in")
  , mOut("out")
{
    SetFade(effect, duration);
}

Fade::Fade(const std::string& name, float duration, Effect effect)
  : Fade(name, base::RandomString(10), duration, effect)
{}
Fade::Fade(const std::string& name)
  : mName(name)
  , mId(base::RandomString(10))
  , mIn("in")
  , mOut("out")
{}

bool Fade::Prepare(const Loader& loader)
{
    const auto& format = mIn.GetFormat();
    mSampleRate = format.sample_rate;
    DEBUG("Fade '%1' output format set to '%2'.", mName, format);
    mOut.SetFormat(format);
    return true;
}

void Fade::Process(EventQueue& events, unsigned milliseconds)
{
    BufferHandle buffer;
    if (!mIn.PullBuffer(buffer))
    {
        WARN("No buffer available on Fade '%1' input port.", mName);
        return;
    }

    const auto& format = mIn.GetFormat();
    if (format.sample_type == SampleType::Int32)
        format.channel_count == 1 ? AdjustGain<int, 1>(buffer) : AdjustGain<int, 2>(buffer);
    else if (format.sample_type == SampleType::Float32)
        format.channel_count == 1 ? AdjustGain<float, 1>(buffer) : AdjustGain<float, 2>(buffer);
    else if (format.sample_type == SampleType::Int16)
        format.channel_count == 1 ? AdjustGain<short, 1>(buffer) : AdjustGain<short, 2>(buffer);
    else WARN("Unsupported format %1", format.sample_type);
}

void Fade::ReceiveCommand(Command& cmd)
{
    if (auto* ptr = cmd.GetIf<SetFadeCmd>())
        SetFade(ptr->effect, ptr->duration);
    else BUG("Unexpected command.");
}

void Fade::SetFade(Effect effect, float duration)
{
    ASSERT(mDuration >= 0.0f);
    mFadeDirection = effect == Effect::FadeIn ? 1.0f : -1.0f;
    mDuration      = duration;
    mSampleTime    = 0.0f;
    DEBUG("Set Fade '%1' fade effect to %2 in %3 seconds.", mName, effect, duration);
}

template<typename DataType, unsigned ChannelCount>
void Fade::AdjustGain(BufferHandle buffer)
{
    // take a shortcut when done.
    if (mSampleTime >= mDuration)
    {
        mOut.PushBuffer(buffer);
        return;
    }

    using AudioFrame = Frame<DataType, ChannelCount>;
    const auto frame_size  = sizeof(AudioFrame);
    const auto buffer_size = buffer->GetByteSize();
    const auto num_frames  = buffer_size / frame_size;
    ASSERT((buffer_size % frame_size) == 0);

    // the sample rate tells us the "duration" of the sample in seconds.
    const auto sample_duration_sec = 1.0f / (float)mSampleRate;

    auto* ptr = static_cast<AudioFrame*>(buffer->GetPtr());
    for (unsigned i=0; i<num_frames; ++i)
    {
        const auto sample_gain_linear = math::clamp(0.0f, 1.0f, mSampleTime / mDuration) * mFadeDirection;
        const auto sample_gain = std::pow(sample_gain_linear, 2.2);
        AdjustFrameGain(&ptr[i], sample_gain);
        mSampleTime += sample_duration_sec;
    }
    mOut.PushBuffer(buffer);
}

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

bool Gain::Prepare(const Loader& loader)
{
    mOut.SetFormat(mIn.GetFormat());
    DEBUG("Gain '%1' output format set to %2.", mName, mIn.GetFormat());
    return true;
}

void Gain::Process(EventQueue& events, unsigned milliseconds)
{
    BufferHandle buffer;
    if (!mIn.PullBuffer(buffer))
    {
        WARN("No buffer available on port %1:%2", mName, mIn.GetName());
        return;
    }
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
        DEBUG("Gain '%1' value set to %2.", mName, mGain);
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

bool Resampler::Prepare(const Loader& loader)
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

void Resampler::Process(EventQueue& events, unsigned milliseconds)
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

FileSource::FileSource(const std::string& name, const std::string& id, const std::string& file, SampleType type)
  : mName(name)
  , mId(id)
  , mFile(file)
  , mPort("out")
{
    mFormat.sample_type = type;
}

FileSource::FileSource(FileSource&& other)
  : mName(other.mName)
  , mId(other.mId)
  , mFile(other.mFile)
  , mDecoder(std::move(other.mDecoder))
  , mPort(std::move(other.mPort))
  , mFormat(other.mFormat)
  , mFramesRead(other.mFramesRead)
{}

FileSource::~FileSource() = default;

bool FileSource::Prepare(const Loader& loader)
{
    auto stream = loader.OpenStream(mFile);
    if (!stream.is_open())
        return false;

    std::unique_ptr<Decoder> decoder;
    const auto& upper = base::ToUpperUtf8(mFile);
    if (base::EndsWith(upper, ".MP3"))
    {
        auto io = std::make_unique<Mpg123FileInputStream>();
        io->UseStream(mFile, std::move(stream));
        auto dec = std::make_unique<Mpg123Decoder>();
        if (!dec->Open(std::move(io), mFormat.sample_type))
            return false;
        decoder = std::move(dec);
    }
    else if (base::EndsWith(upper, ".OGG") ||
             base::EndsWith(upper, ".WAV") ||
             base::EndsWith(upper, ".FLAC"))
    {
        auto io = std::make_unique<SndFileInputStream>();
        io->UseStream(mFile, std::move(stream));
        auto dec = std::make_unique<SndFileDecoder>();
        if (!dec->Open(std::move(io)))
            return false;
        decoder = std::move(dec);
    }
    else
    {
        ERROR("Unsupported audio file format ('%1').", mFile);
        return false;
    }
    Format format;
    format.channel_count = decoder->GetNumChannels();
    format.sample_rate   = decoder->GetSampleRate();
    format.sample_type   = mFormat.sample_type;
    DEBUG("Opened '%1' for reading (%2 frames) %3.", mFile, decoder->GetNumFrames(), format);
    mDecoder = std::move(decoder);
    mPort.SetFormat(format);
    mFormat = format;
    return true;
}

void FileSource::Process(EventQueue& events, unsigned milliseconds)
{
    const auto frame_size = GetFrameSizeInBytes(mFormat);
    const auto frames_per_ms = mFormat.sample_rate / 1000;
    const auto frames_wanted = (unsigned)(frames_per_ms * milliseconds);
    const auto frames_available = mDecoder->GetNumFrames();
    const auto frames = std::min(frames_available - mFramesRead, frames_wanted);

    auto buffer = std::make_shared<VectorBuffer>();
    buffer->SetFormat(mFormat);
    buffer->AllocateBytes(frame_size * frames);
    void* buff = buffer->GetPtr();

    size_t ret = 0;
    if (mFormat.sample_type == SampleType::Float32)
        ret = mDecoder->ReadFrames((float*)buff, frames);
    else if (mFormat.sample_type == SampleType::Int32)
        ret = mDecoder->ReadFrames((int*)buff, frames);
    else if (mFormat.sample_type == SampleType::Int16)
        ret = mDecoder->ReadFrames((short*)buff, frames);

    if (ret != frames)
        WARN("Unexpected number of audio frames. %1 read vs. %2 expected.", ret, frames);

    mFramesRead += ret;
    if (mFramesRead == frames_available)
        DEBUG("File source is done. %1 frames read.", mFramesRead);

    mPort.PushBuffer(buffer);
}

void FileSource::Shutdown()
{
    mDecoder.reset();
}

bool FileSource::IsSourceDone() const
{
    return mFramesRead == mDecoder->GetNumFrames();
}

// static
bool FileSource::ProbeFile(const std::string& file, FileInfo* info)
{
    auto stream = base::OpenBinaryInputStream(file);
    if (!stream.is_open())
        return false;

    std::unique_ptr<Decoder> decoder;
    const auto& upper = base::ToUpperUtf8(file);
    if (base::EndsWith(upper, ".MP3"))
    {
        auto io = std::make_unique<Mpg123FileInputStream>();
        io->UseStream(file, std::move(stream));
        auto dec = std::make_unique<Mpg123Decoder>();
        if (!dec->Open(std::move(io), SampleType::Float32))
            return false;
        decoder = std::move(dec);
    }
    else if (base::EndsWith(upper, ".OGG") ||
             base::EndsWith(upper, ".WAV") ||
             base::EndsWith(upper, ".FLAC"))
    {
        auto io = std::make_unique<SndFileInputStream>();
        io->UseStream(file, std::move(stream));
        auto dec = std::make_unique<SndFileDecoder>();
        if (!dec->Open(std::move(io)))
            return false;
        decoder = std::move(dec);
    }
    else return false;
    info->sample_rate = decoder->GetSampleRate();
    info->channels    = decoder->GetNumChannels();
    info->frames      = decoder->GetNumFrames();
    info->seconds     = (float)info->frames / (float)info->sample_rate;
    return true;
}

BufferSource::BufferSource(const std::string& name, std::unique_ptr<Buffer> buffer,
                 Format format, SampleType type)
  : mName(name)
  , mId(base::RandomString(10))
  , mInputFormat(format)
  , mBuffer(std::move(buffer))
  , mPort("out")
{
    mOutputFormat.sample_type = type;
}

BufferSource::BufferSource(BufferSource&& other)
  : mName(other.mName)
  , mId(other.mId)
  , mInputFormat(other.mInputFormat)
  , mBuffer(std::move(other.mBuffer))
  , mDecoder(std::move(other.mDecoder))
  , mPort(std::move(other.mPort))
  , mOutputFormat(std::move(other.mOutputFormat))
  , mFramesRead(other.mFramesRead)
{}

BufferSource::~BufferSource() = default;

bool BufferSource::Prepare(const Loader& loader)
{
    std::unique_ptr<Decoder> decoder;
    if (mInputFormat == Format::Mp3)
    {
        auto stream = std::make_unique<Mpg123Buffer>(mName, std::move(mBuffer));
        auto dec = std::make_unique<Mpg123Decoder>();
        if (!dec->Open(std::move(stream), mOutputFormat.sample_type))
            return false;
        decoder = std::move(dec);
    }
    else
    {
        auto stream = std::make_unique<SndFileBuffer>(mName, std::move(mBuffer));
        auto dec = std::make_unique<SndFileDecoder>();
        if (!dec->Open(std::move(stream)))
            return false;
        decoder = std::move(dec);
    }
    audio::Format format;
    format.channel_count = decoder->GetNumChannels();
    format.sample_rate   = decoder->GetSampleRate();
    format.sample_type   = mOutputFormat.sample_type;
    DEBUG("Opened buffer '%1' for reading (%2 frames) %3.", mName, decoder->GetNumFrames(), format);
    mDecoder = std::move(decoder);
    mPort.SetFormat(format);
    mOutputFormat = format;
    return true;
}
void BufferSource::Process(EventQueue& events, unsigned milliseconds)
{
    const auto frame_size = GetFrameSizeInBytes(mOutputFormat);
    const auto frames_per_ms = mOutputFormat.sample_rate / 1000;
    const auto frames_wanted = (unsigned)(frames_per_ms * milliseconds);
    const auto frames_available = mDecoder->GetNumFrames();
    const auto frames = std::min(frames_available - mFramesRead, frames_wanted);

    auto buffer = std::make_shared<VectorBuffer>();
    buffer->SetFormat(mOutputFormat);
    buffer->AllocateBytes(frame_size * frames);
    void* buff = buffer->GetPtr();

    size_t ret = 0;
    if (mOutputFormat.sample_type == SampleType::Float32)
        ret = mDecoder->ReadFrames((float*)buff, frames);
    else if (mOutputFormat.sample_type == SampleType::Int32)
        ret = mDecoder->ReadFrames((int*)buff, frames);
    else if (mOutputFormat.sample_type == SampleType::Int16)
        ret = mDecoder->ReadFrames((short*)buff, frames);

    if (ret != frames)
        WARN("Unexpected number of audio frames. %1 read vs. %2 expected.", ret, frames);

    mFramesRead += ret;
    if (mFramesRead == frames_available)
        DEBUG("File source is done. %1 frames read.", mFramesRead);

    mPort.PushBuffer(buffer);
}

void BufferSource::Shutdown()
{
    mDecoder.reset();
}

bool BufferSource::IsSourceDone() const
{
    return mFramesRead == mDecoder->GetNumFrames();
}

MixerSource::MixerSource(const std::string& name, const Format& format)
  : mName(name)
  , mId(base::RandomString(10))
  , mFormat(format)
  , mOut("out")
{
    mOut.SetFormat(mFormat);
}
MixerSource::MixerSource(MixerSource&& other)
  : mName(other.mName)
  , mId(other.mId)
  , mFormat(other.mFormat)
  , mSources(std::move(other.mSources))
  , mOut(std::move(other.mOut))
{}

Element* MixerSource::AddSourcePtr(std::unique_ptr<Element> source, bool paused)
{
    ASSERT(source->IsSource());
    ASSERT(source->GetNumOutputPorts());
    for (unsigned i=0; i<source->GetNumOutputPorts(); ++i)
    {
        const auto& port = source->GetOutputPort(i);
        ASSERT(port.GetFormat() == mFormat);
    }
    auto* ret = source.get();
    const auto key = source->GetName();

    Source src;
    src.element = std::move(source);
    src.paused  = paused;
    src.delay   = 0.0f;
    mSources[key] = std::move(src);
    DEBUG("Added new MixerSource '%1' (%2) source '%3'.", mName, paused ? "Paused" : "Live", key);
    return ret;
}

void MixerSource::DeleteSource(const std::string& name)
{
    auto it = mSources.find(name);
    if (it == mSources.end())
        return;
    mSources.erase(it);
    DEBUG("Deleted MixerSource '%1' source '%2'.", mName, name);
}

void MixerSource::DelaySource(const std::string& name, float delay)
{
    auto it = mSources.find(name);
    if (it == mSources.end())
        return;
    it->second.delay = delay;
    DEBUG("Delayed MixerSource '%1' source '%2' for %3 seconds.", mName, name, delay);
}
void MixerSource::PauseSource(const std::string& name, bool paused)
{
    auto it = mSources.find(name);
    if (it == mSources.end())
        return;
    it->second.paused = paused;
    DEBUG("MixerSource '%1' source '%2' pause is %3.", mName, name, paused ? "On" : "Off");
}

bool MixerSource::IsSourceDone() const
{
    for (const auto& pair : mSources)
    {
        const auto& source = pair.second;
        if (!source.element->IsSourceDone())
            return false;
    }
    return true;
}

bool MixerSource::Prepare(const Loader& loader)
{
    DEBUG("Prepared MixerSource '%1' with out format '%2'.", mName, mFormat);
    return true;
}

void MixerSource::Update(float dt)
{
    for (auto& pair : mSources)
    {
        auto& source = pair.second;
        source.delay = math::clamp(0.0f, source.delay, source.delay - dt);
        source.element->Update(dt);
    }
}

void MixerSource::Process(EventQueue& events, unsigned milliseconds)
{
    std::vector<BufferHandle> src_buffers;

    for (auto& pair : mSources)
    {
        auto& source  = pair.second;
        auto& element = source.element;
        if (source.delay > 0.0)
            continue;
        else if (source.paused)
            continue;

        element->Process(events, milliseconds);
        for (unsigned i=0;i<element->GetNumOutputPorts(); ++i)
        {
            auto& port = element->GetOutputPort(i);
            BufferHandle buffer;
            if (port.PullBuffer(buffer))
                src_buffers.push_back(buffer);
        }
    }

    if (src_buffers.size() == 0)
    {
        WARN("No source buffers available in MixerSource '%1'.", mName);
        return;
    }
    else if (src_buffers.size() == 1)
    {
        mOut.PushBuffer(src_buffers[0]);
        return;
    }

    BufferHandle ret;
    const float src_gain = 1.0f; // / src_buffers.size();
    const auto& format   = mFormat;

    if (format.sample_type == SampleType::Int32)
        ret = format.channel_count == 1 ? MixBuffers<int, 1>(src_buffers, src_gain)
                                        : MixBuffers<int, 2>(src_buffers, src_gain);
    else if (format.sample_type == SampleType::Float32)
        ret = format.channel_count == 1 ? MixBuffers<float, 1>(src_buffers,src_gain)
                                        : MixBuffers<float, 2>(src_buffers, src_gain);
    else if (format.sample_type == SampleType::Int16)
        ret = format.channel_count == 1 ? MixBuffers<short, 1>(src_buffers, src_gain)
                                        : MixBuffers<short, 2>(src_buffers, src_gain);
    else WARN("Unsupported format '%1'.", format.sample_type);
    mOut.PushBuffer(ret);

    for (auto it = mSources.begin(); it != mSources.end();)
    {
        auto& source  = it->second;
        auto& element = source.element;
        if (element->IsSourceDone())
        {
            SourceDoneEvent event;
            event.mixer = mName;
            event.src   = std::move(element);
            events.push(MakeEvent(std::move(event)));
            DEBUG("MixerSource '%1' source '%2' is done.", mName, it->first);

            it = mSources.erase(it);
        } else ++it;
    }
}

void MixerSource::ReceiveCommand(Command& cmd)
{
    if (auto* ptr = cmd.GetIf<AddSourceCmd>())
        AddSourcePtr(std::move(ptr->src), ptr->paused);
    else if (auto* ptr = cmd.GetIf<DeleteSourceCmd>())
        DeleteSource(ptr->name);
    else if (auto* ptr = cmd.GetIf<DelaySourceCmd>())
        DelaySource(ptr->name, ptr->delay);
    else if (auto* ptr = cmd.GetIf<PauseSourceCmd>())
        PauseSource(ptr->name, ptr->paused);
    else BUG("Unexpected command.");
}

bool MixerSource::DispatchCommand(const std::string& dest, Command& cmd)
{
    // see if the receiver of the command is one of the sources.
    for (auto& pair : mSources)
    {
        auto& element = pair.second.element;
        if (element->GetName() != dest)
            continue;

        element->ReceiveCommand(cmd);
        return true;
    }

    // try to dispatch the command recursively.
    for (auto& pair : mSources)
    {
        auto& element = pair.second.element;
        if (element->DispatchCommand(dest, cmd))
            return true;
    }
    return false;
}

ZeroSource::ZeroSource(const std::string& name, const std::string& id, const Format& format)
  : mName(name)
  , mId(id)
  , mFormat(format)
  , mOut("out")
{
    mOut.SetFormat(format);
}
ZeroSource::ZeroSource(const std::string& name, const Format& format)
  : ZeroSource(name, base::RandomString(10), format)
{}

bool ZeroSource::Prepare(const Loader& loader)
{
    DEBUG("Prepared ZeroSource '%1' with out format '%2'.", mName, mFormat);
    return true;
}

void ZeroSource::Process(EventQueue& events, unsigned milliseconds)
{
    const auto frame_size = GetFrameSizeInBytes(mFormat);
    const auto frames_per_ms = mFormat.sample_rate / 1000;
    const auto frames_wanted = frames_per_ms * milliseconds;

    auto buffer = std::make_shared<VectorBuffer>();
    buffer->SetFormat(mFormat);
    buffer->AllocateBytes(frame_size * frames_wanted);
    mOut.PushBuffer(buffer);
}

SineSource::SineSource(const std::string& name,
                       const std::string& id,
                       const Format& format,
                       unsigned frequency,
                       unsigned millisecs)
  : mName(name)
  , mId(id)
  , mDuration(millisecs)
  , mFrequency(frequency)
  , mPort("out")
{
    mFormat = format;
    mPort.SetFormat(mFormat);
}

SineSource::SineSource(const std::string& name,
                       const Format& format,
                       unsigned frequency,
                       unsigned millisecs)
  : SineSource(name, base::RandomString(10), format, frequency, millisecs)
{}

bool SineSource::Prepare(const Loader& loader)
{
    mPort.SetFormat(mFormat);
    DEBUG("Sine '%1' output format set to %2", mName, mFormat);
    return true;
}

void SineSource::Process(EventQueue& events, unsigned milliseconds)
{
    if (mDuration)
    {
        ASSERT(mDuration > mMilliSecs);
        milliseconds = std::min(milliseconds, mDuration - mMilliSecs);
    }

    const auto frame_size = GetFrameSizeInBytes(mFormat);
    const auto frames_in_millisec = mFormat.sample_rate/1000;
    const auto frames = frames_in_millisec * milliseconds;
    const auto bytes  = frames * frame_size;

    auto buffer = std::make_shared<VectorBuffer>();
    buffer->SetFormat(mFormat);
    buffer->AllocateBytes(bytes);

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

template<typename T>
const T* GetArg(const std::unordered_map<std::string, ElementArg>& args,
                const std::string& arg_name,
                const std::string& elem)
{
    if (const auto* variant = base::SafeFind(args, arg_name))
    {
        if (const auto* value = std::get_if<T>(variant))
            return value;
        else ERROR("Mismatch in audio element '%1' argument '%2' type.", elem, arg_name);
    } else ERROR("Missing audio element '%1' argument '%2'.", elem, arg_name);
    return nullptr;
}

template<typename Type, typename Arg0> inline
std::unique_ptr<Element> Construct(const std::string& name,
                                   const std::string& id,
                                   const Arg0* arg0)
{
    if (!arg0) return nullptr;
    return std::make_unique<Type>(name, id, *arg0);
}
template<typename Type, typename Arg0, typename Arg1> inline
std::unique_ptr<Element> Construct(const std::string& name,
                                   const std::string& id,
                                   const Arg0* arg0,
                                   const Arg1* arg1)
{
    if (!arg0 || !arg1) return nullptr;
    return std::make_unique<Type>(name, id, *arg0, *arg1);
}
template<typename Type, typename Arg0, typename Arg1, typename Arg2> inline
std::unique_ptr<Element> Construct(const std::string& name,
                                   const std::string& id,
                                   const Arg0* arg0,
                                   const Arg1* arg1,
                                   const Arg2* arg2)
{
    if (!arg0 || !arg1 || !arg2) return nullptr;
    return std::make_unique<Type>(name, id, *arg0, *arg1, *arg2);
}

std::vector<std::string> ListAudioElements()
{
    static std::vector<std::string> list;
    if (list.empty())
    {
        list.push_back("SineSource");
        list.push_back("ZeroSource");
        list.push_back("FileSource");
        list.push_back("Resampler");
        list.push_back("Gain");
        list.push_back("Null");
        list.push_back("StereoSplitter");
        list.push_back("StereoJoiner");
        list.push_back("StereoMaker");
        list.push_back("Mixer");
    }
    return list;
}

const ElementDesc* FindElementDesc(const std::string& type)
{
    static std::unordered_map<std::string, ElementDesc> map;
    if (map.empty())
    {
        {
            ElementDesc test;
            test.args["frequency"] = 2000u;
            test.args["millisecs"] = 0u;
            test.args["format"]    = audio::Format {audio::SampleType::Float32, 44100, 2};
            test.output_ports.push_back({"out"});
            map["SineSource"] = test;
        }
        {
            ElementDesc zero;
            zero.args["format"] = audio::Format{audio::SampleType::Float32, 44100, 2 };
            zero.output_ports.push_back({"out"});
            map["ZeroSource"] = zero;
        }
        {
            ElementDesc file;
            file.args["file"] = std::string();
            file.args["type"] = audio::SampleType::Float32;
            file.output_ports.push_back({"out"});
            map["FileSource"] = file;
        }
        {
            ElementDesc resampler;
            resampler.args["sample_rate"] = 44100u;
            resampler.input_ports.push_back({"in"});
            resampler.output_ports.push_back({"out"});
            map["Resampler"] = resampler;
        }
        {
            ElementDesc gain;
            gain.args["gain"] = 1.0f;
            gain.input_ports.push_back({"in"});
            gain.output_ports.push_back({"out"});
            map["Gain"] = gain;
        }
        {
            ElementDesc null;
            null.input_ports.push_back({"in"});
            map["Null"] =  null;
        }
        {
            ElementDesc splitter;
            splitter.input_ports.push_back({"in"});
            splitter.output_ports.push_back({"left"});
            splitter.output_ports.push_back({"right"});
            map["StereoSplitter"] = splitter;
        }
        {
            ElementDesc joiner;
            joiner.input_ports.push_back({"left"});
            joiner.input_ports.push_back({"right"});
            joiner.output_ports.push_back({"out"});
            map["StereoJoiner"] = joiner;
        }
        {
            ElementDesc maker;
            maker.input_ports.push_back({"in"});
            maker.output_ports.push_back({"out"});
            maker.args["channel"] = StereoMaker::Channel::Both;
            map["StereoMaker"] = maker;
        }
        {
            ElementDesc mixer;
            mixer.args["num_srcs"] = 2u;
            mixer.input_ports.push_back({"in0"});
            mixer.input_ports.push_back({"in1"});
            mixer.output_ports.push_back({"out"});
            map["Mixer"] = mixer;
        }
    }
    return base::SafeFind(map, type);
}

std::unique_ptr<Element> CreateElement(const ElementCreateArgs& desc)
{
    const auto& args = desc.args;
    const auto& name = desc.type + "/" + desc.name;
    if (desc.type == "StereoMaker")
        return Construct<StereoMaker>(desc.name, desc.id,
            GetArg<StereoMaker::Channel>(args, "channel", name));
    else if (desc.type == "StereoJoiner")
        return std::make_unique<StereoJoiner>(desc.name, desc.id);
    else if (desc.type == "StereoSplitter")
        return std::make_unique<StereoSplitter>(desc.name, desc.id);
    else if (desc.type == "Null")
        return std::make_unique<Null>(desc.name, desc.id);
    else if (desc.type == "Mixer")
        return Construct<Mixer>(desc.name, desc.id,
            GetArg<unsigned>(args, "num_srcs", name));
    else if (desc.type == "Fade")
        return Construct<Fade>(desc.name, desc.id,
            GetArg<float>(args, "duration", name),
            GetArg<Fade::Effect>(args, "effect", name));
    else if (desc.type == "Gain")
        return Construct<Gain>(desc.name, desc.id, 
            GetArg<float>(args, "gain", name));
    else if (desc.type == "Resampler")
        return Construct<Resampler>(desc.name, desc.id, 
            GetArg<unsigned>(args, "sample_rate", name));
    else if (desc.type == "FileSource")
        return Construct<FileSource>(desc.name, desc.id,
            GetArg<std::string>(args, "file", name),
            GetArg<SampleType>(args, "type", name));
    else if (desc.type == "ZeroSource")
        return Construct<ZeroSource>(desc.name, desc.id,
            GetArg<Format>(args, "format", name));
    else if (desc.type == "SineSource")
        return Construct<SineSource>(desc.name, desc.id,
            GetArg<Format>(args, "format", name),
            GetArg<unsigned>(args, "frequency", name),
            GetArg<unsigned>(args, "millisecs", name));
    else BUG("Unsupported audio Element construction.");
    return nullptr;
}


} // namespace

