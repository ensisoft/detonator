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

#include <functional>

#ifdef POSIX_OS
#  include <pthread.h>
#endif

#include "base/logging.h"
#include "audio/player.h"
#include "audio/device.h"
#include "audio/source.h"
#include "audio/stream.h"

namespace audio
{

Player::Player(std::unique_ptr<Device> device) : track_actions_(128)
{
    run_thread_.test_and_set(std::memory_order_acquire);
    thread_.reset(new std::thread(std::bind(&Player::AudioThreadLoop, this, device.get())));
#ifdef POSIX_OS
    sched_param p;
    p.sched_priority = 99;
    ::pthread_setschedparam(thread_->native_handle(), SCHED_FIFO, &p);
#endif
    device.release();
}

Player::~Player()
{
    // signal the audio thread to exit
    run_thread_.clear(std::memory_order_release);
    thread_->join();
}

std::size_t Player::Play(std::unique_ptr<Source> source, std::chrono::milliseconds ms, bool looping)
{
    std::lock_guard<std::mutex> lock(queue_mutex_);

    size_t id = trackid_++;
    Track track;
    track.id       = id;
    track.source   = std::move(source);
    track.when     = std::chrono::steady_clock::now() + ms;
    track.looping  = looping;
    waiting_.push(std::move(track));
    
    return id;
}

std::size_t Player::Play(std::unique_ptr<Source> source, bool looping)
{
    return Play(std::move(source), std::chrono::milliseconds(0), looping);
}

void Player::Pause(std::size_t id)
{
    Action a;
    a.do_what = Action::Type::Pause;
    a.track_id = id;
    track_actions_.push(a);
}

void Player::Resume(std::size_t id)
{
    Action a;
    a.do_what  = Action::Type::Resume;
    a.track_id = id;
    track_actions_.push(a);
}

void Player::Cancel(std::size_t id)
{
    // todo: should maybe be able to cancel currently waiting tracks too?
    Action a;
    a.do_what = Action::Type::Cancel;
    a.track_id = id;
    track_actions_.push(a);
}

bool Player::GetEvent(TrackEvent* event)
{
    std::lock_guard<std::mutex> lock(event_mutex_);
    if (events_.empty())
        return false;
    *event = std::move(events_.front());
    events_.pop();
    return true;
}

void Player::AudioThreadLoop(Device* ptr)
{
    std::unique_ptr<Device> dev(ptr);

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
            if (state == Stream::State::Complete ||
                state == Stream::State::Error)
            {
                auto source = track.stream->GetFinishedSource();

                if (state == Stream::State::Complete && track.looping)
                {
                    // reset state for replay.
                    source->Reset();

                    track.stream = dev->Prepare(std::move(source));
                    track.stream->Play();
                    DEBUG("Looping track ...");                    
                }
                // generate a track completion event
                TrackEvent event;
                event.id      = track.id;
                event.when    = track.when;
                event.status  = state == Stream::State::Complete
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
            item.stream  = dev->Prepare(std::move(const_cast<pq_value_type&>(top).source));
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
