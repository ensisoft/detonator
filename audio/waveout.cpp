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

#ifdef WINDOWS_OS
  #include <mutex>
  #include <Windows.h>
  #include <Mmsystem.h>
  #pragma comment(lib, "Winmm.lib")
#endif

#include <cassert>

#include "base/logging.h"
#include "sample.h"
#include "stream.h"
#include "device.h"

namespace audio
{

#ifdef WINDOWS_OS

// AudioDevice implementation for Waveout
class Waveout : public AudioDevice
{
public:
    Waveout(const char*)
    {}
    virtual std::shared_ptr<AudioStream> Prepare(std::shared_ptr<const AudioSample> sample) override
    {
        auto s = std::make_shared<Stream>(sample);
        streams_.push_back(s);
        return s;
    }
    virtual void Poll()
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

    virtual void Init()
    {}

    virtual State GetState() const override
    { return AudioDevice::State::Ready; }
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
            state_       = AudioStream::State::None;
            offset_      = 0;

            const auto WAVE_FORMAT_IEEE_FLOAT = 0x0003;

            WAVEFORMATEX wfx = {0};
            wfx.nSamplesPerSec  = sample->GetRateHz();
            wfx.wBitsPerSample  = 32;
            wfx.nChannels       = 2;
            wfx.cbSize          = 0; // extra info
            wfx.wFormatTag      = WAVE_FORMAT_IEEE_FLOAT;
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

        virtual AudioStream::State GetState() const override
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            return state_;
        }

        virtual std::string GetName() const override
        { return sample_->GetName(); }

        virtual void Play()
        {
            // enter initial play state. fill all buffers with audio
            // and enqueue them to the device. once a signal is
            // received that the device has consumed a buffer
            // we update the buffer with new data and send it again
            // to the device.
            // we continue this untill all data is consumed or an error
            // has occurred.
            const auto size = sample_->GetBufferSize();

            for (size_t i=0; i<buffers_.size(); ++i)
            {
                offset_ += buffers_[i]->fill(sample_->GetDataPtr(offset_), size - offset_);
            }
            for (size_t i=0; i<buffers_.size(); ++i)
            {
                buffers_[i]->play();
            }

        }

        virtual void Pause()
        {
            waveOutPause(handle_);
        }

        virtual void Resume()
        {
            waveOutRestart(handle_);
        }

        void poll()
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            if (done_buffer_ == last_buffer_)
                return;

            if (state_ == AudioStream::State::Error ||
                state_ == AudioStream::State::Complete)
                return;

            // todo: we have a possible problem here that we might skip
            // a buffer. This would happen if if WOL_DONE occurs more than
            // 1 time between calls to poll()

            const auto num_buffers = buffers_.size();
            const auto free_buffer = done_buffer_ % num_buffers;
            const auto size = sample_->GetBufferSize();

            offset_ += buffers_[free_buffer]->fill(sample_->GetDataPtr(offset_), size - offset_);

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
                    if (this_->offset_ == this_->sample_->GetBufferSize())
                        this_->state_ = AudioStream::State::Complete;
                    break;

                case WOM_OPEN:
                    this_->state_ = AudioStream::State::Ready;
                    break;
            }
        }

    private:
        std::shared_ptr<const AudioSample> sample_;
        std::size_t offset_ = 0;
    private:
        HWAVEOUT handle_;
        std::vector<std::unique_ptr<Buffer>> buffers_;
        std::size_t last_buffer_ = 0;
        std::size_t done_buffer_ = 0;
        mutable std::recursive_mutex mutex_;
    private:
        State state_ = State::None;
    };
private:
    // currently active streams that we have to pump
    std::list<std::weak_ptr<Stream>> streams_;
};

// static
std::unique_ptr<AudioDevice> AudioDevice::Create(const char* appname)
{
    std::unique_ptr<AudioDevice> device;
    device = std::make_unique<Waveout>(appname);
    if (device)
        device->Init();

    return device;
}

#endif // WINDOWS_OS

} // namespace
