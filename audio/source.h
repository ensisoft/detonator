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
#include <optional>
#include <thread>
#include <queue>
#include <condition_variable>

#include <cmath>

#if defined(AUDIO_ENABLE_TEST_SOUND)
#  include "base/math.h"
#endif

#include "base/assert.h"
#include "audio/command.h"

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
        // The audio sample format.
        enum class Format {
            Float32, Int16, Int32
        };
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

    class SourceThreadProxy : public Source
    {
    public:
        SourceThreadProxy(std::unique_ptr<Source> source);
       ~SourceThreadProxy();
        virtual unsigned GetRateHz() const noexcept override;
        virtual unsigned GetNumChannels() const noexcept override;
        virtual Format GetFormat() const noexcept override;
        virtual std::string GetName() const noexcept override;
        virtual void Prepare(unsigned buffer_size) override;
        virtual unsigned FillBuffer(void* buff, unsigned max_bytes) override;
        virtual bool HasMore(std::uint64_t num_bytes_read) const noexcept override;
        virtual void Shutdown() noexcept override;
        virtual void RecvCommand(std::unique_ptr<Command> cmd) noexcept override;
        virtual std::unique_ptr<Event> GetEvent() noexcept override;
    private:
        void ThreadLoop();
    private:
        const unsigned mSampleRate = 0;
        const unsigned mChannels = 0;
        const Format mFormat = Format::Float32;
        const std::string mName;
        mutable std::mutex mMutex;
        unsigned mBufferSize = 0;
        std::unique_ptr<Source> mSource;
        std::unique_ptr<std::thread> mThread;
        std::queue<std::unique_ptr<Event>> mEvents;
        std::queue<std::unique_ptr<Command>> mCommands;
        std::condition_variable mCondition;
        std::queue<std::vector<char>> mEmptyQueue;
        std::queue<std::vector<char>> mFillQueue;
        std::exception_ptr mException;
        bool mShutdown = false;
        bool mDone = false;
    };

    // AudioFile implements reading audio samples from an
    // encoded audio file on the file system.
    // Supported formats, WAV, OGG, FLAC and MP3. This is the
    // super simple way to play an audio clip directly without
    // using a more complicated audio graph.
    class AudioFile : public Source
    {
    public: 
        // Construct audio file audio sample by reading the contents
        // of the given file. You must call Open before (and check for
        // success) before passing the object to the audio device!
        AudioFile(const std::string& filename, const std::string& name, Format format = Format::Float32);
       ~AudioFile();
        // Get the sample rate in Hz.
        virtual unsigned GetRateHz() const noexcept override;
        // Get the number of channels in the sample
        virtual unsigned GetNumChannels() const noexcept override;
        // Get the PCM byte format of the sample.
        virtual Format GetFormat() const noexcept override;
        // Get the human-readable name of the sample if any.
        virtual std::string GetName() const noexcept override;
        // Fill the buffer with PCM data.
        virtual unsigned FillBuffer(void* buff, unsigned max_bytes) override;
        // Returns true if there's more audio data available
        // or false if the source has been depleted.
        virtual bool HasMore(std::uint64_t num_bytes_read) const noexcept override;
        // Shutdown.
        virtual void Shutdown() noexcept override;
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

#ifdef AUDIO_ENABLE_TEST_SOUND
    class SineGenerator : public Source
    {
    public:
        SineGenerator(unsigned frequency, Format format = Format::Float32)
          : mFrequency(frequency)
          , mFormat(format)
        {}
        SineGenerator(unsigned frequency, unsigned millisecs, Format format = Format::Float32)
          : mFrequency(frequency)
          , mFormat(format)
          , mLimitDuration(true)
          , mDuration(millisecs)
        {}
        virtual unsigned GetRateHz() const noexcept
        { return 44100; }
        virtual unsigned GetNumChannels() const noexcept override
        { return 1; }
        virtual Format GetFormat() const noexcept override
        { return mFormat; }
        virtual std::string GetName() const noexcept override
        { return "Sine"; }
        virtual unsigned FillBuffer(void* buff, unsigned max_bytes) override
        {
            const auto num_channels = 1;
            const auto frame_size = num_channels * ByteSize(mFormat);
            const auto frames = max_bytes / frame_size;
            const auto radial_velocity = math::Pi * 2.0 * mFrequency;
            const auto sample_increment = 1.0/44100.0 * radial_velocity;

            for (unsigned i=0; i<frames; ++i)
            {
                // http://blog.bjornroche.com/2009/12/int-float-int-its-jungle-out-there.html
                const float sample = std::sin(mSampleCounter++ * sample_increment);
                if (mFormat == Format::Float32)
                    ((float*)buff)[i] = sample;
                else if (mFormat == Format::Int32)
                    ((int*)buff)[i] = 0x7fffffff * sample;
                else if (mFormat == Format::Int16)
                    ((short*)buff)[i] = 0x7fff * sample;
            }
            return frames * frame_size;
        }
        virtual bool HasMore(std::uint64_t) const noexcept override
        {
            if (!mLimitDuration) return true;
            const auto seconds = mSampleCounter / 44100.0f;
            const auto millis  = seconds * 1000.0f;
            return millis < mDuration;
        }
        virtual void Shutdown() noexcept override
        { /* intentionally empty */ }
    private:
        const unsigned mFrequency = 0;
        const Format mFormat      = Format::Float32;
        const bool mLimitDuration = false;
        const unsigned mDuration  = 0;
        unsigned mSampleCounter   = 0;
    };
#endif

} // namespace
