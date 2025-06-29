// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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

#pragma once

#include "config.h"

#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <exception>
#include <memory>
#include <thread>

#include "base/trace.h"
#include "audio/source.h"
#include "audio/buffer.h"

namespace audio
{
    // Wrap an audio source inside a dedicated standalone thread.
    class ThreadProxySource : public Source
    {
    public:
        explicit ThreadProxySource(std::unique_ptr<Source> source) noexcept;
       ~ThreadProxySource() override;

        unsigned GetRateHz() const noexcept override;
        unsigned GetNumChannels() const noexcept override;
        Format GetFormat() const noexcept override;
        std::string GetName() const noexcept override;
        void Prepare(unsigned buffer_size) override;
        unsigned FillBuffer(void* buff, unsigned max_bytes) override;
        bool HasMore(std::uint64_t num_bytes_read) const noexcept override;
        void Shutdown() noexcept override;
        void RecvCommand(std::unique_ptr<Command> cmd) noexcept override;
        std::unique_ptr<Event> GetEvent() noexcept override;

        unsigned WaitBuffer(void* device_buff, unsigned device_buff_size);

        static void SetThreadTraceWriter(base::TraceWriter* writer)
        {
            std::lock_guard<std::mutex> lock(TraceWriterMutex);
            TraceWriter = writer;
        }
        static void EnableThreadTrace(bool enable)
        {
            std::lock_guard<std::mutex> lock(TraceWriterMutex);
            EnableTrace = enable;
        }

    private:
        unsigned FillBuffer(void* device_buff, unsigned device_buff_size, bool wait_buffer);
        unsigned CopyBuffer(VectorBuffer* source, void* device_buff, unsigned device_buff_size);
        void ThreadLoop();
    private:
        mutable std::mutex mMutex;
        std::unique_ptr<Source> mSource;
        std::unique_ptr<std::thread> mThread;
        std::queue<std::unique_ptr<Event>> mEvents;
        std::queue<std::unique_ptr<Command>> mCommands;
        std::condition_variable mCondition;
        std::queue<VectorBuffer*> mEmptyQueue;
        std::queue<VectorBuffer*> mFillQueue;
        std::vector<VectorBuffer> mBuffers;
        std::exception_ptr mException;
        bool mShutdown = false;
        bool mSourceDone = false;
        bool mFirstBuffer = true;

        static std::mutex TraceWriterMutex;
        static base::TraceWriter* TraceWriter;
        static bool EnableTrace;
    };

} // namespace