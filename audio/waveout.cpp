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
  #include <mmreg.h>
  #pragma comment(lib, "Winmm.lib")
#endif

#include <queue>

#include "base/logging.h"
#include "base/assert.h"
#include "audio/source.h"
#include "audio/stream.h"
#include "audio/device.h"

namespace audio
{

#ifdef WINDOWS_OS

template<typename Function, typename... Args>
void CallWaveout(Function func, const Args&... args)
{
    const MMRESULT ret = func(args...);
    if (ret == MMSYSERR_NOERROR)
        return;

    char message[128] = { 0 };
    waveOutGetErrorTextA(ret, message, sizeof(message));
    throw std::runtime_error(message);
}

// AudioDevice implementation for Waveout
class Waveout : public Device
{
public:
    Waveout(const char*)
    {}
    virtual std::shared_ptr<Stream> Prepare(std::unique_ptr<Source> source) override
    {
        const auto name = source->GetName();
        try
        {
            auto stream = std::make_shared<PlaybackStream>(std::move(source));
            streams_.push_back(stream);
            return stream;
        } 
        catch (const std::exception & e)
        {
            ERROR("Audio source '%1' failed to prepare (%2).", name, e.what());
        }
        return nullptr;

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
                throw std::runtime_error("waveout buffer allocation failed");
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
            ASSERT(ret == MMSYSERR_NOERROR);

            AlignedAllocator::get().free(buffer_);
        }
        std::size_t fill(Source& source)
        {
            const auto pcm_bytes = source.FillBuffer(buffer_, size_);

            ZeroMemory(&header_, sizeof(header_));
            header_.lpData         = (LPSTR)buffer_;
            header_.dwBufferLength = pcm_bytes;
            header_.dwUser         = (DWORD_PTR)this;
            CallWaveout(waveOutPrepareHeader, hWave_, &header_, sizeof(header_));
            return pcm_bytes;
        }
        void play()
        {
            CallWaveout(waveOutWrite, hWave_, &header_, sizeof(header_));
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
            std::lock_guard<decltype(mutex_)> lock(mutex_);

            DEBUG("Creating new WaveOut stream '%1': %2 channel(s) @ %3 Hz, %4",
                source->GetName(), source->GetNumChannels(),
                source->GetRateHz(), source->GetFormat());

            source_      = std::move(source);
            state_       = Stream::State::None;
            num_pcm_bytes_ = 0;

            const auto format = source_->GetFormat();

            WAVEFORMATEX wfx = {0};
            wfx.nSamplesPerSec  = source_->GetRateHz();
            wfx.nChannels       = source_->GetNumChannels();
            wfx.cbSize          = 0; // extra info
            if (format == Source::Format::Float32)
            {
                wfx.wBitsPerSample = 32;
                wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
            }
            else if (format == Source::Format::Int32)
            {
                wfx.wBitsPerSample = 32;
                wfx.wFormatTag = WAVE_FORMAT_PCM;
            }
            else if (format == Source::Format::Int16)
            {
                wfx.wBitsPerSample = 16;
                wfx.wFormatTag = WAVE_FORMAT_PCM;
            } else BUG("Unsupported playback stream.");

            wfx.nBlockAlign     = (wfx.wBitsPerSample * wfx.nChannels) / 8;
            wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;
            CallWaveout(waveOutOpen,
                &handle_,
                WAVE_MAPPER,
                &wfx,
                (DWORD_PTR)waveOutProc,
                (DWORD_PTR)this,
                CALLBACK_FUNCTION);

            const auto block_size = wfx.nBlockAlign;

            // todo: cache and recycle audio buffers, or maybe
            // we need to cache audio streams since the buffers are per HWAVEOUT ?
            buffers_.resize(5);
            for (size_t i=0; i<buffers_.size(); ++i)
            {
                buffers_[i]  = std::unique_ptr<Buffer>(new Buffer(handle_, block_size * 2000, block_size));
            }
        }
       ~PlaybackStream()
        {
            auto ret = waveOutReset(handle_);
            ASSERT(ret == MMSYSERR_NOERROR);

            for (size_t i=0; i<buffers_.size(); ++i)
            {
                buffers_[i].reset();
            }

            ret = waveOutClose(handle_);
            ASSERT(ret == MMSYSERR_NOERROR);
        }

        virtual Stream::State GetState() const override
        {
            std::lock_guard<decltype(mutex_)> lock(mutex_);
            return state_;
        }

