// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#ifdef AUDIO_USE_SDL2
#  include <SDL2/SDL.h>
#endif

#include <cassert>
#include <cstring>
#include <mutex>

#include "base/assert.h"
#include "base/logging.h"
#include "audio/source.h"
#include "audio/stream.h"
#include "audio/device.h"

#ifdef AUDIO_USE_SDL2

namespace audio
{
    class SDLDevice : public Device
    {
    public:
        SDLDevice()
        {
            SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
            if (SDL_Init(SDL_INIT_AUDIO) < 0)
                throw std::runtime_error("SDL_init failed");
            DEBUG("SDL_Init done");
        }

        ~SDLDevice()
        {
            SDL_Quit();
            DEBUG("SDL_Quit");
        }
        virtual std::shared_ptr<Stream> Prepare(std::unique_ptr<Source> source) override
        {
            auto name = source->GetName();
            try
            {
                return std::make_shared<SDLStream>(std::move(source), mBufferSize);
            }
            catch (const std::exception& exception)
            {
                ERROR("Audio source failed to prepare. [name='%1', error='%2']", name,
                      exception.what());
            }
            return nullptr;
        }

        virtual void Poll() override
        {
        }

        virtual void Init() override
        {
        }

        virtual State GetState() const override
        {
            return State::Ready;
        }

        virtual void SetBufferSize(unsigned milliseconds) override
        {
            mBufferSize = milliseconds;
        }
    private:
        unsigned mBufferSize = 20;

    private:
        class SDLStream : public audio::Stream {
        public:
            SDLStream(std::unique_ptr<Source> source, unsigned buffer_size_ms)
              : mSource(std::move(source))
            {
                const Format format = {
                    mSource->GetFormat(),
                    mSource->GetRateHz(),
                    mSource->GetNumChannels()
                };

                const auto buffer_size_bytes = Source::BuffSize(mSource->GetFormat(),
                                                                mSource->GetNumChannels(),
                                                                mSource->GetRateHz(),
                                                                buffer_size_ms);
                const auto frame_size_bytes = GetFrameSizeInBytes(format);

                // may throw
                mSource->Prepare(buffer_size_bytes);

                SDL_AudioSpec spec;
                std::memset(&spec, 0, sizeof(spec));
                spec.callback = write_callback_trampoline;
                spec.userdata = this;
                spec.freq     = mSource->GetRateHz();
                spec.channels = mSource->GetNumChannels();
                spec.samples  = base::NextPOT(buffer_size_bytes / frame_size_bytes);

                const auto sample_type  = mSource->GetFormat();
                if (sample_type == Source::Format::Float32)
                    spec.format = AUDIO_F32LSB;
                else if (sample_type == Source::Format::Int16)
                    spec.format = AUDIO_S16LSB;
                else if (sample_type == Source::Format::Int32)
                    spec.format = AUDIO_S32LSB;
                else BUG("Unsupported audio format.");

                mDevice = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0);
                if (!mDevice)
                    throw std::runtime_error("SDL open audio device failed.");
                DEBUG("SDL audio stream is open on source. [source='%1']", mSource->GetName());
            }
           ~SDLStream()
            {
                SDL_CloseAudioDevice(mDevice);
                DEBUG("SDL audio stream and device close.");
            }
            virtual Stream::State GetState() const override
            {
                std::lock_guard<std::mutex> lock(mMutex);
                return mState;
            }
            virtual std::string GetName() const override
            {
                std::lock_guard<std::mutex> lock(mMutex);
                return mSource->GetName();
            }
            virtual std::uint64_t GetStreamTime() const override
            {
                std::lock_guard<std::mutex> lock(mMutex);
                return mStreamTime;
            }
            virtual std::uint64_t GetStreamBytes() const override
            {
                std::lock_guard<std::mutex> lock(mMutex);
                return mStreamBytes;
            }
            virtual void Play() override
            {
                DEBUG("SDL audio stream play. [name='%1']", mSource->GetName());
                SDL_PauseAudioDevice(mDevice, 0);
            }

            virtual void Pause() override
            {
                DEBUG("SDL audio stream pause. [name='%1']", mSource->GetName());
                SDL_PauseAudioDevice(mDevice, 1);
            }
            virtual void Resume() override
            {
                DEBUG("SDL audio stream resume. [name='%1']", mSource->GetName());
                SDL_PauseAudioDevice(mDevice, 0);
            }
            virtual void Cancel() override
            {
                DEBUG("SDL audio stream cancel. [name='%1']", mSource->GetName());
                std::lock_guard<std::mutex> lock(mMutex);
                mSource->Shutdown();
                mSource.reset();
            }
            virtual void SendCommand(std::unique_ptr<Command> cmd) override
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mSource->RecvCommand(std::move(cmd));
            }

            virtual std::unique_ptr<Event> GetEvent() override
            {
                std::lock_guard<std::mutex> lock(mMutex);
                return mSource->GetEvent();
            }

            virtual std::unique_ptr<Source> GetFinishedSource() override
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (mState == State::Complete || mState == State::Error)
                    return std::move(mSource);

                return nullptr;
            }

        private:
            void write_callback(Uint8* buffer, int len)
            {
                std::lock_guard<std::mutex> lock(mMutex);

                if (mState == State::Complete || !mSource)
                    return;

                ASSERT(buffer && len > 0);

                try
                {
                    const auto bytes = mSource->FillBuffer(buffer, (unsigned) len);

                    const auto sample_size    = Source::ByteSize(mSource->GetFormat());
                    const auto samples_per_ms = mSource->GetRateHz() / 1000u;
                    const auto bytes_per_ms   = mSource->GetNumChannels() * sample_size * samples_per_ms;
                    const auto milliseconds   = bytes / bytes_per_ms;
                    mStreamTime += milliseconds;
                    mStreamBytes += bytes;

                    if (!mSource->HasMore(mStreamBytes))
                    {
                        DEBUG("SDL stream drained source.");
                        SDL_PauseAudioDevice(mDevice, 0);
                        mState = State::Complete;
                    }
                }
                catch (const std::exception& exception)
                {
                    ERROR("SDL audio stream error. [name='%1', error='%2]", mSource->GetName(), exception.what());
                    mState = State::Error;
                }
            }
        private:
            static void write_callback_trampoline(void* user, Uint8* buffer, int len)
            {
                static_cast<SDLStream*>(user)->write_callback(buffer, len);
            }

        private:
            mutable std::mutex mMutex;
            std::unique_ptr<Source> mSource;
            std::uint64_t mStreamTime = 0;
            std::uint64_t mStreamBytes = 0;
            Stream::State mState = State::Ready;
            SDL_AudioDeviceID mDevice = 0;
        };
    };

// static
std::unique_ptr<Device> audio::Device::Create(const char* appname, const Format* format)
{
    return std::make_unique<SDLDevice>();
}

} // namespace

#endif // AUDIO_USE_SDL2
