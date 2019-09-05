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

#include "config.h"

#include <functional>

#include "base/logging.h"
#include "player.h"
#include "device.h"
#include "sample.h"
#include "stream.h"

namespace audio
{

AudioPlayer::AudioPlayer(std::unique_ptr<AudioDevice> device)
{
    thread_.reset(new std::thread(std::bind(&AudioPlayer::runLoop, this, device.get())));
    device.release();
}

AudioPlayer::~AudioPlayer()
{
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stop_ = true;
        cond_.notify_one();
    }
    thread_->join();
}

std::size_t AudioPlayer::play(std::shared_ptr<AudioSample> sample, std::chrono::milliseconds ms, bool looping)
{
    std::lock_guard<std::mutex> lock(queue_mutex_);

    size_t id = trackid_++;
    Track track;
    track.id       = id;
    track.sample   = sample;
    track.when     = std::chrono::steady_clock::now() + ms;
    track.looping  = looping;
    waiting_.push(std::move(track));

    cond_.notify_one();
    return id;
}

std::size_t AudioPlayer::play(std::shared_ptr<AudioSample> sample, bool looping)
{
    return play(sample, std::chrono::milliseconds(0), looping);
}

void AudioPlayer::pause(std::size_t id)
{
    std::lock_guard<std::mutex> lock(queue_mutex_);

    for (auto& track : playing_)
    {
        if (track.id == id)
            track.stream->pause();
    }
}

void AudioPlayer::resume(std::size_t id)
{
    std::lock_guard<std::mutex> lock(queue_mutex_);

    for (auto& track : playing_)
    {
        if (track.id == id)
            track.stream->resume();
    }
}

bool AudioPlayer::get_event(TrackEvent* event)
{
    std::lock_guard<std::mutex> lock(event_mutex_);
    if (events_.empty())
        return false;
    *event = events_.front();
    events_.pop();
    return true;
}

void AudioPlayer::runLoop(AudioDevice* ptr)
{
    std::unique_ptr<AudioDevice> dev(ptr);
    for (;;)
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (stop_)
            return;

        // todo: get rid of the polling (and the waiting) and move
        // to threaded audio device (or waitable audio device)
        // this wait is here to avoid "overheating the cpu" so to speak.
        // however this creates two problems.
        // a) it increases latency in terms of starting the playback of new audio sample.
        // b) creates possibility for a buffer underruns in the audio playback stream.
        cond_.wait_until(lock, std::chrono::steady_clock::now() +
            std::chrono::milliseconds(10));

        dev->poll();

        for (auto it = std::begin(playing_); it != std::end(playing_);)
        {
            auto& track = *it;
            const auto state = track.stream->state();
            if (state == AudioStream::State::complete ||
                state == AudioStream::State::error)
            {
                TrackEvent event;
                event.id      = track.id;
                event.sample  = track.sample;
                event.when    = track.when;
                event.looping = track.looping;
                event.status  = TrackStatus::Success;
                {
                    std::lock_guard<std::mutex> lock(event_mutex_);
                    events_.push(std::move(event));
                }
            }

            if (state == AudioStream::State::complete)
            {
                if (track.looping)
                {
                    track.stream = dev->prepare(track.sample);
                    track.stream->play();
                    DEBUG("Looping track ...");
                }
                else
                {
                    it = playing_.erase(it);
                }
            }
            else if (state == AudioStream::State::error)
            {
                it = playing_.erase(it);
            }
            else it++;
        }


        if (waiting_.empty())
        {
            if (!playing_.empty())
                continue;

            DEBUG("No audio to play, waiting ....");

            cond_.wait(lock);
        }
        if (waiting_.empty())
            continue;

        auto& top = waiting_.top();
        auto now = std::chrono::steady_clock::now();
        if (playing_.empty())
        {
            auto ret = cond_.wait_until(lock, top.when);
            if (ret == std::cv_status::timeout)
            {
                playTop(*dev);
            }
        }
        else if (now >= top.when)
        {
            playTop(*dev);
        }
    }
}


void AudioPlayer::playTop(AudioDevice& dev)
{
    // top is const...
    auto& top = waiting_.top();
    Track item;
    item.id      = top.id;
    item.sample  = top.sample;
    item.stream  = dev.prepare(top.sample);
    item.when    = top.when;
    item.looping = top.looping;
    item.stream->play();

    waiting_.pop();
    playing_.push_back(std::move(item));
}


} // namespace
