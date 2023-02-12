// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#include "graphics/device.h"

namespace dev {
    // OpenGL graphics context. The context is the interface for the device
    // to resolve the (possibly context specific) OpenGL entry points.
    // This abstraction allows the device to remain agnostic as to
    // what kind of windowing system/graphics subsystem is creating the context
    // and what is the ultimate rendering target (pbuffer, pixmap or window)
    class Context
    {
    public:
        enum class Version {
            OpenGL_ES2,
            OpenGL_ES3,
            WebGL_1,
            WebGL_2
        };

        virtual ~Context() = default;
        // Display the current contents of the rendering target.
        virtual void Display() = 0;
        // Make this context as the current context for the calling thread.
        // Note: In OpenGL all the API functions assume an "implicit" context
        // for the calling thread to be a global object that is set through
        // the window system integration layer i.e. through calling some method
        // on  WGL, GLX, EGL or AGL. If an application is creating multiple
        // contexts in some thread before starting to use any particular
        // context it has to be made the "current content".
        virtual void MakeCurrent() = 0;
        // Resolve an OpenGL API function to a function pointer.
        // Note: The function pointers can indeed be different for
        // different contexts depending on their specific configuration.
        // Returns a valid pointer or nullptr if there's no such
        // function. (For example an extension function is not available).
        virtual void* Resolve(const char* name) = 0;
        // Get the context version.
        virtual Version GetVersion() const = 0;
        // Check whether the context is a debug context or not.
        // if the context is a debug context then additional debug
        // features are enabled when supported by the underlying platform.
        virtual bool IsDebug() const
        { return false; }
    private:
    };

    class Device
    {
    public:
        virtual ~Device() = default;

        // If the device has graphics capabilities return the device as graphics device.
        // if the device doesn't have graphics capabilities returns nullptr.
        virtual gfx::Device* AsGraphicsDevice() = 0;
        // Get a shared ptr handle of the device as a graphics device.
        // If the device doesn't have graphics capabilities returns null handle.
        virtual std::shared_ptr<gfx::Device> GetSharedGraphicsDevice() = 0;
    private:
    };

    std::shared_ptr<Device> CreateDevice(std::shared_ptr<Context> context);
    std::shared_ptr<Device> CreateDevice(Context* context);

} // namespace