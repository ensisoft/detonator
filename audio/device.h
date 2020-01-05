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

#include <memory>

namespace audio
{
    class AudioSample;
    class AudioStream;

    // Access to the native audio playback system
    class AudioDevice
    {
    public:
        // State of the audio device.
        enum class State {
            // Created but not yet initialized.
            None, 
            // Initialized succesfully and currently ready to play audio.
            Ready, 
            // An error has occurred and audio cannot be played.
            Error
        };

        virtual ~AudioDevice() = default;

        // Prepare a new audio stream from the already loaded audio sample.
        // the stream is initially paused but ready to play once play is called.
        virtual std::shared_ptr<AudioStream> Prepare(std::shared_ptr<const AudioSample> sample) = 0;

        // Poll and dispatch pending audio device events.
        // Todo: this needs a proper waiting/signaling mechanism.
        virtual void Poll() = 0;

        // Initialize the audio device.
        // this should be called *once* after the device is created.
        virtual void Init() = 0;

        // Get the current audio device state.
        virtual State GetState() const = 0;

        // Create the appropriate audio device for this platform.
        static
        std::unique_ptr<AudioDevice> Create(const char* appname);

    protected:
    private:
    };

} // namespace