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

#include <fstream>
#include <memory>
#include <string>
#include <cstdint>

namespace audio
{
    // Compressed source data buffer containing for example
    // OGG, MP3 or flac encoded PCM data.
    class SourceStream
    {
    public:
        virtual ~SourceStream() = default;
        // Get the read pointer for the contents of the buffer.
        virtual void Read(void* ptr, uint64_t offset, uint64_t bytes) const = 0;
        // Get the size of the buffer's contents in bytes.
        virtual std::uint64_t GetSize() const = 0;
        virtual std::string GetName() const = 0;
    private:
    };

    // the buffers are immutable objects and can thus be shared
    // between multiple audio objects simultaneously decoding/sourcing
    // PCM data from them.
    using SourceStreamHandle = std::shared_ptr<const SourceStream>;

    // Different potential ways to perform IO and load/stream
    // the audio data from a file.
    enum class IOStrategy {
        // Do whatever is the default.
        Default,
        // Try to be smart and automatically decide what is
        // a good way with the given file on the given platform.
        Automatic,
        // Use memory mapped file.
        Memmap,
        // Use a file stream.
        Stream,
        // Load the data to a regular buffer.
        Buffer
    };

    SourceStreamHandle OpenFileStream(const std::string& file,
        IOStrategy strategy = IOStrategy::Default, bool enable_file_caching = false);

    // Interface for accessing the encoded source audio data
    // such as .mp3, .ogg etc. files.
    class Loader
    {
    public:
        using AudioIOStrategy = audio::IOStrategy;
        virtual ~Loader() = default;
        // Load the contents of the given file into an audio buffer object.
        // Returns nullptr (null stream handle) if the file could not be loaded.
        virtual SourceStreamHandle OpenAudioStream(const std::string& file,
            AudioIOStrategy strategy = AudioIOStrategy::Default,
            bool enable_file_caching= false)  const
        { return audio::OpenFileStream(file, strategy, enable_file_caching); }
    private:
    };

} // namespace
