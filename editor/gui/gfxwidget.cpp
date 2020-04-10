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
#  include <color_dialog.hpp>
#include "warnpop.h"

#include "editor/app/eventlog.h"
#include "graphics/drawing.h"
#include "utility.h"
#include "gfxwidget.h"

namespace gui
{

void GfxWidget::initializeGL()
{
    // Context implementation based on widget/context provided by Qt.
    class WidgetContext : public gfx::Device::Context
    {
    public:
        WidgetContext(QOpenGLWidget* widget) : mWidget(widget)
        {}
        virtual void Display() override
        {
            // No need to implement this, Qt will take care of this
        }
        virtual void MakeCurrent() override
        {
            // No need to implement this, Qt will take care of this.
        }
        virtual void* Resolve(const char* name) override
        {
            return (void*)mWidget->context()->getProcAddress(name);
        }
    private:
        QOpenGLWidget* mWidget = nullptr;
    };

    // create custom painter for fancier shader based effects.
    mCustomGraphicsDevice  = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2,
        std::make_shared<WidgetContext>(this));
    mCustomGraphicsPainter = gfx::Painter::Create(mCustomGraphicsDevice);
}

void GfxWidget::paintGL()
{
    const quint64 ms = mClock.restart();

    mCustomGraphicsDevice->BeginFrame();
    mCustomGraphicsDevice->ClearColor(mClearColor);
    if (onPaintScene)
        onPaintScene(*mCustomGraphicsPainter, ms/1000.0);

    if (mShowFps)
    {
        mNumFrames++;
        mFrameTime += ms;
        if (mFrameTime >= 1000)
        {
            // how many frames did get to display in the last period
            const auto secs = mFrameTime / 1000.0;
            mCurrentFps = mNumFrames / secs;
            mNumFrames  = 0;
            mFrameTime  = 0;
        }
        gfx::DrawTextRect(*mCustomGraphicsPainter,
            base::FormatString("%1 FPS", (unsigned)mCurrentFps),
            "fonts/ARCADE.TTF", 28,
            gfx::FRect(10, 20, 150, 100),
            gfx::Color::HotPink,
            gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignTop);
    }
    mCustomGraphicsDevice->EndFrame(true /*display*/);
}
void GfxWidget::timerEvent(QTimerEvent*)
{
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
    if (!mInitialized && onInitScene)
    {
        const auto w = width();
        const auto h = height();
        onInitScene(w, h);
        mInitialized = true;
    }
    repaint();
}

void GfxWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    QAction* color = menu.addAction("Background Color");
    connect(color, &QAction::triggered,
            this,  &GfxWidget::changeColor_triggered);
    QAction* fps = menu.addAction("Show Fps");
    fps->setCheckable(true);
    fps->setChecked(mShowFps);
    connect(fps, &QAction::triggered, this, &GfxWidget::toggleShowFps);
    menu.exec(QCursor::pos());
}

void GfxWidget::mouseMoveEvent(QMouseEvent* mickey)
{
    if (onMouseMove)
        onMouseMove(mickey);
}
void GfxWidget::mousePressEvent(QMouseEvent* mickey)
{
    if (onMousePress)
        onMousePress(mickey);
}
void GfxWidget::mouseReleaseEvent(QMouseEvent* mickey)
{
    if (onMouseRelease)
        onMouseRelease(mickey);
}

void GfxWidget::changeColor_triggered()
{
    const gfx::Color4f current = mClearColor;

    color_widgets::ColorDialog dlg(this);
    dlg.setColor(FromGfx(mClearColor));
    connect(&dlg, &color_widgets::ColorDialog::colorChanged,
            this, &GfxWidget::clearColorChanged);

    if (dlg.exec() == QDialog::Rejected)
    {
        mClearColor = current;
        return;
    }
    mClearColor = ToGfx(dlg.color());
}

void GfxWidget::toggleShowFps()
{
    mShowFps = !mShowFps;
    mFrameTime = 0;
}

void GfxWidget::clearColorChanged(QColor color)
{
    mClearColor = ToGfx(color);
}

} // namespace
