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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <QOpenGLWidget>
#  include <QOpenGLContext>
#  include <QBasicTimer>
#  include <QElapsedTimer>
#include "warnpop.h"

#include <memory>
#include <functional>

#include "graphics/painter.h"
#include "graphics/device.h"
#include "graphics/color4f.h"

namespace gui
{
    // Integrate QOpenGLWidget and custom graphics device and painter
    // implementations from gfx into a reusable widget class.
    class GfxWidget : public QOpenGLWidget
    {
        Q_OBJECT

    public:
        GfxWidget(QWidget* parent) : QOpenGLWidget(parent)
        {
            setFramerate(60);
            // need to enable mouse tracking in order to get mouse move events.
            setMouseTracking(true);

            // need to enable this in order to get keyboard events.
            setFocusPolicy(Qt::StrongFocus);
        }

        void dispose()
        {
            mCustomGraphicsDevice.reset();
            mCustomGraphicsPainter.reset();
        }
        void setFramerate(unsigned target)
        {
            const unsigned ms = 1000 / target;
            mTimer.start(ms, this);
        }

        void reloadShaders()
        {
            // we simply just delete all program objects
            // which will trigger the rebuild of the needed
            // programs which will ultimately need to (re)load
            // and compile the shaders as well.
            mCustomGraphicsDevice->DeletePrograms();
            mCustomGraphicsDevice->DeleteShaders();
        }

        // callback to invoke when paint must be done.
        // secs is the seconds elapsed since last paint.
        std::function<void (gfx::Painter&, double secs)> onPaintScene;

        // callback to invoke to initialize game, i.e. the OpenGL widget
        // has been initialized.
        // width and height are the width and height of the widget's viewport.
        std::function<void (unsigned width, unsigned height)> onInitScene;

        // mouse callbacks
        std::function<void (QMouseEvent* mickey)> onMouseMove;
        std::function<void (QMouseEvent* mickey)> onMousePress;
        std::function<void (QMouseEvent* mickey)> onMouseRelease;
        std::function<bool (QKeyEvent* key)> onKeyPress;
    private slots:
        void changeColor_triggered();
        void clearColorChanged(QColor color);
        void toggleShowFps();

    private:
        virtual void initializeGL() override;
        virtual void paintGL() override;
        virtual void timerEvent(QTimerEvent*) override;
        virtual void contextMenuEvent(QContextMenuEvent* event) override;
        virtual void mouseMoveEvent(QMouseEvent* mickey) override;
        virtual void mousePressEvent(QMouseEvent* mickey) override;
        virtual void mouseReleaseEvent(QMouseEvent* mickey) override;
        virtual void keyPressEvent(QKeyEvent* key) override;

    private:
        std::shared_ptr<gfx::Device> mCustomGraphicsDevice;
        std::unique_ptr<gfx::Painter> mCustomGraphicsPainter;
        gfx::Color4f mClearColor = {0.2f, 0.3f, 0.4f, 1.0f};
    private:
        QBasicTimer mTimer;
        QElapsedTimer mClock;
        bool mInitialized = false;
    private:
        quint64 mFrameTime = 0;
        quint64 mNumFrames = 0;
        bool mShowFps = false;
        float mCurrentFps = 0.0f;
    };

} // namespace

