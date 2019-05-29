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
#include <queue>
#include <memory>
#include <list>
#include <chrono>

namespace invaders
{
    class AudioDevice;
    class AudioStream;
    class AudioSample;

    class AudioPlayer
    {
    public:
        enum class TrackStatus {
            // track was played succesfully.
            Success,
            // track failed to play
            Failure
        };
        // historical event/record of some sample playback.
        // you can get this data through a call to get_event
        struct TrackEvent {
            // the id of the track that was played.
            std::size_t id = 0;
            // the audio sample that was played
            std::shared_ptr<AudioSample> sample;
            // the time point when the track was played.
            std::chrono::steady_clock::time_point when;
            // what was the result
            TrackStatus status = TrackStatus::Success;
            // whether set to looping or not
            bool looping = false;
        };

        AudioPlayer(std::unique_ptr<AudioDevice> device);
       ~AudioPlayer();

        // play the audio sample after the given time elapses.
        // returns an identifier for the audio play back that will happen later.
        // the same id can be used in a call to pause/resume.
        std::size_t play(std::shared_ptr<AudioSample> sample, std::chrono::milliseconds ms, bool looping = false);

        // play the audio sample immediately.
        // returns an identifier for the audio play back that will happen later.
        // the same id can be used in a call to pause/resume.
        std::size_t play(std::shared_ptr<AudioSample> sample, bool looping = false);

        // pause the currently playing audio stream
        void pause(std::size_t id);

        // resume the currently paused audio stream.
        void resume(std::size_t id);

        // get next historical track event.
        // returns true if there was a track event otherwise false.
        bool get_event(TrackEvent* event);

    private:
        void runLoop(AudioDevice* ptr);
        void playTop(AudioDevice& dev);

    private:
        struct Track {
            std::size_t id = 0;
            std::shared_ptr<AudioSample> sample;
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

    private:
        std::unique_ptr<std::thread> thread_;
        std::mutex queue_mutex_;
        std::mutex event_mutex_;

        // unique track id
        std::size_t trackid_ = 1;

        // currently enqueued and waiting tracks.
        std::priority_queue<Track, std::vector<Track>,
            std::greater<Track>> waiting_;

        // currently playing tracks
        std::list<Track> playing_;

        // list of track completion events of tracks
        // that were played.
        std::queue<TrackEvent> events_;

        std::condition_variable cond_;
        bool stop_ = false;
    };


} // namespace