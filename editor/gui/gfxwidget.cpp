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

#include "warnpush.h"
#  include <QMenu>
#  include <QContextMenuEvent>
#  include <QMouseEvent>
#  include <QBasicTimer>
#  include <color_dialog.hpp>
#include "warnpop.h"

#include "editor/app/eventlog.h"
#include "graphics/drawing.h"
#include "utility.h"
#include "gfxwidget.h"

// Sync to VBLANK and multiple OpenGL Contexts:
//
// When rendering to multiple windows there's a problem
// wrt how to manage the rendering rate. If VSYNC is turned
// off the main rendering loop (see main.cpp) will run as fast
// as possible and the application will likely be rendering
// more frames than the display can actually show. This leads
// to waste in terms of CPU and GPU processing. For example
// when running on the laptop within 5 mins the fan is at 100%
// the laptop is burning hot and the battery is drained. This
// is not a great idea.
// However if all the rendering surfaces have VSYNC setting enabled
// then swapBuffers call will block on every window swap which means
// there are as many waits (per second) as there are windows. This
// means that every additional window will decrease the rendering rate.
// For example let's say the display runs @ 60 Hz, then,
// 2 windows -> two swaps -> 30fps
// 3 windows -> three swaps -> 20 fps
// 6 windows -> 6 swaps -> 10 fps.
//
// Using timers or inserting waits anywhere in the rendering path will
// lead to very janky rendering, so these cannot be used.
//
// Ultimately it seems that the solution is to enable sync to VBLANK for
// only a *single* rendering surface. Thus making sure that we're doing
// only a single wait per second in the call to swap buffers. So what we
// try to do here is to have a flag to tell us when some surface has vsync
// enabled and if that surface is destroyed some other surface needs to
// enable this flag.
//
// Notes about Qt.
// setSwapInterval is a member of QSurfaceFormat. QSurface and QOpenGLContext
// both take QSurfaceFormat but for the swap interval setting only the data set
// in the QSurface matters.
// The implementation tries to set the swap interval on every call to the platform's
// (GLX, WGL, EGL) "make current" call. However in the QWindow implementation the
// QSurfaceFormat is only ever set to "requestedFormat" which is only used *before*
// the native platform window is ever created.  This means that a subsequent call
// to QWindow::setFormat will not change the swap interval setting as such but the
// native resources must be destroyed and then re-created. (doh)
//
// See: qwindow.cpp, qglxintegration.cpp
//
// Some old discussion about the problem.
// https://stackoverflow.com/questions/29617370/multiple-opengl-contexts-multiple-windows-multithreading-and-vsync
//
// see commit. e14c0325af44512740ad711d0c13fd276efc25b7

namespace {
    // a global flag to indicate when there's some surface with
    // VSYNC enabled.
    bool have_vsync = false;
    // a global flag for toggling vsync on/off.
    bool should_have_vsync = true;

    std::weak_ptr<QOpenGLContext> shared_context;
    std::weak_ptr<gfx::Device> shared_device;
}// namespace

namespace gui
{

// static
gfx::Device::MinFilter GfxWindow::DefaultMinFilter =
    gfx::Device::MinFilter::Nearest;
// static
gfx::Device::MagFilter GfxWindow::DefaultMagFilter =
    gfx::Device::MagFilter::Nearest;

GfxWindow::~GfxWindow()
{
    ASSERT(!mCustomGraphicsDevice);
    ASSERT(!mCustomGraphicsPainter);
    DEBUG("Destroy GfxWindow");
}

GfxWindow::GfxWindow()
{
    // if we need a vsynced rendering surface but don't
    // have any yet then this window should become one that is
    // synced.
    mVsync = should_have_vsync && !have_vsync;

    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setSwapInterval(mVsync ? 1 : 0);

    setSurfaceType(QWindow::OpenGLSurface);
    setFormat(format);

    have_vsync = should_have_vsync;

    auto context = shared_context.lock();
    if (!context)
    {
        context = std::make_shared<QOpenGLContext>();
        // the default configuration has been set in main
        context->create();
        shared_context = context;
    }
    mContext = context;
    mContext->makeCurrent(this);
    // There's the problem that it seems a bit tricky to get the
    // OpenGLWidget's size (framebuffer size) properly when
    // starting things up. When the widget is loaded  there are
    // multiple invokations of resizeGL where the first call(s)
    // don't report the final size that the widget will eventually
    // have. I suspect this happens because there is some layout
    // engine adjusting the widget sizes. However this is a problem
    // if we want to for example have a user modifyable viewport
    // default to the initial size of the widget.
    // Doing a little hack here and hoping that when the first timer
    // event fires we'd have the final size correctly.
    QTimer::singleShot(10, this, &GfxWindow::doInit);
}

void GfxWindow::dispose()
{
    // Make sure this window's context is current when releasing the graphics
    // resources. If we have multiple GfxWindows each with their own context
    // it's possible that each one might have resources with the same name
    // for example texture 0, however if the wrong context is current
    // then the device will end deleting resources that actually belong
    // to a different device! *OOPS*
    mContext->makeCurrent(this);

    if (mVsync)
    {
        // if this surface was vsynced then toggle the flag
        // in order to indicate that we have vsync no more and
        // some other surface must be recreated.
        DEBUG("Lost VSYNC GfxWindow.");
        have_vsync = false;
    }

    mCustomGraphicsDevice.reset();
    mCustomGraphicsPainter.reset();
    // release the underlying native resources.
    // The Qt documentation https://doc.qt.io/qt-5/qwindow.html#destroy
    // doesn't say whether this would be called automatically by the QWindow
    // implementation or not.
    // However there seems to be a problem that something is leaking some
    // resources. I've experienced an issue several times where the rendering
    // gets slower and slower until at some point X server crashes and the X11
    // session is killed. I'm assuming that this issue is caused by some
    // resource leakage that happens. Maybe adding this explicit call to
    // destroy will help with this.
    // The nvidia-settings application can be used to quickly inspect the
    // memory consumption.
    destroy();

    DEBUG("Released GfxWindow device and painter.");
}

void GfxWindow::initializeGL()
{
    class WindowContext : public gfx::Device::Context
    {
    public:
        WindowContext(QOpenGLContext* context) : mContext(context)
        {}
        virtual void Display() override
        {}
        virtual void MakeCurrent() override
        {}
        virtual void* Resolve(const char* name) override
        {
            return (void*)mContext->getProcAddress(name);
        }
    private:
        QOpenGLContext* mContext = nullptr;
    };

    mContext->makeCurrent(this);
    auto device = shared_device.lock();
    if (!device)
    {
        // create custom painter for fancier shader based effects.
        device = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2,
                            std::make_shared<WindowContext>(mContext.get()));
        shared_device = device;
    }
    mCustomGraphicsDevice  = device;
    mCustomGraphicsPainter = gfx::Painter::Create(mCustomGraphicsDevice);
}

