// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#define LOGTAG "gfx"

#include "config.h"

#include <memory>

#include "editor/app/eventlog.h"
#include "editor/gui/device.h"

namespace {

class WindowContext : public gfx::Device::Context
{
public:
    WindowContext(QSurface* surface)
    {
        mContext.create();
        mContext.makeCurrent(surface);
        mCurrentSurface = surface;
    }
    virtual void Display() override
    {
        mContext.swapBuffers(mCurrentSurface);
    }
    virtual void MakeCurrent() override
    {}
    virtual void* Resolve(const char* name) override
    {
        return (void*)mContext.getProcAddress(name);
    }
    void MakeCurrent(QSurface* surface)
    {
        if (mCurrentSurface == surface)
            return;
        mContext.makeCurrent(surface);
        mCurrentSurface = surface;
    }
private:
    QOpenGLContext mContext;
    QSurface* mCurrentSurface = nullptr;
};

std::shared_ptr<WindowContext> context;
std::shared_ptr<gfx::Device> device;
std::unique_ptr<gfx::Painter> painter;

void CreateGraphicsResources(QSurface* surface)
{
    if (context && device)
        return;

    // create the context and the device.
    // todo: perhaps the painter should be more lightweight and removed from here
    // i.e. the gfx widget would just do Painter p(GetGraphicsDevice()); ?
    context = std::make_shared<WindowContext>(surface);
    device  = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2, context);
    painter = gfx::Painter::Create(device);

    const auto& fmt = QSurfaceFormat::defaultFormat();
    const auto sync_to_vblank = fmt.swapInterval() == 1;
    const auto msaa_samples = fmt.samples();

    DEBUG("Created new graphics device. sync to vblank: %1, MSAA: %2",
        sync_to_vblank ? "ON" : "OFF", msaa_samples);
}

} // namespace

namespace gui
{

gfx::Device* GetGraphicsDevice(QSurface* surface)
{
    CreateGraphicsResources(surface);

    context->MakeCurrent(surface);
    return device.get();
}

gfx::Painter* GetGraphicsPainter(QSurface* surface)
{
    CreateGraphicsResources(surface);

    context->MakeCurrent(surface);
    return painter.get();
}

void CreateGraphicsDevice(unsigned msaa_samples, bool sync_to_vblank)
{
    QSurfaceFormat format;
    format.setVersion(2, 0);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setRenderableType(QSurfaceFormat::OpenGLES);
    format.setDepthBufferSize(0); // currently we don't care
    format.setAlphaBufferSize(8);
    format.setRedBufferSize(8);
    format.setGreenBufferSize(8);
    format.setBlueBufferSize(8);
    format.setStencilBufferSize(8);
    format.setSamples(msaa_samples);
    // Setting an interval value of 0 will turn the vertical refresh syncing off,
    // any value higher than 0 will turn the vertical syncing on.
    // Setting interval to a higher value, for example 10,
    // results in having 10 vertical retraces between every buffer swap.
    format.setSwapInterval(sync_to_vblank ? 1 : 0);

    // Seems that this is somehow required, i.e. giving the QSurfaceFormat
    // to QOpenGLContext through setFormat doesn't seem to work exactly the same!
    QSurfaceFormat::setDefaultFormat(format);

    // the actual device and context creation is now deferred to the point when
    // a QSurface is available so that it's possible to make it current.
    // otherwise the gfx::Device will not have a valid context for the calling
    // thread and obscure things will happen.
}

void DisposeGraphicsDevice()
{
    painter.reset();
    device.reset();
    context.reset();
}

} // namespace
