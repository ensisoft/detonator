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

#include "device/device.h"
#include "graphics/drawing.h"
#include "graphics/transform.h"
#include "graphics/utility.h"
#include "graphics/simple_shape.h"
#include "graphics/material_class.h"
#include "graphics/material_instance.h"
#include "editor/app/resource-uri.h"
#include "editor/app/eventlog.h"
#include "editor/gui/utility.h"
#include "editor/gui/gfxwidget.h"

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
    std::weak_ptr<gfx::Device> shared_gfx_device;
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
// static
gfx::Color4f GfxWindow::ClearColor = {0.2f, 0.3f, 0.4f, 1.0f};

// static
GfxWindow::MouseCursor GfxWindow::WindowMouseCursor = GfxWindow::MouseCursor::Native;

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
    // engine adjusting the widget sizes. However, this is a problem
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
    // info, etc. all provide junk values (0 for texture units.. really??)
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
    mCustomGraphicsDevice.reset();
    mCustomGraphicsPainter.reset();
    // release the underlying native resources.
    // The Qt documentation https://doc.qt.io/qt-5/qwindow.html#destroy
    // doesn't say whether this would be called automatically by the QWindow
    // implementation or not.
    // However, there seems to be a problem that something is leaking some
    // resources. I've experienced an issue several times when the rendering
    // gets slower and slower until at some point X server crashes and the X11
    // session is killed. I'm assuming that this issue is caused by some
    // resource leakage that happens. Maybe adding this explicit call to
    // destroy will help with this.
    // The nvidia-settings application can be used to quickly inspect the
    // memory consumption.
    destroy();

    DEBUG("Released GfxWindow device and painter.");
    if (mVsync)
    {
        DEBUG("Lost VSYNC GfxWindow.");
    }
}

bool GfxWindow::hasInputFocus() const
{
    return mHasFocus;
}

gfx::Color4f GfxWindow::GetCurrentClearColor()
{
    return mClearColor.value_or(GfxWindow::ClearColor);
}

QImage GfxWindow::TakeScreenshot() const
{
    QImage ret;
    if (!isExposed())
        return ret;

    const auto width  = this->width();
    const auto height = this->height();

    const QSurface* surface = this;
    
    mContext->makeCurrent(const_cast<QSurface*>(surface));

    const auto& bmp = mCustomGraphicsDevice->ReadColorBuffer(width, height);
    if (!bmp.IsValid())
        return ret;

    const auto bitmap_width  = bmp.GetWidth();
    const auto bitmap_height = bmp.GetHeight();
    // QImage doesn't own the data, the data ptr must be valid for the lifetime
    // of the QImage too.
    ret = QImage((const uchar*)bmp.GetDataPtr(), bitmap_width, bitmap_height, bitmap_width * 4, QImage::Format_RGBA8888);
    ret = ret.copy(); // make a copy here
    return ret;
}

void GfxWindow::initializeGL()
{

}

