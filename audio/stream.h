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

namespace audio
{
    class AudioSource;

    // Audio stream is the currently running state of some 
    // audio stream that exists on the device and is possibly
    // being played back.
    class AudioStream
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
        virtual ~AudioStream() = default;

        // Get current stream state.
        virtual State GetState() const = 0;

        // Gives back the audio source but only when the state of the
        // stream is is either error or complete, i.e. the stream has
        // finished playback and reading data from the source.
        virtual std::unique_ptr<AudioSource> GetFinishedSource() = 0;

        // Get the human readable stream name if any.
        virtual std::string GetName() const = 0;

        // Start playing the audio stream.
        // This should be called only once, when the stream is initially started. 
        // To control the subsequent playback use Pause and Resume.
        virtual void Play() = 0;

        // Pause the stream if currently playing.
        virtual void Pause() = 0;

        // Resume the stream if currently paused.
        virtual void Resume() = 0;
    protected:
    private:
    };

} // namespace