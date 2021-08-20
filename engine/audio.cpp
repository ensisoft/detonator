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

#include "base/assert.h"
#include "base/logging.h"
#include "audio/device.h"
#include "audio/format.h"
#include "audio/element.h"
#include "audio/graph.h"
#include "audio/player.h"
#include "audio/buffer.h"
#include "engine/audio.h"
#include "engine/loader.h"
#include "engine/data.h"

namespace game
{

// adapt game data interface to audio buffer
class AudioEngine::AudioBuffer : public audio::Buffer
{
public:
    AudioBuffer(GameDataHandle audio): mAudioData(audio)
    {}
    virtual Format GetFormat() const override
    {
        audio::Format format;
        return format;
    }
    virtual const void* GetPtr() const override
    { return mAudioData->GetData(); }

    virtual void* GetPtr() override
    {
        BUG("Unexpected GetPtr call.");
        return nullptr;
    }
    virtual size_t GetByteSize() const override
    { return mAudioData->GetSize(); }
private:
    GameDataHandle mAudioData;
};

AudioEngine::AudioEngine(const std::string& name, const audio::Loader* loader)
  : mLoader(loader)
{
    mFormat.sample_rate   = 44100;
    mFormat.channel_count = 2;
    mFormat.sample_type   = audio::SampleType::Float32;

    auto effect_graph  = std::make_unique<audio::AudioGraph>("FX");
    auto* effect_gain  = (*effect_graph)->AddElement(audio::Gain("gain", 1.0f));
    auto* effect_mixer = (*effect_graph)->AddElement(audio::MixerSource("mixer", mFormat));
    effect_mixer->SetNeverDone(true);
    ASSERT((*effect_graph)->LinkElements("mixer", "out", "gain", "in"));
    ASSERT((*effect_graph)->LinkGraph("gain", "out"));
    ASSERT(effect_graph->Prepare(*mLoader));

    auto music_graph = std::make_unique<audio::AudioGraph>("Music");
    auto* music_gain  = (*music_graph)->AddElement(audio::Gain("gain", 1.0f));
    auto* music_mixer = (*music_graph)->AddElement(audio::MixerSource("mixer", mFormat));
    music_mixer->SetNeverDone(true);
    ASSERT((*music_graph)->LinkElements("mixer", "out", "gain", "in"));
    ASSERT((*music_graph)->LinkGraph("gain", "out"));
    ASSERT(music_graph->Prepare(*mLoader));

    mPlayer = std::make_unique<audio::Player>(audio::Device::Create(name.c_str()));
    mEffectGraphId = mPlayer->Play(std::move(effect_graph), false /* looping */);
    mMusicGraphId  = mPlayer->Play(std::move(music_graph), false /*looping*/);
    DEBUG("Audio effect graph playing with stream id '%1'.", mEffectGraphId);
    DEBUG("Audio music graph playing with stream id '%1'.", mMusicGraphId);
}

AudioEngine::~AudioEngine()
{
    mPlayer->Cancel(mEffectGraphId);
    mPlayer->Cancel(mMusicGraphId);
}

void AudioEngine::SetDebugPause(bool on_off)
{
    if (on_off)
    {
        mPlayer->Pause(mEffectGraphId);
        mPlayer->Pause(mMusicGraphId);
    }
    else
    {
        mPlayer->Resume(mEffectGraphId);
        mPlayer->Resume(mMusicGraphId);
    }
}

bool AudioEngine::AddMusicGraph(const GraphHandle& handle)
{
    ASSERT(handle);

    auto graph = std::make_unique<audio::Graph>(handle);
    if (!graph->Prepare(*mLoader))
        ERROR_RETURN(false, "Failed to prepare music audio graph.");
    const auto& port = graph->GetOutputPort(0);
    if (port.GetFormat() != mFormat)
        ERROR_RETURN(false, "Music audio graph '%1' has incompatible output PCM format '%2'.", handle->GetName(), port.GetFormat());

    audio::MixerSource::AddSourceCmd cmd;
    cmd.src    = std::move(graph);
    cmd.paused = true;
    mPlayer->SendCommand(mMusicGraphId, audio::AudioGraph::MakeCommand("mixer", std::move(cmd)));
    return true;
}

bool AudioEngine::PlayMusicGraph(const GraphHandle& handle)
{
    if (!AddMusicGraph(handle))
        return false;
    PlayMusic(handle->GetName(), 0);
    return true;
}

void AudioEngine::PlayMusic(const std::string& track, unsigned when)
{
    audio::MixerSource::PauseSourceCmd cmd;
    cmd.name      = track;
    cmd.paused    = false;
    cmd.millisecs = when;
    mPlayer->SendCommand(mMusicGraphId, audio::AudioGraph::MakeCommand("mixer", std::move(cmd)));
}

void AudioEngine::PauseMusic(const std::string& track, unsigned when)
{
    audio::MixerSource::PauseSourceCmd cmd;
    cmd.name      = track;
    cmd.paused    = true;
    cmd.millisecs = when;
    mPlayer->SendCommand(mMusicGraphId, audio::AudioGraph::MakeCommand("mixer", std::move(cmd)));
}

void AudioEngine::KillMusic(const std::string& track, unsigned when)
{
    audio::MixerSource::DeleteSourceCmd cmd;
    cmd.name      = track;
    cmd.millisecs = when;
    mPlayer->SendCommand(mMusicGraphId, audio::AudioGraph::MakeCommand("mixer", std::move(cmd)));
}
void AudioEngine::CancelMusicCmds(const std::string& track)
{
    audio::MixerSource::CancelSourceCmdCmd cmd;
    cmd.name = track;
    mPlayer->SendCommand(mMusicGraphId, audio::AudioGraph::MakeCommand("mixer", std::move(cmd)));
}

void AudioEngine::SetMusicEffect(const std::string& track, unsigned duration, Effect effect)
{
    std::unique_ptr<audio::MixerSource::Effect> mixer_effect;
    if (effect == Effect::FadeIn)
        mixer_effect = std::make_unique<audio::MixerSource::FadeIn>(duration);
    else if (effect == Effect::FadeOut)
        mixer_effect = std::make_unique<audio::MixerSource::FadeOut>(duration);

    audio::MixerSource::SetEffectCmd cmd;
    cmd.src    = track;
    cmd.effect = std::move(mixer_effect);
    mPlayer->SendCommand(mMusicGraphId, audio::AudioGraph::MakeCommand("mixer", std::move(cmd)));
}

void AudioEngine::SetMusicGain(float gain)
{
    audio::Gain::SetGainCmd cmd;
    cmd.gain = gain;
    mPlayer->SendCommand(mMusicGraphId, audio::AudioGraph::MakeCommand("gain", std::move(cmd)));
}

bool AudioEngine::PlaySoundEffect(const GraphHandle& handle, unsigned when)
{
    const auto name   = std::to_string(mEffectCounter);
    const auto paused = when != 0;

    auto graph = std::make_unique<audio::Graph>(name, handle);
    if (!graph->Prepare(*mLoader))
        ERROR_RETURN(false, "Failed to prepare effect audio graph.");

    const auto& port = graph->GetOutputPort(0);
    if (port.GetFormat() != mFormat)
        ERROR_RETURN(false, "Effect '%1' audio graph has incorrect PCM format '%2'.", handle->GetName(), port.GetFormat());

    audio::MixerSource::AddSourceCmd add_cmd;
    add_cmd.src    = std::move(graph);
    add_cmd.paused = true;
    mPlayer->SendCommand(mEffectGraphId, audio::AudioGraph::MakeCommand("mixer", std::move(add_cmd)));

    audio::MixerSource::PauseSourceCmd play_cmd;
    play_cmd.name      = name;
    play_cmd.paused    = false;
    play_cmd.millisecs = when;
    mPlayer->SendCommand(mEffectGraphId, audio::AudioGraph::MakeCommand("mixer", std::move(play_cmd)));

    ++mEffectCounter;
    return true;
}

void AudioEngine::SetSoundEffectGain(float gain)
{
    audio::Gain::SetGainCmd cmd;
    cmd.gain = gain;
    mPlayer->SendCommand(mEffectGraphId, audio::AudioGraph::MakeCommand("gain", std::move(cmd)));
}

void AudioEngine::Tick(AudioEventQueue* events)
{
    // pump audio events from the audio player thread
    audio::Player::Event event;
    while (mPlayer->GetEvent(&event))
    {
        if (const auto* ptr = std::get_if<audio::Player::SourceCompleteEvent>(&event))
            OnAudioPlayerEvent(*ptr, events);
        else if (const auto* ptr = std::get_if<audio::Player::SourceEvent>(&event))
            OnAudioPlayerEvent(*ptr, events);
        else BUG("Unexpected audio player event.");
    }
}

void AudioEngine::OnAudioPlayerEvent(const audio::Player::SourceCompleteEvent& event, AudioEventQueue* events)
{
    DEBUG("Audio track '%1' complete event (%2). ",event.id, event.status);

    // intentionally empty for now.
}
void AudioEngine::OnAudioPlayerEvent(const audio::Player::SourceEvent& event, AudioEventQueue* events)
{
    DEBUG("Audio track '%1' source event.", event.id);
    if (events == nullptr)
        return;
    else if (event.id != mMusicGraphId)
        return;

    if (auto* done = event.event->GetIf<audio::MixerSource::SourceDoneEvent>())
    {
        MusicEvent event;
        event.track = done->src->GetName();
        event.type  = MusicEvent::Type::TrackDone;
        events->push_back(std::move(event));
    }
    else if (auto* done = event.event->GetIf<audio::MixerSource::EffectDoneEvent>())
    {
        MusicEvent event;
        event.type  = MusicEvent::Type::EffectDone;
        event.track = done->src;
        events->push_back(std::move(event));
    }
}

} // namespace
