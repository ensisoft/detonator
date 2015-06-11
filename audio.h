// Copyright (c) 2014 Sami Väisänen, Ensisoft 
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
#include "warnpush.h"
#  include <QString>
#  include <QDebug>
#include "warnpop.h"
#include <pulse/pulseaudio.h>
#include <stdexcept>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <queue>
#include <memory>
#include <functional>
#include <vector>
#include <list>
#include <queue>
#include <chrono>
#include <cstring>

namespace invaders
{
    // audiosample is class to encapsulate loading and the details
    // of simple audio samples such as wav.
    class AudioSample
    {
    public:
        using u8 = std::uint8_t;

        enum class format {
            S16LE
        };

        // load sample from the provided byte buffer
        AudioSample(const u8* ptr, std::size_t len);

        // load sample from either a resource or from a file
        AudioSample(const QString& path);

        // return the sample rate in hz
        unsigned rate() const 
        { return sample_rate_; }

        // return the number of channels in the sample
        unsigned channels() const
        { return num_channels_; }

        unsigned frames() const 
        { return num_frames_; }

        unsigned size() const 
        { return buffer_.size(); }

        const void* data(std::size_t offset) const 
        { 
            assert(offset < buffer_.size());
            return &buffer_[offset]; 
        }

    private:
        std::vector<u8> buffer_;
        unsigned sample_rate_;
        unsigned num_channels_;
        unsigned num_frames_;
    };

    class AudioStream
    {
    public:
        enum class State {
            none, ready, error, complete
        };

        virtual ~AudioStream() = default;

        virtual State state() const = 0;

    protected:
    private:
    };


    // access to the native audio playback system
    class AudioDevice
    {
    public:
        enum class State {
            none, ready, error
        };

        virtual ~AudioDevice() = default;

        // prepare a new audio stream from the already loaded audio sample
        virtual std::unique_ptr<AudioStream> prepare(std::shared_ptr<const AudioSample> sample) = 0;

        virtual std::size_t num_streams() const = 0;

        // play the audio stream right away. 
        virtual void play(std::unique_ptr<AudioStream> stream) = 0;

        // poll and dispatch pending audio device events. 
        // Todo: this needs a proper waiting/signaling mechanism. 
        virtual void poll() = 0;

        // get the current audio device state.
        virtual State state() const = 0;

    protected:
    private:
    };

    // AudioDevice implementation for PulseAudio
    class PulseAudio : public AudioDevice
    {
    public:
        PulseAudio(const char* appname) : loop_(nullptr), main_(nullptr), context_(nullptr), state_(State::none)
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

        virtual std::unique_ptr<AudioStream> prepare(std::shared_ptr<const AudioSample> sample) override
        {
            std::unique_ptr<Stream> s(new Stream(sample, context_));

            return std::move(s);
        }

        virtual void play(std::unique_ptr<AudioStream> stream) override
        {
            auto* priv = dynamic_cast<Stream*>(stream.get());
            assert(priv);

            priv->play();

            playlist_.push_back(std::move(stream));
        }

        virtual void poll() override
        {
            int retval = 0;
            pa_mainloop_iterate(loop_, 0, &retval);

            for (auto it = std::begin(playlist_); it != std::end(playlist_);)
            {
                const auto state = (*it)->state();
                if (state == AudioStream::State::complete)
                {
                    it = playlist_.erase(it);
                }
                else it++;
            }
        }

        virtual State state() const override
        {
            return state_;
        }

        virtual std::size_t num_streams() const override
        { return playlist_.size(); }


        PulseAudio(const PulseAudio&) = delete;
        PulseAudio& operator=(const PulseAudio&) = delete;
    private:
        class Stream  : public AudioStream
        {
        public: 
            Stream(std::shared_ptr<const AudioSample> sample, pa_context* context) 
                : sample_(sample), stream_(nullptr), state_(AudioStream::State::none)
            {
                pa_sample_spec spec;
                spec.channels = sample->channels();
                spec.rate     = sample->rate();                
                spec.format   = PA_SAMPLE_S16NE;
                stream_ = pa_stream_new(context, "sample", &spec, nullptr);
                if (!stream_)
                    throw std::runtime_error("create stream failed");

                pa_stream_set_state_callback(stream_, state, this);
                pa_stream_set_write_callback(stream_, write, this);
            }

           ~Stream()
            {
                pa_stream_disconnect(stream_);
                pa_stream_unref(stream_);
            }

            virtual AudioStream::State state() const override
            { return state_; }

            void play()
            {
                offset_ = 0;
                pa_stream_connect_playback(stream_, 
                    nullptr, // device 
                    nullptr, // pa_buffer_attr
                    (pa_stream_flags)0, // stream flags 0 for default
                    nullptr,  // volume
                    nullptr); // sync stream
            }

