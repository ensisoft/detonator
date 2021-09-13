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

#include <cstddef>

namespace audio
{
    // Audio decoder interface for reading encoded audio data
    // such as mp3, ogg or flac.
    class Decoder
    {
    public:
        virtual ~Decoder() = default;
        // Get the audio data sampling rate. The value is in Hz
        // for example 44100 for CD quality.
        virtual unsigned GetSampleRate() const = 0;
        // Get the number of audio channels. Currently supported values
        // are 1 for mono and 2 for stereo. Each audio frame will then
        // contain a sample for each channel.
        virtual unsigned GetNumChannels() const = 0;
        // Get the number of audio frames availablẹ.
        virtual unsigned GetNumFrames() const = 0;
        // Notes about the ReadFrames. Depending on the underlying
        // encoder/decoder implementation it might not be possible
        // to use any read function arbitrarily. For example when using
        // mpg123 the expected read format is given at the decoder
        // creation time and cannot be changed. Thus the caller must
        // make sure to use the right function to read the PCM frames
        // based on the sample type used when creating the decoder!
        // Each read function returns the number of frames read which
        // might be 0 if the the end of stream has been reached.
        // In case of an unexpected error an exception will be thrown.
        virtual size_t ReadFrames(float* ptr, size_t frames) = 0;
        virtual size_t ReadFrames(short* ptr, size_t frames) = 0;
        virtual size_t ReadFrames(int* ptr, size_t frames) = 0;
        // Reset decoder state for looped playback.
        virtual void Reset() = 0;
    protected:
    private:
    };

} // namespace
