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

#include <list>

#include "base/assert.h"
#include "base/logging.h"
#include "audio/player.h"
#include "audio/device.h"
#include "audio/source.h"
#include "audio/stream.h"
#include "audio/command.h"

namespace {
struct EnqueueCmd {
    std::unique_ptr<audio::Source> source;
    bool looping = false;
    bool paused  = false;
};
} // namespace

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

std::size_t Player::Play(std::unique_ptr<Source> source)
{
    const auto id = trackid_;

    EnqueueCmd cmd;
    cmd.source   = std::move(source);
    cmd.paused   = false; // todo: take as param
    auto cmd_ptr = MakeCommand(std::move(cmd));

    Action action;
    action.track_id = id;
    action.do_what  = Action::Type::Enqueue;
    action.cmd      = cmd_ptr.release(); // remember to delete the raw ptr!
    track_actions_.push(action);

    ++trackid_;
    return id;
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

    struct Track {
        std::size_t id = 0;
        std::shared_ptr<Stream> stream;
        bool paused  = false;
    };
    std::list<Track> playing;    // currently playing tracks

    while (run_thread_.test_and_set(std::memory_order_acquire))
    {
        // iterate audio device state once. (dispatches stream/device state changes)
        dev->Poll();

        // dispatch the queued track actions
        Action track_action;    
        while (track_actions_.pop(track_action))
        {
            std::unique_ptr<Command> cmd(track_action.cmd);
            if (track_action.do_what == Action::Type::Enqueue)
            {
                auto* enqueue_cmd = cmd->GetIf<EnqueueCmd>();
                auto stream = dev->Prepare(std::move(enqueue_cmd->source));
                if (!stream)
                {
                    SourceCompleteEvent event;
                    event.id      = track_action.track_id;
                    event.status  = TrackStatus::Failure;
                    std::lock_guard<std::mutex> lock(event_mutex_);
                    events_.push(std::move(event));
                }
                else
                {
                    Track item;
                    item.id      = track_action.track_id;
                    item.paused  = false;
                    item.stream  = stream;
                    item.stream->Play();
                    playing.push_back(std::move(item));
                }
                continue;
            }

            auto it = std::find_if(std::begin(playing), std::end(playing),
                [=](const Track& t) {
                    return t.id == track_action.track_id;
                });
            if (it == std::end(playing))
                continue;

            auto& p = *it;
            if (track_action.do_what == Action::Type::Pause)
            {
                p.stream->Pause();
                p.paused = true;
            }
            else if (track_action.do_what == Action::Type::Resume)
            {
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
            // propagate events from the stream/source if any.
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
                // generate a track completion event
                SourceCompleteEvent event;
                event.id      = track.id;
                event.status  = state == Stream::State::Complete
                                ? TrackStatus::Success
                                : TrackStatus::Failure;
                std::lock_guard<std::mutex> lock(event_mutex_);
                events_.push(std::move(event));
                source->Shutdown();
                it = playing.erase(it);
            }
            else it++;
        }

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
