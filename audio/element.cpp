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
#include "base/trace.h"
#include "audio/element.h"
#include "audio/sndfile.h"
#include "audio/mpg123.h"
#include "audio/loader.h"
#include "audio/algo.h"

namespace audio
{

Playlist::Playlist(const std::string& name, const std::string& id, const std::vector<PortDesc>& srcs)
  : mName(name)
  , mId(id)
  , mOut("out")
{
    for (const auto& desc : srcs)
    {
        SingleSlotPort p(desc.name);
        mSrcs.push_back(std::move(p));
    }
}
Playlist::Playlist(const std::string& name, const std::vector<PortDesc>& srcs)
  : Playlist(name, base::RandomString(10), srcs)
{}

bool Playlist::Prepare(const Loader& loader, const PrepareParams& params)
{
    // all input ports should have the same format,
    // otherwise no deal! the user can use a re-sampler
    // to resample/convert the streams in order to make
    // sure that they all have matching format spec.
    const auto& master_format = mSrcs[0].GetFormat();
    if (!IsValid(master_format))
    {
        ERROR("Audio playlist input port format is invalid. [elem=%1, port=%2]", mName, mSrcs[0].GetName());
        return false;
    }

    for (const auto& src : mSrcs)
    {
        const auto& format = src.GetFormat();
        const auto& name   = src.GetName();
        if (format != master_format)
        {
            ERROR("Audio playlist port is incompatible with other ports. [elem=%1, port=%2, format=%3]", mName, name, format);
            return false;
        }
    }
    DEBUG("Audio playlist prepared successfully. [elem=%1, srcs=%2, output=%3]", mName, mSrcs.size(), master_format);
    mOut.SetFormat(master_format);
    return true;
}

void Playlist::Process(Allocator& allocator, EventQueue& events, unsigned milliseconds)
{
    TRACE_SCOPE("Playlist");

    if (mSrcIndex == mSrcs.size())
        return;
    BufferHandle buffer;
    if (!mSrcs[mSrcIndex].PullBuffer(buffer))
        return;

    // if all the sources of audio buffers on the current
    // input port are done based on the buffer info tags
    // then we move onto to next source port.
    bool all_sources_done = true;
    for (size_t i=0; i<buffer->GetNumInfoTags(); ++i)
    {
        const auto& tag = buffer->GetInfoTag(i);
        if (!tag.element.source)
            continue;
        if (!tag.element.source_done)
            all_sources_done = false;
    }
    if (all_sources_done)
        mSrcIndex++;

    mOut.PushBuffer(buffer);
}

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
void StereoJoiner::Join(Allocator& allocator, BufferHandle left, BufferHandle right)
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

Splitter::Splitter(const std::string& name, unsigned num_outs)
  : Splitter(name, base::RandomString(10), num_outs)
{}

Splitter::Splitter(const std::string& name, const std::string& id, unsigned num_outs)
  : mName(name)
  , mId(id)
  , mIn("in")
{
    ASSERT(num_outs);
    for (unsigned i=0; i<num_outs; ++i)
    {
        SingleSlotPort port(base::FormatString("out%1", i));
        mOuts.push_back(std::move(port));
    }
}
Splitter::Splitter(const std::string& name, const std::string& id, const std::vector<PortDesc>& outs)
  : mName(name)
  , mId(id)
  , mIn("in")
{
    for (const auto& out : outs)
    {
        SingleSlotPort port(out.name);
        mOuts.push_back(std::move(port));
    }
}

bool Splitter::Prepare(const Loader& loader, const PrepareParams& params)
{
    const auto& format = mIn.GetFormat();
    if (!IsValid(format))
    {
        ERROR("Audio splitter input format is invalid. [elem=%1, port=%2]", mName, mIn.GetName());
        return false;
    }
    for (auto& out : mOuts)
    {
        out.SetFormat(format);
    }
    DEBUG("Audio splitter prepared successfully. [elem=%1, format=%2]", format);
    return true;
}
void Splitter::Process(Allocator& allocator, EventQueue& events, unsigned milliseconds)
{
    TRACE_SCOPE("Splitter");

    BufferHandle src_buffer;
    if (!mIn.PullBuffer(src_buffer))
        return;

    for (auto& out : mOuts)
    {
        BufferHandle out_buffer = allocator.Allocate(src_buffer->GetByteSize());
        out_buffer->SetFormat(out_buffer->GetFormat());
        out_buffer->CopyData(*src_buffer);
        out_buffer->CopyInfoTags(*src_buffer);
        out.PushBuffer(out_buffer);
    }
}


bool Queue::Prepare(const Loader& loader, const PrepareParams& params)
{
    const auto format = mIn.GetFormat();
    mOut.SetFormat(format);
    DEBUG("Audio queue element prepared successfully. [elem=%1, output=%2]", mName, format);
    return true;
}

void Queue::Process(Allocator& allocator, EventQueue& events, unsigned int milliseconds)
{
    TRACE_SCOPE("Queue");

    BufferHandle buffer;
    if (mIn.PullBuffer(buffer))
        mQueue.push(std::move(buffer));

    if (mQueue.empty())
        return;
    if (mOut.IsFull())
        return;

    if (mOut.PushBuffer(mQueue.front()))
        mQueue.pop();
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

Mixer::Mixer(const std::string& name, const std::string& id,
             const std::vector<PortDesc>& srcs)
  : mName(name)
  , mId(id)
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

Delay::Delay(const std::string& name, const std::string& id, unsigned delay)
  : mName(name)
  , mId(id)
  , mIn("in")
  , mOut("out")
  , mDelay(delay)
{}
Delay::Delay(const std::string& name, unsigned delay)
  : Delay(name, base::RandomString(10), delay)
{}

bool Delay::Prepare(const Loader& loader, const PrepareParams& params)
{
    const auto& format = mIn.GetFormat();
    mOut.SetFormat(format);
    DEBUG("Audio delay element prepared successfully. [elem=%1, output=%2]", mName, format);
    return true;
}
void Delay::Process(Allocator& allocator, EventQueue& events, unsigned milliseconds)
{
    TRACE_SCOPE("Delay");

    if (mDelay > 0u)
        return;
    BufferHandle buffer;
    if (mIn.PullBuffer((buffer)))
        mOut.PushBuffer(buffer);
}

void Delay::Advance(unsigned milliseconds)
{
    const auto min = std::min(mDelay, milliseconds);
    mDelay -= min;
}

Effect::Effect(const std::string& name, const std::string& id, unsigned time, unsigned duration, Kind effect)
  : mName(name)
  , mId(id)
  , mIn("in")
  , mOut("out")
{
    SetEffect(effect, time, duration);
}
Effect::Effect(const std::string& name, unsigned time, unsigned duration, Kind effect)
  : Effect(name, base::RandomString(10), time, duration, effect)
{}
Effect::Effect(const std::string& name)
  : mName(name)
  , mId(base::RandomString(10))
  , mIn("in")
  , mOut("out")
{}

bool Effect::Prepare(const Loader& loader, const PrepareParams& params)
{
    const auto& format = mIn.GetFormat();
    mSampleRate = format.sample_rate;
    mOut.SetFormat(format);
    DEBUG("Audio effect prepared successfully. [name=%1, output=%2]", mName, format);
    return true;
}

void Effect::Process(Allocator& allocator, EventQueue& events, unsigned milliseconds)
{
    TRACE_SCOPE("Effect");

    BufferHandle buffer;
    if (!mIn.PullBuffer(buffer))
        return;

    const auto& format = mIn.GetFormat();
    if (format.sample_type == SampleType::Int32)
        format.channel_count == 1 ? FadeInOut<int, 1>(buffer) : FadeInOut<int, 2>(buffer);
    else if (format.sample_type == SampleType::Float32)
        format.channel_count == 1 ? FadeInOut<float, 1>(buffer) : FadeInOut<float, 2>(buffer);
    else if (format.sample_type == SampleType::Int16)
        format.channel_count == 1 ? FadeInOut<short, 1>(buffer) : FadeInOut<short, 2>(buffer);
    else WARN("Audio effect input buffer has incompatible format. [elem=%1, format=%2]", mName, format.sample_type);
}

void Effect::ReceiveCommand(Command& cmd)
{
    if (auto* ptr = cmd.GetIf<SetEffectCmd>())
        SetEffect(ptr->effect, ptr->time, ptr->duration);
    else BUG("Unexpected command.");
}

void Effect::SetEffect(Kind effect, unsigned time, unsigned duration)
{
    mEffect        = effect;
    mDuration      = duration;
    mStartTime     = time;
    mSampleTime    = 0.0f;
    DEBUG("Set audio effect. [elem=%1, effect=%2, time=%3, duration=%4]",
          mName, effect, time/1000.0f, duration/1000.0f);
}

template<typename DataType, unsigned ChannelCount>
void Effect::FadeInOut(BufferHandle buffer)
{
    // take a shortcut when possible
    if (mSampleTime >= (mStartTime + mDuration))
    {
        if (mEffect == Kind::FadeOut)
            std::memset(buffer->GetPtr(), 0, buffer->GetByteSize());

        mOut.PushBuffer(buffer);
        return;
    }

    if (mEffect == Kind::FadeIn)
        mSampleTime = FadeBuffer<DataType, ChannelCount>(buffer, mSampleTime, mStartTime, mDuration, true);
    else if (mEffect == Kind::FadeOut)
        mSampleTime = FadeBuffer<DataType, ChannelCount>(buffer, mSampleTime, mStartTime, mDuration, false);
    else BUG("???");

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

// raw PCM data blob
struct FileSource::PCMBuffer {
    bool complete = false;
    unsigned rate        = 0;
    unsigned channels    = 0;
    unsigned frame_count = 0;
    std::vector<char> pcm;
    audio::SampleType type = audio::SampleType::Float32;
};

// A pass through decoder that does no actual decoding
// but just reads a PCM buffer
class FileSource::PCMDecoder : public audio::Decoder
{
public:
    PCMDecoder(const std::shared_ptr<const PCMBuffer>& pcm)
      : mBuffer(pcm)
    {}
    virtual unsigned GetSampleRate() const override
    { return mBuffer->rate; }
    virtual unsigned GetNumChannels() const override
    { return mBuffer->channels; }
    virtual unsigned GetNumFrames() const override
    { return mBuffer->frame_count; }
    virtual size_t ReadFrames(float* ptr, size_t frames) override
    {
        ASSERT(mBuffer->type == audio::SampleType::Float32);
        return ReadFrames<float>(ptr, frames);
    }
    virtual size_t ReadFrames(short* ptr, size_t frames) override
    {
        ASSERT(mBuffer->type == audio::SampleType::Int16);
        return ReadFrames<short>(ptr, frames);
    }
    virtual size_t ReadFrames(int* ptr, size_t frames) override
    {
        ASSERT(mBuffer->type == audio::SampleType::Int32);
        return ReadFrames<int>(ptr, frames);
    }
    virtual void Reset() override
    {
        mFrame = 0;
    }
private:
    template<typename T>
    size_t ReadFrames(T* ptr, size_t frames)
    {
        const auto frame_size = sizeof(T) * mBuffer->channels;
        const auto byte_count = frame_size * frames;
        const auto byte_offset = frame_size * mFrame;
        ASSERT(byte_offset + byte_count <= mBuffer->pcm.size());
        std::memcpy(ptr, &mBuffer->pcm[byte_offset], byte_count);
        mFrame += frames;
        return frames;
    }
private:
    std::shared_ptr<const PCMBuffer> mBuffer;
    std::uint64_t mFrame = 0;
};

// static
FileSource::PCMCache FileSource::pcm_cache;
// static
FileSource::FileInfoCache FileSource::file_info_cache;

FileSource::FileSource(const std::string& name, const std::string& file, SampleType type, unsigned loops)
  : mName(name)
  , mId(base::RandomString(10))
  , mFile(file)
  , mPort("out")
  , mLoopCount(loops)
{
    mFormat.sample_type = type;
}

FileSource::FileSource(const std::string& name, const std::string& id, const std::string& file, SampleType type, unsigned loops)
  : mName(name)
  , mId(id)
  , mFile(file)
  , mPort("out")
  , mLoopCount(loops)
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

bool FileSource::Prepare(const Loader& loader, const PrepareParams& params)
{
    std::shared_ptr<PCMBuffer> cached_pcm_buffer;
    std::unique_ptr<Decoder> decoder;

    const bool enable_pcm_caching = params.enable_pcm_caching && mEnablePcmCaching;
    if (enable_pcm_caching)
    {
        auto it = pcm_cache.find(mId);
        if (it != pcm_cache.end())
            cached_pcm_buffer = it->second;
    }

    // if we have a file info available (discovered through preload)
    // then we don't need the actual codec in order to prepare
    // the FileSource. Rather we can use the cached information
    // (assuming it's correct) and then defer the decoder open to
    // background task in the thread pool that is waited on when
    // the first call to process audio data is done.
    const auto* info = base::SafeFind(file_info_cache, mFile);

    // if there already exists a complete PCM blob for the
    // contents of this (as identified by ID) FileSource
    // element's audio file then we can use that data directly
    // and not perform any duplicate mp3/ogg/flac decoding.
    if (cached_pcm_buffer && cached_pcm_buffer->complete)
    {
        decoder = std::make_unique<PCMDecoder>(cached_pcm_buffer);
        DEBUG("Using a cached PCM audio buffer. [elem=%1, file='%2', id=%3]", mName, mFile, mId);
    }
    else
    {
        auto source = loader.OpenAudioStream(mFile, mIOStrategy, mEnableFileCaching);
        if (!source)
            return false;

        const auto& upper = base::ToUpperUtf8(mFile);
        if (base::EndsWith(upper, ".MP3"))
        {
            class OpenDecoderTask : public base::ThreadTask {
            public:
                OpenDecoderTask(std::unique_ptr<Mpg123Decoder> decoder,
                                std::shared_ptr<const SourceStream> stream,
                                SampleType type)
                  : mDecoder(std::move(decoder))
                  , mStream(std::move(stream))
                  , mSampleType(type)
                {}
                virtual void DoTask() override
                {
                    if (!mDecoder->Open(mStream, mSampleType))
                    {
                        mFlags.set(Flags::Error, true);
                    }
                }
                virtual void GetValue(const char* key, void* ptr) override
                {
                    ASSERT(!std::strcmp(key, "decoder"));
                    using DecoderPtr = std::unique_ptr<Decoder>;
                    auto* foo = static_cast<DecoderPtr*>(ptr);
                    *foo = std::move(mDecoder);
                }
            private:
                std::unique_ptr<Mpg123Decoder> mDecoder;
                std::shared_ptr<const SourceStream> mStream;
                SampleType mSampleType;
            };

            auto mpg123_decoder = std::make_unique<Mpg123Decoder>();
            if (info && base::HaveGlobalThreadPool())
            {
                auto* pool = base::GetGlobalThreadPool();
                auto task = std::make_unique<OpenDecoderTask>(std::move(mpg123_decoder), std::move(source), mFormat.sample_type);
                mOpenDecoderTask = pool->SubmitTask(std::move(task), base::ThreadPool::AnyWorkerThreadID);
                DEBUG("Submitted new audio decoder open task. [file='%1']", mFile);
            }
            else
            {
                if (!mpg123_decoder->Open(source, mFormat.sample_type))
                    return false;
                decoder = std::move(mpg123_decoder);
            }
        }
        else if (base::EndsWith(upper, ".OGG") ||
                 base::EndsWith(upper, ".WAV") ||
                 base::EndsWith(upper, ".FLAC"))
        {
            class OpenDecoderTask : public base::ThreadTask {
            public:
                OpenDecoderTask(std::unique_ptr<SndFileDecoder> decoder,
                                std::shared_ptr<const SourceStream> stream)
                  : mDecoder(std::move(decoder))
                  , mStream(std::move(stream))
                {}
                virtual void DoTask() override
                {
                    if (!mDecoder->Open(mStream))
                    {
                        mFlags.set(Flags::Error, true);
                    }
                }
                virtual void GetValue(const char* key, void* ptr) override
                {
                    ASSERT(!std::strcmp(key, "decoder"));
                    using DecoderPtr = std::unique_ptr<Decoder>;
                    auto* foo = static_cast<DecoderPtr*>(ptr);
                    *foo = std::move(mDecoder);
                }
            private:
                std::unique_ptr<SndFileDecoder> mDecoder;
                std::shared_ptr<const SourceStream> mStream;
            };

            auto snd_file_decoder = std::make_unique<SndFileDecoder>();
            if (info && base::HaveGlobalThreadPool())
            {
                auto* pool = base::GetGlobalThreadPool();
                auto task = std::make_unique<OpenDecoderTask>(std::move(snd_file_decoder), std::move(source));
                mOpenDecoderTask = pool->SubmitTask(std::move(task), base::ThreadPool::AnyWorkerThreadID);
                DEBUG("Submitted new audio decoder open task. [file='%1']", mFile);
            }
            else
            {
                if (!snd_file_decoder->Open(source))
                    return false;
                decoder = std::move(snd_file_decoder);
            }
        }
        else
        {
            ERROR("Audio file source file format is unsupported. [elem=%1, file='%2']", mName, mFile);
            return false;
        }
        ASSERT(mOpenDecoderTask.IsValid() || decoder);

        if (!cached_pcm_buffer && enable_pcm_caching)
        {
            cached_pcm_buffer = std::make_shared<PCMBuffer>();
            cached_pcm_buffer->complete    = false;
            cached_pcm_buffer->channels    = info ? info->channels : decoder->GetNumChannels();
            cached_pcm_buffer->rate        = info ? info->sample_rate : decoder->GetSampleRate();
            cached_pcm_buffer->frame_count = info ? info->frames : decoder->GetNumFrames();
            cached_pcm_buffer->type        = mFormat.sample_type;
            pcm_cache[mId] = cached_pcm_buffer;
            mPCMBuffer = cached_pcm_buffer;
        }
    }
    Format format;
    format.channel_count = info ? info->channels : decoder->GetNumChannels();
    format.sample_rate   = info ? info->sample_rate : decoder->GetSampleRate();
    format.sample_type   = mFormat.sample_type;

    DEBUG("Audio file source prepared successfully. [elem=%1, file='%2', format=%3]", mName, mFile,format);
    if (info == nullptr)
    {
        FileInfo fileinfo;
        fileinfo.sample_rate = decoder->GetSampleRate();
        fileinfo.channels    = decoder->GetNumChannels();
        fileinfo.frames      = decoder->GetNumFrames();
        file_info_cache[mFile] = fileinfo;
        DEBUG("Saved audio file source file info. [file='%1']", mFile);
    }

    mDecoder = std::move(decoder);
    mPort.SetFormat(format);
    mFormat = format;
    return true;
}

void FileSource::Process(Allocator& allocator, EventQueue& events, unsigned milliseconds)
{
    TRACE_SCOPE("FileSource");

    if (mOpenDecoderTask.IsValid())
    {
        // wait here? maybe not... generate silence ?
        if (!mOpenDecoderTask.IsComplete())
            return;

        auto task = mOpenDecoderTask.GetTask();
        if (task->Failed())
        {
            ERROR("Failed to open decoder on audio stream. [elem=%1, file='%2']", mName, mFile);
            return;
        }
        task->GetValue("decoder", &mDecoder);
        ASSERT(mDecoder);

        mOpenDecoderTask.Clear();
        DEBUG("Audio decoder open task is complete.");
    }

    const auto frame_size = GetFrameSizeInBytes(mFormat);
    const auto frames_per_ms = mFormat.sample_rate / 1000;
    const auto frames_wanted = (unsigned)(frames_per_ms * milliseconds);
    const auto frames_available = mDecoder->GetNumFrames();
    const auto frames = std::min(frames_available - mFramesRead, frames_wanted);

    auto buffer = allocator.Allocate(frame_size * frames);
    buffer->SetFormat(mFormat);
    buffer->SetByteSize(frame_size * frames);
    void* buff = buffer->GetPtr();

    size_t ret = 0;
    TRACE_BLOCK("Decode",
        if (mFormat.sample_type == SampleType::Float32)
            ret = mDecoder->ReadFrames((float*)buff, frames);
        else if (mFormat.sample_type == SampleType::Int32)
            ret = mDecoder->ReadFrames((int*)buff, frames);
        else if (mFormat.sample_type == SampleType::Int16)
            ret = mDecoder->ReadFrames((short*)buff, frames);
        else BUG("Missing sampletype");
    );

    if (mPCMBuffer && !mPCMBuffer->complete)
    {
        const auto old_size = mPCMBuffer->pcm.size();
        const auto new_size = old_size + ret * frame_size;
        mPCMBuffer->pcm.resize(new_size);
        std::memcpy(&mPCMBuffer->pcm[old_size], buff, ret * frame_size);
    }

    if (ret != frames)
    {
        WARN("Unexpected number of audio frames decoded. [elem=%1, expected=%2, decoded=%3]", mName, frames, ret);
    }

    mFramesRead += ret;
    if (mFramesRead == frames_available)
    {
        if (mPCMBuffer)
        {
            const auto size = mPCMBuffer->pcm.size();
            mPCMBuffer->complete = true;
            DEBUG("Audio PCM buffer is complete. [elem=%1, file='%2', id=%3, bytes=%4]", mName, mFile, mId, size);
        }

        if (++mPlayCount != mLoopCount)
        {
            if (mPCMBuffer)
            {
                mDecoder = std::make_unique<PCMDecoder>(mPCMBuffer);
            }
            mDecoder->Reset();
            mFramesRead = 0;
            DEBUG("Audio file source was reset for looped playback. [elem=%1, file='%2', count=%3]", mName, mFile, mPlayCount+1);
        }
        else
        {
            DEBUG("Audio file source is done. [elem=%1, file='%2']", mName, mFile);
        }
        mPCMBuffer.reset();
    }

    mPort.PushBuffer(buffer);
}

void FileSource::Shutdown()
{
    mDecoder.reset();
}

bool FileSource::IsSourceDone() const
{
    if (mOpenDecoderTask.IsValid())
        return false;
    if (!mDecoder)
        return true;

    return mFramesRead == mDecoder->GetNumFrames();
}

// static
bool FileSource::ProbeFile(const std::string& file, FileInfo* info)
{
    auto stream = audio::OpenFileStream(file);
    if (!stream)
        return false;

    std::unique_ptr<Decoder> decoder;
    const auto& upper = base::ToUpperUtf8(file);
    if (base::EndsWith(upper, ".MP3"))
    {
        auto dec = std::make_unique<Mpg123Decoder>();
        if (!dec->Open(stream, SampleType::Float32))
            return false;
        decoder = std::move(dec);
    }
    else if (base::EndsWith(upper, ".OGG") ||
             base::EndsWith(upper, ".WAV") ||
             base::EndsWith(upper, ".FLAC"))
    {
        auto dec = std::make_unique<SndFileDecoder>();
        if (!dec->Open(stream))
            return false;
        decoder = std::move(dec);
    }
    else return false;
    info->sample_rate = decoder->GetSampleRate();
    info->channels    = decoder->GetNumChannels();
    info->frames      = decoder->GetNumFrames();
    info->seconds     = (float)info->frames / (float)info->sample_rate;
    info->bytes       = stream->GetSize();
    return true;
}
// static
void FileSource::ClearCache()
{
    pcm_cache.clear();
    file_info_cache.clear();
}

StreamSource::StreamSource(const std::string& name, std::shared_ptr<const SourceStream> buffer,
                           Format format, SampleType type)
  : mName(name)
  , mId(base::RandomString(10))
  , mInputFormat(format)
  , mBuffer(std::move(buffer))
  , mPort("out")
{
    mOutputFormat.sample_type = type;
}

StreamSource::StreamSource(StreamSource&& other)
  : mName(other.mName)
  , mId(other.mId)
  , mInputFormat(other.mInputFormat)
  , mBuffer(std::move(other.mBuffer))
  , mDecoder(std::move(other.mDecoder))
  , mPort(std::move(other.mPort))
  , mOutputFormat(std::move(other.mOutputFormat))
  , mFramesRead(other.mFramesRead)
{}

StreamSource::~StreamSource() = default;

bool StreamSource::Prepare(const Loader& loader, const PrepareParams& params)
{
    std::unique_ptr<Decoder> decoder;
    if (mInputFormat == Format::Mp3)
    {
        auto dec = std::make_unique<Mpg123Decoder>();
        if (!dec->Open(mBuffer, mOutputFormat.sample_type))
            return false;
        decoder = std::move(dec);
    }
    else
    {
        auto dec = std::make_unique<SndFileDecoder>();
        if (!dec->Open(mBuffer))
            return false;
        decoder = std::move(dec);
    }
    audio::Format format;
    format.channel_count = decoder->GetNumChannels();
    format.sample_rate   = decoder->GetSampleRate();
    format.sample_type   = mOutputFormat.sample_type;
    DEBUG("Audio buffer source prepared successfully. [elem=%1, output=%2]", mName, format);
    mDecoder = std::move(decoder);
    mPort.SetFormat(format);
    mOutputFormat = format;
    return true;
}
void StreamSource::Process(Allocator& allocator, EventQueue& events, unsigned milliseconds)
{
    const auto frame_size = GetFrameSizeInBytes(mOutputFormat);
    const auto frames_per_ms = mOutputFormat.sample_rate / 1000;
    const auto frames_wanted = (unsigned)(frames_per_ms * milliseconds);
    const auto frames_available = mDecoder->GetNumFrames();
    const auto frames = std::min(frames_available - mFramesRead, frames_wanted);

    auto buffer = allocator.Allocate(frame_size * frames);
    buffer->SetFormat(mOutputFormat);
    buffer->SetByteSize(frame_size * frames);
    void* buff = buffer->GetPtr();

    size_t ret = 0;
    if (mOutputFormat.sample_type == SampleType::Float32)
        ret = mDecoder->ReadFrames((float*)buff, frames);
    else if (mOutputFormat.sample_type == SampleType::Int32)
        ret = mDecoder->ReadFrames((int*)buff, frames);
    else if (mOutputFormat.sample_type == SampleType::Int16)
        ret = mDecoder->ReadFrames((short*)buff, frames);

    if (ret != frames)
    {
        WARN("Unexpected number of audio frames decoded. [elem=%1, expected=%2, decoded=%3]", mName, frames, ret);
    }

    mFramesRead += ret;
    if (mFramesRead == frames_available)
    {
        DEBUG("Audio buffer source is done. [elem=%1]", mName);
    }

    mPort.PushBuffer(buffer);
}

void StreamSource::Shutdown()
{
    mDecoder.reset();
}

bool StreamSource::IsSourceDone() const
{
    return mFramesRead == mDecoder->GetNumFrames();
}

void MixerSource::FadeIn::Apply(BufferHandle buffer)
{
    const auto& format = buffer->GetFormat();
    if (format.sample_type == SampleType::Int32)
        format.channel_count == 1 ? ApplyFadeIn<int, 1>(buffer) : ApplyFadeIn<int, 2>(buffer);
    else if (format.sample_type == SampleType::Float32)
        format.channel_count == 1 ? ApplyFadeIn<float, 1>(buffer) : ApplyFadeIn<float, 2>(buffer);
    else if (format.sample_type == SampleType::Int16)
        format.channel_count == 1 ? ApplyFadeIn<short, 1>(buffer) : ApplyFadeIn<short, 2>(buffer);
    else WARN("Audio mixer fade-in effect input buffer has unsupported format. [format=%1]", format.sample_type);
}
template<typename DataType, unsigned ChannelCount>
void MixerSource::FadeIn::ApplyFadeIn(BufferHandle buffer)
{
    mTime = FadeBuffer<DataType, ChannelCount>(buffer, mTime, 0.0f, mDuration, true);
}

void MixerSource::FadeOut::Apply(BufferHandle buffer)
{
    const auto& format = buffer->GetFormat();
    if (format.sample_type == SampleType::Int32)
        format.channel_count == 1 ? ApplyFadeOut<int, 1>(buffer) : ApplyFadeOut<int, 2>(buffer);
    else if (format.sample_type == SampleType::Float32)
        format.channel_count == 1 ? ApplyFadeOut<float, 1>(buffer) : ApplyFadeOut<float, 2>(buffer);
    else if (format.sample_type == SampleType::Int16)
        format.channel_count == 1 ? ApplyFadeOut<short, 1>(buffer) : ApplyFadeOut<short, 2>(buffer);
    else WARN("Audio mixer fade-out effect input buffer has unsupported format. [format=%1]", format.sample_type);
}
template<typename DataType, unsigned ChannelCount>
void MixerSource::FadeOut::ApplyFadeOut(BufferHandle buffer)
{
    mTime = FadeBuffer<DataType, ChannelCount>(buffer, mTime, 0.0f, mDuration, false);
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
    mSources[key] = std::move(src);
    DEBUG("Add audio mixer source object. [elem=%1, key=%2, paused=%3]", mName, key, paused);
    return ret;
}

void MixerSource::CancelSourceCommands(const std::string& name)
{
    for (size_t i=0; i<mCommands.size();)
    {
        bool match = false;
        auto& cmd = mCommands[i];
        std::visit([&match, &name](auto& variant_value) {
            if (variant_value.name == name)
                match = true;
        }, cmd);
        if (match)
        {
            const auto last = mCommands.size()-1;
            std::swap(mCommands[i], mCommands[last]);
            mCommands.pop_back();
        }
        else ++i;
    }
}

void MixerSource::DeleteSource(const std::string& name)
{
    auto it = mSources.find(name);
    if (it == mSources.end())
        return;
    mSources.erase(it);
    DEBUG("Delete audio mixer source. [elem=%1, source=%2]", mName, name);
}

void MixerSource::DeleteSources()
{
    mSources.clear();
    DEBUG("Delete all audio mixer sources. [elem=%1]", mName);
}

void MixerSource::PauseSource(const std::string& name, bool paused)
{
    auto it = mSources.find(name);
    if (it == mSources.end())
        return;
    it->second.paused = paused;
    DEBUG("Pause audio mixer source. [elem=%1, source=%2, pause=%3]", mName, name, paused);
}

void MixerSource::SetSourceEffect(const std::string& name, std::unique_ptr<Effect> effect)
{
    auto it = mSources.find(name);
    if (it == mSources.end())
        return;
    DEBUG("Set audio mixer source effect. [elem=%1, source=%2, effect=%3]", mName, name, effect->GetName());
    it->second.effect = std::move(effect);
}

bool MixerSource::IsSourceDone() const
{
    if (mNeverDone) return false;

    for (const auto& pair : mSources)
    {
        const auto& source = pair.second;
        if (!source.element->IsSourceDone())
            return false;
    }
    return true;
}

bool MixerSource::Prepare(const Loader& loader, const PrepareParams& params)
{
    DEBUG("Audio mixer successfully prepared. [elem=%1, output=%2]", mName, mFormat);
    return true;
}

void MixerSource::Advance(unsigned int ms)
{
    for (size_t i=0; i<mCommands.size();)
    {
        bool command_done = false;
        auto& cmd = mCommands[i];
        std::visit([this, ms, &command_done](auto& variant_value) {
            variant_value.millisecs -= std::min(ms, variant_value.millisecs);
            if (!variant_value.millisecs)
            {
                command_done = true;
                ExecuteCommand(variant_value);
            }
        }, cmd);

        if (command_done)
        {
            const auto last = mCommands.size()-1;
            std::swap(mCommands[i], mCommands[last]);
            mCommands.pop_back();
        }
        else ++i;
    }

    for (auto& pair : mSources)
    {
        auto& source  = pair.second;
        source.element->Advance(ms);
    }
}

void MixerSource::Process(Allocator& allocator, EventQueue& events, unsigned milliseconds)
{
    TRACE_SCOPE("MixerSource");

    std::vector<BufferHandle> src_buffers;

    for (auto& pair : mSources)
    {
        auto& source  = pair.second;
        auto& element = source.element;
        if (source.paused || source.element->IsSourceDone())
            continue;

        element->Process(allocator, events, milliseconds);
        for (unsigned i=0;i<element->GetNumOutputPorts(); ++i)
        {
            auto& port = element->GetOutputPort(i);
            BufferHandle buffer;
            if (port.PullBuffer(buffer))
            {
                if (source.effect)
                    source.effect->Apply(buffer);
                src_buffers.push_back(buffer);
            }
        }
    }
    RemoveDoneEffects(events);
    RemoveDoneSources(events);

    if (src_buffers.size() == 0)
        return;
    else if (src_buffers.size() == 1)
    {
        mOut.PushBuffer(src_buffers[0]);
        return;
    }

    BufferHandle ret;
    const float src_gain = 1.0f; // / src_buffers.size();
    const auto& format   = mFormat;

    {
        TRACE_SCOPE("MixBuffers");
        if (format.sample_type == SampleType::Int32)
            ret = format.channel_count == 1 ? MixBuffers<int, 1>(src_buffers, src_gain)
                                            : MixBuffers<int, 2>(src_buffers, src_gain);
        else if (format.sample_type == SampleType::Float32)
            ret = format.channel_count == 1 ? MixBuffers<float, 1>(src_buffers,src_gain)
                                            : MixBuffers<float, 2>(src_buffers, src_gain);
        else if (format.sample_type == SampleType::Int16)
            ret = format.channel_count == 1 ? MixBuffers<short, 1>(src_buffers, src_gain)
                                            : MixBuffers<short, 2>(src_buffers, src_gain);
        else WARN("Audio mixer output format is unsupported. [elem=%1, format=%2]", mName, format.sample_type);
    }
    mOut.PushBuffer(ret);
}

void MixerSource::ReceiveCommand(Command& cmd)
{
    if (auto* ptr = cmd.GetIf<AddSourceCmd>())
        AddSourcePtr(std::move(ptr->src), ptr->paused);
    else if (auto* ptr = cmd.GetIf<CancelSourceCmdCmd>())
        CancelSourceCommands(ptr->name);
    else if (auto* ptr = cmd.GetIf<SetEffectCmd>())
        SetSourceEffect(ptr->src, std::move(ptr->effect));
    else if (auto* ptr = cmd.GetIf<DeleteSourceCmd>())
    {
        if (ptr->millisecs)
            mCommands.push_back(*ptr);
        else DeleteSource(ptr->name);
    }
    else if (auto* ptr = cmd.GetIf<PauseSourceCmd>())
    {
        if (ptr->millisecs)
            mCommands.push_back(*ptr);
        else PauseSource(ptr->name, ptr->paused);
    }
    else if (auto* ptr = cmd.GetIf<DeleteAllSrcCmd>())
    {
        if (ptr->millisecs)
            mCommands.push_back(*ptr);
        else DeleteSources();
    }
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
void MixerSource::ExecuteCommand(const DeleteAllSrcCmd& cmd)
{ DeleteSources(); }
void MixerSource::ExecuteCommand(const DeleteSourceCmd& cmd)
{ DeleteSource(cmd.name); }
void MixerSource::ExecuteCommand(const PauseSourceCmd& cmd)
{ PauseSource(cmd.name, cmd.paused); }

void MixerSource::RemoveDoneEffects(EventQueue& events)
{
    for (auto& pair : mSources)
    {
        auto& source = pair.second;
        if (!source.effect || !source.effect->IsDone())
            continue;
        const auto effect_name = source.effect->GetName();
        const auto src_name = source.element->GetName();
        EffectDoneEvent event;
        event.mixer  = mName;
        event.src    = source.element->GetName();
        event.effect = std::move(source.effect);
        events.push(MakeEvent(std::move(event)));
        DEBUG("Audio mixer source effect is done. [elem=%1, source=%2, effect=%3]", mName, src_name, effect_name);
    }
}

void MixerSource::RemoveDoneSources(EventQueue& events)
{
    for (auto it = mSources.begin(); it != mSources.end();)
    {
        auto& source = it->second;
        auto& element = source.element;
        if (element->IsSourceDone())
        {
            element->Shutdown();

            SourceDoneEvent event;
            event.mixer = mName;
            event.src   = std::move(element);
            events.push(MakeEvent(std::move(event)));
            DEBUG("Audio mixer source is done. [elem=%1, source=%2]", mName, it->first);
            it = mSources.erase(it);
        }
        else ++it;
    }
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

bool ZeroSource::Prepare(const Loader& loader, const PrepareParams& params)
{
    DEBUG("Audio zero source prepared successfully. [elem=%1, output=%2]", mName, mFormat);
    return true;
}

void ZeroSource::Process(Allocator& allocator, EventQueue& events, unsigned milliseconds)
{
    TRACE_SCOPE("ZeroSource");

    const auto frame_size = GetFrameSizeInBytes(mFormat);
    const auto frames_per_ms = mFormat.sample_rate / 1000;
    const auto frames_wanted = frames_per_ms * milliseconds;

    auto buffer = allocator.Allocate(frame_size * frames_wanted);
    buffer->SetFormat(mFormat);
    buffer->SetByteSize(frame_size * frames_wanted);
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

template<typename T>
const T* GetArg(const std::unordered_map<std::string, ElementArg>& args,
                const std::string& arg_name,
                const std::string& elem)
{
    if (const auto* variant = base::SafeFind(args, arg_name))
    {
        if (const auto* value = std::get_if<T>(variant))
            return value;
        else ERROR("Mismatch in audio element argument type. [elem=%1, arg=%2]", elem, arg_name);
    } else ERROR("Missing audio element argument. [elem=%1, arg=%2]", elem, arg_name);
    return nullptr;
}

template<typename T>
const T* GetOptionalArg(const std::unordered_map<std::string, ElementArg>& args,
                        const std::string& arg_name,
                        const std::string& elem)
{
    if (const auto* variant = base::SafeFind(args, arg_name))
    {
        if (const auto* value = std::get_if<T>(variant))
            return value;
        else WARN("Mismatch in audio element argument type. [elem=%1, arg=%2]", elem, arg_name);
    }
    return nullptr;
}

template<typename Type, typename Arg0> inline
std::unique_ptr<Type> Construct(const std::string& name,
                                   const std::string& id,
                                   const Arg0* arg0)
{
    if (!arg0) return nullptr;
    return std::make_unique<Type>(name, id, *arg0);
}
template<typename Type, typename Arg0, typename Arg1> inline
std::unique_ptr<Type> Construct(const std::string& name,
                                   const std::string& id,
                                   const Arg0* arg0,
                                   const Arg1* arg1)
{
    if (!arg0 || !arg1) return nullptr;
    return std::make_unique<Type>(name, id, *arg0, *arg1);
}
template<typename Type, typename Arg0, typename Arg1, typename Arg2> inline
std::unique_ptr<Type> Construct(const std::string& name,
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
        list.push_back("Effect");
        list.push_back("Gain");
        list.push_back("Null");
        list.push_back("StereoSplitter");
        list.push_back("StereoJoiner");
        list.push_back("StereoMaker");
        list.push_back("Splitter");
        list.push_back("Mixer");
        list.push_back("Delay");
        list.push_back("Playlist");
        list.push_back("Queue");
    }
    return list;
}

const ElementDesc* FindElementDesc(const std::string& type)
{
    static std::unordered_map<std::string, ElementDesc> map;
    if (map.empty())
    {
        {
            ElementDesc playlist;
            playlist.input_ports.push_back({"in0"});
            playlist.input_ports.push_back({"in1"});
            playlist.output_ports.push_back({"out"});
            map["Playlist"] = playlist;
        }
        {
            ElementDesc test;
            test.args["frequency"] = 2000u;
            test.args["duration"]  = 0u;
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
            file.args["file"]  = std::string();
            file.args["type"]  = audio::SampleType::Float32;
            file.args["loops"] = 1u;
            file.args["pcm_caching"] = false;
            file.args["file_caching"] = false;
            file.args["io_strategy"] = FileSource::IOStrategy::Default;
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
            ElementDesc effect;
            effect.args["time"] = 0u;
            effect.args["duration"] = 0u;
            effect.args["effect"] = Effect::Kind::FadeIn;
            effect.input_ports.push_back({"in"});
            effect.output_ports.push_back({"out"});
            map["Effect"] = effect;
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
        {
            ElementDesc delay;
            delay.args["delay"] = 0u;
            delay.input_ports.push_back({"in"});
            delay.output_ports.push_back({"out"});
            map["Delay"] = delay;
        }
        {
            ElementDesc splitter;
            splitter.args["num_outs"] = 2u;
            splitter.input_ports.push_back({"in"});
            splitter.output_ports.push_back({"out0"});
            splitter.output_ports.push_back({"out1"});
            map["Splitter"] = splitter;
        }

        {
            ElementDesc queue;
            queue.input_ports.push_back({"in"});
            queue.output_ports.push_back({"out"});
            map["Queue"] = std::move(queue);
        }
    }
    return base::SafeFind(map, type);
}

std::unique_ptr<Element> CreateElement(const ElementCreateArgs& desc)
{
    const auto& args = desc.args;
    const auto& name = desc.type + "/" + desc.name;
    if (desc.type == "Queue")
        return std::make_unique<Queue>(desc.name, desc.id);
    else if (desc.type == "Playlist")
        return Construct<Playlist>(desc.name, desc.id, &desc.input_ports);
    else if (desc.type == "StereoMaker")
        return Construct<StereoMaker>(desc.name, desc.id,
            GetArg<StereoMaker::Channel>(args, "channel", name));
    else if (desc.type == "StereoJoiner")
        return std::make_unique<StereoJoiner>(desc.name, desc.id);
    else if (desc.type == "StereoSplitter")
        return std::make_unique<StereoSplitter>(desc.name, desc.id);
    else if (desc.type == "Null")
        return std::make_unique<Null>(desc.name, desc.id);
    else if (desc.type == "Mixer")
        return Construct<Mixer>(desc.name, desc.id, &desc.input_ports);
    else if (desc.type == "Splitter")
        return Construct<Splitter>(desc.name, desc.id, &desc.output_ports);
    else if (desc.type == "Delay")
        return Construct<Delay>(desc.name, desc.id,
            GetArg<unsigned>(args, "delay", name));
    else if (desc.type == "Effect")
        return Construct<Effect>(desc.name, desc.id,
            GetArg<unsigned>(args, "time", name),
            GetArg<unsigned>(args, "duration", name),
            GetArg<Effect::Kind>(args, "effect", name));
    else if (desc.type == "Gain")
        return Construct<Gain>(desc.name, desc.id, 
            GetArg<float>(args, "gain", name));
    else if (desc.type == "Resampler")
        return Construct<Resampler>(desc.name, desc.id, 
            GetArg<unsigned>(args, "sample_rate", name));
    else if (desc.type == "FileSource")
    {
        auto ret = Construct<FileSource>(desc.name, desc.id,
            GetArg<std::string>(args, "file", name),
            GetArg<SampleType>(args, "type", name),
            GetArg<unsigned>(args, "loops", name));
        if (const auto* arg = GetOptionalArg<bool>(args, "pcm_caching", name))
            ret->EnablePcmCaching(*arg);
        if (const auto* arg = GetOptionalArg<bool>(args, "file_caching", name))
            ret->EnableFileCaching(*arg);
        if (const auto* arg = GetOptionalArg<FileSource::IOStrategy>(args, "io_strategy", name))
            ret->SetIOStrategy(*arg);
        return ret;
    }
    else if (desc.type == "ZeroSource")
        return Construct<ZeroSource>(desc.name, desc.id,
            GetArg<Format>(args, "format", name));
    else if (desc.type == "SineSource")
        return Construct<SineSource>(desc.name, desc.id,
            GetArg<Format>(args, "format", name),
            GetArg<unsigned>(args, "frequency", name),
            GetArg<unsigned>(args, "duration", name));
    else BUG("Unsupported audio Element construction.");
    return nullptr;
}

void ClearCaches()
{
    FileSource::ClearCache();
}


} // namespace

