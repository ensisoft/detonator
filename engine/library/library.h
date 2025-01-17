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

#pragma once

#include "config.h"

#include <memory>
#include <algorithm>

#include "base/platform.h"
#include "base/trace.h"
#include "base/logging.h"
#include "engine/engine.h"
#include "engine/loader.h"

// Win32 / MSVS export/import declarations
#if defined(__MSVC__)
#  define ENGINE_DLL_EXPORT __declspec(dllexport)
#  define ENGINE_DLL_IMPORT __declspec(dllimport)
#  if defined(ENGINE_DLL_IMPLEMENTATION)
#    define ENGINE_DLL_API __declspec(dllexport)
#  else 
#    define ENGINE_DLL_API __declspec(dllimport)
#  endif
#else
#  define ENGINE_DLL_EXPORT
#  define ENGINE_DLL_IMPORT
#  define ENGINE_DLL_API
#endif

namespace interop {
    // a pseudo COM interface for binary fire-walling stuff
    // so that we can (hopefully) avoid issues related to static
    // etc in several binary / link unit.
    class IRuntime
    {
    public:
        virtual void AddRealThread(size_t threadId) = 0;
        virtual void AddMainThread() = 0;
        virtual void ShutdownThreads() = 0;
        virtual void ExecuteMainThread() = 0;

        virtual void SetGlobalLogger(base::Logger* logger) = 0;
        virtual void EnableLogEvent(base::LogEvent event, bool on_off) = 0;

        virtual void SetThisThreadTracer(base::TraceLog* tracer) = 0;
        virtual void SetGlobalTraceWriter(base::TraceWriter* writer) = 0;
        virtual void EnableTracing(bool on_off) = 0;

        virtual void Release() = 0;
    protected:
        virtual ~IRuntime() = 0;
    };

    template<typename Type>
    class Pointer
    {
    public:
        explicit Pointer(Type* object) noexcept
          : mRuntime(object)
        {}
        Pointer() = default;
        ~Pointer()
        {
            if (mRuntime)
                mRuntime->Release();
        }
        Pointer(Pointer&& other) noexcept
           : mRuntime(other.mRuntime)
        {
            other.mRuntime = nullptr;
        }
        Pointer(const Pointer&) = delete;
        Pointer& operator=(const Pointer&) = delete;

        inline Pointer& operator=(Pointer&& other) noexcept
        {
            Pointer tmp(std::move(other));
            std::swap(mRuntime, tmp.mRuntime);
            return *this;
        }
        inline Type* operator->() noexcept
        {
            return mRuntime;
        }
        inline const Type* operator->() const noexcept
        {
            return mRuntime;
        }
        inline Type* get() noexcept
        {
            return mRuntime;
        }
        inline const Type* get() const noexcept
        {
            return mRuntime;
        }
        inline Type*& get_ref() noexcept
        {
            return mRuntime;
        }
        inline void Reset() noexcept
        {
            if (mRuntime)
                mRuntime->Release();

            mRuntime = nullptr;
        }
        inline operator bool() const noexcept
        {
            return mRuntime != nullptr;
        }
    private:
        Type* mRuntime = nullptr;
    };

    using Runtime = Pointer<IRuntime>;

} // namespace

// The below interface only exists currently for simplifying
// the structure of the builds. I.e the dependencies for creating
// environment objects (such as ContentLoader) can be wrapped inside
// the game library itself and this lets the loader application to
// remain free of these dependencies.
// This is currently only an implementation detail and this mechanism
// might go away. However currently we provide this helper that will
// do the wrapping and then expect the game libs to include the right
// translation unit in their builds.
struct Gamestudio_Loaders {
    std::unique_ptr<engine::JsonFileClassLoader> ContentLoader;
    std::unique_ptr<engine::FileResourceLoader> ResourceLoader;
};

// Main interface for bootstrapping/loading the game/app
extern "C" {
    // return a new app implementation allocated on the free store.
    ENGINE_DLL_API engine::Engine* Gamestudio_CreateEngine();
    ENGINE_DLL_API void Gamestudio_CreateRuntime(interop::IRuntime**);
    ENGINE_DLL_API void Gamestudio_CreateFileLoaders(Gamestudio_Loaders* out);
} // extern "C"

typedef engine::Engine* (*Gamestudio_CreateEngineFunc)();
typedef void (*Gamestudio_CreateFileLoadersFunc)(Gamestudio_Loaders*);
typedef void (*Gamestudio_CreateRuntimeFunc)(interop::IRuntime**);
