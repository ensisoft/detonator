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

#include "base/assert.h"
#include "base/logging.h"
#include "audio/player.h"
#include "audio/device.h"
#include "audio/source.h"
#include "audio/stream.h"
#include "audio/command.h"

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

void Player::SendCommand(std::size_t id, std::unique_ptr<Command> cmd)
{
    Action a;
    a.do_what  = Action::Type::Command;
    a.track_id = id;
    a.cmd      = cmd.release(); // remember to delete the raw ptr!
    track_actions_.push(a);
}
void Player::AskProgress(std::size_t id)
{
    Action a;
    a.do_what  = Action::Type::Progress;
    a.track_id = id;
    track_actions_.push(a);
}

bool Player::GetEvent(Event* event)
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

    // currently playing tracks
    std::list<Track> playing;

    while (run_thread_.test_and_set(std::memory_order_acquire))
    {
        // iterate audio device state once. (dispatches stream/device state changes)
        dev->Poll();

        // dispatch the queued track actions
        Action track_action;    
        while (track_actions_.pop(track_action))
        {
            std::unique_ptr<Command> cmd(track_action.cmd);

            auto it = std::find_if(std::begin(playing), std::end(playing),
                [=](const Track& t) {
                    return t.id == track_action.track_id;
                });
            if (it == std::end(playing))
                continue;

            auto& p = *it;
            if (track_action.do_what == Action::Type::Pause)
            {
                ASSERT(p.paused == false);
                p.stream->Pause();
                p.paused = true;
            }
            else if (track_action.do_what == Action::Type::Resume)
            {
                ASSERT(p.paused == true);
                p.stream->Resume();
                p.paused = false;
            }
            else if (track_action.do_what == Action::Type::Command)
            {
                p.stream->SendCommand(std::move(cmd));
            }
            else if (track_action.do_what == Action::Type::Cancel)
            {
                p.stream->Cancel();
                playing.erase(it);
            }
            else if (track_action.do_what == Action::Type::Progress)
            {
                SourceProgressEvent event;
                event.id    = p.id;
                event.time  = p.stream->GetStreamTime();
                event.bytes = p.stream->GetStreamBytes();
                std::lock_guard<std::mutex> lock(event_mutex_);
                events_.push(std::move(event));
            }
        }

        // realize the state updates (if any) of currently playing audio streams
        // and create outgoing stream events (if any).
        for (auto it = std::begin(playing); it != std::end(playing);)
        {
            auto& track = *it;
            // propagate events
            while (auto event = track.stream->GetEvent())
            {
                SourceEvent ev;
                ev.id    = track.id;
                ev.event = std::move(event);
                std::lock_guard<std::mutex> lock(event_mutex_);
                events_.push(std::move(ev));
            }

            const auto state = track.stream->GetState();
            if (state == Stream::State::Complete || state == Stream::State::Error)
            {
                auto source = track.stream->GetFinishedSource();
                if (state == Stream::State::Complete && track.looping)
                {
                    if (source->Reset())
                    {
                        DEBUG("Looping track %1", track.id);
                        track.stream = dev->Prepare(std::move(source));
                        track.stream->Play();
                    }
                    else
                    {
                        ERROR("Track %1 ('%2') failed to reset. Can't replay.", track.id, source->GetName());
                        track.looping = false;
                    }
                }
                // generate a track completion event
                SourceCompleteEvent event;
                event.id      = track.id;
                event.when    = track.when;
                event.looping = track.looping;
                event.status  = state == Stream::State::Complete ? TrackStatus::Success
                                                                 : TrackStatus::Failure;
                {
                    std::lock_guard<std::mutex> lock(event_mutex_);
                    events_.push(std::move(event));
                }
                if (!track.looping)
                    it = playing.erase(it);
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

            auto stream = dev->Prepare(std::move(const_cast<pq_value_type&>(top).source));
            if (!stream)
            {
                SourceCompleteEvent event;
                event.id   = top.id;
                event.when = top.when;
                event.looping = top.looping;
                event.status  = TrackStatus::Failure;
                std::lock_guard<std::mutex> lock(event_mutex_);
                events_.push(std::move(event));
            }
            else
            {
                Track item;
                item.id      = top.id;
                item.stream  = stream;
                item.when    = top.when;
                item.looping = top.looping;
                item.stream->Play();
                playing.push_back(std::move(item));
            }

            waiting_.pop();
        }
        while (false);

        // this wait is here to avoid "overheating the cpu" so to speak.
        // however this creates two problems.
        // a) it increases latency in terms of starting the playback of new audio sample.
        // b) creates possibility for a buffer underruns in the audio playback stream. 
        // todo: probably need something more sophisticated ?
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    for (auto& p : playing)
    {
        p.stream->Cancel();
    }
}

} // namespace
