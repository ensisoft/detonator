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

#ifdef POSIX_OS
#  include <pthread.h>
#endif

#include "base/logging.h"
#include "player.h"
#include "device.h"
#include "sample.h"
#include "stream.h"

namespace audio
{

AudioPlayer::AudioPlayer(std::unique_ptr<AudioDevice> device) : track_actions_(128)
{
    run_thread_.test_and_set(std::memory_order_acquire);
    thread_.reset(new std::thread(std::bind(&AudioPlayer::AudioThreadLoop, this, device.get())));
#ifdef POSIX_OS
    sched_param p;
    p.sched_priority = 99;
    ::pthread_setschedparam(thread_->native_handle(), SCHED_FIFO, &p);
#endif
    device.release();
}

AudioPlayer::~AudioPlayer()
{
    // signal the audio thread to exit
    run_thread_.clear(std::memory_order_release);
    thread_->join();
}

std::size_t AudioPlayer::Play(std::unique_ptr<AudioSample> sample, std::chrono::milliseconds ms, bool looping)
{
    std::lock_guard<std::mutex> lock(queue_mutex_);

    size_t id = trackid_++;
    Track track;
    track.id       = id;
    track.sample   = std::move(sample);
    track.when     = std::chrono::steady_clock::now() + ms;
    track.looping  = looping;
    waiting_.push(std::move(track));
    
    return id;
}

std::size_t AudioPlayer::Play(std::unique_ptr<AudioSample> sample, bool looping)
{
    return Play(std::move(sample), std::chrono::milliseconds(0), looping);
}

void AudioPlayer::Pause(std::size_t id)
{
    Action a;
    a.do_what = Action::Type::Pause;
    a.track_id = id;
    track_actions_.push(a);
}

void AudioPlayer::Resume(std::size_t id)
{
    Action a;
    a.do_what  = Action::Type::Resume;
    a.track_id = id;
    track_actions_.push(a);
}

void AudioPlayer::Cancel(std::size_t id)
{
    // todo: should maybe be able to cancel currently waiting tracks too?
    Action a;
    a.do_what = Action::Type::Cancel;
    a.track_id = id;
    track_actions_.push(a);
}

bool AudioPlayer::GetEvent(TrackEvent* event)
{
    std::lock_guard<std::mutex> lock(event_mutex_);
    if (events_.empty())
        return false;
    *event = std::move(events_.front());
    events_.pop();
    return true;
}

void AudioPlayer::AudioThreadLoop(AudioDevice* ptr)
{
    std::unique_ptr<AudioDevice> dev(ptr);

    while (run_thread_.test_and_set(std::memory_order_acquire))
    {
        // iterate audio device state once. (dispatches stream/device state changes)
        dev->Poll();

        // dispatch the queued track actions
        Action track_action;    
        while (track_actions_.pop(track_action))
        {
            auto it = std::find_if(std::begin(playing_), std::end(playing_),
                [=](const Track& t) {
                    return t.id == track_action.track_id;
                });
            if (it == std::end(playing_))
                continue;

            auto& p = *it;
            if (track_action.do_what == Action::Type::Pause)
                p.stream->Pause();
            else if (track_action.do_what == Action::Type::Resume)
                p.stream->Resume();
            else if (track_action.do_what == Action::Type::Cancel)
            {
                p.stream->Pause();
                playing_.erase(it);
            }
        }

        // realize the state updates (if any) of currently playing audio  streams
        // and create outgoing stream events (if any).
        for (auto it = std::begin(playing_); it != std::end(playing_);)
        {
            auto& track = *it;
            const auto state = track.stream->GetState();
            if (state == AudioStream::State::Complete ||
                state == AudioStream::State::Error)
            {
                auto sample = track.stream->GetFinishedSample();

                if (state == AudioStream::State::Complete && track.looping)
                {
                    // reset sample state for replay.
                    sample->Reset();

                    track.stream = dev->Prepare(std::move(sample));
                    track.stream->Play();
                    DEBUG("Looping track ...");                    
                }
                // generate a track completion event
                TrackEvent event;
                event.id      = track.id;
                event.when    = track.when;
                event.status  = state == AudioStream::State::Complete 
                    ? TrackStatus::Success 
                    : TrackStatus::Failure; 
                event.looping = track.looping;
                {
                    std::lock_guard<std::mutex> lock(event_mutex_);
                    events_.push(std::move(event));
                }
                if (!track.looping)
                    it = playing_.erase(it);
            }
            else it++;
        }
        // service the waiting queue once
        do 
        {
            // don't wait around if the lock is contended but   
            // instead just proceed to service the currently playing
            // audio streams.
            std::unique_lock<std::mutex> lock(queue_mutex_, std::try_to_lock);
            if (!lock.owns_lock())
                break;

            if (waiting_.empty())
                break;
    
            // look at the top item (needs to play earliest)
            auto& top = waiting_.top();
            auto now  = std::chrono::steady_clock::now();
            if (now < top.when)
                break;

            // std::priority_queue only provides a const public interface
            // because it tries to protect from people changing parameters
            // of the contained objects so that the priority order invariable
            // would no longer hold. however this is also super annoying
            // in cases where you for example want to get a std::unique_ptr<T>
            // out of the damn thing. 
            using pq_value_type = std::remove_cv<audio_pq::value_type>::type;
            
            Track item;
            item.id      = top.id;
            item.stream  = dev->Prepare(std::move(const_cast<pq_value_type&>(top).sample));
            item.when    = top.when;
            item.looping = top.looping;
            item.stream->Play();
            waiting_.pop();
            playing_.push_back(std::move(item));            
        }
        while (false);

        // this wait is here to avoid "overheating the cpu" so to speak.
        // however this creates two problems.
        // a) it increases latency in terms of starting the playback of new audio sample.
        // b) creates possibility for a buffer underruns in the audio playback stream. 
        // todo: probably need something more sophisticated ?
        std::this_thread::sleep_for(std::chrono::milliseconds(playing_.empty() ? 5 : 1));
    }
}

} // namespace
