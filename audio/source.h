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
#include <vector>
#include <memory>

#if defined(AUDIO_ENABLE_TEST_SOUND)
#  include "base/math.h"
#  include <cmath>
#endif

#include "base/assert.h"

namespace audio
{
    class Decoder;

    // for general audio terminology see the below reference
    // https://larsimmisch.github.io/pyalsaaudio/terminology.html

    // Source provides low level device access to series of buffers of
    // PCM encoded audio data. This interface is designed for integration
    // against the platforms/OS's audio API and is something that is called
    // by the platform specific audio system. For example on Linux the when
    // a pulse audio callback occurs for stream write the data is sourced
    // from the source object in an audio thread.
    class Source
    {
    public:
        // The audio sample format.
        enum class Format {
            Float32, Int16, Int32
        };
        // dtor.
        virtual ~Source() = default;
        // Get the sample rate in Hz.
        virtual unsigned GetRateHz() const noexcept = 0;
        // Get the number of channels. Typically either 1 for Mono
        // or 2 for stereo sound.
        virtual unsigned GetNumChannels() const noexcept = 0;
        // Get the PCM byte format of the sample.
        virtual Format GetFormat() const noexcept = 0;
        // Get the (human readable) name of the sample if any
        // For example the underlying filename.
        virtual std::string GetName() const noexcept = 0;
        // Fill the given device buffer with PCM data.
        // Returns the number of *bytes* written into buff.
        // May throw an exception.
        virtual unsigned FillBuffer(void* buff, unsigned max_bytes) = 0;
        // Returns true if there's more audio data available 
        // or false if the source has been depleted.
        // num bytes is the current number of PCM data extracted
        // and played back from the sample.
        virtual bool HasNextBuffer(std::uint64_t num_bytes_read) const noexcept = 0;
        // Reset the sample for looped playback, i.e. possibly rewind
        // to the start of the data. Should return true on success.
        virtual bool Reset() noexcept = 0;

        static int ByteSize(Format format) noexcept
        {
            if (format == Format::Float32) return 4;
            else if (format == Format::Int32) return 4;
            else if (format == Format::Int16) return 2;
            else BUG("Unhandled format.");
            return 0;
        }
    private:
    };

    // AudioFile implements reading audio samples from an
    // encoded audio file on the file system.
    // Supported formats, WAV, OGG, MP3 (todo: check which others)
    class AudioFile : public Source
    {
    public: 
        // Construct audio file audio sample by reading the contents
        // of the given file. You must call Open before (and check for
        // success) before passing the object to the audio device!
        AudioFile(const std::string& filename, const std::string& name);
       ~AudioFile();
        // Get the sample rate in Hz.
        virtual unsigned GetRateHz() const noexcept override;
        // Get the number of channels in the sample
        virtual unsigned GetNumChannels() const noexcept override;
        // Get the PCM byte format of the sample.
        virtual Format GetFormat() const noexcept override;
        // Get the human readable name of the sample if any.
        virtual std::string GetName() const noexcept override;
        // Fill the buffer with PCM data.
        virtual unsigned FillBuffer(void* buff, unsigned max_bytes) override;
        // Returns true if there's more audio data available
        // or false if the source has been depleted.
        virtual bool HasNextBuffer(std::uint64_t num_bytes_read) const noexcept override;
        // Reset the sample for looped playback, i.e. possibly rewind
        // to the start of the data.
        virtual bool Reset() noexcept override;

        // Get the filename.
        std::string GetFilename() const noexcept
        { return filename_; }
        void SetFormat(Format format) noexcept
        { format_ = format; }

        // Try to open the audio file for reading. Returns true if
        // successful otherwise false and error is logged.
        bool Open();
    private:
        const std::string filename_;
        const std::string name_;
        Format format_ = Format::Float32;
        std::unique_ptr<Decoder> decoder_;
        std::size_t frames_ = 0;
    };

#ifdef AUDIO_ENABLE_TEST_SOUND
    class SineGenerator : public Source
    {
    public:
        SineGenerator(unsigned frequency) : frequency_(frequency)
        {}
        virtual unsigned GetRateHz() const noexcept
        { return 44100; }
        virtual unsigned GetNumChannels() const noexcept override
        { return 1; }
        virtual Format GetFormat() const noexcept override
        { return format_; }
        virtual std::string GetName() const noexcept override
        { return "Sine"; }
        virtual unsigned FillBuffer(void* buff, unsigned max_bytes) override
        {
            const auto num_channels = 1;
            const auto frame_size = num_channels * ByteSize(format_);
            const auto frames = max_bytes / frame_size;
            const auto radial_velocity = math::Pi * 2.0 * frequency_;
            const auto sample_increment = 1.0/44100.0 * radial_velocity;

            for (unsigned i=0; i<frames; ++i)
            {
                // http://blog.bjornroche.com/2009/12/int-float-int-its-jungle-out-there.html
                const float sample = std::sin(sample_++ * sample_increment);
                if (format_ == Format::Float32)
                    ((float*)buff)[i] = sample;
                else if (format_ == Format::Int32)
                    ((int*)buff)[i] = 0x7fffffff * sample;
                else if (format_ == Format::Int16)
                    ((short*)buff)[i] = 0x7fff * sample;
            }
            return frames * frame_size;
        }
        virtual bool HasNextBuffer(std::uint64_t) const noexcept override
        { return true; }
        virtual bool Reset() noexcept override
        { return true; /* intentionally empty */ }
        void SetFormat(Format format)
        { format_ = format; }
    private:
        const unsigned frequency_ = 0;
        unsigned sample_ = 0;
        Format format_ = Format::Float32;
    };
#endif

} // namespace
