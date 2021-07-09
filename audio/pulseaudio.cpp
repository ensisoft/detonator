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

#ifdef LINUX_OS
  #include <pulse/pulseaudio.h>
#endif

#include <cassert>
#include <cstring>

#include "base/assert.h"
#include "base/logging.h"
#include "audio/source.h"
#include "audio/stream.h"
#include "audio/device.h"

#ifdef LINUX_OS
namespace {
void ThrowPaException(pa_context* context, const char* what)
{
    const auto err = pa_context_errno(context);
    ASSERT(err != PA_OK);
    const auto* str = pa_strerror(err);
    if (str && std::strlen(str))
        throw std::runtime_error(str);
    throw std::runtime_error(what);
}
}  // namespace
#endif

namespace audio
{

#ifdef LINUX_OS

// AudioDevice implementation for PulseAudio
class PulseAudio : public Device
{
public:
    PulseAudio(const char* appname)
    {
        loop_ = pa_mainloop_new();
        main_ = pa_mainloop_get_api(loop_);
        context_ = pa_context_new(main_, appname);

        pa_context_set_state_callback(context_, state, this);
        pa_context_connect(context_, nullptr, PA_CONTEXT_NOAUTOSPAWN, nullptr);
    }

   ~PulseAudio()
    {
        pa_context_disconnect(context_);
        pa_context_unref(context_);
        pa_mainloop_free(loop_);
    }

    virtual std::shared_ptr<Stream> Prepare(std::unique_ptr<Source> source) override
    {
        const auto name = source->GetName();
        try
        {
            auto stream = std::make_shared<PlaybackStream>(std::move(source), context_);

            // todo: fix this silly looping
            while (stream->GetState() == Stream::State::None)
            {
                pa_mainloop_iterate(loop_, 0, nullptr);
            }
            if (stream->GetState() == Stream::State::Ready)
                return stream;

            ERROR("Audio source '%1' failed to prepare.", name);
        }
        catch (const std::exception& e)
        { ERROR("Audio source '%1' failed to prepare (%2).", name, e.what()); }
        return nullptr;
    }

    virtual void Poll() override
    {
        pa_mainloop_iterate(loop_, 0, nullptr);
    }

    virtual void Init() override
    {
        // this is kinda ugly...
        while (state_ == Device::State::None)
        {
            pa_mainloop_iterate(loop_, 0, nullptr);
        }

        if (state_ == Device::State::Error)
            throw std::runtime_error("pulseaudio error");

    }

    virtual State GetState() const override
    {
        return state_;
    }

    PulseAudio(const PulseAudio&) = delete;
    PulseAudio& operator=(const PulseAudio&) = delete;
private:
    class PlaybackStream  : public Stream
    {
    public:
        PlaybackStream(std::unique_ptr<Source> source, pa_context* context)
            : source_(std::move(source))
        {
            const std::string& name = source_->GetName();
            pa_sample_spec spec;
            spec.channels = source_->GetNumChannels();
            spec.rate     = source_->GetRateHz();
            if (source_->GetFormat() == Source::Format::Float32)
                spec.format = PA_SAMPLE_FLOAT32NE;
            else if (source_->GetFormat() == Source::Format::Int16)
                spec.format = PA_SAMPLE_S16NE;
            else if (source_->GetFormat() == Source::Format::Int32)
                spec.format = PA_SAMPLE_S32NE;
            else BUG("Unsupported playback format.");

            stream_ = pa_stream_new(context, name.c_str(), &spec, nullptr);
            if (!stream_)
                ThrowPaException(context, "create stream failed.");

            pa_stream_set_state_callback(stream_, state_callback, this);
            pa_stream_set_write_callback(stream_, write_callback, this);
            pa_stream_set_underflow_callback(stream_, underflow_callback, this);

            if (pa_stream_connect_playback(stream_,
                nullptr, // device
                nullptr, // pa_buffer_attr
                PA_STREAM_START_CORKED, // stream flags 0 for default
                nullptr,  // volume
                nullptr)) // sync stream
                ThrowPaException(context, "stream playback failed.");
        }

       ~PlaybackStream()
        {
            pa_stream_disconnect(stream_);
            pa_stream_unref(stream_);
        }

        virtual Stream::State GetState() const override
        {  return state_; }
        
        virtual std::unique_ptr<Source> GetFinishedSource() override
        {
            std::unique_ptr<Source> ret;
            if (state_ == State::Complete || state_ == State::Error)
                ret = std::move(source_);
            return ret;
        }

        virtual std::string GetName() const override
        { return source_->GetName(); }

        virtual void Play() override
        {
            pa_stream_cork(stream_, 0, nullptr, nullptr);
        }

        virtual void Pause() override
        {
            pa_stream_cork(stream_, 1, nullptr, nullptr);
        }

        virtual void Resume() override
        {
            pa_stream_cork(stream_, 0, nullptr, nullptr);
        }
    private:
        static void underflow_callback(pa_stream* stream, void* user)
        {
            DEBUG("underflow!");
        }

