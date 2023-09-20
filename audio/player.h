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

#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <memory>
#include <variant>

#if defined(AUDIO_LOCK_FREE_QUEUE)
#  include <boost/lockfree/queue.hpp>
#endif

#include "audio/command.h"

namespace audio
{
    class Device;
    class Stream;
    class Source;

    // Play audio samples using the given audio device. Once audio 
    // is played the results are stored in TrackEvents which can be
    // retrieved by a call to GetEvent. The application should 
    // periodically call this function and remove the pending
    // track events and do any processing (such as starting the next
    // audio track) it wishes to do.
    class Player
    {
    public:
        // Track specific playback status.
        enum class TrackStatus {
            // track was played successfully.
            Success,
            // track failed to play
            Failure
        };

        // Completion event of audio source.
        struct SourceCompleteEvent {
            // the id of the track/source that was played.
            std::size_t id = 0;
            // what was the result
            TrackStatus status = TrackStatus::Success;
        };

        // Audio source progress event. This is only generated
        // when the caller has first requested for a progress event
        // through a call to AskProgress.
        struct SourceProgressEvent {
            // The id of the track/source to which the progress pertains.
            std::size_t id = 0;
            // The current stream/source time in milliseconds.
            std::uint64_t time = 0;
            // The number of PCM bytes played so far.
            std::uint64_t bytes = 0;
        };

        // Source event during audio playback.
        struct SourceEvent {
            // the id of the track/source that generated the event.
            std::size_t id = 0;
            // the actual event object. see the source implementations
            // for possible events.
            std::unique_ptr<Event> event;
        };

        using Event = std::variant<SourceCompleteEvent,
                SourceEvent, SourceProgressEvent>;

        // Create a new audio player using the given audio device.
        Player(std::unique_ptr<Device> device);
        // dtor.
       ~Player();

        // Play the audio samples sourced from the source object.
        // Returns an identifier for the audio stream that can then be used
        // to control the playback in call to pause/resume/send command.
        std::size_t Play(std::unique_ptr<Source> source);

        // Pause the audio stream identified by id. 
        void Pause(std::size_t id);

        // Resume the audio stream identified by id.
        void Resume(std::size_t id);

        // Cancel (stop playback and delete the rest of the stream) of the given audio stream.
        void Cancel(std::size_t id);

        // Send a command to the audio stream's source object.
        void SendCommand(std::size_t id, std::unique_ptr<Command> cmd);

        // Ask for a stream progress event for some particular track.
        void AskProgress(std::size_t id);

        // Get next playback event if any.
        // Returns true if there was an event otherwise false.
        bool GetEvent(Event* event);

#if !defined(AUDIO_USE_PLAYER_THREAD)
        void ProcessOnce();
#endif
    private:
        struct Track {
            std::size_t id = 0;
            std::shared_ptr<Stream> stream;
            bool paused  = false;
        };
        void AudioThreadLoop(Device* device);
        void RunAudioUpdateOnce(Device& device, std::list<Track>& track_list);

    private:
        struct Action {
            enum class Type {
                None, Enqueue, Resume, Pause,  Cancel, Command, Progress
            };
            Type do_what = Type::None;
            std::size_t track_id = 0;
            // ugly raw ptr because boost::lockfree::queue requires
            // trivial destructor type trait.
            Command* cmd = nullptr;
        };
        void QueueAction(Action&& action);
        bool DequeueAction(Action* action);
    private:

        // unique track id
        std::size_t trackid_ = 1;

        // queue of track completion events of tracks
        // that were played.
        std::mutex event_mutex_;
        std::queue<Event> events_;

        // if we use an audio thread the possibility is to use a
        // lock free queue or a std::queue + std::mutex.
#if defined(AUDIO_USE_PLAYER_THREAD)
        // audio thread stop flag
        std::atomic_flag run_thread_ = ATOMIC_FLAG_INIT;
        std::unique_ptr<std::thread> thread_;
  #if defined(AUDIO_LOCK_FREE_QUEUE)
        // queue of actions for tracks (pause/resume)
        boost::lockfree::queue<Action> track_actions_;
  #else
        std::mutex action_mutex_;
        std::queue<Action> track_actions_;
  #endif
#else
        std::list<Track> track_list_;
        std::unique_ptr<Device> device_;
        std::queue<Action> track_actions_;
#endif
    };

} // namespace