void GfxWindow::paintGL()
{
    if (!mInitDone || !mContext || !isExposed())
    {
        // avoid taking large jumps if the window was previous
        // not exposed and then becomes exposed again.
        mTimeStamp = 0.0;
        return;
    }

    if (mTimeStamp == 0.0)
        mTimeStamp = base::GetTime();

    const auto now = base::GetTime();
    const auto dt = now - mTimeStamp;
    mTimeStamp = now;

    ASSERT(mCustomGraphicsDevice);
    ASSERT(mCustomGraphicsPainter);

    mContext->makeCurrent(this);

    mCustomGraphicsDevice->BeginFrame();
    mCustomGraphicsDevice->ClearColor(mClearColor.value_or(GfxWindow::ClearColor));
    mCustomGraphicsDevice->ClearDepth(1.0f);
    mCustomGraphicsDevice->SetDefaultTextureFilter(DefaultMagFilter);
    mCustomGraphicsDevice->SetDefaultTextureFilter(DefaultMinFilter);
    if (onPaintScene)
    {
        const auto surface_width  = (float)width();
        const auto surface_height = (float)height();

        // set to defaults, the paint can then change these if needed.
        mCustomGraphicsPainter->SetProjectionMatrix(gfx::MakeOrthographicProjection(surface_width, surface_height));
        mCustomGraphicsPainter->SetViewport(0, 0, surface_width, surface_height);
        mCustomGraphicsPainter->SetSurfaceSize(surface_width, surface_height);
        mCustomGraphicsPainter->ResetViewMatrix();
        onPaintScene(*mCustomGraphicsPainter, dt);

        // reset these for subsequent drawing (below) since the widget's paint function
        // might have changed these unexpectedly.
        mCustomGraphicsPainter->SetProjectionMatrix(gfx::MakeOrthographicProjection(surface_width, surface_height));
        mCustomGraphicsPainter->SetViewport(0, 0, surface_width, surface_height);
        mCustomGraphicsPainter->SetSurfaceSize(surface_width, surface_height);
        mCustomGraphicsPainter->ResetViewMatrix();
    }

    mTimeAccum += dt;

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
            material = std::make_shared<gfx::ColorClass>(gfx::MaterialClass::Type::Color);
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
            //material->SetBaseColor(gfx::Color4f(0x14, 0x8c, 0xD2, 0xFF));
        }
        const auto& palette = QApplication::palette();
        material->SetBaseColor(ToGfx(palette.color(QPalette::Highlight)));

        gfx::Rectangle rect(gfx::SimpleShapeStyle::Outline);
        gfx::Transform transform;
        transform.Resize(width()-2.0f, height()-2.0f);
        transform.Translate(1.0f, 1.0f);
        mCustomGraphicsPainter->Draw(rect, transform, gfx::MaterialInstance(material), 2.0f);
    }

    if (WindowMouseCursor == MouseCursor::Custom)
    {
        static std::shared_ptr<gfx::ColorClass> arrow_cursor_material;
        if (!arrow_cursor_material)
        {
            arrow_cursor_material = std::make_shared<gfx::MaterialClass>(gfx::MaterialClass::Type::Color);
            arrow_cursor_material->SetBaseColor(gfx::Color::Silver);
        }
        static std::shared_ptr<gfx::TextureMap2DClass> crosshair_cursor_material;
        if (!crosshair_cursor_material)
        {
            crosshair_cursor_material = std::make_shared<gfx::MaterialClass>(gfx::CreateMaterialClassFromImage(res::CrosshairCursor));
            crosshair_cursor_material->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
            crosshair_cursor_material->SetBaseColor(gfx::Color::HotPink);
        }


        const auto& mickey = mapFromGlobal(QCursor::pos());
        const auto width = this->width();
        const auto height = this->height();
        if (mickey.x() >= 0 && mickey.x() <= width && mickey.y() >= 0 && mickey.y() < height)
        {
            gfx::Transform transform;
            transform.Resize(20.0f, 20.0f);
            transform.MoveTo((float)mickey.x(), (float)mickey.y());
            if (mCursorShape == CursorShape::ArrowCursor)
            {
                mCustomGraphicsPainter->Draw(gfx::ArrowCursor(), transform,
                                             gfx::MaterialInstance(arrow_cursor_material));
            }
            else if (mCursorShape == CursorShape::CrossHair)
            {
                transform.Resize(40.0f, 40.0f);
                transform.Translate(-20.0f, -20.0f);
                mCustomGraphicsPainter->Draw(gfx::BlockCursor(), transform,
                                             gfx::MaterialInstance(crosshair_cursor_material));
            }
        }
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
    DEBUG("Created rendering surface. [VSYNC=%1]", vsync);
}

void GfxWindow::SetCursorShape(CursorShape shape)
{
    if (WindowMouseCursor == MouseCursor::Native)
    {
        if (shape == CursorShape::ArrowCursor)
            setCursor(Qt::ArrowCursor);
        else if (shape == CursorShape::CrossHair)
            setCursor(Qt::CrossCursor);
        else BUG("Bug on mouse cursor");
    }
    else
    {
        setCursor(Qt::BlankCursor);
    }
    mCursorShape = shape;
}

