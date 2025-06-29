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

#include <string>
#include <vector>
#include <memory>

#include "audio/command.h"
#include "audio/buffer.h"
#include "audio/format.h"

namespace audio
{
    class Decoder;

    // for general audio terminology see the below reference
    // https://larsimmisch.github.io/pyalsaaudio/terminology.html

    // Source provides low level access to series of buffers of PCM encoded
    // audio data. This interface is designed for integration against the
    // platforms/OS's audio API and is something that is called by the platform
    // specific audio system. For example on Linux when a pulse audio callback
    // occurs for stream write the data is sourced from the source object in a
    // separate audio/playback thread.
    class Source
    {
    public:
        using Format = audio::SampleType;
        // dtor.
        virtual ~Source() = default;
        // Get the sample rate in Hz.
        virtual unsigned GetRateHz() const noexcept = 0;
        // Get the number of channels. Typically, either 1 for Mono
        // or 2 for stereo sound.
        virtual unsigned GetNumChannels() const noexcept = 0;
        // Get the PCM sample data format.
        virtual Format GetFormat() const noexcept = 0;
        // Get the (human-readable) name of the source if any.
        // Could be for example the underlying filename.
        virtual std::string GetName() const noexcept = 0;
        // Prepare source for device access and playback. After this
        // there will be calls to FillBuffer and HasMore.
        // buffer_size is the maximum expected buffer size that will
        // used when calling FillBuffer.
        virtual void Prepare(unsigned buffer_size)
        { /* intentionally empty  */ }
        // Fill the given device buffer with PCM data.
        // Should return the number of *bytes* written into buff.
        // May throw an exception.
        virtual unsigned FillBuffer(void* buff, unsigned max_bytes) = 0;
        // Returns true if there's more audio data available 
        // or false if the source has been depleted.
        // num bytes is the current number of PCM data (in bytes)
        // extracted and played back from the source.
        virtual bool HasMore(std::uint64_t num_bytes_read) const noexcept = 0;
        // Shutdown the source when playback is finished and the source
        // will no longer be used for playback. (I.e. there will be no
        // more calls to FillBuffer or HasMore)
        virtual void Shutdown() noexcept = 0;
        // Receive and handle a source specific command. A command can be
        // used to for example adjust some source specific parameter.
        virtual void RecvCommand(std::unique_ptr<Command> cmd) noexcept
        { BUG("Unexpected command."); }
        // Get next stream source event if any. Returns nullptr if no
        // event was available.
        virtual std::unique_ptr<Event> GetEvent() noexcept
        { return nullptr; }

        static int ByteSize(Format format) noexcept
        {
            if (format == Format::Float32) return 4;
            else if (format == Format::Int32) return 4;
            else if (format == Format::Int16) return 2;
            else BUG("Unhandled format.");
            return 0;
        }
        static int BuffSize(Format format, unsigned channels, unsigned hz, unsigned  ms)
        {
            const auto samples_per_ms = hz / 1000.0;
            const auto sample_size = ByteSize(format);
            const auto samples = (unsigned)std::ceil(samples_per_ms * ms);
            const auto buff_size = sample_size * samples * channels;
            return buff_size;
        }
    private:
    };

} // namespace
