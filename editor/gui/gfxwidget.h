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

#include "warnpush.h"
#  include <QWindow>
#  include <QOpenGLContext>
#  include <QKeyEvent>
#  include <QElapsedTimer>
#  include <QPalette>
#include "warnpop.h"

#include <memory>
#include <functional>
#include <optional>

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

        void triggerPaint()
        {
            paintGL();
        }

        bool hasInputFocus() const;

        const gfx::Color4f* getClearColor() const
        {
            if (!mClearColor.has_value())
                return nullptr;
            return &mClearColor.value();
        }
        gfx::Device& getDevice() const
        { return *mCustomGraphicsDevice; }
        gfx::Painter& getPainter() const
        { return *mCustomGraphicsPainter; }
        gfx::Device::ResourceStats getDeviceResourceStats() const
        {
            gfx::Device::ResourceStats stats;
            if (mCustomGraphicsDevice)
                mCustomGraphicsDevice->GetResourceStats(&stats);
            return stats;
        }

        void setClearColor(const gfx::Color4f& color)
        { mClearColor = color; }
        void resetClearColor()
        { mClearColor.reset(); }

        bool haveVSYNC() const;

        float getCurrentFPS() const
        { return mCurrentFps; }

        void CreateRenderingSurface(bool vsync);

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
        std::function<void (QMouseEvent* mickey)> onMouseDoubleClick;
        // keyboard callbacks
        std::function<bool (QKeyEvent* key)>      onKeyPress;

        static void SetDefaultFilter(gfx::Device::MinFilter filter)
        { DefaultMinFilter = filter; }
        static void SetDefaultFilter(gfx::Device::MagFilter filter)
        { DefaultMagFilter = filter; }
        static void SetDefaultClearColor(const gfx::Color4f& color)
        { ClearColor = color; }
        static gfx::Color4f& GetDefaultClearColor()
        { return ClearColor; }
        static void CleanGarbage();

        static void BeginFrame();
        static void EndFrame();

        static void SetVSYNC(bool on_off);
        static bool GetVSYNC();
    public slots:
        void clearColorChanged(QColor color);
    private slots:
        void doInit();

    private:
        void initializeGL();
        void paintGL();
        virtual void mouseMoveEvent(QMouseEvent* mickey) override;
        virtual void mousePressEvent(QMouseEvent* mickey) override;
        virtual void mouseReleaseEvent(QMouseEvent* mickey) override;
        virtual void mouseDoubleClickEvent(QMouseEvent* mickey) override;
        virtual void keyPressEvent(QKeyEvent* key) override;
        virtual void wheelEvent(QWheelEvent* wheel) override;
        virtual void focusInEvent(QFocusEvent* event) override;
        virtual void focusOutEvent(QFocusEvent* event) override;

    private:
        std::shared_ptr<gfx::Device> mCustomGraphicsDevice;
        std::unique_ptr<gfx::Painter> mCustomGraphicsPainter;
        std::optional<gfx::Color4f> mClearColor;
    private:
        QElapsedTimer mClock;
        bool mVsync       = false;
        bool mHasFocus    = false;
        bool mInitDone    = false;
    private:
        quint64 mNumFrames = 0;
        float mCurrentFps  = 0.0f;

    private:
        std::shared_ptr<QOpenGLContext> mContext;
    private:
        static gfx::Device::MinFilter DefaultMinFilter;
        static gfx::Device::MagFilter DefaultMagFilter;
        static gfx::Color4f ClearColor;

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

        bool hasInputFocus() const
        { return mWindow->hasInputFocus(); }
        bool haveVSYNC() const
        { return mWindow->haveVSYNC(); }
        float getCurrentFPS() const
        { return mWindow->getCurrentFPS(); }

        const gfx::Color4f* getClearColor() const
        { return mWindow->getClearColor(); }
        gfx::Device& getDevice() const
        { return mWindow->getDevice(); }
        gfx::Painter& getPainter() const
        { return mWindow->getPainter(); }
        gfx::Device::ResourceStats getDeviceResourceStats() const
        { return mWindow->getDeviceResourceStats(); }

        void setClearColor(const gfx::Color4f& color)
        { mWindow->setClearColor(color); }
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
        std::function<void (QMouseEvent* mickey)> onMouseDoubleClick;
        // keyboard callbacks. Returns true if the key press event
        // was consumed. This will stop further processing of the
        // keypress.
        std::function<bool (QKeyEvent* key)>      onKeyPress;

        // zoom in/out callbacks
        std::function<void ()> onZoomIn;
        std::function<void ()> onZoomOut;

    public slots:
        void dispose();
        void reloadShaders();
        void reloadTextures();
        void triggerPaint();

    private:
        void showColorDialog();
        void translateZoomInOut(QWheelEvent* event);
        void toggleVSync();

    private:
        virtual void resizeEvent(QResizeEvent* event) override;
        virtual void focusInEvent(QFocusEvent* focus) override;
        virtual void focusOutEvent(QFocusEvent* focus) override;
    private:
        GfxWindow* mWindow = nullptr;
        QWidget* mContainer = nullptr;
    };

} // namespace

