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

#include <algorithm>

#include "audio/source.h"
#include "base/assert.h"
#include "base/logging.h"

namespace audio
{
SourceThreadProxy::SourceThreadProxy(std::unique_ptr<Source> source)
  : mSource(std::move(source))
{}
SourceThreadProxy::~SourceThreadProxy()
{
    Shutdown();
}
unsigned SourceThreadProxy::GetRateHz() const noexcept
{ return mSource->GetRateHz(); }
unsigned SourceThreadProxy::GetNumChannels() const noexcept
{ return mSource->GetNumChannels(); }
Source::Format SourceThreadProxy::GetFormat() const noexcept
{ return mSource->GetFormat(); }
std::string SourceThreadProxy::GetName() const noexcept
{ return mSource->GetName(); }
void SourceThreadProxy::Prepare(unsigned int buffer_size)
{
    ASSERT(!mThread);
    ASSERT(!mBuffers.size());

    // todo: maybe use multiple buffers ?
    // But how about latency ?
    // And have to deal with the partial buffers so that the
    // data is stored somewhere temporarily

    // alternative idea, use a single ring buffer

    mBuffers.resize(2);
    for (auto& buff : mBuffers)
    {
        buff.Resize(buffer_size);
        mEmptyQueue.push(&buff);
    }

    DEBUG("Preparing audio source thread buffers. [num=%1, size=%2 b]",
          mEmptyQueue.size(),buffer_size);

    mThread.reset(new std::thread(&SourceThreadProxy::ThreadLoop, this));
}
unsigned SourceThreadProxy::FillBuffer(void* device_buff, unsigned device_buff_size)
{
    return FillBuffer(device_buff, device_buff_size, false);
}

bool SourceThreadProxy::HasMore(std::uint64_t num_bytes_read) const noexcept
{
    return !mSourceDone;
}

void SourceThreadProxy::Shutdown() noexcept
{
    if (mThread)
    {
        {
            std::unique_lock<decltype(mMutex)> lock(mMutex);
            mShutdown = true;
            mCondition.notify_one();
        }
        mThread->join();
        mThread.reset();
        DEBUG("Joined audio source thread. [name='%1']", mSource->GetName());
    }

    if (mSource)
    {
        mSource->Shutdown();
        mSource.reset();
    }
}
void SourceThreadProxy::RecvCommand(std::unique_ptr<Command> cmd) noexcept
{
    std::unique_lock<decltype(mMutex)> lock(mMutex);
    mCommands.push(std::move(cmd));
}
std::unique_ptr<Event> SourceThreadProxy::GetEvent() noexcept
{
    if (!mMutex.try_lock())
        return nullptr;
    std::unique_ptr<Event> ret;
    if (!mEvents.empty())
    {
        ret = std::move(mEvents.front());
        mEvents.pop();
    }
    mMutex.unlock();
    return ret;
}

unsigned SourceThreadProxy::WaitBuffer(void* device_buff, unsigned int device_buff_size)
{
    return FillBuffer(device_buff, device_buff_size, true);
}


unsigned SourceThreadProxy::FillBuffer(void* device_buff, unsigned device_buff_size, bool wait_buffer)
{
    unsigned bytes_copied = 0;

    while ((device_buff_size - bytes_copied) > 0)
    {
        VectorBuffer* buffer = nullptr;
        {
            std::unique_lock<decltype(mMutex)> lock(mMutex);

            if (wait_buffer)
            {
                while (mFillQueue.empty() && !mSourceDone && !mException)
                {
                    mCondition.wait(lock);
                }
            }

            if (mException)
                std::rethrow_exception(mException);

            // if data isn't yet available we're in trouble
            // should we just return here ?
            if (mFillQueue.empty())
            {
                return bytes_copied;
            }
            buffer = mFillQueue.front();
        }

        auto* device_buff_ptr = (char*)device_buff;

        bytes_copied += CopyBuffer(buffer,
                                   device_buff_ptr + bytes_copied,
                                   device_buff_size - bytes_copied);

        ASSERT(device_buff_size >= bytes_copied);
    }
    return bytes_copied;
}

unsigned SourceThreadProxy::CopyBuffer(VectorBuffer* source, void* device_buff, unsigned int device_buff_size)
{
    auto* buffer_ptr = (char*)source->GetPtr();

    const unsigned bytes_in_buff = source->GetByteSize();
    const unsigned bytes_to_copy = std::min(device_buff_size, bytes_in_buff);
    const unsigned bytes_to_remain = bytes_in_buff - bytes_to_copy;

    std::memcpy(device_buff, buffer_ptr, bytes_to_copy);

    if (bytes_to_remain)
    {
        // shift the contents in the buffer.
        // todo: get rid of the memmove and use a buffer offset.
        std::memmove(buffer_ptr, buffer_ptr + bytes_to_copy, bytes_to_remain);
        source->SetByteSize(bytes_to_remain);
    }
    else
    {
        if (source->TestFlag(Buffer::Flags::LastBuffer))
        {
            mSourceDone = true;
        }

        source->Clear();

        std::unique_lock<decltype(mMutex)> lock(mMutex);
        mFillQueue.pop();
        mEmptyQueue.push(source);
        mCondition.notify_one();
    }
    return bytes_to_copy;
}

void SourceThreadProxy::ThreadLoop()
{
    DEBUG("Hello from audio source thread. [name='%1']", mSource->GetName());
    std::uint64_t bytes_read = 0;
    try
    {
        for (;;)
        {
            VectorBuffer* buffer = nullptr;
            while (!buffer)
            {
                std::unique_lock<decltype(mMutex)> lock(mMutex);

                if (mShutdown)
                    return;

                while (!mCommands.empty())
                {
                    std::unique_ptr<Command> cmd;
                    cmd = std::move(mCommands.front());
                    mCommands.pop();
                    mSource->RecvCommand(std::move(cmd));
                }

                if (!mEmptyQueue.empty())
                {
                    buffer = mEmptyQueue.front();
                    mEmptyQueue.pop();
                }

                if (!buffer)
                {
                    mCondition.wait(lock);
                }
            }

            const auto buffer_size  = buffer->GetCapacity();
            const auto buffer_used  = buffer->GetByteSize();
            const auto buffer_avail = buffer_size - buffer_used;
            auto* ptr = (char*)buffer->GetPtr() + buffer_used;

            const auto ret = mSource->FillBuffer(ptr, buffer_avail);
            ASSERT(ret <= buffer_avail);
            ASSERT(ret + buffer_used <= buffer_size);

            buffer->SetByteSize(buffer_used + ret);

            bytes_read += ret;

            const bool has_more = mSource->HasMore(bytes_read);
            if (!has_more)
            {
                buffer->SetFlag(Buffer::Flags::LastBuffer, true);
            }

            std::queue<std::unique_ptr<Event>> events;
            while (auto event = mSource->GetEvent())
            {
                events.push(std::move(event));
            }

            {
                std::unique_lock<decltype(mMutex)> lock(mMutex);
                mFillQueue.push(buffer);
                mCondition.notify_one();
                mEvents = std::move(events);
                if (!has_more)
                {
                    break;
                }
            }
        }
        DEBUG("Audio source thread exit. [name='%1']", mSource->GetName());
    }
    catch (const std::exception& e)
    {
        ERROR("Exception in audio source thread. [what='%1']", e.what());
        std::unique_lock<decltype(mMutex)> lock(mMutex);
        mException = std::current_exception();
        mCondition.notify_one();
    }
} // ThreadLoop

} // nameespace
