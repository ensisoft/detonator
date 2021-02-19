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
#include <vector>
#include <memory>

#if defined(AUDIO_ENABLE_TEST_SOUND)
#  include "base/math.h"
#  include <cmath>
#endif

namespace audio
{
    // for general audio terminology see the below reference
    // https://larsimmisch.github.io/pyalsaaudio/terminology.html

    // AudioSource provides access to series of buffers
    // of PCM encoded audio sample data.
    class AudioSource
    {
    public:
        // The audio sample format.
        enum class Format {
            // 32bit float Native Endian
            Float32_NE 
        };
        virtual ~AudioSource() = default;

        // Get the sample rate in Hz.
        virtual unsigned GetRateHz() const = 0;
        // Get the number of channels. Typically either 1 for Mono
        // or 2 for stereo sound.
        virtual unsigned GetNumChannels() const = 0;
        // Get the PCM byte format of the sample.
        virtual Format GetFormat() const = 0;
        // Get the (human readable) name of the sample if any
        // For example the underlying filename.
        virtual std::string GetName() const = 0;
        // Fill the given buffer with PCM data. 
        // Returns the number of *bytes* written into buff.
        virtual unsigned FillBuffer(void* buff, unsigned max_bytes) = 0;
        // Returns true if there's more audio data available 
        // or false if the source has been depleted.
        // num bytes is the current number of PCM data extracted
        // and played back from the sample.
        virtual bool HasNextBuffer(std::uint64_t num_bytes_read) const = 0;
        // Reset the sample for looped playback, i.e. possibly rewind
        // to the start of the data.
        virtual void Reset() = 0;
    private:
    };

    // only forward declare, keep the implementation
    // inside a translation unit.
    class SndfileAudioBuffer;

    // AudioFile implements reading audio samples from an
    // encoded audio file on the file system.
    // Supported formats, WAV, OGG, MP3 (todo: check which others)
    class AudioFile : public AudioSource
    {
    public: 
        // Construct audio file audio sample by reading the contents
        // of the given file. If name is empty the name of the file
        // is used instead.
        AudioFile(const std::string& filename, const std::string& name);
       ~AudioFile();

        // Get the sample rate in Hz.
        virtual unsigned GetRateHz() const override;

        // Get the number of channels in the sample
        virtual unsigned GetNumChannels() const override;

        // Get the PCM byte format of the sample.
        virtual Format GetFormat() const override
        { return Format::Float32_NE; }

        // Get the human readable name of the sample if any.
        virtual std::string GetName() const override;

        // Fill the buffer with PCM data.
        virtual unsigned FillBuffer(void* buff, unsigned max_bytes) override;

        // Returns true if there's more audio data available
        // or false if the source has been depleted.
        virtual bool HasNextBuffer(std::uint64_t num_bytes_read) const override;

        // Reset the sample for looped playback, i.e. possibly rewind
        // to the start of the data. 
        virtual void Reset() override;

        // Get the filename.
        std::string GetFilename() const
        { return filename_; }
        
    private:
        const std::string filename_;
        const std::string name_;
        std::unique_ptr<SndfileAudioBuffer> buffer_;
    };

#ifdef AUDIO_ENABLE_TEST_SOUND
    class SineGenerator : public AudioSource
    {
    public:
        SineGenerator(unsigned frequency) : frequency_(frequency)
        {}
        virtual unsigned GetRateHz() const
        { return 44100; }
        virtual unsigned GetNumChannels() const override
        { return 1; }
        virtual Format GetFormat() const override
        { return Format::Float32_NE; }
        virtual std::string GetName() const override
        { return "Sine"; }
        virtual unsigned FillBuffer(void* buff, unsigned max_bytes) override
        {
            const auto num_channels = 1;
            const auto frame_size = num_channels * sizeof(float);
            const auto frames = max_bytes / frame_size;
            const auto radial_velocity = math::Pi * 2.0 * frequency_;
            const auto sample_increment = 1.0/44100.0 * radial_velocity;
            float* ptr = (float*)buff;

            for (unsigned i=0; i<frames; ++i)
            {
                ptr[i] = std::sin(sample_++ * sample_increment);
            }
            return frames * frame_size;
        }
        virtual bool HasNextBuffer(std::uint64_t) const override
        { return true; }
        virtual void Reset() override
        { /* intentionally empty */ }
    private:
        const unsigned frequency_ = 0;
        unsigned sample_ = 0;
    };
#endif

} // namespace
