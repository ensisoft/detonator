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
#include <string>
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
        AudioSample(const u8* ptr, std::size_t len,
            const std::string& name = std::string());

        // load sample from either a resource or from a file
        AudioSample(const QString& path,
            const std::string& name = std::string());

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

        std::string name() const
        {
            return !name_.empty() ? name_ : "sample";
        }

    private:
        std::string name_;
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

        // get current stream state
        virtual State state() const = 0;

        // get the stream name if any
        virtual std::string name() const = 0;

        // start playing the audio stream
        virtual void play() = 0;

        // pause the stream
        virtual void pause() = 0;

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

        // prepare a new audio stream from the already loaded audio sample.
        // the stream is initially paused but ready to play once play is called.
        virtual std::unique_ptr<AudioStream> prepare(std::shared_ptr<const AudioSample> sample) = 0;

        // poll and dispatch pending audio device events.
        // Todo: this needs a proper waiting/signaling mechanism.
        virtual void poll() = 0;

        // initialize the audio device.
        // this should be called *once* after the device is created.
        virtual void init() = 0;

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

            while (s->state() == AudioStream::State::none)
            {
                pa_mainloop_iterate(loop_, 0, nullptr);
            }
            if (s->state() == AudioStream::State::error)
                throw std::runtime_error("pulseaudio stream error");

            return s;
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
        class Stream  : public AudioStream
        {
        public:
            Stream(std::shared_ptr<const AudioSample> sample, pa_context* context)
                : sample_(sample), stream_(nullptr), state_(AudioStream::State::none), offset_(0)
            {
                const std::string sample_name = sample->name();
                pa_sample_spec spec;
                spec.channels = sample->channels();
                spec.rate     = sample->rate();
                spec.format   = PA_SAMPLE_S16NE;
                stream_ = pa_stream_new(context, sample_name.c_str(), &spec, nullptr);
                if (!stream_)
                    throw std::runtime_error("create stream failed");

                pa_stream_set_state_callback(stream_, state, this);
                pa_stream_set_write_callback(stream_, write, this);
                pa_stream_set_underflow_callback(stream_, underflow, this);

                pa_stream_connect_playback(stream_,
                    nullptr, // device
                    nullptr, // pa_buffer_attr
                    PA_STREAM_START_CORKED, // stream flags 0 for default
                    nullptr,  // volume
                    nullptr); // sync stream
            }

           ~Stream()
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

        private:
            static void underflow(pa_stream* stream, void* user)
            {
                qDebug("underflow!");
            }

            static void drain(pa_stream* stream, int success, void* user)
            {
                qDebug("Drained stream!");

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

                const auto* ptr  = sample->data(this_->offset_);

                pa_stream_write(this_->stream_, ptr, bytes, nullptr, 0, PA_SEEK_RELATIVE);
                this_->offset_ += bytes;

                if (this_->offset_ == size)
                {
                    // reaching the end of the stream, i.e. we're providing the last
                    // write of data. schedule the drain operation callback on the stream.
                    pa_operation_unref(pa_stream_drain(this_->stream_, drain, this_));
                }
            }

            static void state(pa_stream* stream, void* user)
            {
                auto* this_ = static_cast<Stream*>(user);
                switch (pa_stream_get_state(stream))
                {
                    case PA_STREAM_CREATING:
                        qDebug("PA_STREAM_CREATING");
                        break;

                    case PA_STREAM_UNCONNECTED:
                        qDebug("PA_STREAM_UNCONNECTED");
                        break;

                    // stream finished cleanly, but this state transition
                    // only is dispatched when the pa_stream_disconnect is connected.
                    case PA_STREAM_TERMINATED:
                        qDebug("PA_STREAM_TERMINATED");
                        //this_->state_ = AudioStream::State::complete;
                        break;

                    case PA_STREAM_FAILED:
                       qDebug("PA_STREAM_FAILED");
                       this_->state_ = AudioStream::State::error;
                       break;

                    case PA_STREAM_READY:
                        qDebug("PA_STREAM_READY");
                        this_->state_ = AudioStream::State::ready;
                        break;
                }
            }
        private:
            std::shared_ptr<const AudioSample> sample_;
            pa_stream*  stream_;
            AudioStream::State state_;
            std::size_t offset_;
        private:
        };


    private:
        static void state(pa_context* context, void* user)
        {
            auto* this_ = static_cast<PulseAudio*>(user);
            switch (pa_context_get_state(context))
            {
                case PA_CONTEXT_CONNECTING:
                    qDebug("PA_CONTEXT_CONNECTING");
                    break;
                case PA_CONTEXT_AUTHORIZING:
                    qDebug("PA_CONTEXT_AUTHORIZING");
                    break;
                case PA_CONTEXT_SETTING_NAME:
                    qDebug("PA_CONTEXT_SETTING_NAME");
                    break;
                case PA_CONTEXT_UNCONNECTED:
                    qDebug("PA_CONTEXT_UNCONNECTED");
                    break;
                case PA_CONTEXT_TERMINATED:
                    qDebug("PA_CONTEXT_TERMINATED");
                    break;

                case PA_CONTEXT_READY:
                    qDebug("PA_CONTEXT_READY");
                    this_->state_ = State::ready;
                    break;

                case PA_CONTEXT_FAILED:
                    qDebug("PA_CONTEXT_FAILED");
                    this_->state_ = State::error;
                    break;

            }
        }
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
            trackid_ = 1;
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

        std::size_t play(std::shared_ptr<AudioSample> sample, std::chrono::milliseconds ms, bool looping = false)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            size_t id = trackid_++;
            Track track;
            track.id       = id;
            track.sample   = sample;
            track.when     = std::chrono::steady_clock::now() + ms;
            track.looping  = looping;
            waiting_.push(std::move(track));

            cond_.notify_one();
            return id;
        }

        std::size_t play(std::shared_ptr<AudioSample> sample, bool looping = false)
        {
            return play(sample, std::chrono::milliseconds(0), looping);
        }

        void pause(std::size_t id)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            for (auto& track : playing_)
            {
                if (track.id == id)
                    track.stream->pause();
            }
        }

        void resume(std::size_t id)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            for (auto& track : playing_)
            {
                if (track.id == id)
                    track.stream->play();
            }
        }

    private:
        void runLoop(AudioDevice* ptr)
        {
            std::unique_ptr<AudioDevice> dev(ptr);
            for (;;)
            {
                std::unique_lock<std::mutex> lock(mutex_);
                if (stop_)
                    return;

                // todo: get rid of the polling (and the waiting) and move
                // to threaded audio device (or waitable audio device)
                // this wait is here to avoid "overheating the cpu" so to speak.
                // however this creates two problems.
                // a) it increases latency in terms of starting the playback of new audio sample.
                // b) creates possibility for a buffer underruns in the audio playback stream.
                cond_.wait_until(lock, std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(10));

                dev->poll();

                for (auto it = std::begin(playing_); it != std::end(playing_);)
                {
                    auto& track = *it;
                    const auto state = track.stream->state();
                    if (state == AudioStream::State::complete)
                    {
                        if (track.looping)
                        {
                            track.stream = dev->prepare(track.sample);
                            track.stream->play();
                            qDebug("Looping track ...");
                        }
                        else
                        {
                            it = playing_.erase(it);
                        }
                    }
                    else if (state == AudioStream::State::error)
                    {
                        it = playing_.erase(it);
                    }
                    else it++;
                }


                if (waiting_.empty())
                {
                    if (!playing_.empty())
                        continue;

                    qDebug("No audio to play, waiting ....");

                    cond_.wait(lock);
                }
                if (waiting_.empty())
                    continue;

                auto& top = waiting_.top();
                auto now = std::chrono::steady_clock::now();
                if (playing_.empty())
                {
                    auto ret = cond_.wait_until(lock, top.when);
                    if (ret == std::cv_status::timeout)
                    {
                        play_top(*dev);
                    }
                }
                else if (now >= top.when)
                {
                    play_top(*dev);
                }
            }
        }
        void play_top(AudioDevice& dev)
        {
            // top is const...
            auto& top = waiting_.top();
            Track item;
            item.id      = top.id;
            item.sample  = top.sample;
            item.stream  = dev.prepare(top.sample);
            item.when    = top.when;
            item.looping = top.looping;
            item.stream->play();

            waiting_.pop();
            playing_.push_back(std::move(item));
        }

    private:
        struct Track {
            std::size_t id;
            std::shared_ptr<AudioSample> sample;
            std::unique_ptr<AudioStream> stream;
            std::chrono::steady_clock::time_point when;
            bool looping;

            bool operator<(const Track& other) const {
                return when < other.when;
            }
            bool operator>(const Track& other) const {
                return when > other.when;
            }
        };

    private:
        std::unique_ptr<std::thread> thread_;
        std::mutex mutex_;

        // unique track id
        std::size_t trackid_;

        // currently enqueued and waiting tracks.
        std::priority_queue<Track,
            std::vector<Track>,
            std::greater<Track>> waiting_;

        // currently playing tracks
        std::list<Track> playing_;

        std::condition_variable cond_;
        bool stop_;
    };

} // invaders
