// Copyright (c) 2014-2019 Sami Väisänen, Ensisoft
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

#include "config.h"

#ifdef LINUX_OS
  #include <pulse/pulseaudio.h>
#endif

#include <cassert>

#include "base/logging.h"
#include "sample.h"
#include "stream.h"
#include "device.h"

namespace audio
{

#ifdef LINUX_OS

// AudioDevice implementation for PulseAudio
class PulseAudio : public AudioDevice
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

    virtual std::shared_ptr<AudioStream> prepare(std::shared_ptr<const AudioSample> sample) override
    {
        auto stream = std::make_shared<PlaybackStream>(sample, context_);

        while (stream->state() == AudioStream::State::none)
        {
            pa_mainloop_iterate(loop_, 0, nullptr);
        }
        if (stream->state() == AudioStream::State::error)
            throw std::runtime_error("pulseaudio stream error");

        return stream;
    }

    virtual void poll() override
    {
        pa_mainloop_iterate(loop_, 0, nullptr);
    }

    virtual void init() override
    {
        // this is kinda ugly...
        while (state_ == AudioDevice::State::none)
        {
            pa_mainloop_iterate(loop_, 0, nullptr);
        }

        if (state_ == AudioDevice::State::error)
            throw std::runtime_error("pulseaudio error");

    }

    virtual State state() const override
    {
        return state_;
    }

    PulseAudio(const PulseAudio&) = delete;
    PulseAudio& operator=(const PulseAudio&) = delete;
private:
    class PlaybackStream  : public AudioStream
    {
    public:
        PlaybackStream(std::shared_ptr<const AudioSample> sample, pa_context* context) : sample_(sample)
        {
            const std::string& sample_name = sample_->name();
            pa_sample_spec spec;
            spec.channels = sample_->channels();
            spec.rate     = sample_->rate();
            spec.format   = PA_SAMPLE_FLOAT32NE;
            stream_ = pa_stream_new(context, sample_name.c_str(), &spec, nullptr);
            if (!stream_)
                throw std::runtime_error("create stream failed");

            pa_stream_set_state_callback(stream_, state_callback, this);
            pa_stream_set_write_callback(stream_, write_callback, this);
            pa_stream_set_underflow_callback(stream_, underflow_callback, this);

            pa_stream_connect_playback(stream_,
                nullptr, // device
                nullptr, // pa_buffer_attr
                PA_STREAM_START_CORKED, // stream flags 0 for default
                nullptr,  // volume
                nullptr); // sync stream
        }

       ~PlaybackStream()
        {
            pa_stream_disconnect(stream_);
            pa_stream_unref(stream_);
        }

        virtual AudioStream::State state() const override
        { return state_; }

        virtual std::string name() const override
        { return sample_->name(); }

        virtual void play() override
        {
            pa_stream_cork(stream_, 0, nullptr, nullptr);
        }

        virtual void pause() override
        {
            pa_stream_cork(stream_, 1, nullptr, nullptr);
        }

        virtual void resume() override
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
            this_->state_ = AudioStream::State::complete;
        }
        static void write_callback(pa_stream* stream, size_t length, void* user)
        {
            auto* this_  = static_cast<PlaybackStream*>(user);
            auto& sample = this_->sample_;

            const auto size  = sample->size();
            const auto avail = size - this_->offset_;
            const auto bytes = std::min(avail, length);

            if (bytes == 0)
                return;

            const auto* ptr  = sample->data(this_->offset_);

            pa_stream_write(this_->stream_, ptr, bytes, nullptr, 0, PA_SEEK_RELATIVE);
            this_->offset_ += bytes;

            if (this_->offset_ == size)
            {
                // reaching the end of the stream, i.e. we're providing the last
                // write of data. schedule the drain operation callback on the stream.
                pa_operation_unref(pa_stream_drain(this_->stream_, drain_callback, this_));
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
                   this_->state_ = AudioStream::State::error;
                   break;

                case PA_STREAM_READY:
                    DEBUG("PA_STREAM_READY");
                    this_->state_ = AudioStream::State::ready;
                    break;
            }
        }
    private:
        std::shared_ptr<const AudioSample> sample_;
        pa_stream*  stream_  = nullptr;
        AudioStream::State state_ = AudioStream::State::none;
        std::size_t offset_ = 0;
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
                this_->state_ = State::ready;
                break;

            case PA_CONTEXT_FAILED:
                DEBUG("PA_CONTEXT_FAILED");
                this_->state_ = State::error;
                break;

        }
    }
private:
    pa_mainloop* loop_     = nullptr;
    pa_mainloop_api* main_ = nullptr;
    pa_context* context_   = nullptr;
private:
    State state_ = AudioDevice::State::none;
};

// static
std::unique_ptr<AudioDevice> AudioDevice::create(const char* appname)
{
    std::unique_ptr<AudioDevice> device;
    device = std::make_unique<PulseAudio>(appname);
    if (device)
        device->init();

    return device;
}

#endif // LINUX_OS

} // namespace
