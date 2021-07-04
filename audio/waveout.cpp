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

#ifdef WINDOWS_OS
  #include <mutex>
  #include <Windows.h>
  #include <Mmsystem.h>
  #pragma comment(lib, "Winmm.lib")
#endif

#include <cassert>

#include "base/logging.h"
#include "audio/source.h"
#include "audio/stream.h"
#include "audio/device.h"

namespace audio
{

#ifdef WINDOWS_OS

// AudioDevice implementation for Waveout
class Waveout : public Device
{
public:
    Waveout(const char*)
    {}
    virtual std::shared_ptr<Stream> Prepare(std::unique_ptr<Source> source) override
    {
        auto s = std::make_shared<PlaybackStream>(std::move(source));
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
                std::shared_ptr<PlaybackStream> s = stream.lock();
                s->poll();
                it++;
            }
        }
    }

    virtual void Init()
    {}

    virtual State GetState() const override
    { return Device::State::Ready; }
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
            void* base = nullptr;
            bool used = false;
            size_t size = 0;
            size_t alignment = 0;
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
        std::size_t fill(Source& source)
        {
            const auto pcm_bytes = source.FillBuffer(buffer_, size_);

            ZeroMemory(&header_, sizeof(header_));
            header_.lpData         = (LPSTR)buffer_;
            header_.dwBufferLength = pcm_bytes;
            const auto ret = waveOutPrepareHeader(hWave_, &header_, sizeof(header_));
            assert(ret == MMSYSERR_NOERROR);
            return pcm_bytes;
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
        HWAVEOUT hWave_ = NULL;
        WAVEHDR  header_;
        std::size_t size_ = 0;
        void* buffer_ = nullptr;
    };

    class PlaybackStream : public Stream
    {
    public:
        PlaybackStream(std::unique_ptr<Source> source)
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);

            source_      = std::move(source);
            done_buffer_ = std::numeric_limits<decltype(done_buffer_)>::max();
            last_buffer_ = std::numeric_limits<decltype(last_buffer_)>::max();
            state_       = Stream::State::None;
            num_pcm_bytes_ = 0;

            const auto WAVE_FORMAT_IEEE_FLOAT = 0x0003;

            WAVEFORMATEX wfx = {0};
            wfx.nSamplesPerSec  = source_->GetRateHz();
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
       ~PlaybackStream()
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

        virtual Stream::State GetState() const override
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            return state_;
        }

        virtual std::unique_ptr<Source> GetFinishedSource() override
        {
            std::unique_ptr<Source> ret;
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            if (state_ == State::Complete || state_ == State::Error)
                ret = std::move(source_);
            return ret;
        }

        virtual std::string GetName() const override
        { return source_->GetName(); }

        virtual void Play()
        {
            // enter initial play state. fill all buffers with audio
            // and enqueue them to the device. once a signal is
            // received that the device has consumed a buffer
            // we update the buffer with new data and send it again
            // to the device.
            // we continue this untill all data is consumed or an error
            // has occurred 

            for (size_t i=0; i<buffers_.size(); ++i)
            {
                num_pcm_bytes_ += buffers_[i]->fill(*source_);
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

            if (state_ == Stream::State::Error ||
                state_ == Stream::State::Complete)
                return;

            // todo: we have a possible problem here that we might skip
            // a buffer. This would happen if if WOL_DONE occurs more than
            // 1 time between calls to poll()

            const auto num_buffers = buffers_.size();
            const auto free_buffer = done_buffer_ % num_buffers;
            
            num_pcm_bytes_ += buffers_[free_buffer]->fill(*source_);

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

            auto* this_ = reinterpret_cast<PlaybackStream*>(dwInstance);

            std::lock_guard<std::recursive_mutex> lock(this_->mutex_);

            switch (uMsg)
            {
                case WOM_CLOSE:
                    break;

                case WOM_DONE:
                    this_->done_buffer_++;
                    if (this_->source_ && !this_->source_->HasNextBuffer(this_->num_pcm_bytes_))
                        this_->state_ = Stream::State::Complete;
                    break;

                case WOM_OPEN:
                    this_->state_ = Stream::State::Ready;
                    break;
            }
        }

    private:
        std::unique_ptr<Source> source_;
        std::uint64_t num_pcm_bytes_ = 0;
    private:
        HWAVEOUT handle_ = NULL;
        std::vector<std::unique_ptr<Buffer>> buffers_;
        std::size_t last_buffer_ = 0;
        std::size_t done_buffer_ = 0;
        mutable std::recursive_mutex mutex_;
    private:
        State state_ = State::None;
    };
private:
    // currently active streams that we have to pump
    std::list<std::weak_ptr<PlaybackStream>> streams_;
};

// static
std::unique_ptr<Device> Device::Create(const char* appname)
{
    std::unique_ptr<Device> device;
    device = std::make_unique<Waveout>(appname);
    if (device)
        device->Init();

    return device;
}

#endif // WINDOWS_OS

} // namespace
