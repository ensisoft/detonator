// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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
#include <string>
#include <cstddef>

#include "audio/source.h"
#include "audio/format.h"

namespace audio
{
    class Decoder;

    // AudioFileSource implements reading audio samples from an
    // encoded audio file on the file system.
    // Supported formats, WAV, OGG, FLAC and MP3. This is the
    // super simple way to play an audio clip directly without
    // using a more complicated audio graph.
    class AudioFileSource : public Source
    {
    public:
        // Construct audio file audio sample by reading the contents
        // of the given file. You must call Open before (and check for
        // success) before passing the object to the audio device!
        AudioFileSource(const std::string& filename, const std::string& name, Format format = Format::Float32);
       ~AudioFileSource() override;
        // Get the sample rate in Hz.
        unsigned GetRateHz() const noexcept override;
        // Get the number of channels in the sample
        unsigned GetNumChannels() const noexcept override;
        // Get the PCM byte format of the sample.
        Format GetFormat() const noexcept override;
        // Get the human-readable name of the sample if any.
        std::string GetName() const noexcept override;
        // Fill the buffer with PCM data.
        unsigned FillBuffer(void* buff, unsigned max_bytes) override;
        // Returns true if there's more audio data available
        // or false if the source has been depleted.
        bool HasMore(std::uint64_t num_bytes_read) const noexcept override;
        // Shutdown.
        void Shutdown() noexcept override;
        // Try to open the audio file for reading. Returns true if
        // successful otherwise false and error is logged.
        bool Open();

        // Set the number of loops (the number of times) the file is
        // to be played. pass 0 for "infinite" looping.
        void SetLoopCount(unsigned count)
        { mLoopCount = count; }
    private:
        const std::string mFilename;
        const std::string mName;
        const Format mFormat = Format::Float32;
        std::unique_ptr<Decoder> mDecoder;
        std::size_t mFrames = 0;
        unsigned mLoopCount = 1;
        unsigned mPlayCount = 0;
    };

} // namespace