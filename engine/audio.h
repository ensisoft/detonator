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

#pragma once

#include "config.h"

#include <memory>
#include <queue> // priority_queue
#include <chrono>

#include "audio/fwd.h"
#include "audio/format.h"
#include "audio/element.h"
#include "audio/player.h"

namespace engine
{
    class ClassLibrary;

    // this is in the namespace scope since it might move to
    // a separate file since scripting interface requires this.
    struct AudioEvent {
        enum class Type {
            TrackDone
        };
        std::string source;
        std::string track;
        Type type = Type::TrackDone;
    };

    // AudioEngine is the audio part of the game engine. Currently it
    // provides 2 audio streams for the game to use:
    // a music stream and an FX (effect) stream. Both streams can be
    // controlled independently and can support an arbitrary number of
    // mixer sources.
    class AudioEngine
    {
    public:
        // The possible effect.
        enum class Effect {
            // Ramp up the stream gain from 0.0f to 1.0f
            FadeIn,
            // Ramp down the stream gain from 1.0f to 0.0f
            FadeOut
        };
        using GraphHandle = std::shared_ptr<const audio::GraphClass>;

        AudioEngine(const std::string& name);
       ~AudioEngine();
        void EnableCaching(bool on_off)
        { mEnableCaching = on_off; }
        void SetLoader(const audio::Loader* loader)
        { mLoader = loader; }
        void SetFormat(const audio::Format& format)
        { mFormat = format; }
        void SetClassLibrary(const ClassLibrary* library)
        { mClassLib = library; }
        void SetBufferSize(unsigned milliseconds)
        { mBufferSize = milliseconds; }
        const ClassLibrary* GetClassLibrary() const
        { return mClassLib; }
        const audio::Loader* GetLoader() const
        { return mLoader; }
        // Start the audio engine. You must call this before calling any
        // actual playback functions.
        void Start();
        // Pause current audio streams.
        void SetDebugPause(bool on_off);
        // Add a new audio graph for music playback.
        // The audio graph is initially only prepared and sent to the audio
        // player but set to paused state. In order to begin playing the track
        // ResumeMusic must be called separately.
        // Returns false if the music graph could not be loaded.
        bool PrepareMusicGraph(const GraphHandle& graph);
        // Similar to PrepareMusicGraph except that also schedules a command
        // to start the music playback after 'when' milliseconds elapses.
        // Returns false if the music graph could not be loaded.
        bool PlayMusic(const GraphHandle& graph, unsigned when = 0);
        // Schedule a command to start playing the named music track that
        // has previously been paused after 'when' milliseconds elapses.
        void ResumeMusic(const std::string& track, unsigned when = 0);
        // Schedule a command to pause the named music track after 'when' milliseconds elapses.
        // Note that this will not remove the music track from the mixer.
        void PauseMusic(const std::string& track, unsigned when = 0);
        // Kill and remove the named music track from the music mixer.
        void KillMusic(const std::string& track, unsigned when = 0);
        // Kill all currently playing music tracks.
        void KillAllMusic(unsigned when = 0);
        // Cancel any pending commands on a music track to play/pause/kill.
        void CancelMusicCmds(const std::string& track);

        // Set an effect on the music tracks audio graph. The effect will take place
        // immediately when the audio is playing.
        void SetMusicEffect(const std::string& track, unsigned duration, Effect effect);
        // Adjust the gain (volume) on the music stream. There's no strict range
        // for the gain value, but you likely want to keep this around (0.0f, 1.0f)
        void SetMusicGain(float gain);
        // Schedule a sound effect for playback after 'when' milliseconds elapse.
        // Returns false if the audio effect could not be loaded.
        bool PlaySoundEffect(const GraphHandle& graph, unsigned when = 0);
        // Adjust the gain (volume) on the effects stream. There's no strict range
        // for the gain value, but you likely want to keep this around (0.0f, 1.0f)
        void SetSoundEffectGain(float gain);
        // Kill all currently playing sound effects.
        void KillAllSoundEffects();

        void EnableEffects(bool on_off)
        { mEnableEffects = on_off; }  
   
        using AudioEventQueue = std::vector<AudioEvent>;
        // Tick the audio engine/player and optionally receive a list of
        // audio events that have happened.
        void Update(AudioEventQueue* events = nullptr);
    private:
        void OnAudioPlayerEvent(const audio::Player::SourceCompleteEvent& event, AudioEventQueue* events);
        void OnAudioPlayerEvent(const audio::Player::SourceEvent& event, AudioEventQueue* events);
    private:
        class AudioBuffer;
        const audio::Loader* mLoader = nullptr;
        const ClassLibrary* mClassLib = nullptr;
        const std::string mName;
        audio::Format mFormat;
        // approximate default audio buffer size in milliseconds.
        unsigned mBufferSize = 20;
        // the audio player.
        std::unique_ptr<audio::Player> mPlayer;
        // Id of the effect audio graph in the audio player.
        std::size_t mEffectGraphId = 0;
        // Id of the music audio graph in the audio player.
        std::size_t mMusicGraphId  = 0;
        // Counter to generate unique element names for effect graphs
        std::size_t mEffectCounter = 0;
        bool mEnableMusic = true;
        bool mEnableEffects = true;
        bool mEnableCaching = false;
    };

} // namespace
