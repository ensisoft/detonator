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

#include "config.h"

#if defined(AUDIO_USE_OPENAL)
#  include <AL/al.h>
#  include <AL/alc.h>
#endif

#include <list>

#include "base/assert.h"
#include "base/logging.h"
#include "audio/device.h"
#include "audio/source.h"
#include "audio/stream.h"

#if defined(AUDIO_USE_OPENAL)

namespace {
// Audio device implementation using OpenAL
class OpenALDevice : public audio::Device
{
public:
    OpenALDevice(const char* name)
    {}

    ~OpenALDevice()
    {
        if (mContext)
            alcDestroyContext(mContext);
        if (mDevice)
            alcCloseDevice(mDevice);
    }
    virtual std::shared_ptr<audio::Stream> Prepare(std::unique_ptr<audio::Source> source) override
    {
        const auto& name = source->GetName();
        try
        {
            auto stream = std::make_shared<PlaybackStream>(std::move(source), mDevice, mContext, mBufferSize);
            mStreams.push_back(stream);
            return stream;
        }
        catch (const std::exception& e)
        { ERROR("Audio source failed to prepare: [name=%1]", name); }
        return nullptr;
    }
    virtual void Poll() override
    {
        // poll each stream for a state update.
        for (auto it = mStreams.begin(); it != mStreams.end();)
        {
            auto& stream_ref = *it;
            if (stream_ref.expired())
            {
                it = mStreams.erase(it);
                continue;
            }
            auto stream = stream_ref.lock();
            stream->Poll();
            ++it;
        }
    }
    virtual void Init() override
    {
        const auto* default_device = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
        DEBUG("Using OpenAL default device. [name=%1]", default_device);
        mDevice = alcOpenDevice(default_device);
        if (!mDevice)
            throw std::runtime_error("failed to open OpenAL audio device.");
        mContext = alcCreateContext(mDevice, NULL);
        if (!mContext)
            throw std::runtime_error("failed to create OpenAL audio context.");

        // todo: refactor this, this cannot work if there ever are more than one device.
        alcMakeContextCurrent(mContext);

        mState = State::Ready;
    }
    virtual State GetState() const override
    {
        return mState;
    }
    virtual void SetBufferSize(unsigned milliseconds) override
    {
        mBufferSize = milliseconds;
    }
private:
    class PlaybackStream : public audio::Stream
    {
    public:
        PlaybackStream(std::unique_ptr<audio::Source> source, ALCdevice* device, ALCcontext* context, unsigned buffer_duration)
          : mBufferDuration(buffer_duration)
          , mSource(std::move(source))
          , mDevice(device)
          , mContext(context)
        {
            const auto channels   = mSource->GetNumChannels();
            const auto samplerate = mSource->GetRateHz();
            const auto format     = mSource->GetFormat();
            DEBUG("Creating new OpenAL playback stream. [name='%1', channels=%2, rate=%3, format=%4]",
                  mSource->GetName(), mSource->GetNumChannels(),
                  mSource->GetRateHz(), mSource->GetFormat());

            const auto valid_channels = channels == 1 || channels == 2;
            const auto valid_format   = format == audio::Source::Format::Float32 ||
                                        format == audio::Source::Format::Int16;
            const auto valid_rate     = samplerate == 8000 || samplerate == 16000 ||
                                        samplerate == 11025 || samplerate == 22050 ||
                                        samplerate == 32000 || samplerate == 44100 ||
                                        samplerate == 48000 || samplerate == 88200;
            if (!valid_channels || !valid_format || !valid_rate)
                throw std::runtime_error("invalid OpenAL audio format");

            // we throw an exception here so that the semantics are similar to what
            // happens on Pulseaudio/Waveout. Those APIs have checks in their stream
            // creation that fill then indicate an error if we pass garbage format or
            // parameters for stream creation. It might make sense however to move
            // the checking for valid input formats somewhere else (higher in the stack).

            // todo: error checking
            alGenSources(1, &mHandle);
            alGenBuffers(NumBuffers, mBuffers);
            DEBUG("OpenAL stream source handle. [handle=%1]", mHandle);
        }
       ~PlaybackStream()
        {
            alDeleteSources(1, &mHandle);
            alDeleteBuffers(NumBuffers, mBuffers);
            DEBUG("OpenAL stream delete. [handle=%1]", mHandle);
        }
        virtual State GetState() const override
        { return mState; }
        virtual std::unique_ptr<audio::Source> GetFinishedSource() override
        { return std::move(mSource); }
        virtual std::string GetName() const override
        { return mSource->GetName(); }
        virtual std::uint64_t GetStreamTime() const override
        { return mCurrentTime; }
        virtual std::uint64_t GetStreamBytes() const override
        { return mCurrentBytes;}
        virtual void Play() override
        {
            const auto sample_size    = audio::Source::ByteSize(mSource->GetFormat());
            const auto samples_per_ms = mSource->GetRateHz() / 1000u;
            const auto bytes_per_ms   = mSource->GetNumChannels() * sample_size * samples_per_ms;
            const auto buffer_size    = mBufferDuration * bytes_per_ms;
            const auto buffer_format  = GetOpenALFormat();

            // enter initial play state. fill and enqueue all source buffers
            // to the source queue and then start playback.
            try
            {
                // the core OpenAL API seems to have only alBufferData and no way to
                // have a the audio frame work provided buffer
                std::vector<char> buffer;
                buffer.resize(buffer_size);

                for (int i = 0; i < NumBuffers; ++i)
                {
                    const auto buffer_handle = mBuffers[i];
                    if (mSource->HasMore(mCurrentBytes))
                    {
                        const auto pcm_bytes = mSource->FillBuffer(&buffer[0], buffer_size);
                        alBufferData(buffer_handle, buffer_format, &buffer[0], pcm_bytes, mSource->GetRateHz());
                        alSourceQueueBuffers(mHandle, 1, &buffer_handle);
                        const auto milliseconds = pcm_bytes / bytes_per_ms;
                        mCurrentBytes += pcm_bytes;
                        mCurrentTime += milliseconds;
                    }
                }
                // start playback.
                alSourcePlay(mHandle);
            }
            catch (const std::exception& e)
            {
                ERROR("Audio stream error. [name='%1', error='%2']", mSource->GetName(), e.what());
                mState = State::Error;
            }
        }
        virtual void Pause() override
        {
            alSourcePause(mHandle);
        }
        virtual void Resume() override
        {
            alSourcePlay(mHandle);
        }
        virtual void Cancel() override
        {
            alSourceStop(mHandle);
        }
        virtual void SendCommand(std::unique_ptr<audio::Command> cmd) override
        {
            mSource->RecvCommand(std::move(cmd));
        }
        virtual std::unique_ptr<audio::Event> GetEvent() override
        {
            return mSource->GetEvent();
        }