void GfxWindow::paintGL()
{
    if (!mCustomGraphicsDevice)
        return;

    // Remove the check and condition for early exit and skipping of swap buffers.
    // If the code returns early when the window is not exposed then sync to vlank
    // is also skipped. (See comment up top about sync to vblank). Why this then
    // matters is that if all the gfx windows are obscured/not visible, i.e. none
    // are exposed then the editor's main loop runs unthrottle (since there's no
    // longer sync to vblank). It looks like the easiest way to counter this issue
    // is thus simply to omit the check for window exposure.
    //
    //if (!isExposed())
    //    return;

    if (should_have_vsync && !have_vsync && !mVsync)
    {
        recreateRenderingSurface(true);
        have_vsync = true;
    }
    else if (!should_have_vsync && have_vsync && mVsync)
    {
        recreateRenderingSurface(false);
        have_vsync = false;
    }

    mContext->makeCurrent(this);

    mCustomGraphicsDevice->BeginFrame();
    mCustomGraphicsDevice->ClearColor(mClearColor);
    mCustomGraphicsDevice->SetDefaultTextureFilter(DefaultMagFilter);
    mCustomGraphicsDevice->SetDefaultTextureFilter(DefaultMinFilter);
    if (onPaintScene)
    {
        mCustomGraphicsPainter->SetTopLeftView((float)width(), (float)height());
        mCustomGraphicsPainter->SetViewport(0, 0, width(), height());
        mCustomGraphicsPainter->SetSurfaceSize(width(), height());
        onPaintScene(*mCustomGraphicsPainter, 0.0);
    }

    if (mShowFps)
    {
        mNumFrames++;
        const auto elapsed = mClock.elapsed();
        if (elapsed >= 1000)
        {
            // how many frames did get to display in the last period
            const auto secs = elapsed / 1000.0;
            mCurrentFps = mNumFrames / secs;
            mNumFrames  = 0;
            mClock.restart();
        }
        gfx::DrawTextRect(*mCustomGraphicsPainter,
            base::FormatString("%1 FPS", (unsigned)mCurrentFps),
            "app://fonts/ARCADE.TTF", 28,
            gfx::FRect(10, 20, 150, 100),
            gfx::Color::HotPink,
            gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignTop);
    }
    mCustomGraphicsDevice->EndFrame(false /*display*/);
    //mCustomGraphicsDevice->CleanGarbage(60);
    mContext->swapBuffers(this);
}

void GfxWindow::recreateRenderingSurface(bool vsync)
{
    // set swap interval value on the surface format.
    QSurfaceFormat fmt = format();
    fmt.setSwapInterval(vsync ? 1 : 0);
    setFormat(fmt);
    // native resources must be recreated. see the comment up top
    destroy();
    create();
    show();

    mVsync = vsync;
}

void GfxWindow::doInit()
{
    initializeGL();

    const auto w = width();
    const auto h = height();
    onInitScene(w, h);

    mClock.start();
 }