        private:
            static void drain(pa_stream* stream, int success, void* user)
            {
                auto* this_ = static_cast<Stream*>(user);

                this_->state_ = AudioStream::State::complete;
            }
            static void write(pa_stream* stream, size_t length, void* user)
            {
                auto* this_  = static_cast<Stream*>(user);
                auto& sample = this_->sample_;

                const auto size  = sample->size();
                const auto avail = size - this_->offset_;
                const auto bytes = std::min(avail, length);

                if (bytes == 0)
                    return;
                else if (bytes < length)
                {
                    pa_operation_unref(pa_stream_drain(this_->stream_, drain, this_));
                    return;
                }

                const auto* ptr  = sample->data(this_->offset_);

                pa_stream_write(this_->stream_, ptr, bytes, nullptr, 0, PA_SEEK_RELATIVE);
                this_->offset_ += bytes;
            }

            static void state(pa_stream* stream, void* user)
            {
                auto* this_ = static_cast<Stream*>(user);
                switch (pa_stream_get_state(stream))
                {
                    case PA_STREAM_CREATING:
                        break;

                    case PA_STREAM_UNCONNECTED:
                        break;

                    case PA_STREAM_TERMINATED:
                        break;

                    case PA_STREAM_FAILED:
                       this_->state_ = AudioStream::State::error;
                       break;

                    case PA_STREAM_READY:
                        this_->state_ = AudioStream::State::ready;
                        break;
                }
            }
        private:
            std::shared_ptr<const AudioSample> sample_;
            pa_stream*  stream_;
            AudioStream::State state_;
            std::size_t offset_;
        };


    private:
        static void state(pa_context* context, void* user)
        {
            auto* this_ = static_cast<PulseAudio*>(user);
            switch (pa_context_get_state(context))
            {
                case PA_CONTEXT_CONNECTING:
                case PA_CONTEXT_AUTHORIZING:
                case PA_CONTEXT_SETTING_NAME:
                case PA_CONTEXT_UNCONNECTED:
                case PA_CONTEXT_TERMINATED:
                    break;

                case PA_CONTEXT_READY:
                    this_->state_ = State::ready;
                    break;

                case PA_CONTEXT_FAILED:
                    this_->state_ = State::error;
                    break;

            }
        }
    private:
        std::list<std::unique_ptr<AudioStream>> playlist_;
    private:
        pa_mainloop* loop_;
        pa_mainloop_api* main_;
        pa_context* context_;        
    private:
        State state_;
    };


    class AudioPlayer 
    {
    public:
        AudioPlayer(std::unique_ptr<AudioDevice> device) : stop_(false)
        {
            thread_.reset(new std::thread(std::bind(&AudioPlayer::runLoop, this, device.get())));
            device.release();
        }

       ~AudioPlayer()
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stop_ = true;
                cond_.notify_one();
            }
            thread_->join();
        }

        void play(std::shared_ptr<AudioSample> sample)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            struct sample s;
            s.s = sample;
            s.p = std::chrono::steady_clock::now();
            samples_.push(s);

            cond_.notify_one();
        }


        void play(std::shared_ptr<AudioSample> sample, std::chrono::milliseconds ms)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            struct sample s;
            s.s = sample;
            s.p = std::chrono::steady_clock::now() + ms;
            samples_.push(s);

            cond_.notify_one();
        }

    private:
        void runLoop(AudioDevice* ptr)
        {
            std::unique_ptr<AudioDevice> dev(ptr);
            for (;;)
            {
                if (dev->num_streams())
                    dev->poll();

                std::unique_lock<std::mutex> lock(mutex_);
                if (stop_)
                    return;

                if (samples_.empty())
                {
                    if (dev->num_streams())
                        continue;

                    cond_.wait(lock);
                }
                if (samples_.empty())
                    continue;

                auto top = samples_.top();
                auto now = std::chrono::steady_clock::now();
                if (!dev->num_streams())
                {
                    auto ret = cond_.wait_until(lock, top.p);
                    if (ret == std::cv_status::timeout)
                    {
                        auto sample = top.s;
                        auto stream = dev->prepare(sample);
                        dev->play(std::move(stream)); 
                        samples_.pop();
                    }
                }
                else if (now >= top.p)
                {
                    auto sample = top.s;
                    auto stream = dev->prepare(sample);
                    dev->play(std::move(stream));
                    samples_.pop();
                }
            }
        }
    private:
        struct sample {
            std::shared_ptr<AudioSample> s;
            std::chrono::steady_clock::time_point p;

            bool operator<(const sample& other) const {
                return p < other.p;
            }
            bool operator>(const sample& other) const {
                return p > other.p;
            }
        };

    private:
        std::unique_ptr<std::thread> thread_;
        std::mutex mutex_;

        std::priority_queue<sample,
            std::vector<sample>,
            std::greater<sample>> samples_;

        std::condition_variable cond_;
        bool stop_;
    };

} // invaders