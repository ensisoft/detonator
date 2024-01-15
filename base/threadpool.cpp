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


#include "config.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>

#include "base/assert.h"
#include "base/logging.h"
#include "base/threadpool.h"

namespace {
    base::ThreadPool* global_thread_pool;
} // namespace

namespace base
{
struct ThreadPool::State {
    std::atomic<size_t> num_tasks = 0;
};

class ThreadPool::Thread
{
public:
    virtual ~Thread() = default;
    virtual void Submit(std::shared_ptr<ThreadTask> task) = 0;
};

class ThreadPool::RealThread : public ThreadPool::Thread
{
public:
    explicit RealThread(std::shared_ptr<State> state) noexcept
       : state_(std::move(state))
    {}

    virtual void Submit(std::shared_ptr<ThreadTask> task) override
    {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push(std::move(task));
        cond.notify_one();
    }
    void Shutdown()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            run_loop = false;
            cond.notify_one();
        }
        thread->join();

        ASSERT(queue.empty());
    }
    void Start()
    {
        std::lock_guard<std::mutex> lock(mutex);
        thread.reset(new std::thread([this]() {
            ThreadMain();
        }));
    }

private:
    void ThreadMain()
    {
        while (true)
        {
            std::shared_ptr<ThreadTask> task;

            // use an explicit scope here to make sure the lock is
            // released as soon as we have something
            {
                std::unique_lock<std::mutex> lock(mutex);
                // use a loop in order to protect against spurious
                // signals on the condition
                while (true)
                {
                    if (!run_loop)
                        return;
                    if (queue.empty())
                        cond.wait(lock);

                    if (!queue.empty())
                    {
                        task = std::move(queue.front());
                        queue.pop();
                        break;
                    }
                }
            }
            ASSERT(task);

            task->Execute();

            state_->num_tasks--;
        }
    }

private:
    std::shared_ptr<State> state_;
    std::mutex mutex;
    std::condition_variable cond;
    std::unique_ptr<std::thread> thread;
    std::queue<std::shared_ptr<ThreadTask>> queue;
    bool run_loop = true;
};

class ThreadPool::MainThread : public ThreadPool::Thread
{
public:
    explicit MainThread(std::shared_ptr<State> state) noexcept
      : state_(std::move(state))
    {}

    virtual void Submit(std::shared_ptr<ThreadTask> task) override
    {
        queue_.push(std::move(task));
    }

    void ExecuteMainThread()
    {
        while (!queue_.empty())
        {
            auto task = queue_.front();

            queue_.pop();

            if (!task->IsComplete())
                task->Execute();

            state_->num_tasks--;
        }
    }
private:
    std::shared_ptr<State> state_;
    std::queue<std::shared_ptr<ThreadTask>> queue_;

};


void TaskHandle::Wait() noexcept
{
    // assuming our current thread is the "main" thread
    // if we call here to wait for completion we'll spin
    // indefinitely since the current thread would then
    // have to call the execute main thread.

    if (mTask->GetAffinity() == ThreadTask::Affinity::MainThread)
    {
        if (!IsComplete())
            mTask->Execute();
    }

    while (!IsComplete())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
}


ThreadPool::ThreadPool()
  : mState(std::make_shared<State>())
{}

ThreadPool::~ThreadPool()
{
    Shutdown();
}

void ThreadPool::AddRealThread()
{
    auto thread = std::make_unique<RealThread>(mState);
    thread->Start();
    mRealThreads.push_back(std::move(thread));
    DEBUG("Added real thread pool thread.");
}

void ThreadPool::AddMainThread()
{
    ASSERT(mMainThread == nullptr);
    mMainThread = std::make_unique<MainThread>(mState);
    DEBUG("Added thread pool main thread.");
}

TaskHandle ThreadPool::SubmitTask(std::unique_ptr<ThreadTask> task)
{
    const auto num_threads  = mRealThreads.size();
    const auto affinity = task->GetAffinity();
    Thread* thread = nullptr;

    if (affinity == ThreadTask::Affinity::MainThread)
    {
        ASSERT(mMainThread);
        thread = mMainThread.get();
    }
    else if (affinity == ThreadTask::Affinity::AnyThread)
    {
        if (num_threads == 0)
        {
            ASSERT(mMainThread);
            thread = mMainThread.get();
        }
        else
        {
            thread = mRealThreads[mRoundRobin % num_threads].get();
            mRoundRobin++;
        }
    }
    else if (affinity == ThreadTask::Affinity::SingleThread)
    {
        if (num_threads == 0)
        {
            ASSERT(mMainThread);
            thread = mMainThread.get();
        }
        else
        {
            const auto id = task->GetParentId();
            thread = mRealThreads[id % num_threads].get();
        }
    }

    std::shared_ptr<ThreadTask> shared(std::move(task));

    TaskHandle handle(shared);

    thread->Submit(std::move(shared));

    mState->num_tasks++;

    return handle;
}

void ThreadPool::Shutdown()
{
    DEBUG("Thread pool shutdown.");

    for (auto& thread : mRealThreads)
    {
        thread->Shutdown();
    }
    mRealThreads.clear();

    mMainThread.reset();
}

void ThreadPool::WaitAll()
{
    // this is quick and dirty waiting
    while (mState->num_tasks)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

void ThreadPool::ExecuteMainThread()
{
    if (mMainThread)
        mMainThread->ExecuteMainThread();
}

ThreadPool* GetGlobalThreadPool()
{
    return global_thread_pool;
}

void SetGlobalThreadPool(ThreadPool* pool)
{
    global_thread_pool = pool;
}

} // namespace