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

#include <memory>

namespace audio
{
    class Source;
    class Stream;

    // Access to the native audio playback system
    class Device
    {
    public:
        // State of the audio device.
        enum class State {
            // Created but not yet initialized.
            None, 
            // Initialized successfully and currently ready to play audio.
            Ready, 
            // An error has occurred and audio cannot be played.
            Error
        };

        virtual ~Device() = default;

        // Prepare a new audio stream from the already loaded audio sample.
        // the stream is initially paused but ready to play once play is called.
        // should return nullptr if the stream failed to prepare.
        virtual std::shared_ptr<Stream> Prepare(std::unique_ptr<Source> sample) = 0;

        // Poll and dispatch pending audio device events.
        // Todo: this needs a proper waiting/signaling mechanism.
        virtual void Poll() = 0;

        // Initialize the audio device.
        // this should be called *once* after the device is created.
        virtual void Init() = 0;

        // Get the current audio device state.
        virtual State GetState() const = 0;

        // Set the requested default audio buffer size defined in milliseconds.
        // The bigger, the buffer size the more latency there can be and the less
        // accurate the stream timing since the stream time is derived from the
        // buffer playback. On the other hand if the buffer size is too small
        // there's a risk of buffer underruns whenever the system fails to deliver
        // audio buffers fast enough. Recommended value is around 20ms which seems
        // to work quite well so far.
        virtual void SetBufferSize(unsigned milliseconds) = 0;

        // Create the appropriate audio device for this platform.
        static
        std::unique_ptr<Device> Create(const char* appname);

    protected:
    private:
    };

} // namespace
