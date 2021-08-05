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

#include <string>
#include <cstdint>

#include "audio/command.h"

namespace audio
{
    class Source;

    // Audio stream is the currently running state of some 
    // audio stream that exists on the audio device. Typically
    // represents some audio stream/connection to the platform
    // specific audio system such as Pulseaudio or Waveout.
    class Stream
    {
    public:
        // State of the stream.
        enum class State {
            // Initial state, stream object exists but the stream
            // is not yet available on the actual audio device.
            None, 
            // Stream exists on the device and is ready play.
            Ready, 
            // An error has occurred.
            Error,
            // Stream playback is complete.
            Complete
        };
        // dtor.
        virtual ~Stream() = default;
        // Get current stream state.
        virtual State GetState() const = 0;
        // Get back the audio source but only when the state of the
        // stream is is either error or complete, i.e. the stream has
        // finished playback and reading data from the source.
        virtual std::unique_ptr<Source> GetFinishedSource() = 0;
        // Get the human readable stream name if any.
        virtual std::string GetName() const = 0;
        // Get the current stream time in milliseconds.
        virtual std::uint64_t GetStreamTime() const = 0;
        // Get the current number of bytes processed by the stream.
        virtual std::uint64_t GetStreamBytes() const = 0;
        // Start playing the audio stream.
        // This should be called only once, when the stream is initially started. 
        // To control the subsequent playback use Pause and Resume.
        virtual void Play() = 0;
        // Pause the stream if currently playing.
        virtual void Pause() = 0;
        // Resume the stream if currently paused.
        virtual void Resume() = 0;
        // Cancel the stream including any pending playback immediately.
        // This is called before the stream is deleted when stopping
        // the stream during playback. In other words if the stream has
        // already completed (either successfully or by error) cancel is
        // not called.
        virtual void Cancel() = 0;
        // Send command to the stream source.
        virtual void SendCommand(std::unique_ptr<Command> cmd) = 0;
        // Get next stream event if any. Returns nullptr if no
        // event was available.
        virtual std::unique_ptr<Event> GetEvent() = 0;
    protected:
    private:
    };

} // namespace