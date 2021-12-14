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
    : mSampleRate(source->GetRateHz())
    , mChannels(source->GetNumChannels())
    , mFormat(source->GetFormat())
    , mName(source->GetName())
    , mSource(std::move(source))
{}
SourceThreadProxy::~SourceThreadProxy()
{
    ASSERT(!mThread);
}
unsigned SourceThreadProxy::GetRateHz() const noexcept
{ return mSampleRate; }
unsigned SourceThreadProxy::GetNumChannels() const noexcept
{ return mChannels; }
Source::Format SourceThreadProxy::GetFormat() const noexcept
{ return mFormat; }
std::string SourceThreadProxy::GetName() const noexcept
{ return mName; }
void SourceThreadProxy::Prepare(unsigned int buffer_size)
{
    std::vector<char> buffer(buffer_size);
    mBufferSize = buffer_size;
    mEmptyQueue.push(std::move(buffer));
    mThread.reset(new std::thread(&SourceThreadProxy::ThreadLoop, this));
}
unsigned SourceThreadProxy::FillBuffer(void* buff, unsigned max_bytes)
{
    std::unique_lock<decltype(mMutex)> lock(mMutex);
    // if data isn't yet available we're in trouble
    if (mFillQueue.empty())
    {
        for (;;)
        {
            mCondition.wait(lock);
            if (!mFillQueue.empty() || mException)
                break;
        }
    }
    if (mException)
        std::rethrow_exception(mException);

    auto& buffer = mFillQueue.front();

    const auto bytes_in_buff = (unsigned)buffer.size();
    const auto bytes_to_copy = std::min(max_bytes, bytes_in_buff);
    std::memcpy(buff, &buffer[0], bytes_to_copy);

    if (bytes_to_copy == bytes_in_buff)
    {
        mEmptyQueue.push(std::move(buffer));
        mFillQueue.pop();
        mCondition.notify_one();
    }
    else
    {
        const auto bytes_left = bytes_in_buff - bytes_to_copy;
        std::memmove(&buffer[0], &buffer[bytes_to_copy], bytes_left);
        buffer.resize(bytes_left);
    }
    return bytes_to_copy;
}

bool SourceThreadProxy::HasMore(std::uint64_t num_bytes_read) const noexcept
{
    std::unique_lock<decltype(mMutex)> lock(mMutex);
    return !mFillQueue.empty() || !mDone;
}

void SourceThreadProxy::Shutdown() noexcept
{
    {
        std::unique_lock<decltype(mMutex)> lock(mMutex);
        mShutdown = true;
        mCondition.notify_one();
    }
    mThread->join();
    mThread.reset();
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
    DEBUG("Hello from audio source thread.");
    std::uint64_t bytes_read = 0;
    try
    {
        for (;;)
        {
            std::vector<char> buffer;
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
                while (auto event = mSource->GetEvent())
                {
                    mEvents.push(std::move(event));
                }
                if (mEmptyQueue.empty())
                {
                    for (;;)
                    {
                        mCondition.wait(lock);
                        if (mShutdown)
                            return;
                        if (!mEmptyQueue.empty())
                            break;
                    }
                    buffer = std::move(mEmptyQueue.front());
                    mEmptyQueue.pop();
                }
            }
            buffer.resize(mBufferSize);
            const auto ret = mSource->FillBuffer(&buffer[0], buffer.size());
            bytes_read += ret;
            {
                std::unique_lock<decltype(mMutex)> lock(mMutex);

                mFillQueue.push(std::move(buffer));
                mCondition.notify_one();
                if (!mSource->HasMore(bytes_read))
                {
                    mDone = true;
                    break;
                }
            }
        }https://en.wikipedia.org/wiki/Alpha_compositing
        DEBUG("Audio source thread exiting...");
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