        static void drain_callback(pa_stream* stream, int success, void* user)
        {
            DEBUG("Drained stream!");

            auto* this_ = static_cast<PlaybackStream*>(user);
            this_->state_ = Stream::State::Complete;
        }
        static void write_callback(pa_stream* stream, size_t length, void* user)
        {
            auto* this_ = static_cast<PlaybackStream*>(user);
            auto& source = this_->source_;

            if (this_->state_ != State::Ready)
                return;

            // we should always still have the source object as long
            // as the state is ready and playing and we're expected
            // to write more data into the stream!
            ASSERT(source);

            // callback while the stream is being drained. weird..
            if (!source->HasNextBuffer(this_->num_pcm_bytes_))
                return;

            // don't let exceptions propagate into the C library...
            try
            {
                // try to see if we have a happy case and can get a buffer
                // with a matching size from the server. however it's possible
                // that the returned buffer doesn't have enough capacity
                // in which case it cannot be used. :(
                void* buffer = nullptr;
                size_t buffer_size = -1;
                if (pa_stream_begin_write(this_->stream_, &buffer, &buffer_size))
                    throw std::runtime_error("pa_stream_begin_write failed.");

                std::vector<std::uint8_t> backup_buff;
                if (buffer_size < length)
                {
                    // important: cancel the pa_stream_begin_write!
                    pa_stream_cancel_write(this_->stream_);

                    backup_buff.resize(length);
                    buffer = &backup_buff[0];
                }

                // may throw.
                const auto bytes = this_->source_->FillBuffer(buffer, length);

                // it seems that if the pa_stream_write doesn't write exactly
                // as many bytes as requested the playback stops and the
                // callback is no longer invoked.
                // as of July-2021 there's an open bug about this.
                // https://gitlab.freedesktop.org/pulseaudio/pulseaudio/-/issues/1132
                if (pa_stream_write(this_->stream_, buffer, bytes, nullptr, 0, PA_SEEK_RELATIVE))
                    throw std::runtime_error("pa_stream_write failed.");

                this_->num_pcm_bytes_ += bytes;

                // reaching the end of the stream, i.e. we're providing the last
                // write of data. schedule the drain operation callback on the stream.
                if (!source->HasNextBuffer(this_->num_pcm_bytes_))
                    pa_operation_unref(pa_stream_drain(this_->stream_, drain_callback, this_));
                else if (bytes < length * 0.8)
                    WARN("Write callback requested %1 bytes but only %2 provided", length, bytes);
            }
            catch (const std::exception& exception)
            {
                ERROR("Audio source '%1' write error (%2).", source->GetName(), exception.what());
                this_->state_ = State::Error;
            }
        }

        static void state_callback(pa_stream* stream, void* user)
        {
            auto* this_ = static_cast<PlaybackStream*>(user);
            switch (pa_stream_get_state(stream))
            {
                case PA_STREAM_CREATING:
                    DEBUG("PA_STREAM_CREATING");
                    break;

                case PA_STREAM_UNCONNECTED:
                    DEBUG("PA_STREAM_UNCONNECTED");
                    break;

                // stream finished cleanly, but this state transition
                // only is dispatched when the pa_stream_disconnect is connected.
                case PA_STREAM_TERMINATED:
                    DEBUG("PA_STREAM_TERMINATED");
                    //this_->state_ = AudioStream::State::complete;
                    break;

                case PA_STREAM_FAILED:
                   DEBUG("PA_STREAM_FAILED");
                   this_->state_ = Stream::State::Error;
                   break;

                case PA_STREAM_READY:
                    DEBUG("PA_STREAM_READY");
                    this_->state_ = Stream::State::Ready;
                    break;
            }
        }
    private:
        std::unique_ptr<Source> source_;
        pa_stream*  stream_  = nullptr;
        Stream::State state_ = Stream::State::None;
        std::uint64_t num_pcm_bytes_ = 0;
    private:
    };
private:
    static void state(pa_context* context, void* user)
    {
        auto* this_ = static_cast<PulseAudio*>(user);
        switch (pa_context_get_state(context))
        {
            case PA_CONTEXT_CONNECTING:
                DEBUG("PA_CONTEXT_CONNECTING");
                break;
            case PA_CONTEXT_AUTHORIZING:
                DEBUG("PA_CONTEXT_AUTHORIZING");
                break;
            case PA_CONTEXT_SETTING_NAME:
                DEBUG("PA_CONTEXT_SETTING_NAME");
                break;
            case PA_CONTEXT_UNCONNECTED:
                DEBUG("PA_CONTEXT_UNCONNECTED");
                break;
            case PA_CONTEXT_TERMINATED:
                DEBUG("PA_CONTEXT_TERMINATED");
                break;
            case PA_CONTEXT_READY:
                DEBUG("PA_CONTEXT_READY");
                this_->state_ = State::Ready;
                break;
            case PA_CONTEXT_FAILED:
                DEBUG("PA_CONTEXT_FAILED");
                this_->state_ = State::Error;
                break;
        }
    }
private:
    pa_mainloop* loop_     = nullptr;
    pa_mainloop_api* main_ = nullptr;
    pa_context* context_   = nullptr;
private:
    State state_ = Device::State::None;
};

// static
std::unique_ptr<Device> Device::Create(const char* appname)
{
    std::unique_ptr<Device> device;
    device = std::make_unique<PulseAudio>(appname);
    if (device)
        device->Init();

    return device;
}

#endif // LINUX_OS

} // namespace
