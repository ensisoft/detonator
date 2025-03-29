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
#include "audio/elements/effect.h"

namespace audio
{

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

} // namespace