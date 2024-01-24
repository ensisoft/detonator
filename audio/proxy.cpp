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
    ASSERT(!mThread);
    ASSERT(!mSource);
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

    mBuffers.resize(1);
    for (auto& buff : mBuffers)
    {
        buff.Resize(buffer_size);
        mEmptyQueue.push(&buff);
    }

    mThread.reset(new std::thread(&SourceThreadProxy::ThreadLoop, this));
}
unsigned SourceThreadProxy::FillBuffer(void* device_buff, unsigned max_bytes)
{
    VectorBuffer* buffer = nullptr;
    while (!buffer)
    {
        std::unique_lock<decltype(mMutex)> lock(mMutex);
        if (mException)
            std::rethrow_exception(mException);

        // if data isn't yet available we're in trouble
        // should we just return here ?
        if (!mFillQueue.empty())
        {
            buffer = mFillQueue.front();
            mFillQueue.pop();
        }

        if (!buffer)
        {
            //DEBUG("Argh, no PCM buffer from source thread!");
            mCondition.wait(lock);
        }
    }

    const auto bytes_in_buff = (unsigned)buffer->GetByteSize();
    const auto bytes_to_copy = std::min(max_bytes, bytes_in_buff);
    std::memcpy(device_buff, buffer->GetPtr(), bytes_to_copy);

    if (max_bytes < bytes_in_buff)
    {
        // if the thread produced more data than was consumed we shift
        // the remaining data in the buffer to the start and let the
        // thread fill more to the buffer end.

        const auto bytes_left_over = bytes_in_buff - bytes_to_copy;
        char* ptr = (char*)buffer->GetPtr();
        std::memmove(ptr, ptr+bytes_to_copy, bytes_left_over);
        buffer->SetByteSize(bytes_left_over);
    }
    else if (max_bytes > bytes_in_buff)
    {
        // if the device wanted more PCM than the thread has produced
        // log a warning for now.
        WARN("Device requested more PCM data than audio source thread has produced. [out=%1 b, avail=%2 b]",
             max_bytes, bytes_in_buff);
        // should have copied everything we have produced.
        ASSERT(bytes_to_copy == bytes_in_buff);

        buffer->Clear();
    }
    else if (max_bytes == bytes_in_buff)
    {
        buffer->Clear();
    }

    // enqueue the buffer back for filling
    {
        std::unique_lock<decltype(mMutex)> lock(mMutex);
        mEmptyQueue.push(buffer);
        mCondition.notify_one();
    }

    //DEBUG("Got audio buffer from source thread. [bytes=%1 b]", bytes_to_copy);

    return bytes_to_copy;
}

bool SourceThreadProxy::HasMore(std::uint64_t num_bytes_read) const noexcept
{
    std::unique_lock<decltype(mMutex)> lock(mMutex);
    return !mFillQueue.empty() || !mDone;
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
            {
                std::unique_lock<decltype(mMutex)> lock(mMutex);

                mFillQueue.push(buffer);
                mCondition.notify_one();
                if (!mSource->HasMore(bytes_read))
                {
                    mDone = true;
                    break;
                }

                while (auto event = mSource->GetEvent())
                {
                    mEvents.push(std::move(event));
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
