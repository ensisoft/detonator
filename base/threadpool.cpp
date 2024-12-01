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
#include "base/trace.h"

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
    virtual size_t GetThreadId() const = 0;
};

class ThreadPool::RealThread : public ThreadPool::Thread
{
public:
    explicit RealThread(std::shared_ptr<State> state, std::size_t id) noexcept
       : mState(std::move(state))
       , mThreadId(id)
    {}

    void Submit(std::shared_ptr<ThreadTask> task) override
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mTaskQueue.push(std::move(task));
        mCondition.notify_one();
    }

    size_t GetThreadId() const override
    {
        return mThreadId;
    }

    void Shutdown()
    {
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mRunThread = false;
            mCondition.notify_one();
        }
        mThread->join();

        ASSERT(mTaskQueue.empty());
    }
    void Start()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mThread.reset(new std::thread([this]() {
            ThreadMain();
        }));
    }

    void SetThreadTraceWriter(base::TraceWriter* writer)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mTraceWriter = writer;
        mCondition.notify_one();
    }
    void EnableThreadTrace(bool enable)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mEnableTrace = enable;
        mCondition.notify_one();
    }

private:
    void ThreadMain()
    {
        DEBUG("Hello from thread pool thread %1.", mThreadId);
        std::unique_ptr<base::TraceLog> trace;

        while (true)
        {
            // enable disable tracing on this thread
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (mTraceWriter && !base::GetThreadTrace())
                {
                    trace = std::make_unique<base::TraceLog>(1000, base::TraceLog::ThreadId::TaskThread + mThreadId);
                    base::SetThreadTrace(trace.get());
                }
                else if (!mTraceWriter && base::GetThreadTrace())
                {
                    trace.reset();
                    base::SetThreadTrace(nullptr);
                }
                base::EnableTracing(mEnableTrace);
            }

            std::shared_ptr<ThreadTask> task;
            bool running = true;

            // most of the trace calls are commented out because of the large
            // volume of data that is generated. Only have kept the task tracing
            // for now and only when the flag is set.
            TRACE_START();
            ///TRACE_ENTER(MainLoop);

            // use an explicit scope here to make sure the lock is
            // released as soon as we have something
            {
                ///TRACE_SCOPE("WaitTask");
                std::unique_lock<std::mutex> lock(mMutex);

                // use a loop in order to protect against spurious
                // signals on the condition
                while (mRunThread && mTaskQueue.empty())
                {
                    mCondition.wait(lock);
                }

                if (!mTaskQueue.empty())
                {
                    task = std::move(mTaskQueue.front());
                    mTaskQueue.pop();
                }
                running = mRunThread;
            }

            if (!running)
            {
                ///TRACE_LEAVE(MainLoop);
                break;
            }

            if (task)
            {
                if (task->TestFlag(ThreadTask::Flags::Tracing))
                {
                    TRACE_CALL("Task::Execute", task->Execute());
                }
                else
                {
                    task->Execute();
                }
                mState->num_tasks--;
            }

            ///TRACE_LEAVE(MainLoop);

            // take a mutex on the trace writer to make sure that it will
            // not be deleted from underneath us while we're dumping
            // this thread's trace log to the writer.
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (mTraceWriter && trace)
                {
                    trace->Write(*mTraceWriter);
                }
            }

        }
        DEBUG("Thread pool thread exiting...");
    }

private:
    std::shared_ptr<State> mState;
    std::mutex mMutex;
    std::condition_variable mCondition;
    std::unique_ptr<std::thread> mThread;
    std::queue<std::shared_ptr<ThreadTask>> mTaskQueue;

    base::TraceWriter* mTraceWriter = nullptr;
    bool mEnableTrace = false;
    bool mRunThread = true;
    std::size_t mThreadId = 0;
};

class ThreadPool::MainThread : public ThreadPool::Thread
{
public:
    explicit MainThread(std::shared_ptr<State> state) noexcept
      : state_(std::move(state))
    {}

    void Submit(std::shared_ptr<ThreadTask> task) override
    {
        queue_.push(std::move(task));
    }
    size_t GetThreadId() const override
    {
        return ThreadPool::MainThreadID;
    }

    void ExecuteMainThread()
    {
        ///TRACE_SCOPE("ExecuteMainThread");

        while (!queue_.empty())
        {
            auto task = queue_.front();

            queue_.pop();

            if (!task->IsComplete())
            {
                if (task->TestFlag(ThreadTask::Flags::Tracing))
                {
                    TRACE_CALL("Task::Execute", task->Execute());
                }
                else
                {
                    task->Execute();
                }
            }

            state_->num_tasks--;
        }
    }
private:
    std::shared_ptr<State> state_;
    std::queue<std::shared_ptr<ThreadTask>> queue_;

};


void TaskHandle::Wait(WaitStrategy strategy) noexcept
{
    // assuming our current thread is the "main" thread
    // if we call here to wait for completion we'll spin
    // indefinitely since the current thread would then
    // have to call the execute main thread.

    if (mThreadId == ThreadPool::MainThreadID)
    {
        if (!IsComplete())
        {
            mTask->Execute();
        }
    }

    while (!IsComplete())
    {
        // holy hell batman, let's not cause a stall here by blocking the
        // calling thread for unexpectedly long time. the reason why this
        // might actually sleep for much longer is due to the system timer
        // granularity. Especially on the web we have no control (no API
        // such as timeBeginPeriod) to try to adjust the granularity.
        if (strategy == WaitStrategy::Sleep)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    }
}


ThreadPool::ThreadPool()
  : mState(std::make_shared<State>())
{}

ThreadPool::~ThreadPool()
{
    Shutdown();
}

void ThreadPool::AddRealThread(size_t threadId)
{
    auto thread = std::make_unique<RealThread>(mState, threadId);
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

TaskHandle ThreadPool::SubmitTask(std::unique_ptr<ThreadTask> task, size_t threadId)
{
    Thread* thread = nullptr;

    if (threadId == ThreadPool::MainThreadID)
    {
        ASSERT(mMainThread && "Main thread has not been added to the thread pool");
        thread = mMainThread.get();
    }
    else if (threadId == ThreadPool::AnyWorkerThreadID)
    {
        std::vector<RealThread*> workers;
        for (auto& t : mRealThreads)
        {
            if (t->GetThreadId() & 0xff00)
                workers.push_back(t.get());

        }

        ASSERT(!workers.empty() && "The thread pool has no worker threads.");
        thread = workers[mRoundRobin % workers.size()];
        mRoundRobin++;

    }
    else
    {
        for (auto& t : mRealThreads)
        {
            if (t->GetThreadId() == threadId)
            {
                thread = t.get();
                break;
            }
        }
        ASSERT(thread && "No such named thread has been added to the thread pool.");
    }

    std::shared_ptr<ThreadTask> shared(std::move(task));

    TaskHandle handle(shared, threadId);

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

void ThreadPool::SetThreadTraceWriter(base::TraceWriter* writer)
{
    for (auto& thread : mRealThreads)
    {
        thread->SetThreadTraceWriter(writer);
    }
}
void ThreadPool::EnableThreadTrace(bool enable)
{
    for (auto& thread : mRealThreads)
    {
        thread->EnableThreadTrace(enable);
    }
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
