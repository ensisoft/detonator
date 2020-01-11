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
        }

        // callback to invoke when paint must be done.
        // secs is the seconds elapsed since last paint.
        std::function<void (gfx::Painter&, double secs)> onPaintScene;

        // callback to invoke to initialize scene, i.e. the OpenGL widget
        // has been initialized.
        // width and height are the width and height of the widget's viewport.
        std::function<void (unsigned width, unsigned height)> onInitScene;
    private:
        virtual void initializeGL() override
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
        virtual void paintGL() override
        {
            const quint64 ms = mClock.restart();

            mCustomGraphicsDevice->BeginFrame();
            mCustomGraphicsDevice->ClearColor(gfx::Color4f(0.2f, 0.3f, 0.4f, 1.0f));
            if (onPaintScene)
                onPaintScene(*mCustomGraphicsPainter, ms/1000.0);

            mCustomGraphicsDevice->EndFrame(true /*display*/);
        }
        virtual void timerEvent(QTimerEvent*) override
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
    private:
        std::shared_ptr<gfx::Device> mCustomGraphicsDevice;
        std::unique_ptr<gfx::Painter> mCustomGraphicsPainter;
    private:
        QBasicTimer mTimer;
        QElapsedTimer mClock;
        bool mInitialized = false;
        
    };

} // namespace