void GfxWindow::doInit()
{
    if (WindowMouseCursor == MouseCursor::Native)
    {
        // WAR on Qt!
        // There's weird issue that sometimes the cursor changes to IBeam.
        // Somehow I feel that this bug comes from the QWindow + what's underneath + X11.
        // Trying to work around here by explicitly setting the cursor an Arrow.
        setCursor(Qt::ArrowCursor);
    }
    else
    {
        setCursor(Qt::BlankCursor);
    }

    CreateRenderingSurface(false);

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

    class WindowContext : public dev::Context
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
        virtual Version GetVersion() const override
        {
            return Version::OpenGL_ES3;
        }
    private:
        QOpenGLContext* mContext = nullptr;
    };

    auto gfx_device = shared_gfx_device.lock();
    if (!gfx_device)
    {
        // create custom painter for fancier shader based effects.
        auto dev = dev::CreateDevice(std::make_shared<WindowContext>(mContext.get()));
        gfx_device = gfx::CreateDevice(dev->GetSharedGraphicsDevice());
        shared_gfx_device = gfx_device;
    }
    mCustomGraphicsDevice  = gfx_device;
    mCustomGraphicsPainter = gfx::Painter::Create(mCustomGraphicsDevice);
    // this magic flag here turns all statics into "non-statics"
    // and lets resources that we create with static flags to
    // re-inspect their content for modification and then possibly
    // re-upload/regenerate the required GPU objects.
    mCustomGraphicsPainter->SetEditingMode(true);

    const auto w = width();
    const auto h = height();
    onInitScene(w, h);

    mClock.start();

    // set flag to indicate that we're finally done initializing.
    // this is done because the init actually happens on timer some
    // time after the widget has actually been created.
    mInitDone = true;
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
    // However, there seems to be some occasional hiccups with keyboard
    // shortcuts not working. I'm guessing the issue is the lack of
    // QWindow::keyPressEvent call.
    // The QWidget documentation however stresses how important it is
    // to call QWidget::keyPressEvent if the derived class implementation
    // doesn't act upon the key.
    if (!onKeyPress(key))
        QWindow::keyPressEvent(key);
}

void GfxWindow::keyReleaseEvent(QKeyEvent* key)
{
    if (onKeyRelease(key))
        QWindow::keyReleaseEvent(key);
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
    auto device = shared_gfx_device.lock();
    if (!device)
        return;
    device->CleanGarbage(120, gfx::Device::GCFlags::Textures |
                              gfx::Device::GCFlags::Programs |
                              gfx::Device::GCFlags::Geometries |
                              gfx::Device::GCFlags::FBOs);
}

// static
void GfxWindow::DeleteTexture(const std::string& gpuId)
{
    auto device = shared_gfx_device.lock();
    if (!device)
        return;

    device->DeleteTexture(gpuId);
}

