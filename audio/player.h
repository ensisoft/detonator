// Copyright (c) 2014-2018 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#pragma once

#include "config.h"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <memory>
#include <list>
#include <chrono>

#include <boost/lockfree/queue.hpp>

namespace audio
{
    class AudioDevice;
    class AudioStream;
    class AudioSample;

    // Play audio samples using the given audio device. Once audio 
    // is played the results are stored in TrackEvents which can be
    // retrieved by a call to GetEvent. The application should 
    // periodically call this function and remove the pending
    // track events and do any processing (such as starting the next
    // audio track) it wishes to do.
    class AudioPlayer
    {
    public:
        // Track specific playback status.
        enum class TrackStatus {
            // track was played succesfully.
            Success,
            // track failed to play
            Failure
        };
        // Historical event/record of some sample playback.
        // you can get this data through a call to get_event
        struct TrackEvent {
            // the id of the track that was played.
            std::size_t id = 0;
            // the time point when the track was played.
            std::chrono::steady_clock::time_point when;
            // what was the result
            TrackStatus status = TrackStatus::Success;
            // whether set to looping or not
            bool looping = false;
        };

        // Create a new audio player using the given audio device.
        AudioPlayer(std::unique_ptr<AudioDevice> device);
        // dtor.
       ~AudioPlayer();

        // Play the audio sample after the given time elapses (i.e. in X milliseconds after now)
        // Returns an identifier for the audio play back that will happen later.
        // The same id can be used in a call to pause/resume.
        std::size_t Play(std::unique_ptr<AudioSample> sample, std::chrono::milliseconds ms, bool looping = false);

        // Play the audio sample immediately.
        // returns an identifier for the audio play back that will happen later.
        // the same id can be used in a call to pause/resume.
        std::size_t Play(std::unique_ptr<AudioSample> sample, bool looping = false);

        // Pause the audio stream identified by id. 
        void Pause(std::size_t id);

        // Resume the audio stream identified by id.
        void Resume(std::size_t id);

        // Cancel (stop playback and delete the rest of the stream) of the given audio stream.
        void Cancel(std::size_t id);

        // Get next historical track event.
        // Returns true if there was a track event otherwise false.
        bool GetEvent(TrackEvent* event);

    private:
        void AudioThreadLoop(AudioDevice* ptr);

    private:
        struct Track {
            std::size_t id = 0;
            std::unique_ptr<AudioSample> sample;
            std::shared_ptr<AudioStream> stream;
            std::chrono::steady_clock::time_point when;
            bool looping = false;

            bool operator<(const Track& other) const {
                return when < other.when;
            }
            bool operator>(const Track& other) const {
                return when > other.when;
            }
        };
        struct Action {
            enum class Type {
                None, Resume, Pause,  Cancel
            };
            Type do_what = Type::None;
            std::size_t track_id = 0;
        };
    private:
        std::unique_ptr<std::thread> thread_;
        std::mutex queue_mutex_;
        std::mutex event_mutex_;

        // queue of actions for tracks (pause/resume)
        boost::lockfree::queue<Action> track_actions_;
        
        // unique track id
        std::size_t trackid_ = 1;

        // currently enqueued and waiting tracks.
        using audio_pq = std::priority_queue<Track, std::vector<Track>,
            std::greater<Track>>;
        audio_pq waiting_;

        // currently playing tracks
        std::list<Track> playing_;

        // list of track completion events of tracks
        // that were played.
        std::queue<TrackEvent> events_;

        // audio thread stop flag
        std::atomic_flag run_thread_ = ATOMIC_FLAG_INIT;
    };


} // namespace