void GfxWindow::mouseMoveEvent(QMouseEvent* mickey)
{
    onMouseMove(mickey);
}
void GfxWindow::mousePressEvent(QMouseEvent* mickey)
{
    onMousePress(mickey);
}
void GfxWindow::mouseReleaseEvent(QMouseEvent* mickey)
{
    onMouseRelease(mickey);
}

void GfxWindow::keyPressEvent(QKeyEvent* key)
{
    onKeyPress(key);
}

void GfxWindow::wheelEvent(QWheelEvent* wheel)
{
    onMouseWheel(wheel);
}

void GfxWindow::mouseDoubleClickEvent(QMouseEvent* mickey)
{
    onMouseDoubleClick(mickey);
}

void GfxWindow::toggleShowFps()
{
    mShowFps = !mShowFps;
    mNumFrames = 0;
}

void GfxWindow::clearColorChanged(QColor color)
{
    mClearColor = ToGfx(color);
}

// static
void GfxWindow::CleanGarbage()
{
    auto device = shared_device.lock();
    if (!device)
        return;
    device->CleanGarbage(120);
}

GfxWidget::GfxWidget(QWidget* parent) : QWidget(parent)
{
    mWindow = new GfxWindow();
    // the container takes ownership of the window.
    mContainer = QWidget::createWindowContainer(mWindow, this);
    mWindow->onPaintScene = [&] (gfx::Painter& painter, double secs){
        if (mWindow && onPaintScene)
            onPaintScene(painter, secs);
    };
    mWindow->onInitScene = [&](unsigned width, unsigned height) {
        if (mWindow && onInitScene)
            onInitScene(width, height);
    };
    mWindow->onMouseMove = [&](QMouseEvent* mickey) {
        if (mWindow && onMouseMove)
            onMouseMove(mickey);
    };
    mWindow->onMousePress = [&](QMouseEvent* mickey) {
        if (mWindow && onMousePress)
            onMousePress(mickey);
    };
    mWindow->onMouseRelease = [&](QMouseEvent* mickey) {
        if (mWindow && onMouseRelease)
            onMouseRelease(mickey);
    };
    mWindow->onKeyPress = [&](QKeyEvent* key) {
        // The context menu can no longer be used since QWindow
        // doesn't support it. Also we cannot use the context
        // menu with the container widget either because this
        // window then renders on top obscuring the context menu (?)
        // TODO: Ultimately some better UI means are needed for these
        // options.
        if (key->key() == Qt::Key_F1)
            mWindow->toggleShowFps();
        else if (key->key() == Qt::Key_F2)
            showColorDialog();
        else if (key->key() == Qt::Key_F3)
            toggleVSync();

        if (mWindow && onKeyPress)
            return onKeyPress(key);
        return false;
    };
    mWindow->onMouseWheel = [&](QWheelEvent* wheel) {
        translateZoomInOut(wheel);
    };
    mWindow->onMouseDoubleClick = [&](QMouseEvent* mickey) {
        if (mWindow && onMouseDoubleClick)
            onMouseDoubleClick(mickey);
    };
}

GfxWidget::~GfxWidget()
{
    DEBUG("Destroy GfxWidget");
}

void GfxWidget::dispose()
{
    // dispose of the graphics resources.
    mWindow->dispose();
    DEBUG("Disposed GfxWindow.");
}

void GfxWidget::resizeEvent(QResizeEvent* resize)
{
    // resize the container to be same size as this shim
    // widget.
    // the container has taken ownership of the window and
    // will then in turn resize the window.
    mContainer->resize(resize->size());
}

void GfxWidget::showColorDialog()
{
    const gfx::Color4f current = mWindow->getClearColor();

    color_widgets::ColorDialog dlg(this);
    dlg.setColor(FromGfx(current));
    connect(&dlg, &color_widgets::ColorDialog::colorChanged,
            mWindow, &GfxWindow::clearColorChanged);

    if (dlg.exec() == QDialog::Rejected)
    {
        mWindow->setClearColor(current);
        return;
    }
    mWindow->setClearColor(ToGfx(dlg.color()));
}

void GfxWidget::translateZoomInOut(QWheelEvent* wheel)
{
    const auto mods = wheel->modifiers();
    if (mods != Qt::ControlModifier)
        return;

    const QPoint& num_degrees = wheel->angleDelta() / 8;
    const QPoint& num_steps = num_degrees / 15;
    // only consider the wheel scroll steps on the vertical
    // axis for zooming.
    // if steps are positive the wheel is scrolled away from the user
    // and if steps are negative the wheel is scrolled towards the user.
    const int num_zoom_steps = num_steps.y();

    //DEBUG("Zoom steps: %1", num_zoom_steps);

    for (int i=0; i<std::abs(num_zoom_steps); ++i)
    {
        if (num_zoom_steps > 0 && onZoomIn)
            onZoomIn();
        else if (num_zoom_steps < 0 && onZoomOut)
            onZoomOut();
    }
}

void GfxWidget::toggleVSync()
{
    should_have_vsync = !should_have_vsync;
    DEBUG("VSYNC set to %1", should_have_vsync);
}

} // namespace
