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

#define SOKOL_IMPL
#include <cassert>
#include <cstring>

#if defined(AUDIO_USE_SOKOL)

#include "third_party/sokol_audio.h"
#include "base/assert.h"
#include "base/logging.h"
#include "audio/source.h"
#include "audio/stream.h"
#include "audio/device.h"

namespace audio
{
    class SokolDevice : public Device
    {
    public:
        SokolDevice()
        {
            DEBUG("Create sokol audio device.");
        }
        ~SokolDevice()
        {
            DEBUG("Delete sokol audio device.");
        }
        virtual std::shared_ptr<Stream> Prepare(std::unique_ptr<Source> source) override
        {
            auto name = source->GetName();
            try
            {
                return std::make_shared<SokolStream>(std::move(source), mBufferSize);
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

        virtual void SetBufferSize(unsigned milliseconds)
        {
            mBufferSize = milliseconds;
        }
    private:
        unsigned mBufferSize = 20;

    private:
        class SokolStream : public Stream {
        public:
            SokolStream(std::unique_ptr<Source> source, unsigned buffer_size_ms)
              : mSource(std::move(source))
            {
                const Format format = {
                    mSource->GetFormat(),
                    mSource->GetRateHz(),
                    mSource->GetNumChannels()
                };
                if (format.sample_type != Source::Format::Float32)
                    throw std::runtime_error("Unsupported audio stream format: " +
                                             base::ToString(format.sample_type));

                const auto buffer_size_bytes = Source::BuffSize(mSource->GetFormat(),
                                                                mSource->GetNumChannels(),
                                                                mSource->GetRateHz(),
                                                                buffer_size_ms);
                const auto frame_size_bytes = audio::GetFrameSizeInBytes(format);
                const auto buffer_size_frames = buffer_size_bytes / frame_size_bytes;

                // todo: fix the buffer size in frames
                // may throw
                mSource->Prepare(frame_size_bytes * 2048);

                saudio_desc desc;
                std::memset(&desc, 0, sizeof(desc));
                desc.user_data          = this;
                desc.sample_rate        = mSource->GetRateHz();
                desc.num_channels       = mSource->GetNumChannels();
                desc.stream_userdata_cb = write_callback_trampoline;
                desc.logger.func        = log_callback;
                // todo: this fails
                //desc.buffer_frames      = 2048; // buffer_size_frames;
                saudio_setup(desc);
                if (!saudio_isvalid())
                    throw std::runtime_error("saudio_setup failed.");

                //auto foo = saudio_buffer_frames();

                DEBUG("Sokol audio stream is open on source. [source='%1']", mSource->GetName());
            }
            ~SokolStream()
            {
                // does pthread_join on the backend audio tread.
                saudio_shutdown();
                DEBUG("Sokol audio stream and device close.");
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
                // todo: fake with silence

                DEBUG("Sokol audio stream play. [name='%1']", mSource->GetName());
            }

            virtual void Pause() override
            {
                // todo fake with silence

                DEBUG("Sokol audio stream pause. [name='%1']", mSource->GetName());
            }
            virtual void Resume() override
            {
                // todo: fake with silence

                DEBUG("Sokol audio stream resume. [name='%1']", mSource->GetName());
            }
            virtual void Cancel() override
            {
                DEBUG("Sokol audio stream cancel. [name='%1']", mSource->GetName());
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
            static void log_callback(const char* tag, uint32_t log_level, uint32_t log_item_id,
                                     const char* msg, uint32_t line,
                                     const char* file, void* user)
            {
                if (log_level == 0)
                    ERROR("Sokol audio panic. [msg='%1']", msg ? msg : "");
                else if (log_level == 1)
                    ERROR("Sokol audio error. [msg='%1']", msg ? msg : "");
                else if (log_level == 2)
                    WARN("Sokol audio warning. [msg='%1']", msg ? msg : "");
                else if (log_level == 3)
                    INFO("Sokol audio info. [msg='%1']", msg ? msg : "");
            }

            void write_callback(float* buffer, int num_frames, int num_channels)
            {
                // guard the THIS object from concurrent access
                // by the sokol write_callback thread and the main thread
                std::lock_guard<std::mutex> lock(mMutex);

                if (mState == State::Complete || !mSource || !buffer)
                {
                    // ok.. crashes when we're shutting down the stream..
                    // smells like we're getting junk parameters here.
                    // the implementation seems to do a pthread_join on saudio_sutdown
                    // so we should not be having a race condition here either..

                    //std::memcpy(buffer, 0, buffer_bytes);
                    return;
                }

                const auto buffer_bytes = num_frames * sizeof(float) * num_channels;

                try
                {
                    ASSERT(mSource);

                    const auto bytes_filled = mSource->FillBuffer(buffer, buffer_bytes);
                    const auto sample_size = Source::ByteSize(mSource->GetFormat());
                    const auto samples_per_ms = mSource->GetRateHz() / 1000u;
                    const auto bytes_per_ms = mSource->GetNumChannels() * sample_size * samples_per_ms;
                    const auto milliseconds = bytes_filled / bytes_per_ms;
                    mStreamTime += milliseconds;
                    mStreamBytes += bytes_filled;

                    if (!mSource->HasMore(mStreamBytes))
                    {
                        DEBUG("Sokol audio stream drained source.");
                        mState = State::Complete;
                    }
                }
                catch (const std::exception& exception)
                {
                    ERROR("Sokol audio stream error. [name='%1', error='%2']", mSource->GetName(), exception.what());
                    mState = State::Error;
                }
            }

            static void write_callback_trampoline(float* buffer, int num_frames, int num_channels, void* user)
            {
                static_cast<SokolStream*>(user)->write_callback(buffer, num_frames, num_channels);
            }

        private:
            mutable std::mutex mMutex;
            std::unique_ptr<Source> mSource;
            std::uint64_t mStreamTime = 0;
            std::uint64_t mStreamBytes = 0;
            Stream::State mState = State::Ready;
        };
    };

// static
std::unique_ptr<Device> Device::Create(const char* appname, const Format* hint)
{
    return std::make_unique<SokolDevice>();
}

} // namespace

#endif // AUDIO_USE_SOKOL