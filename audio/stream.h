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

#include <string>

namespace audio
{
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