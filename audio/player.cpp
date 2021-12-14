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

Player::Player(std::unique_ptr<Device> device)
#if defined(AUDIO_LOCK_FREE_QUEUE)
  : track_actions_(128)
#endif
{
#if defined(AUDIO_USE_PLAYER_THREAD)
    run_thread_.test_and_set(std::memory_order_acquire);
    thread_.reset(new std::thread(std::bind(&Player::AudioThreadLoop, this, device.get())));
    device.release();
#else
    device->Init();
    device_ = std::move(device);
#endif
}

Player::~Player()
{
#if defined(AUDIO_USE_PLAYER_THREAD)
    // signal the audio thread to exit
    run_thread_.clear(std::memory_order_release);
    thread_->join();
#else
    for (auto& p : track_list_)
    {
        p.stream->Cancel();
    }
    track_list_.clear();
    device_.reset();
#endif
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
    QueueAction(std::move(action));
    ++trackid_;
    return id;
}

void Player::Pause(std::size_t id)
{
    Action a;
    a.do_what = Action::Type::Pause;
    a.track_id = id;
    QueueAction(std::move(a));
}

void Player::Resume(std::size_t id)
{
    Action a;
    a.do_what  = Action::Type::Resume;
    a.track_id = id;
    QueueAction(std::move(a));
}

void Player::Cancel(std::size_t id)
{
    Action a;
    a.do_what = Action::Type::Cancel;
    a.track_id = id;
    QueueAction(std::move(a));
}

void Player::SendCommand(std::size_t id, std::unique_ptr<Command> cmd)
{
    Action a;
    a.do_what  = Action::Type::Command;
    a.track_id = id;
    a.cmd      = cmd.release(); // remember to delete the raw ptr!
    QueueAction(std::move(a));
}
void Player::AskProgress(std::size_t id)
{
    Action a;
    a.do_what  = Action::Type::Progress;
    a.track_id = id;
    QueueAction(std::move(a));
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

#if !defined(AUDIO_USE_PLAYER_THREAD)
void Player::ProcessOnce()
{
    RunAudioUpdateOnce(*device_, track_list_);
}
#endif

void Player::QueueAction(Action&& action)
{
#if defined(AUDIO_LOCK_FREE_QUEUE)
    track_actions_.push(std::move(action));
#else
    std::unique_lock<decltype(action_mutex_)> lock(action_mutex_);
    track_actions_.push(std::move(action));
#endif
}

bool Player::DequeueAction(Action* action)
{
#if defined(AUDIO_LOCK_FREE_QUEUE)
    return track_actions_.pop(*action);
#else
    std::unique_lock<decltype(action_mutex_)> lock(action_mutex_);
    if (track_actions_.empty())
        return false;
    *action = track_actions_.front();
    track_actions_.pop();
    return true;
#endif
}

void Player::AudioThreadLoop(Device* device)
{
#if defined(AUDIO_USE_PLAYER_THREAD)
    DEBUG("Hello from audio player thread.");
    try
    {
        std::unique_ptr<Device> raii(device);

        // call device init on *this* thread in case the device
        // has thread affinity.
        device->Init();

        std::list<Track> track_list;

        while (run_thread_.test_and_set(std::memory_order_acquire))
        {
            RunAudioUpdateOnce(*device, track_list);
            // this wait is here to avoid "overheating the cpu" so to speak.
            // however this creates two problems.
            // a) it increases latency in terms of starting the playback of new audio sample.
            // b) creates possibility for a buffer underruns in the audio playback stream.
            // todo: probably need something more sophisticated ?
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        // cancel pending audio streams
        for (auto& p: track_list)
        {
            p.stream->Cancel();
        }
    }
    catch (const std::exception& e)
    {
        ERROR("Audio thread error. [error=%1]", e.what());
    }
    DEBUG("Audio player thread exiting.");
#endif
}

void Player::RunAudioUpdateOnce(Device& device, std::list<Track>& track_list)
{
    // iterate audio device state once. (dispatches stream/device state changes)
    device.Poll();

    // dispatch the queued track actions
    Action track_action;
    while (DequeueAction(&track_action))
    {
        std::unique_ptr<Command> cmd(track_action.cmd);
        if (track_action.do_what == Action::Type::Enqueue)
        {
            auto* enqueue_cmd = cmd->GetIf<EnqueueCmd>();
            auto stream = device.Prepare(std::move(enqueue_cmd->source));
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
                track_list.push_back(std::move(item));
            }
            continue;
        }

        auto it = std::find_if(std::begin(track_list), std::end(track_list),
                               [=](const Track& t) {
                                   return t.id == track_action.track_id;
                               });
        if (it == std::end(track_list))
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
            track_list.erase(it);
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
    for (auto it = std::begin(track_list); it != std::end(track_list);)
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
            it = track_list.erase(it);
        }
        else it++;
    }
}

} // namespace