        void Poll()
        {
            const auto sample_size    = audio::Source::ByteSize(mSource->GetFormat());
            const auto samples_per_ms = mSource->GetRateHz() / 1000u;
            const auto bytes_per_ms   = mSource->GetNumChannels() * sample_size * samples_per_ms;
            const auto buffer_size    = mBufferDuration * bytes_per_ms;
            const auto buffer_format  = GetOpenALFormat();

            try
            {
                ALint buffers_processed = 0;
                ALint buffers_queued    = 0;
                ALuint buffer_handles[NumBuffers] = {};
                // number of buffers that have been processed.
                alGetSourcei(mHandle, AL_BUFFERS_PROCESSED, &buffers_processed);
                // number of buffers that are still queued.
                alGetSourcei(mHandle, AL_BUFFERS_QUEUED, &buffers_queued);

                if (buffers_queued == 0)
                {
                    if (!mSource->HasMore(mCurrentBytes))
                    {
                        mState = State::Complete;
                        return;
                    }
                    else
                    {
                        // if source has more but the AL source queue has drained
                        // we're likely too slow and are having a buffer underrun :<
                        WARN("OpenAL stream encountered likely audio buffer underrun. [name='%1']", mSource->GetName());
                    }
                }
                // the core OpenAL API seems to have only alBufferData and no way to
                // have a the audio frame work provided buffer
                std::vector<char> buffer;
                buffer.resize(buffer_size);

                // get the handles of the buffers that are now free and
                // fill them with more PCM data and then queue back into the
                // source queue.
                alSourceUnqueueBuffers(mHandle, buffers_processed, buffer_handles);
                for (int i = 0; i < buffers_processed; ++i)
                {
                    const auto buffer_handle = buffer_handles[i];
                    if (mSource->HasMore(mCurrentBytes))
                    {
                        const auto pcm_bytes = mSource->FillBuffer(&buffer[0], buffer_size);
                        alBufferData(buffer_handle, buffer_format, &buffer[0], pcm_bytes, mSource->GetRateHz());
                        alSourceQueueBuffers(mHandle, 1, &buffer_handle);
                        const auto milliseconds   = pcm_bytes / bytes_per_ms;
                        mCurrentBytes += pcm_bytes;
                        mCurrentTime  += milliseconds;
                    }
                }
            }
            catch (const std::exception& e)
            {
                ERROR("Audio stream error. [name='%1', error='%2']", mSource->GetName(), e.what());
                mState = State::Error;
            }
        }
    private:
        ALenum GetOpenALFormat() const
        {
// these require AL_EXT_float32 actually.
#define AL_FORMAT_MONO_FLOAT32                   0x10010
#define AL_FORMAT_STEREO_FLOAT32                 0x10011
            const auto format   = mSource->GetFormat();
            const auto channels = mSource->GetNumChannels();
            if (channels == 1)
            {
                if (format == audio::Source::Format::Int16)
                    return AL_FORMAT_MONO16;
                else if (format == audio::Source::Format::Float32)
                    return AL_FORMAT_MONO_FLOAT32;
            }
            else if (channels == 2)
            {
                if (format == audio::Source::Format::Int16)
                    return AL_FORMAT_STEREO16;
                else if (format == audio::Source::Format::Float32)
                    return AL_FORMAT_STEREO_FLOAT32;
            }
            BUG("Unrecognized audio format.");
            return 0;
        }

    private:
        static constexpr auto NumBuffers = 5;
        const unsigned mBufferDuration = 0;
        audio::Stream::State mState = audio::Stream::State::None;
        std::unique_ptr<audio::Source> mSource;
        // stream time in milliseconds based on the PCM data
        std::uint64_t mCurrentTime = 0;
        // number of PCM bytes processed.
        std::uint64_t mCurrentBytes = 0;
        ALCdevice* mDevice = nullptr;
        ALCcontext* mContext = nullptr;
        ALuint mHandle = 0;
        ALuint mBuffers[NumBuffers];

    };
private:
    State mState = State::None;
    ALCdevice* mDevice   = nullptr;
    ALCcontext* mContext = nullptr;
    unsigned mBufferSize = 20; // milliseconds
    std::list<std::weak_ptr<PlaybackStream>> mStreams;
};

} // namespace

namespace audio
{

// static
std::unique_ptr<Device> Device::Create(const char* name)
{
    auto device = std::make_unique<OpenALDevice>(name);
    device->Init();
    return device;
}

} // namespace

#endif // AUDIO_USE_OPENAL