        virtual std::unique_ptr<Source> GetFinishedSource() override
        {
            std::unique_ptr<Source> ret;
            std::lock_guard<decltype(mutex_)> lock(mutex_);
            if (state_ == State::Complete || state_ == State::Error)
                ret = std::move(source_);
            return ret;
        }

        virtual std::string GetName() const override
        { return source_->GetName(); }

        virtual void Play() override
        {
            // enter initial play state. fill all buffers with audio
            // and enqueue them to the device. once a signal is
            // received that the device has consumed a buffer
            // we update the buffer with new data and send it again
            // to the device.
            // we continue this untill all data is consumed or an error
            // has occurred 
            try
            {

                for (size_t i = 0; i < buffers_.size(); ++i)
                {
                    num_pcm_bytes_ += buffers_[i]->fill(*source_);
                }
                for (size_t i = 0; i < buffers_.size(); ++i)
                {
                    buffers_[i]->play();
                }
            }
            catch (const std::exception & e)
            {
                ERROR("Audio stream '%1' play error (%2).", source_->GetName(), e.what());
                state_ = Stream::State::Error;
            }
        }

        virtual void Pause() override
        {
            waveOutPause(handle_);
            DEBUG("Pause waveout stream '%1'.", source_->GetName());
        }

        virtual void Resume() override
        {
            waveOutRestart(handle_);
            DEBUG("Resume waveout stream '%1'.", source_->GetName());
        }

        virtual void Cancel() override
        {
            // anything that needs to be done?
            DEBUG("Cancel waveout stream '%1'.", source_->GetName());
        }

        virtual void SendCommand(std::unique_ptr<Command> cmd) override
        {
            source_->RecvCommand(std::move(cmd));
        }
        virtual std::unique_ptr<Event> GetEvent() override
        {
            return source_->GetEvent();
        }

        void poll()
        {
            std::unique_lock<decltype(mutex_)> lock(mutex_);

            std::queue<Buffer*> empty_buffers;

            while (!message_queue_.empty())
            {
                auto message = message_queue_.front();
                if (message.message == WOM_OPEN)
                {
                    state_ = Stream::State::Ready;
                    DEBUG("WOM_OPEN");
                }
                else if (message.message == WOM_DONE)
                {
                    auto* buffer = (Buffer*)message.header.dwUser;
                    empty_buffers.push(buffer);
                }
                message_queue_.pop();
            }
            lock.unlock();

            if (state_ == Stream::State::Error || state_ == Stream::State::Complete)
                return;

            if (empty_buffers.size() == buffers_.size())
            {
                // if all the buffers have been returned from the waveout 
                // device it's likely that we're too slow providing buffers
                WARN("Likely audio buffer underrun detected.");
            }

            try
            {
                while (!empty_buffers.empty())
                {
                    auto* buffer = empty_buffers.front();
                    empty_buffers.pop();
                    if (!source_->HasMore(num_pcm_bytes_))
                    {
                        state_ = Stream::State::Complete;
                        break;
                    }

                    num_pcm_bytes_ += buffer->fill(*source_);
                    buffer->play();
                }
            }
            catch (const std::exception & e)
            {
                ERROR("Audio stream '%1' play error (%2).", source_->GetName(), e.what());
                state_ = Stream::State::Error;
            }
        }

    private:
        static void CALLBACK waveOutProc(HWAVEOUT handle, UINT uMsg,
            DWORD_PTR dwInstance,
            DWORD_PTR dwParam1,
            DWORD_PTR dwParam2)
        {
            if (dwInstance == 0)
                return;

            // this callback is called by the waveout thread. we need to be 
            // very careful regarding which functions are okay to call!

            WaveOutMessage message;
            message.message = uMsg;

            if (uMsg == WOM_DONE)
            {
                const WAVEHDR* header = (WAVEHDR*)dwParam1;
                std::memcpy(&message.header, header, sizeof(WAVEHDR));
            }

            auto* this_ = reinterpret_cast<PlaybackStream*>(dwInstance);

            // enqueue the message so that the main audio thread can process it.
            std::lock_guard<decltype(this_->mutex_)> lock(this_->mutex_);
            this_->message_queue_.push(message);
        }

    private:
        std::unique_ptr<Source> source_;
        std::uint64_t num_pcm_bytes_ = 0;
    private:
        HWAVEOUT handle_ = NULL;

        struct WaveOutMessage {
            UINT message = 0;
            WAVEHDR header;
        };
        mutable std::recursive_mutex mutex_;
        std::queue<WaveOutMessage> message_queue_;
        std::vector<std::unique_ptr<Buffer>> buffers_;
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

