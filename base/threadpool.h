// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#include <thread>
#include <atomic>
#include <memory>
#include <stdexcept>
#include <chrono>
#include <optional>
#include <typeinfo>

#include "base/platform.h"
#include "base/logging.h"
#include "base/bitflag.h"

namespace base
{
    class TraceWriter;

    class ThreadTask
    {
    public:
        struct Description {
            std::string name;
            std::string desc;
        };

        enum class Flags {
            Error, Tracing, TraceLogging
        };

        explicit ThreadTask() noexcept
          : mTaskId(GetNextTaskId())
        {
            mFlags.set(Flags::Tracing, true);
        }
        virtual ~ThreadTask() = default;

        // Get the ID of the task
        inline size_t GetTaskId() const noexcept
        { return mTaskId; }

        inline bool IsComplete() const noexcept
        { return mDone.load(std::memory_order_acquire); }

        inline bool TestFlag(Flags flag) const noexcept
        { return mFlags.test(flag); }

        inline bool Failed() const noexcept
        { return TestFlag(Flags::Error); }

        inline void SetFlag(Flags flag, bool on_off) noexcept
        { mFlags.set(flag, on_off); }

        bool HasException() const noexcept
        { return !!mException; }

        inline void SetDescription(Description description) noexcept
        { mDescription = std::move(description); }
        inline void SetTaskDescription(std::string desc) noexcept
        {
            if (!mDescription.has_value())
                mDescription = Description{};
            mDescription->desc = std::move(desc);
        }
        inline void SetTaskName(std::string name) noexcept
        {
            if (!mDescription.has_value())
                mDescription = Description{};
            mDescription->name = std::move(name);
        }

        inline bool HasDescription() const noexcept
        { return mDescription.has_value(); }

        inline std::string GetTaskDescription() const noexcept
        {
            if (mDescription.has_value())
                return mDescription.value().desc;
            return "";
        }

        inline std::string GetTaskName() const noexcept
        {
            if (mDescription.has_value())
                return mDescription.value().name;
            return typeid(*this).name();
        }
        inline std::string GetErrorString() const noexcept
        { return mErrorString; }

        void Execute();
        void RethrowException() const;

        virtual void GetValue(const char* key, void* ptr)
        {}

    protected:
        virtual void DoTask() = 0;

    protected:
        base::bitflag<Flags> mFlags;

        void SetError(std::string error) noexcept
        {
            mFlags.set(Flags::Error, true);
            mErrorString = std::move(error);
        }
        void SetError() noexcept
        { mFlags.set(Flags::Error, true); }


    private:
        static size_t GetNextTaskId() noexcept
        {
            static std::atomic<size_t> id(1);
            return id++;
        }

    private:
        std::size_t mTaskId = 0;
        std::exception_ptr mException;
        std::atomic<bool> mDone = {false};
        std::optional<Description> mDescription;
        std::string mErrorString;
    };


    class TaskHandle
    {
    public:
        TaskHandle() = default;

        explicit TaskHandle(std::shared_ptr<ThreadTask> task, size_t threadId)
          : mTask(std::move(task))
          , mThreadId(threadId)
        {}

        inline bool IsComplete() const noexcept
        {
            return mTask->IsComplete();
        }

        enum class WaitStrategy {
            BusyLoop, Sleep, WaitCondition
        };

        void Wait(WaitStrategy strategy) noexcept;

        const ThreadTask* GetTask() const noexcept
        {
            if (!IsComplete())
                return nullptr;
            return mTask.get();
        }
        ThreadTask* GetTask() noexcept
        {
            if (!IsComplete())
                return nullptr;
            return mTask.get();
        }
        std::shared_ptr<ThreadTask> GetSharedTask() noexcept
        {
            if (!IsComplete())
                return nullptr;
            return mTask;
        }
        std::shared_ptr<const ThreadTask> GetSharedTask() const noexcept
        {
            if (!IsComplete())
                return nullptr;
            return mTask;
        }
        inline bool IsValid() const noexcept
        { return !!mTask; }

        inline void Clear() noexcept
        {
            mTask.reset();
        }
        inline operator bool () const noexcept
        {
            return IsValid();
        }

        inline std::string GetTaskDescription() const noexcept
        {
            return mTask->GetTaskDescription();
        }
        inline std::string GetTaskName() const noexcept
        {
            return mTask->GetTaskName();
        }

    private:
        std::shared_ptr<ThreadTask> mTask;
        std::size_t mThreadId = 0;
    };

    class ThreadPool
    {
    public:
        class Thread;

        static constexpr size_t MainThreadID      = 0;
        static constexpr size_t AudioThreadID     = 1;
        static constexpr size_t UpdateThreadID    = 2;
        static constexpr size_t RenderThreadID    = 3;

        static constexpr size_t Worker0ThreadID   = 1 << 8;
        static constexpr size_t Worker1ThreadID   = 2 << 8;
        static constexpr size_t Worker2ThreadID   = 3 << 8;
        static constexpr size_t Worker3ThreadID   = 4 << 8;

        static constexpr size_t AnyWorkerThreadID = 0xffff;

        ThreadPool();
       ~ThreadPool();

        void AddRealThread(size_t threadId);
        void AddMainThread();

        TaskHandle SubmitTask(std::unique_ptr<ThreadTask> task,
                              std::size_t threadID = AnyWorkerThreadID);

        void Shutdown();

        void WaitAll();

        bool HasPendingTasks() const;

        bool HasThread(std::size_t threadId) const;

        void ExecuteMainThread();

        void SetThreadTraceWriter(base::TraceWriter* writer);
        void EnableThreadTrace(bool enable);

    private:
        struct State;
        class RealThread;
        class MainThread;
    private:
        std::shared_ptr<State> mState;
        std::vector<std::unique_ptr<RealThread>> mRealThreads;
        std::unique_ptr<MainThread> mMainThread;
        std::size_t mRoundRobin = 0;
    };

    ThreadPool* GetGlobalThreadPool();

    void SetGlobalThreadPool(ThreadPool* pool);

    inline bool HaveGlobalThreadPool() noexcept
    {
        return GetGlobalThreadPool() != nullptr;
    }

} // namespace
