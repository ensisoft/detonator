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

#define LOGTAG "gfx"

#include "config.h"

#include "warnpush.h"
#  include <QMenu>
#  include <QContextMenuEvent>
#  include <QMouseEvent>
#  include <QBasicTimer>
#  include <color_dialog.hpp>
#include "warnpop.h"

#include <thread>
#include <chrono>

#include "editor/app/eventlog.h"
#include "graphics/drawing.h"
#include "graphics/transform.h"
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
// when running on the laptop, within 5 mins the fan is at 100%
// and the laptop is burning hot and the battery is drained.
// This doesn't seem like such a great idea.
// However if all the rendering surfaces have VSYNC setting enabled
// then swapBuffers call will block on every window swap which means
// there are as many waits (per second) as there are windows. This
// means that every additional window will decrease the rendering rate.
// For example let's say the display runs @ 60 Hz, then,
// 2 windows -> two swaps -> 30fps
// 3 windows -> three swaps -> 20 fps
// 6 windows -> 6 swaps -> 10 fps.
//
// It would seem that the solution is to enable sync to VBLANK for
// only a *single* rendering surface. Thus making sure that we're doing
// only a single wait per render loop iteration  when swapping.
// This however has 2 problems:
// - Swapping on a non-exposed surface is undefined behaviour (at least
//   according to the debug output that Qt (libANGLE) produces on Windows.
//   Means that if the surface with setSwapInterval set is not the active tab
//   we're invoking undefined behaviour when swapping and if we're not swapping
//   then we're not syncing and end up in the busy loop again.
//
// - With multiple windows open this still somehow begins to feel sluggish
//   compared to running without VSYNC enabled.
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
    // a global flag for toggling vsync on/off.
    bool should_have_vsync = false;
    std::weak_ptr<QOpenGLContext> shared_context;
    std::weak_ptr<gfx::Device> shared_device;
    // current surfaces ugh.
    std::unordered_set<gui::GfxWindow*> surfaces;
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
    surfaces.erase(this);
    ASSERT(!mCustomGraphicsDevice);
    ASSERT(!mCustomGraphicsPainter);
    DEBUG("Destroy GfxWindow");
}

GfxWindow::GfxWindow()
{
    surfaces.insert(this);
    // There's the problem that it seems a bit tricky to get the
    // OpenGLWidget's size (framebuffer size) properly when
    // starting things up. When the widget is loaded  there are
    // multiple invocations of resizeGL where the first call(s)
    // don't report the final size that the widget will eventually
    // have. I suspect this happens because there is some layout
    // engine adjusting the widget sizes. However this is a problem
    // if we want to for example have a user modifiable viewport
    // default to the initial size of the widget.
    // Doing a little hack here and hoping that when the first timer
    // event fires we'd have the final size correctly.
    //
    // Second issue discovery. When loading the editor with multiple
    // widgets being restored from the start there's a weird issue that
    // if the device is created "too soon" even though supposedly we
    // have a surface and a valid context that is current with the surface
    // the device isn't created correctly. The calls to probe the device
    // in graphics/opengl_es2_device for number of texture units, color buffer
    // info, etc all provide junk values (0 for texture units.. really??)
    // Moving all context/device creation to take place after a hack delay.
    // yay! 
    QTimer::singleShot(10, this, &GfxWindow::doInit);
}

bool GfxWindow::haveVSYNC() const
{
    return should_have_vsync;
}

