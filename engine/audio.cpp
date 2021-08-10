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

    auto effect_graph  = std::make_unique<audio::AudioGraph>(name + " FX");
    auto* effect_gain  = (*effect_graph)->AddElement(audio::Gain("gain", 1.0f));
    auto* effect_mixer = (*effect_graph)->AddElement(audio::MixerSource("mixer", mFormat));
    effect_mixer->AddSource(audio::ZeroSource("zero", mFormat));
    ASSERT((*effect_graph)->LinkElements("mixer", "out", "gain", "in"));
    ASSERT((*effect_graph)->LinkGraph("gain", "out"));
    ASSERT(effect_graph->Prepare(*mLoader));

    auto music_graph = std::make_unique<audio::AudioGraph>(name + " Music");
    auto* music_gain  = (*music_graph)->AddElement(audio::Gain("gain", 1.0f));
    auto* music_mixer = (*music_graph)->AddElement(audio::MixerSource("mixer", mFormat));
    music_mixer->AddSource(audio::ZeroSource("zero", mFormat));
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

bool AudioEngine::AddMusicTrack(const std::string& name, const std::string& uri)
{
    auto music = std::make_unique<audio::Graph>(name);
    music->AddElement(audio::FileSource(name + "/file", uri, audio::SampleType::Float32));
    music->AddElement(audio::StereoMaker(name + "/stereo"));
    music->AddElement(audio::Resampler(name + "/resampler", 44100));
    music->AddElement(audio::Effect(name + "/effect"));
    ASSERT(music->LinkElements(name + "/file",   "out", name + "/stereo",    "in"));
    ASSERT(music->LinkElements(name + "/stereo", "out", name + "/effect",    "in"));
    ASSERT(music->LinkElements(name + "/effect", "out", name + "/resampler", "in"));
    ASSERT(music->LinkGraph(name + "/resampler", "out"));

    if (!music->Prepare(*mLoader))
        ERROR_RETURN(false, "Failed to prepare music audio graph.");

    const auto& port = music->GetOutputPort(0);
    if (port.GetFormat() != mFormat)
        ERROR_RETURN(false, "Music '%1' audio graph has incorrect PCM format '%2'.", uri, port.GetFormat());

    audio::MixerSource::AddSourceCmd cmd;
    cmd.src    = std::move(music);
    cmd.paused = true;
    mPlayer->SendCommand(mMusicGraphId, audio::AudioGraph::MakeCommand("mixer", std::move(cmd)));
    return true;
}

void AudioEngine::PlayMusic(const std::string& track, std::chrono::milliseconds when)
{
    audio::MixerSource::PauseSourceCmd cmd;
    cmd.name      = track;
    cmd.paused    = false;
    cmd.millisecs = when.count();
    mPlayer->SendCommand(mMusicGraphId, audio::AudioGraph::MakeCommand("mixer", std::move(cmd)));
}

void AudioEngine::PlayMusic(const std::string& track)
{
    audio::MixerSource::PauseSourceCmd cmd;
    cmd.name   = track;
    cmd.paused = false;
    mPlayer->SendCommand(mMusicGraphId, audio::AudioGraph::MakeCommand("mixer", std::move(cmd)));
}

void AudioEngine::PauseMusic(const std::string& track, std::chrono::milliseconds when)
{
    audio::MixerSource::PauseSourceCmd cmd;
    cmd.name   = track;
    cmd.paused = true;
    cmd.millisecs = when.count();
    mPlayer->SendCommand(mMusicGraphId, audio::AudioGraph::MakeCommand("mixer", std::move(cmd)));
}

void AudioEngine::PauseMusic(const std::string& track)
{
    audio::MixerSource::PauseSourceCmd cmd;
    cmd.name   = track;
    cmd.paused = true;
    mPlayer->SendCommand(mMusicGraphId, audio::AudioGraph::MakeCommand("mixer", std::move(cmd)));
}

void AudioEngine::RemoveMusicTrack(const std::string& name)
{
    audio::MixerSource::DeleteSourceCmd cmd;
    cmd.name = name;
    mPlayer->SendCommand(mMusicGraphId, audio::AudioGraph::MakeCommand("mixer", std::move(cmd)));
}

void AudioEngine::SetMusicEffect(const std::string& track, float duration, Effect effect)
{
    audio::Effect::SetEffectCmd cmd;
    cmd.effect   = effect;
    cmd.time     = 0;
    cmd.duration = duration * 1000;
    mPlayer->SendCommand(mMusicGraphId, audio::AudioGraph::MakeCommand(track + "/effect", std::move(cmd)));
}

void AudioEngine::SetMusicGain(float gain)
{
    audio::Gain::SetGainCmd cmd;
    cmd.gain = gain;
    mPlayer->SendCommand(mMusicGraphId, audio::AudioGraph::MakeCommand("gain", std::move(cmd)));
}

bool AudioEngine::PlaySoundEffect(const std::string& uri, std::chrono::milliseconds ms)
{
    const auto name   = std::to_string(mEffectCounter);
    const auto paused = ms.count() != 0;

    auto effect = std::make_unique<audio::Graph>(name);
    effect->AddElement(audio::FileSource(uri, uri, audio::SampleType::Float32));
    effect->AddElement(audio::StereoMaker("stereo"));
    effect->AddElement(audio::Resampler("resampler", 44100));

    ASSERT(effect->LinkElements(uri, "out", "stereo", "in"));
    ASSERT(effect->LinkElements("stereo", "out", "resampler", "in"));
    ASSERT(effect->LinkGraph("resampler", "out"));

    if (!effect->Prepare(*mLoader))
        ERROR_RETURN(false, "Failed to prepare effect audio graph.");

    const auto& port = effect->GetOutputPort(0);
    if (port.GetFormat() != mFormat)
        ERROR_RETURN(false, "Effect '%1' audio graph has incorrect PCM format '%2'.", uri, port.GetFormat());

    audio::MixerSource::AddSourceCmd cmd;
    cmd.src    = std::move(effect);
    cmd.paused = paused;
    mPlayer->SendCommand(mEffectGraphId, audio::AudioGraph::MakeCommand("mixer", std::move(cmd)));

    if (paused)
    {
        audio::MixerSource::PauseSourceCmd cmd;
        cmd.name      = name;
        cmd.paused    = false;
        cmd.millisecs = ms.count();
        mPlayer->SendCommand(mEffectGraphId, audio::AudioGraph::MakeCommand("mixer", std::move(cmd)));
    }
    ++mEffectCounter;
    return true;
}

bool AudioEngine::PlaySoundEffect(const std::string& uri)
{
    return PlaySoundEffect(uri, std::chrono::milliseconds(0));
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
    if (events == nullptr) return;
    else if (event.id != mMusicGraphId) return;

    if (auto* done = event.event->GetIf<audio::MixerSource::SourceDoneEvent>())
    {
        MusicEvent me;
        me.track = done->src->GetName();
        me.type  = MusicEvent::Type::TrackDone;
        events->push_back(std::move(me));
    }
}

} // namespace
