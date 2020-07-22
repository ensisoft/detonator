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
#  include <QWindow>
#  include <QOpenGLContext>
#  include <QKeyEvent>
#  include <QElapsedTimer>
#  include <QPalette>
#include "warnpop.h"

#include <memory>
#include <functional>

#include "base/assert.h"
#include "graphics/painter.h"
#include "graphics/device.h"
#include "graphics/color4f.h"

namespace gui
{
    // Integrate QOpenGLWidget and custom graphics device and painter
    // implementations from gfx into a reusable widget class.
    class GfxWindow : public QWindow
    {
        Q_OBJECT

    public:
        GfxWindow();
       ~GfxWindow();

        // Important to call dispose to cleanly dispose of all the graphics
        // resources while the Qt's OpenGL context is still valid,
        // I.e the window still exists and hasn't been closed or anything.
        void dispose();

        void reloadShaders()
        {
            // we simply just delete all program objects
            // which will trigger the rebuild of the needed
            // programs which will ultimately need to (re)load
            // and compile the shaders as well.
            mCustomGraphicsDevice->DeletePrograms();
            mCustomGraphicsDevice->DeleteShaders();
        }
        void reloadTextures()
        {
            mCustomGraphicsDevice->DeleteTextures();
        }

        gfx::Color4f getClearColor() const
        { return mClearColor; }

        void setClearColor(const gfx::Color4f& color)
        { mClearColor = color; }

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
        std::function<void (QWheelEvent* wheel)>  onMouseWheel;
        // keyboard callbacks
        std::function<bool (QKeyEvent* key)>      onKeyPress;


    public slots:
        void clearColorChanged(QColor color);
        void toggleShowFps();
    private slots:
        void doInit();

    private:
        void initializeGL();
        void paintGL();
        virtual void mouseMoveEvent(QMouseEvent* mickey) override;
        virtual void mousePressEvent(QMouseEvent* mickey) override;
        virtual void mouseReleaseEvent(QMouseEvent* mickey) override;
        virtual void keyPressEvent(QKeyEvent* key) override;
        virtual void wheelEvent(QWheelEvent* wheel) override;
        virtual bool event(QEvent* event) override;

    private:
        std::shared_ptr<gfx::Device> mCustomGraphicsDevice;
        std::unique_ptr<gfx::Painter> mCustomGraphicsPainter;
        gfx::Color4f mClearColor = {0.2f, 0.3f, 0.4f, 1.0f};
    private:
        QElapsedTimer mClock;
        bool mInitialized = false;
    private:
        quint64 mFrameTime = 0;
        quint64 mNumFrames = 0;
        bool  mShowFps     = false;
        float mCurrentFps  = 0.0f;
    private:
        QOpenGLContext mContext;
    };

    // This is now a "widget shim" that internally creates a QOpenGLWindow
    // and places it in a window container. This is done because of
    // using QOpenGLWindow provides slightly better performance
    // than QOpenGLWidget.
    class GfxWidget : public QWidget
    {
        Q_OBJECT

    public:
        GfxWidget(QWidget* parent);
       ~GfxWidget();

        void dispose();

        void setFramerate(unsigned target)
        {
            // no longer used.
            //mWindow->setFramerate(target);
        }
        void reloadShaders()
        {
            mWindow->reloadShaders();
        }
        void reloadTextures()
        {
            mWindow->reloadTextures();
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
        // keyboard callbacks.
        std::function<bool (QKeyEvent* key)>      onKeyPress;

        // zoom in/out callbacks
        std::function<void ()> onZoomIn;
        std::function<void ()> onZoomOut;
    private:
        void showColorDialog();
        void translateZoomInOut(QWheelEvent* event);

    private:
        virtual void resizeEvent(QResizeEvent* event) override;
    private:
        GfxWindow* mWindow = nullptr;
        QWidget* mContainer = nullptr;
    };

} // namespace

