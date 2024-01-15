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

#include "base/platform.h"
#include "base/logging.h"
#include "base/bitflag.h"

namespace base
{
    class ThreadTask
    {
    public:
        enum class Flags {
            Error
        };

        enum class Affinity {
            // The task is to be executed by the main "gui" thread only.
            MainThread,
            // The task can be executed by any thread in any order.
            AnyThread,
            // The task can be executed only by a certain thread with
            // a mapping to the task id. this means that all tasks with
            // the same parent id will be submitted to the same thread.
            SingleThread
        };
        explicit ThreadTask(Affinity affinity = Affinity::AnyThread, size_t parent = 0) noexcept
          : mAffinity(affinity)
          , mTaskId(GetNextTaskId())
          , mParentId(parent)
        {}
        virtual ~ThreadTask() = default;

        inline Affinity GetAffinity() const noexcept
        { return mAffinity; }

        // Get the ID of the task
        inline size_t GetTaskId() const noexcept
        { return mTaskId; }

        inline size_t GetParentId() const noexcept
        { return mParentId; }

        inline bool IsComplete() const noexcept
        { return mDone.load(std::memory_order_acquire); }

        inline bool TestFlag(Flags flag) const noexcept
        { return mFlags.test(flag); }

        inline bool Failed() const noexcept
        { return TestFlag(Flags::Error); }

        void Execute()
        {
            try
            {
                DoTask();
            }
            catch (const std::exception& e)
            {
               mException = std::current_exception();
            }
            mDone.store(true, std::memory_order_release);
        }
        virtual void GetValue(const char* key, void* ptr)
        {}

    protected:
        virtual void DoTask() = 0;

    protected:
        base::bitflag<Flags> mFlags;

    private:
        static size_t GetNextTaskId() noexcept
        {
            static std::atomic<size_t> id(1);
            return id++;
        }

    private:
        const Affinity mAffinity;
        const std::size_t mTaskId = 0;
        const std::size_t mParentId = 0;
        std::exception_ptr mException;
        std::atomic<bool> mDone = {false};

    };


    class TaskHandle
    {
    public:
        TaskHandle() = default;

        explicit TaskHandle(std::shared_ptr<ThreadTask> task)
          : mTask(std::move(task))
        {}

        inline bool IsComplete() const noexcept
        {
            return mTask->IsComplete();
        }

        void Wait() noexcept;

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

    private:
        std::shared_ptr<ThreadTask> mTask;
    };

    class ThreadPool
    {
    public:
        class Thread;

        ThreadPool();
       ~ThreadPool();

        void AddRealThread();
        void AddMainThread();

        TaskHandle SubmitTask(std::unique_ptr<ThreadTask> task);

        void Shutdown();

        void WaitAll();

        void ExecuteMainThread();

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