void GfxWindow::dispose()
{
    if (!mContext)
        return;

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

bool GfxWindow::hasInputFocus() const
{
    return mHasFocus;
}

void GfxWindow::initializeGL()
{

}

void GfxWindow::paintGL()
{
    if (!mContext)
        return;
    else if (!isExposed())
        return;

    ASSERT(mCustomGraphicsDevice);
    ASSERT(mCustomGraphicsPainter);

    mContext->makeCurrent(this);

    mCustomGraphicsDevice->BeginFrame();
    mCustomGraphicsDevice->ClearColor(mClearColor);
    mCustomGraphicsDevice->SetDefaultTextureFilter(DefaultMagFilter);
    mCustomGraphicsDevice->SetDefaultTextureFilter(DefaultMinFilter);
    if (onPaintScene)
    {
        mCustomGraphicsPainter->SetOrthographicView((float) width() , (float) height());
        mCustomGraphicsPainter->SetViewport(0, 0, width(), height());
        mCustomGraphicsPainter->SetSurfaceSize(width(), height());
        onPaintScene(*mCustomGraphicsPainter, 0.0);
    }

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
    if (mHasFocus)
    {
        static std::shared_ptr<gfx::ColorClass> material;
        if (!material)
        {
            material = std::make_shared<gfx::ColorClass>();
            material->SetBaseColor(mFocusColor);
        }
        gfx::Rectangle rect(gfx::Drawable::Style::Outline, 2.0f);
        gfx::Transform transform;
        transform.Resize(width(), height());
        mCustomGraphicsPainter->Draw(rect, transform, *material);
    }

    mContext->swapBuffers(this);

    mCustomGraphicsDevice->EndFrame(false /*display*/);
    // using a shared device means that this must be done now
    // centrally once per render iteration, not once per GfxWidget.
    //mCustomGraphicsDevice->CleanGarbage(60);
}

void GfxWindow::CreateRenderingSurface(bool vsync)
{
    // native resources must be recreated. see the comment up top
    destroy();

    // set swap interval value on the surface format.
    QSurfaceFormat fmt = format();
    fmt.setSwapInterval(vsync ? 1 : 0);

    setFormat(fmt);
    create();
    show();

    mVsync = vsync;
    DEBUG("Created rendering surface with VSYNC set to %1", vsync);
}

void GfxWindow::doInit()
{
    if (surfaces.empty() && should_have_vsync)
        CreateRenderingSurface(true);
    else CreateRenderingSurface(false);

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
    // the Qt documents don't say anything regarding QWindow::keyPressEvent
    // whether the base class implementation should be called or not.
    // However there seems to be some occasional hiccups with keyboard
    // shortcuts not working. I'm guessing the issue is the lack of
    // QWindow::keyPressEvent call.
    // The QWidget documentation however stresses how important it is
    // to call QWidget::keyPressEvent if the derived class implementation
    // doesn't act upon the key.
    if (!onKeyPress(key))
        QWindow::keyPressEvent(key);
}

void GfxWindow::wheelEvent(QWheelEvent* wheel)
{
    onMouseWheel(wheel);
}

void GfxWindow::focusInEvent(QFocusEvent* event)
{
    //DEBUG("GfxWindow gain focus");
    mHasFocus = true;
}
void GfxWindow::focusOutEvent(QFocusEvent* event)
{
    //DEBUG("GfxWindow lost focus.");
    mHasFocus = false;
}

void GfxWindow::mouseDoubleClickEvent(QMouseEvent* mickey)
{
    onMouseDoubleClick(mickey);
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

// static
void GfxWindow::BeginFrame()
{
    if (should_have_vsync)
    {
        bool have_vsync = false;
        for (auto* window : surfaces)
        {
            if (window->mVsync)
            {
                have_vsync = true;
                break;
            }
        }
        if (!have_vsync && !surfaces.empty())
        {
            (*surfaces.begin())->CreateRenderingSurface(true);
        }
    }
    else
    {
        for (auto* window : surfaces)
        {
            if (window->mVsync)
                window->CreateRenderingSurface(false);
        }
    }
}
// static
void GfxWindow::EndFrame()
{
    bool did_vsync = false;

    for (auto* window : surfaces)
    {
        if (!window->mContext)
            continue;
        else if (!window->isExposed())
            continue;

        // can't be done here because this will not work
        // with dialogs since those don't put the application
        // in the "accelerated" render loop. 
        //window->mContext->makeCurrent(window);
        //window->mContext->swapBuffers(window);
        if (window->mVsync)
            did_vsync = true;
    }
    if (should_have_vsync && did_vsync)
        return;

    // this is ugly but is there something else we could do?
    // ideally wait for some time *or* until there's pending user
    // input/message from the underlying OS.
    // Maybe a custom QAbstractEventDispatcher implementation could work.
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

GfxWidget::GfxWidget(QWidget* parent) : QWidget(parent)
{
    mWindow = new GfxWindow();

    // okay, so we've got a QWindow which means it doesn't get the normal
    // QWidget functionality out of the box. For example the Focus rectangle
    // doesn't simply just work.
    // It's possible that we draw that ourselves as a visual goodie but figuring out the
    // color is a little bit too difficult.. The focus rect is drawn by the style
    // engine and doesn't seem actually use any of the palette colors!
    // https://github.com/ensisoft/qt-palette-colors is a tool to display palette colors
    // for all combinations of color group and role.
    // Testing on Linux with kvantum as the theme the focus rect is blue but there's
    // no such blue in any of the palette color combinations.
    // going to use some hardcoded blue here for now.
    mWindow->setFocusColor(gfx::Color4f(0x14, 0x8c, 0xD2, 0xFF));

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
        if (key->key() == Qt::Key_F2)
            showColorDialog();
        else if (key->key() == Qt::Key_F3)
            toggleVSync();

        if (mWindow && onKeyPress)
            return onKeyPress(key);
        return false;
    };
    mWindow->onMouseWheel = [&](QWheelEvent* wheel) {
        translateZoomInOut(wheel);
        if (mWindow && onMouseWheel)
            onMouseWheel(wheel);
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

void GfxWidget::reloadShaders()
{
    mWindow->reloadShaders();
}
void GfxWidget::reloadTextures()
{
    mWindow->reloadTextures();
}
void GfxWidget::triggerPaint()
{
    mWindow->triggerPaint();
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
