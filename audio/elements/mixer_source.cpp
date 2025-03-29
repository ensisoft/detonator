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
#include "audio/elements/mixer_source.h"

namespace audio
{

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
    mTime = FadeBuffer<DataType, ChannelCount>(std::move(buffer), mTime, 0.0f, mDuration, true);
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
    mTime = FadeBuffer<DataType, ChannelCount>(std::move(buffer), mTime, 0.0f, mDuration, false);
}

MixerSource::MixerSource(std::string name, const Format& format)
  : mName(std::move(name))
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
            mCommands.emplace_back(*ptr);
        else DeleteSource(ptr->name);
    }
    else if (auto* ptr = cmd.GetIf<PauseSourceCmd>())
    {
        if (ptr->millisecs)
            mCommands.emplace_back(*ptr);
        else PauseSource(ptr->name, ptr->paused);
    }
    else if (auto* ptr = cmd.GetIf<DeleteAllSrcCmd>())
    {
        if (ptr->millisecs)
            mCommands.emplace_back(*ptr);
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
{
    DeleteSources();
}
void MixerSource::ExecuteCommand(const DeleteSourceCmd& cmd)
{
    DeleteSource(cmd.name);
}
void MixerSource::ExecuteCommand(const PauseSourceCmd& cmd)
{
    PauseSource(cmd.name, cmd.paused);
}

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

} // namespace