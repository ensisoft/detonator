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
#include "base/trace.h"
#include "audio/device.h"
#include "audio/format.h"
#include "audio/player.h"
#include "audio/buffer.h"
#include "engine/audio.h"
#include "audio/element.h"
#include "audio/elements/mixer_source.h"
#include "audio/elements/gain.h"
#include "audio/elements/mixer.h"
#include "audio/elements/graph_class.h"
#include "audio/elements/graph.h"
#include "audio/thread_proxy_source.h"
#include "audio/audio_graph_source.h"
#include "engine/loader.h"
#include "engine/data.h"

namespace engine
{

AudioEngine::AudioEngine(const std::string& name)
  : mName(name)
{
    mFormat.sample_rate   = 44100;
    mFormat.channel_count = 2;
    mFormat.sample_type   = audio::SampleType::Float32;
}

AudioEngine::~AudioEngine()
{
#if defined(ENGINE_ENABLE_AUDIO)
    if (mPlayer)
    {
        if (mAudioGraphId)
        {
            mPlayer->Cancel(mAudioGraphId);
        }
        else
        {
            mPlayer->Cancel(mEffectGraphId);
            mPlayer->Cancel(mMusicGraphId);
        }
    }
#endif
}

void AudioEngine::Start()
{
    ASSERT(!mPlayer);
    ASSERT(mEffectGraphId == 0);
    ASSERT(mMusicGraphId  == 0);
#if defined(ENGINE_ENABLE_AUDIO)
    audio::AudioGraphSource::PrepareParams p;
    p.enable_pcm_caching = false;

    auto device = audio::Device::Create(mName.c_str(), &mFormat);
    device->SetBufferSize(mBufferSize);
    DEBUG("Created audio device '%1'. [format=%2, buffer=%3ms]", device->GetBackend(), mFormat, mBufferSize);

    mPlayer = std::make_unique<audio::Player>(std::move(device));

#if defined(GAMESTUDIO_MULTIPLE_AUDIO_STREAMS)
    {
        auto effect_graph = std::make_unique<audio::AudioGraph>("FX");
        auto* effect_gain = (*effect_graph)->AddElement(audio::Gain("effect_gain", 1.0f));
        auto* effect_mixer = (*effect_graph)->AddElement(audio::MixerSource("effect_mixer", mFormat));
        effect_mixer->SetNeverDone(true);
        ASSERT((*effect_graph)->LinkElements("effect_mixer", "out", "effect_gain", "in"));
        ASSERT((*effect_graph)->LinkGraph("effect_gain", "out"));
        ASSERT(effect_graph->Prepare(*mLoader, p));

        auto proxy = std::make_unique<audio::SourceThreadProxy>(std::move(effect_graph));

        mEffectGraphId = mPlayer->Play(std::move(proxy));
        DEBUG("Audio effect graph is ready. [id=%1]", mEffectGraphId);
    }

    {

        auto music_graph = std::make_unique<audio::AudioGraph>("Music");
        auto* music_gain = (*music_graph)->AddElement(audio::Gain("music_gain", 1.0f));
        auto* music_mixer = (*music_graph)->AddElement(audio::MixerSource("music_mixer", mFormat));
        music_mixer->SetNeverDone(true);
        ASSERT((*music_graph)->LinkElements("music_mixer", "out", "music_gain", "in"));
        ASSERT((*music_graph)->LinkGraph("music_gain", "out"));
        ASSERT(music_graph->Prepare(*mLoader, p));

        auto proxy = std::make_unique<audio::SourceThreadProxy>(std::move(music_graph));

        mMusicGraphId = mPlayer->Play(std::move(proxy));
        DEBUG("Audio music graph is ready. [id=%1]", mMusicGraphId);
    }
#else
    auto audio_graph = std::make_unique<audio::AudioGraphSource>(mName.c_str());
    auto main_mixer = (*audio_graph)->AddElement(audio::Mixer("mixer"));
    auto effect_mixer = (*audio_graph)->AddElement(audio::MixerSource("effect_mixer", mFormat));
    auto effect_gain = (*audio_graph)->AddElement(audio::Gain("effect_gain", 2.0f));
    auto music_mixer = (*audio_graph)->AddElement(audio::MixerSource("music_mixer", mFormat));
    auto music_gain = (*audio_graph)->AddElement(audio::Gain("music_gain", 2.0f));

    effect_mixer->SetNeverDone(true);
    music_mixer->SetNeverDone(true);

    ASSERT((*audio_graph)->LinkElements("effect_mixer", "out", "effect_gain", "in"));
    ASSERT((*audio_graph)->LinkElements("effect_gain", "out", "mixer", "in0"));
    ASSERT((*audio_graph)->LinkElements("music_mixer", "out", "music_gain", "in"));
    ASSERT((*audio_graph)->LinkElements("music_gain", "out", "mixer", "in1"));
    ASSERT((*audio_graph)->LinkGraph("mixer", "out"));
    ASSERT(audio_graph->Prepare(*mLoader, p));

    auto proxy = std::make_unique<audio::ThreadProxySource>(std::move(audio_graph));
    mAudioGraphId = mPlayer->Play(std::move(proxy));
    DEBUG("Audio graph is ready. [id=%1]", mAudioGraphId);

    // they're all the same!
    mMusicGraphId  = mAudioGraphId;
    mEffectGraphId = mAudioGraphId;
#endif

#endif
}

void AudioEngine::SetDebugPause(bool on_off)
{
#if defined(ENGINE_ENABLE_AUDIO)
    if (on_off)
    {
        if (mAudioGraphId)
        {
            mPlayer->Pause(mAudioGraphId);
        }
        else
        {
            mPlayer->Pause(mEffectGraphId);
            mPlayer->Pause(mMusicGraphId);
        }
    }
    else
    {
        if (mAudioGraphId)
        {
            mPlayer->Resume(mEffectGraphId);
        }
        else
        {
            mPlayer->Resume(mMusicGraphId);
            mPlayer->Resume(mAudioGraphId);
        }
    }
#endif
}

bool AudioEngine::PrepareMusicGraph(const GraphHandle& graph)
{
    ASSERT(graph);
#if defined(ENGINE_ENABLE_AUDIO)
    auto instance = std::make_unique<audio::Graph>(graph);
    audio::Graph::PrepareParams p;
    p.enable_pcm_caching = mEnableCaching;
    if (!instance->Prepare(*mLoader, p))
        ERROR_RETURN(false, "Audio engine music graph prepare error. [graph=%1]", graph->GetName());
    const auto& port = instance->GetOutputPort(0);
    if (port.GetFormat() != mFormat)
        ERROR_RETURN(false, "Audio engine music graph has incompatible output format. [graph=%1, format=%2]", graph->GetName(), port.GetFormat());

    audio::MixerSource::AddSourceCmd cmd;
    cmd.src    = std::move(instance);
    cmd.paused = true;
    mPlayer->SendCommand(mMusicGraphId, audio::AudioGraphSource::MakeCommand("music_mixer", std::move(cmd)));
#endif
    return true;
}

bool AudioEngine::PlayMusic(const GraphHandle& graph, unsigned when)
{
#if defined(ENGINE_ENABLE_AUDIO)
    if (!PrepareMusicGraph(graph))
        return false;
    ResumeMusic(graph->GetName(), when);
#endif
    return true;
}

void AudioEngine::ResumeMusic(const std::string& track, unsigned when)
{
#if defined(ENGINE_ENABLE_AUDIO)
    audio::MixerSource::PauseSourceCmd cmd;
    cmd.name      = track;
    cmd.paused    = false;
    cmd.millisecs = when;
    mPlayer->SendCommand(mMusicGraphId, audio::AudioGraphSource::MakeCommand("music_mixer", std::move(cmd)));
#endif
}

void AudioEngine::PauseMusic(const std::string& track, unsigned when)
{
#if defined(ENGINE_ENABLE_AUDIO)
    audio::MixerSource::PauseSourceCmd cmd;
    cmd.name      = track;
    cmd.paused    = true;
    cmd.millisecs = when;
    mPlayer->SendCommand(mMusicGraphId, audio::AudioGraphSource::MakeCommand("music_mixer", std::move(cmd)));
#endif
}

void AudioEngine::KillMusic(const std::string& track, unsigned when)
{
#if defined(ENGINE_ENABLE_AUDIO)
    audio::MixerSource::DeleteSourceCmd cmd;
    cmd.name      = track;
    cmd.millisecs = when;
    mPlayer->SendCommand(mMusicGraphId, audio::AudioGraphSource::MakeCommand("music_mixer", std::move(cmd)));
#endif
}
void AudioEngine::KillAllMusic(unsigned int when)
{
#if defined(ENGINE_ENABLE_AUDIO)
    audio::MixerSource::DeleteAllSrcCmd cmd;
    cmd.millisecs = when;
    mPlayer->SendCommand(mMusicGraphId, audio::AudioGraphSource::MakeCommand("music_mixer", std::move(cmd)));
#endif
}

void AudioEngine::CancelMusicCmds(const std::string& track)
{
#if defined(ENGINE_ENABLE_AUDIO)
    audio::MixerSource::CancelSourceCmdCmd cmd;
    cmd.name = track;
    mPlayer->SendCommand(mMusicGraphId, audio::AudioGraphSource::MakeCommand("music_mixer", std::move(cmd)));
#endif
}

void AudioEngine::SetMusicEffect(const std::string& track, unsigned duration, Effect effect)
{
#if defined(ENGINE_ENABLE_AUDIO)
    std::unique_ptr<audio::MixerSource::Effect> mixer_effect;
    if (effect == Effect::FadeIn)
        mixer_effect = std::make_unique<audio::MixerSource::FadeIn>(duration);
    else if (effect == Effect::FadeOut)
        mixer_effect = std::make_unique<audio::MixerSource::FadeOut>(duration);

    audio::MixerSource::SetEffectCmd cmd;
    cmd.src    = track;
    cmd.effect = std::move(mixer_effect);
    mPlayer->SendCommand(mMusicGraphId, audio::AudioGraphSource::MakeCommand("music_mixer", std::move(cmd)));
#endif
}

void AudioEngine::SetMusicGain(float gain)
{
#if defined(ENGINE_ENABLE_AUDIO)
    audio::Gain::SetGainCmd cmd;
    cmd.gain = gain;
    mPlayer->SendCommand(mMusicGraphId, audio::AudioGraphSource::MakeCommand("music_gain", std::move(cmd)));
#endif
}

bool AudioEngine::PlaySoundEffect(const GraphHandle& handle, unsigned when)
{
    TRACE_SCOPE("AudioEngine::PlaySoundEffect");

#if defined(ENGINE_ENABLE_AUDIO)
    if (!mEnableEffects)
        return true;

    const auto name   = handle->GetName();
    const auto paused = when != 0;

    auto graph = std::make_unique<audio::Graph>(name, handle);
    audio::Graph::PrepareParams p;
    p.enable_pcm_caching = mEnableCaching;
    if (!graph->Prepare(*mLoader, p))
        ERROR_RETURN(false, "Audio engine sound effect audio graph prepare error. [graph=%1]", handle->GetName());

    const auto& port = graph->GetOutputPort(0);
    if (port.GetFormat() != mFormat)
        ERROR_RETURN(false, "Audio engine sound effect graph has incompatible output format. [graph=%1, format=%2]", handle->GetName(), port.GetFormat());

    audio::MixerSource::AddSourceCmd add_cmd;
    add_cmd.src    = std::move(graph);
    add_cmd.paused = true;
    mPlayer->SendCommand(mEffectGraphId, audio::AudioGraphSource::MakeCommand("effect_mixer", std::move(add_cmd)));

    audio::MixerSource::PauseSourceCmd play_cmd;
    play_cmd.name      = name;
    play_cmd.paused    = false;
    play_cmd.millisecs = when;
    mPlayer->SendCommand(mEffectGraphId, audio::AudioGraphSource::MakeCommand("effect_mixer", std::move(play_cmd)));
#endif
    return true;
}

void AudioEngine::SetSoundEffectGain(float gain)
{
#if defined(ENGINE_ENABLE_AUDIO)
    audio::Gain::SetGainCmd cmd;
    cmd.gain = gain;
    mPlayer->SendCommand(mEffectGraphId, audio::AudioGraphSource::MakeCommand("effect_gain", std::move(cmd)));
#endif
}
void AudioEngine::KillAllSoundEffects(unsigned when)
{
#if defined(ENGINE_ENABLE_AUDIO)
    audio::MixerSource::DeleteAllSrcCmd cmd;
    cmd.millisecs = when;
    mPlayer->SendCommand(mEffectGraphId, audio::AudioGraphSource::MakeCommand("effect_mixer", std::move(cmd)));
#endif
}

void AudioEngine::KillSoundEffect(const std::string& track, unsigned when)
{
#if defined(ENGINE_ENABLE_AUDIO)
    audio::MixerSource::DeleteSourceCmd cmd;
    cmd.name = track;
    cmd.millisecs = when;
    mPlayer->SendCommand(mEffectGraphId, audio::AudioGraphSource::MakeCommand("effect_mixer", std::move(cmd)));
#endif
}

void AudioEngine::Update(AudioEventQueue* events)
{
#if defined(ENGINE_ENABLE_AUDIO)

#if !defined(AUDIO_USE_PLAYER_THREAD)
    mPlayer->ProcessOnce();
#endif

    // pump audio events from the audio player thread
    audio::Player::Event event;
    while (mPlayer->GetEvent(&event))
    {
        if (const auto* ptr = std::get_if<audio::Player::SourceCompleteEvent>(&event))
            OnAudioPlayerEvent(*ptr, events);
        else if (const auto* ptr = std::get_if<audio::Player::SourceEventEvent>(&event))
            OnAudioPlayerEvent(*ptr, events);
        else BUG("Unexpected audio player event.");
    }
#endif
}

void AudioEngine::OnAudioPlayerEvent(const audio::Player::SourceCompleteEvent& event, AudioEventQueue* events)
{
    DEBUG("Audio engine source event. [id=%1, status=%2]", event.id, event.status);

    // intentionally empty for now.
}
void AudioEngine::OnAudioPlayerEvent(const audio::Player::SourceEventEvent& event, AudioEventQueue* events)
{
    if (event.id == mAudioGraphId)
        DEBUG("Audio engine music+effect graph source event.");
    else if (event.id == mMusicGraphId)
        DEBUG("Audio engine music graph source event.");
    else if (event.id == mEffectGraphId)
        DEBUG("Audio engine effect graph source event.");
    else BUG("Unknown audio graph id");

    if (events == nullptr)
        return;

    if (auto* done = event.event->GetIf<audio::MixerSource::SourceDoneEvent>())
    {
        AudioEvent ev;
        ev.track = done->src->GetName();
        ev.type  = AudioEvent::Type::TrackDone;
        if (event.id == mAudioGraphId)
            ev.source = "audio";
        else if (event.id == mMusicGraphId)
            ev.source = "music";
        else if (event.id == mEffectGraphId)
            ev.source = "effect";
        else BUG("Unexpected audio event id.");

        events->push_back(std::move(ev));
    }
}

void AudioEngine::SetAudioThreadTraceWriter(base::TraceWriter* writer)
{
#if defined(ENGINE_ENABLE_AUDIO)
  #if defined(AUDIO_USE_PLAYER_THREAD)
       // todo: enable tracing in the audio player thread
  #else
      audio::ThreadProxySource::SetThreadTraceWriter(writer);
  #endif
#endif
}

void AudioEngine::EnableAudioThreadTrace(bool on_off)
{
#if defined(ENGINE_ENABLE_AUDIO)
  #if defined(AUDIO_USE_PLAYER_THREAD)
      // todo
  #else
      audio::ThreadProxySource::EnableThreadTrace(on_off);
  #endif
#endif
}

} // namespace