// static
void GfxWindow::BeginFrame()
{
    if (should_have_vsync)
    {
        // look for any vsync windows.
        bool have_vsync = false;
        for (auto* window : surfaces)
        {
            if (!window->mInitDone)
                continue;

            if (window->mVsync && window->isExposed())
            {
                have_vsync = true;
                break;
            }
        }
        // got on, ok done!
        if (have_vsync)
            return;

        for (auto* window : surfaces)
        {
            if (!window->mInitDone)
                continue;

            if (window->mVsync)
                window->CreateRenderingSurface(false);
        }

        // find first window that has been initialized
        // and re-create the rendering surface with the
        // vsync flag.
        for (auto* window : surfaces)
        {
            if (!window->mInitDone)
                continue;

            if (window->isExposed())
            {
                window->CreateRenderingSurface(true);
                break;
            }
        }
    }
    else
    {
        // recreate any vsynced windows.
        for (auto* window : surfaces)
        {
            if (window->mVsync && window->mInitDone && window->isExposed())
                window->CreateRenderingSurface(false);
        }
    }
}
// static
bool GfxWindow::EndFrame()
{
    bool did_vsync = false;

    for (auto* window : surfaces)
    {
        if (!window->mInitDone)
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
    return did_vsync;
}
// static
void GfxWindow::SetVSYNC(bool on_off)
{
    should_have_vsync = on_off;
    DEBUG("Set GfxWindow VSYNC to: %1", on_off);
}
// static
bool GfxWindow::GetVSYNC()
{
    return should_have_vsync;
}

void GfxWindow::SetMouseCursor(MouseCursor cursor)
{
    WindowMouseCursor = cursor;
    for (auto* window : surfaces)
    {
        if (cursor == MouseCursor::Native)
        {
            const auto shape = window->GetCursorShape();
            if (shape == CursorShape::ArrowCursor)
                window->setCursor(Qt::ArrowCursor);
            else if (shape == CursorShape::CrossHair)
                window->setCursor(Qt::CrossCursor);
            else BUG("But on mouse cursor");
        }
        else window->setCursor(Qt::BlankCursor);
    }
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

        // let the GfxWidget host take a first pass at handling the
        // key presses.
        if (mWindow && onKeyPress)
        {
            if (onKeyPress(key))
                return true;
        }

        // The context menu can no longer be used since QWindow
        // doesn't support it. Also we cannot use the context
        // menu with the container widget either because this
        // window then renders on top obscuring the context menu (?)
        // TODO: Ultimately some better UI means are needed for these
        // options.
        if (key->modifiers() == Qt::ShiftModifier && key->key() == Qt::Key_F2)
            ShowColorDialog();
        else if (key->modifiers() == Qt::ShiftModifier && key->key() == Qt::Key_F3)
            mWindow->ResetClearColor();
        else if (key->key() == Qt::Key_Tab)
            FocusNextPrev(WidgetFocus::FocusNextWidget);
        else if (key->key() == Qt::Key_Backtab)
            FocusNextPrev(WidgetFocus::FocusPrevWidget);
        else return false;

        return true;
    };
    mWindow->onKeyRelease = [&](QKeyEvent* key) {
        if (mWindow && onKeyRelease)
            return onKeyRelease(key);
        return false;
    };
    mWindow->onMouseWheel = [&](QWheelEvent* wheel) {
        TranslateZoomInOut(wheel);
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

void GfxWidget::focusInEvent(QFocusEvent* focus)
{
    //DEBUG("GfxWidget got focus");

    // try to delegate the focus to the embedded QWindow since this
    // widget is just a shim.
    mWindow->requestActivate();
    // And try the same with a timer as a fallback for the case when the
    // widget has just been created and the call to requestActivate fails
    QTimer::singleShot(100, mWindow, &QWindow::requestActivate);
    // there's still the problem that the QWindow will not activate
    // when swapping between applications. For example when the GfxWindow
    // isActive swapping to another application and then back to the editor
    // will not activate the previous GfxWindow but the mainwindow window
    // which will then activate some widgets in the main window.
    // I don't know how to solve this problem reliably yet.
}

void GfxWidget::focusOutEvent(QFocusEvent* focus)
{
    //DEBUG("GfxWidget lost focus");
}

void GfxWidget::SetClearColor(const QColor& color)
{
    mWindow->SetClearColor(ToGfx(color));
}

void GfxWidget::ShowColorDialog()
{
    gfx::Color4f clear_color;
    bool own_color = false;
    if (const auto* color = mWindow->GetClearColor())
    {
        clear_color = *color;
        own_color   = true;
    }

    color_widgets::ColorDialog dlg(this);
    dlg.setColor(FromGfx(own_color ? clear_color : GfxWindow::GetDefaultClearColor()));
    connect(&dlg, &color_widgets::ColorDialog::colorChanged, mWindow, &GfxWindow::clearColorChanged);

    if (dlg.exec() == QDialog::Rejected)
    {
        if (own_color)
            mWindow->SetClearColor(clear_color);
        else mWindow->ResetClearColor();
        return;
    }
    mWindow->SetClearColor(ToGfx(dlg.color()));
}

void GfxWidget::SetCursorShape(CursorShape shape)
{
    mWindow->SetCursorShape(shape);
}

void GfxWidget::StartPaintTimer()
{
    connect(&mTimer, &QTimer::timeout, this, &GfxWidget::triggerPaint);
    mTimer.setInterval(1000.0 / 60.0);
    mTimer.start();
}

void GfxWidget::TranslateZoomInOut(QWheelEvent* wheel)
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

void GfxWidget::ToggleVSync()
{
    should_have_vsync = !should_have_vsync;
    DEBUG("VSYNC set to %1", should_have_vsync);
}

void GfxWidget::FocusNextPrev(WidgetFocus which)
{
    // Trying to play nice-nice with the focus movement. The goal is to move the
    // focus to next/prev widget as one might expect based on the tab list order set
    // in the designer. Seems simple enough, right.. but in reality this is really
    // f***n confusing. There seem to be so many brain farts here...
    //
    // Summary some of the weirdness here:
    //
    // - focusNextChild, focusPrevChild are non-virtual protected members of QWidget.
    //   This means that we can't call those on the *parent* (or any other widget for
    //   that matter). We can call them on *this* widget but the results are junk,
    //   random focus movement. BOO!
    //
    // - focusNextPrevChild seems to be a function for moving the focus onto a child
    //   inside a "container" widget. Maybe relevant, not sure really.
    //   The documentation seems to have some brain farts.
    //
    //   <snip>
    //     Sometimes, you will want to reimplement this function. For example,
    //     a web browser might reimplement it to move its "current active link"
    //     forward or backward, and call focusNextPrevChild() only when it reaches
    //     the last or first link on the "page".
    //   </snip>
    //
    // Ok, seems sensible enough, but WTF!? is this then:
    //
    //   <snip>
    //     Child widgets call focusNextPrevChild() on their parent widgets, but only
    //     the window that contains the child widgets decides where to redirect focus.
    //     By reimplementing this function for an object, you thus gain control of focus
    //     traversal for all child widgets.
    //   </snip>
    //
    // How does the child widget call this on their parent when the function is a
    // protected function?? And why would the child call this? It would seem to make
    // more sense if the parent called this on the child????
    //
    // So none of the above seem to make any sense for setting the focus. Let's focus
    // (pun intended) on nextInFocusChain and previousInFocusChain. Maybe these would
    // produce a tablist similar to what the designer produces (see any .ui file source)
    // But Alas, the results are unpredictable since the focus chain is full of widgets
    // that don't even take focus such as labels and also there's a bunch of crap such
    // as qt_spinbox_lineedit etc which seem some sort of Qt internal widgets.
    // Also the "chain" seems to be a looping data structure. Hey maybe it's a looping
    // linked list and there's finally a use case for the "find whether this linked list
    // contains a loop" CS trivia algo!

    {
        // these are junk.
        //auto* next = nextInFocusChain();
        //auto* prev = previousInFocusChain();
        //DEBUG("GfxWidget '%1' focus chain next = '%2' prev = '%3'.", objectName(),
        //      next ? next->objectName() : "null",
        //      prev ? prev->objectName() : "null");

        // build a list of widgets in the focus list. note that this will also list
        // widgets such as qt_spinbox_lineedit, ScrollLeftButton, ScrollRightButton
        // which seem to be Qt's internal widgets.
        //
        // watch out of for the infinite loop! \o/
        //
        std::vector<QWidget*> widgets;
        QWidget* iterator = this;
        do
        {
            // filter out obvious stuff that should not focus..
            if (iterator->focusPolicy() == Qt::NoFocus)
                continue;

            //DEBUG("Next focus chain widget = '%1'", iterator ? iterator->objectName() : "null");
            widgets.push_back(iterator);
        }
        while ((iterator = iterator->nextInFocusChain()) && iterator != this);

        size_t this_index = 0;
        for (this_index=0; this_index<widgets.size(); ++this_index) {
            if (widgets[this_index] == this)
                break;
        }
        if (this_index == widgets.size())
            return;

        if (which == WidgetFocus::FocusNextWidget)
        {
            // try to set the focus to previous widgets in the focus list until
            // a widget finally takes the focus.
            auto next_index = this_index + 1;
            for (size_t i=0; i<widgets.size(); ++i)
            {
                if (next_index >= widgets.size())
                    next_index = 0;

                widgets[next_index]->setFocus();
                if (widgets[next_index]->hasFocus())
                    break;
                ++next_index;
            }
            //DEBUG("Next widget is '%1'", widgets[next_index]->objectName());
        }
        else if (which == WidgetFocus::FocusPrevWidget)
        {
            // try to set the focus to previous widgets in the focus list until
            // a widget finally takes the focus.
            auto prev_index = this_index - 1;
            for (size_t i=0; i<widgets.size(); ++i)
            {
                if (prev_index >= widgets.size())
                    prev_index = widgets.size() -1;

                widgets[prev_index]->setFocus();
                if (widgets[prev_index]->hasFocus())
                    break;
                --prev_index;
            }
            //DEBUG("Prev widget is '%1'", widgets[prev_index]->objectName());
        }

        // this is garbage and won't work as expected.
        /*
        if (which == WidgetFocus::FocusNextWidget && next)
            next->setFocus();
        else if (which == WidgetFocus::FocusPrevWidget && prev)
            prev->setFocus();
            */
    }
}

} // namespace
