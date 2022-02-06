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
#include <cstdint>

namespace audio
{
    // Compressed source data buffer containing for example
    // OGG, MP3 or flac encoded PCM data.
    class SourceBuffer
    {
    public:
        virtual ~SourceBuffer() = default;
        // Get the read pointer for the contents of the buffer.
        virtual const void* GetData() const = 0;
        // Get the size of the buffer's contents in bytes.
        virtual size_t GetSize() const = 0;
    private:
    };

    class SourceStream
    {
    public:
        // How to seek in the stream.
        enum class Whence {
            // Perform seek from the start of the stream towards the end.
            FromStart,
            // Perform seek relative to the current stream position.
            // The seek can happen in either direction, i.e. towards
            // the start or the end of the stream.
            FromCurrent,
            // Seek from th end towards the start of the stream.
            FromEnd
        };
        virtual ~SourceStream() = default;
        // Read bytes from the current stream position into ptr.
        // Return the number of bytes read.
        virtual std::int64_t Read(void* ptr, std::int64_t bytes) = 0;
        // Tell the current stream position.
        virtual std::int64_t Tell() const = 0;
        // Seek in the stream to an offset relative to whence.
        // Return the current stream position after the seek.
        virtual std::int64_t Seek(std::int64_t offset, Whence whence) = 0;
        // Get the stream content size in bytes.
        virtual std::int64_t GetSize() const = 0;
    private:
    };

    // the buffers are immutable objects and can thus be shared
    // between multiple audio objects simultaneously decoding/sourcing
    // PCM data from them.
    using SourceBufferHandle = std::shared_ptr<const SourceBuffer>;
    // the stream is a stateful object which means that it cannot be shared
    // and must be unique to each audio object.
    using SourceStreamHandle = std::unique_ptr<SourceStream>;

    // default/convenience implementations for accessing audio data.
    SourceStreamHandle OpenFileStream(const std::string& file);
    SourceBufferHandle LoadFileBuffer(const std::string& file);

    // Interface for accessing the encoded source audio data
    // such as .mp3, .ogg etc. files.
    class Loader
    {
    public:
        virtual ~Loader() = default;
        // Open an audio stream to the given file. Returns nullptr (null stream handle)
        // if the stream could not be opened.
        virtual SourceStreamHandle OpenAudioStream(const std::string& file) const
        { return audio::OpenFileStream(file); }
        // Load the contents of the given file into an audio buffer object.
        // Returns nullptr (null stream handle) if the file could not be loaded.
        virtual SourceBufferHandle LoadAudioBuffer(const std::string& file) const
        { return audio::LoadFileBuffer(file); }
    private:
    };

} // namespace
