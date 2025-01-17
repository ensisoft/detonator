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

#define ENGINE_DLL_IMPLEMENTATION

#include "base/logging.h"
#include "base/threadpool.h"
#include "engine/library/library.h"
#include "engine/loader.h"

// helper stuff for dependency management for now.
// see interface.h for more details.

namespace interop {
    IRuntime::~IRuntime()
    {}
}

extern "C" {
ENGINE_DLL_EXPORT void Gamestudio_CreateFileLoaders(Gamestudio_Loaders* out)
{
    out->ContentLoader  = engine::JsonFileClassLoader::Create();
    out->ResourceLoader = engine::FileResourceLoader::Create();
}

ENGINE_DLL_EXPORT void Gamestudio_CreateRuntime(interop::IRuntime** factory)
{
    class Runtime : public interop::IRuntime {
    public:
       ~Runtime() override
        {
            DEBUG("Delete library binary interop runtime.");
            base::SetGlobalThreadPool(nullptr);
        }
        Runtime()
        {
            DEBUG("Created library binary interop runtime.");
            base::SetGlobalThreadPool(&mThreadPool);
        }
        void AddRealThread(size_t threadId) override
        {
            mThreadPool.AddRealThread(threadId);
        }
        void AddMainThread() override
        {
            mThreadPool.AddMainThread();
        }
        void ShutdownThreads() override
        {
            mThreadPool.Shutdown();
        }
        void ExecuteMainThread() override
        {
            mThreadPool.ExecuteMainThread();
        }

        void SetGlobalLogger(base::Logger* logger) override
        {
            base::SetGlobalLog(logger);
        }
        void EnableLogEvent(base::LogEvent event, bool on_off) override
        {
            base::EnableLogEvent(event, on_off);
        }
        void SetThisThreadTracer(base::TraceLog* tracer) override
        {
            base::SetThreadTrace(tracer);
        }

        void SetGlobalTraceWriter(base::TraceWriter* writer) override
        {
            mThreadPool.SetThreadTraceWriter(writer);
        }
        void EnableTracing(bool on_off) override
        {
            base::EnableTracing(on_off);

            mThreadPool.EnableThreadTrace(on_off);
        }

        void Release() override
        {
            delete this;
        }
    private:
        base::ThreadPool mThreadPool;
    };
    *factory = new Runtime();
}

} // extern "C"
