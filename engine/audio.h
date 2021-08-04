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
#include <variant>

#include "audio/fwd.h"
#include "audio/format.h"
#include "audio/element.h"
#include "audio/player.h"

namespace game
{

    // this is in the namespace scope since it might move to
    // a separate file since scripting interface requires this.
    struct MusicEvent {
        enum class Type {
            TrackDone
        };
        std::string track;
        Type type = Type::TrackDone;
    };
    using AudioEvent = std::variant<MusicEvent>;

    // AudioEngine is the audio part of the game engine. Currently it
    // provides 2 audio streams for the game to use:
    // a music stream and an FX (effect) stream. Both streams can be
    // controlled independently and can support an arbitrary number of
    // mixer sources.
    class AudioEngine
    {
    public:
        using Effect = audio::Effect::Kind;

        AudioEngine(const std::string& name, const audio::Loader* loader);
       ~AudioEngine();

        // Add a new music track identified by name. The name should be unique
        // and can later be used to refer to the track in subsequent calls.
        // The audio track is initially only prepared and sent to the audio
        // player but set to muted state. In order to begin playing the track
        // PlayMusic must be called separately.
        // Returns false if the music track could not be loaded.
        // TODO: URI
        bool AddMusicTrack(const std::string& name, const std::string& uri);
        // Schedule a command to start playing the named music track after
        // 'when' milliseconds elapses.
        void PlayMusic(const std::string& track, std::chrono::milliseconds when);
        // Send a command to start playing the named music track immediately.
        void PlayMusic(const std::string& track);
        // Schedule a command to pause the named music track after 'when' milliseconds elapse.
        // Note that this will not remove the music track from the mixer.
        void PauseMusic(const std::string& track, std::chrono::milliseconds when);
        // Send a command to pause the named music track immediately.
        // Note that this will not remove the music track from the mixer.
        void PauseMusic(const std::string& track);
        // Set an effect on the music tracks audio graph. The effect will take place
        // immediately when the audio is playing.
        void SetMusicEffect(const std::string& track, float duration, Effect effect);
        // Remove the named music track from the music mixer.
        void RemoveMusicTrack(const std::string& name);
        // Adjust the gain (volume) on the music stream. There's no strict range
        // for the gain value but you likely want to keep this around (0.0f, 1.0f)
        void SetMusicGain(float gain);

        // Schedule a sound effect for playback after 'when' milliseconds elapse.
        // Returns false if the audio effect could not be loaded.
        bool PlaySoundEffect(const std::string& uri, std::chrono::milliseconds when);
        // Play a sound effect immediately.
        // Returns false if the audio effect could not be loaded.
        bool PlaySoundEffect(const std::string& uri);
        // Adjust the gain (volume) on the effects stream. There's no strict range
        // for the gain value but you likely want to keep this around (0.0f, 1.0f)
        void SetSoundEffectGain(float gain);

        using AudioEventQueue = std::vector<AudioEvent>;
        // Tick the audio engine/player and optionally receive a list of
        // audio events that have happened. You must call the tick at some
        // decent granularity in order to dispatch the scheduled audio commands
        // without too much latency.
        void Tick(AudioEventQueue* events = nullptr);
    private:
        void OnAudioPlayerEvent(const audio::Player::SourceCompleteEvent& event, AudioEventQueue* events);
        void OnAudioPlayerEvent(const audio::Player::SourceEvent& event, AudioEventQueue* events);
    private:
        class AudioBuffer;
        const audio::Loader* mLoader = nullptr;
        audio::Format mFormat;
        // A pending audio command to be sent to the audio player.
        struct Command {
            std::string dest;
            std::unique_ptr<audio::Element::Command> cmd;
            std::chrono::steady_clock::time_point  when;
            std::size_t track = 0;
            bool operator<(const Command& other) const {
                return when < other.when;
            }
            bool operator>(const Command& other) const {
                return when > other.when;
            }
        };
        using CmdQueue = std::priority_queue<Command, std::vector<Command>,
            std::greater<Command>>;
        // Pending commands that have been scheduled to be sent
        // to the audio player.
        CmdQueue mCommands;
        // the audio player.
        std::unique_ptr<audio::Player> mPlayer;
        // Id of the effect audio graph in the audio player.
        std::size_t mEffectGraphId = 0;
        // Id of the music audio graph in the audio player.
        std::size_t mMusicGraphId  = 0;
        // Counter to generate unique element names for effect graphs
        std::size_t mEffectCounter = 0;
    };

} // namespace
