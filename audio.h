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

#ifdef USE_PULSEAUDIO
#  include <pulse/pulseaudio.h>
#endif

#ifdef USE_WAVEOUT
#  include <Windows.h>
#  include <Mmsystem.h>
#  pragma comment(lib, "Winmm.lib")
#endif

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
#include <algorithm>
#include <cstring>
#include <cassert>

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

        // start playing the audio stream.
        // this should be called only once, when the stream is
        // initially started. to control the playback use pause/resume
        virtual void play() = 0;

        // pause the stream
        virtual void pause() = 0;

        // resume a paused stream
        virtual void resume() = 0;
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
        virtual std::shared_ptr<AudioStream> prepare(std::shared_ptr<const AudioSample> sample) = 0;

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

#ifdef USE_WAVEOUT
    // AudioDevice implementation for Waveout
    class Waveout : public AudioDevice
    {
    public:
        Waveout(const char*)
        {}
        virtual std::shared_ptr<AudioStream> prepare(std::shared_ptr<const AudioSample> sample) override
        {
            auto s = std::make_shared<Stream>(sample);
            streams_.push_back(s);
            return s;
        }
        virtual void poll()
        {
            for (auto it = std::begin(streams_); it != std::end(streams_); )
            {
                auto& stream = *it;
                if (stream.expired())
                {
                    it = streams_.erase(it);
                }
                else
                {
                    std::shared_ptr<Stream> s = stream.lock();
                    s->poll();
                    it++;
                }
            }
        }

        virtual void init()
        {}

        virtual State state() const override
        { return AudioDevice::State::ready; }
    private:
        class AlignedAllocator
        {
        public:
           ~AlignedAllocator()
            {
                for (auto& buff : buffers_)
                    _aligned_free(buff.base);
            }

            static AlignedAllocator& get()
            {
                static AlignedAllocator allocator;
                return allocator;
            }
            void* allocate(std::size_t bytes, std::size_t alignment)
            {
                auto it = std::find_if(std::begin(buffers_), std::end(buffers_),
                    [=](const auto& buffer) {
                        return buffer.used == false && buffer.size >= bytes && buffer.alignment == alignment;
                    });
                if (it != std::end(buffers_))
                {
                    buffer& buff = *it;
                    buff.used = true;
                    return buff.base;
                }
                void* base = _aligned_malloc(bytes, alignment);
                if (base == nullptr)
                    throw std::runtime_error("buffer allocation failed");
                buffer buff;
                buff.base = base;
                buff.size = bytes;
                buff.used = true;
                buff.alignment = alignment;
                buffers_.push_back(buff);
                return base;
            }

            void free(void* base)
            {
                auto it = std::find_if(std::begin(buffers_), std::end(buffers_),
                    [=](const auto& buffer) {
                        return buffer.base == base;
                    });
                assert(it != std::end(buffers_));
                auto& buff = *it;
                buff.used = false;
            }

        private:
            struct buffer {
                void* base;
                bool used;
                size_t size;
                size_t alignment;
            };

            std::vector<buffer> buffers_;
        };

        // a waveout buffer
        class Buffer
        {
        public:
            Buffer(HWAVEOUT hWave, std::size_t bytes, std::size_t alignment)
            {
                hWave_  = hWave;
                size_   = bytes;
                buffer_ = AlignedAllocator::get().allocate(bytes, alignment);
            }
           ~Buffer()
            {
                // todo: should we somehow make sure here not to free the buffer
                // while it's being used in the waveout write??
                const auto ret = waveOutUnprepareHeader(hWave_, &header_, sizeof(header_));
                assert(ret == MMSYSERR_NOERROR);

                AlignedAllocator::get().free(buffer_);
            }
            std::size_t fill(const void* data, std::size_t chunk_size)
            {
                const auto avail = size_;
                const auto bytes = std::min(chunk_size, avail);

                CopyMemory(buffer_, data, bytes);

                ZeroMemory(&header_, sizeof(header_));
                header_.lpData         = (LPSTR)buffer_;
                header_.dwBufferLength = bytes;
                const auto ret = waveOutPrepareHeader(hWave_, &header_, sizeof(header_));
                assert(ret == MMSYSERR_NOERROR);
                return bytes;
            }
            void play()
            {
                const auto ret = waveOutWrite(hWave_, &header_, sizeof(header_));
                if (ret != MMSYSERR_NOERROR)
                    throw std::runtime_error("waveOutWrite failed");
            }

            Buffer(const Buffer&) = delete;
            Buffer& operator=(const Buffer&) = delete;
        private:
            HWAVEOUT hWave_;
            WAVEHDR  header_;
            std::size_t size_;
            void* buffer_;
        };

        class Stream : public AudioStream
        {
        public:
            Stream(std::shared_ptr<const AudioSample> sample)
            {
                std::lock_guard<std::recursive_mutex> lock(mutex_);

                sample_      = sample;
                done_buffer_ = std::numeric_limits<decltype(done_buffer_)>::max();
                last_buffer_ = std::numeric_limits<decltype(last_buffer_)>::max();
                state_       = AudioStream::State::none;
                offset_      = 0;

                WAVEFORMATEX wfx = {0};
                wfx.nSamplesPerSec  = sample->rate();
                wfx.wBitsPerSample  = 16;
                wfx.nChannels       = 2;
                wfx.cbSize          = 0; // extra info
                wfx.wFormatTag      = WAVE_FORMAT_PCM;
                wfx.nBlockAlign     = (wfx.wBitsPerSample * wfx.nChannels) / 8;
                wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;
                MMRESULT ret = waveOutOpen(
                    &handle_,
                    WAVE_MAPPER,
                    &wfx,
                    (DWORD_PTR)waveOutProc,
                    (DWORD_PTR)this,
                    CALLBACK_FUNCTION);
                if (ret != MMSYSERR_NOERROR)
                    throw std::runtime_error("waveOutOpen failed");

                const auto block_size = wfx.nBlockAlign;

                // todo: cache and recycle audio buffers, or maybe
                // we need to cache audio streams since the buffers are per HWAVEOUT ?
                buffers_.resize(3);
                for (size_t i=0; i<buffers_.size(); ++i)
                {
                    buffers_[i]  = std::unique_ptr<Buffer>(new Buffer(handle_, block_size * 10000, block_size));
                }
            }
           ~Stream()
            {
                auto ret = waveOutReset(handle_);
                assert(ret == MMSYSERR_NOERROR);

                for (size_t i=0; i<buffers_.size(); ++i)
                {
                    buffers_[i].reset();
                }

                ret = waveOutClose(handle_);
                assert(ret == MMSYSERR_NOERROR);
            }

            virtual AudioStream::State state() const override
            {
                std::lock_guard<std::recursive_mutex> lock(mutex_);
                return state_;
            }

            virtual std::string name() const override
            { return sample_->name(); }

            virtual void play()
            {
                // enter initial play state. fill all buffers with audio
                // and enqueue them to the device. once a signal is
                // received that the device has consumed a buffer
                // we update the buffer with new data and send it again
                // to the device.
                // we continue this untill all data is consumed or an error
                // has occurred.
                const auto size = sample_->size();

                for (size_t i=0; i<buffers_.size(); ++i)
                {
                    offset_ += buffers_[i]->fill(sample_->data(offset_), size - offset_);
                }
                for (size_t i=0; i<buffers_.size(); ++i)
                {
                    buffers_[i]->play();
                }

            }

            virtual void pause()
            {
                waveOutPause(handle_);
            }

            virtual void resume()
            {
                waveOutRestart(handle_);
            }

            void poll()
            {
                std::lock_guard<std::recursive_mutex> lock(mutex_);
                if (done_buffer_ == last_buffer_)
                    return;

                if (state_ == AudioStream::State::error ||
                    state_ == AudioStream::State::complete)
                    return;

                // todo: we have a possible problem here that we might skip
                // a buffer. This would happen if if WOL_DONE occurs more than
                // 1 time between calls to poll()

                const auto num_buffers = buffers_.size();
                const auto free_buffer = done_buffer_ % num_buffers;
                const auto size = sample_->size();

                offset_ += buffers_[free_buffer]->fill(sample_->data(offset_), size - offset_);

                buffers_[free_buffer]->play();

                last_buffer_ = done_buffer_;
            }

        private:
            static void CALLBACK waveOutProc(HWAVEOUT handle, UINT uMsg,
                DWORD_PTR dwInstance,
                DWORD_PTR dwParam1,
                DWORD_PTR dwParam2)
            {
                if (dwInstance == 0)
                    return;

                auto* this_ = reinterpret_cast<Stream*>(dwInstance);

                std::lock_guard<std::recursive_mutex> lock(this_->mutex_);

                switch (uMsg)
                {
                    case WOM_CLOSE:
                        break;

                    case WOM_DONE:
                        this_->done_buffer_++;
                        if (this_->offset_ == this_->sample_->size())
                            this_->state_ = AudioStream::State::complete;
                        break;

                    case WOM_OPEN:
                        this_->state_ = AudioStream::State::ready;
                        break;
                }
            }

        private:
            std::shared_ptr<const AudioSample> sample_;
            std::size_t offset_;
        private:
            HWAVEOUT handle_;
            std::vector<std::unique_ptr<Buffer>> buffers_;
            std::size_t last_buffer_;
            std::size_t done_buffer_;
            mutable std::recursive_mutex mutex_;
        private:
            State state_;
        };
    private:
        // currently active streams that we have to pump
        std::list<std::weak_ptr<Stream>> streams_;
    };

#endif // USE_WAVEOUT

#ifdef USE_PULSEAUDIO
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
                spec.format   = PA_SAMPLE_S16NE;
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
                qDebug("underflow!");
            }

            static void drain_callback(pa_stream* stream, int success, void* user)
            {
                qDebug("Drained stream!");

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
        pa_mainloop* loop_     = nullptr;
        pa_mainloop_api* main_ = nullptr;
        pa_context* context_   = nullptr;
    private:
        State state_ = AudioDevice::State::none;
    };
#endif // USE_PULSEAUDIO


    class AudioPlayer
    {
    public:
        AudioPlayer(std::unique_ptr<AudioDevice> device)
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
                    track.stream->resume();
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
            std::size_t id = 0;
            std::shared_ptr<AudioSample> sample;
            std::shared_ptr<AudioStream> stream;
            std::chrono::steady_clock::time_point when;
            bool looping = false;

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
        std::size_t trackid_ = 1;

        // currently enqueued and waiting tracks.
        std::priority_queue<Track,
            std::vector<Track>,
            std::greater<Track>> waiting_;

        // currently playing tracks
        std::list<Track> playing_;

        std::condition_variable cond_;
        bool stop_ = false;
    };

} // invaders
