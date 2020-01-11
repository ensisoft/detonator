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
#  include "ui_materialwidget.h"
#include "warnpop.h"

#include <QMap>

#include "editor/gui/mainwidget.h"
#include "graphics/types.h"

namespace gfx {
    class Material;
    class Painter;
} // material

namespace gui
{
    class MaterialWidget : public MainWidget
    {
        Q_OBJECT
    public:
        MaterialWidget();
       ~MaterialWidget();
        virtual void addActions(QToolBar& bar) override;
        virtual void addActions(QMenu& menu) override;              
        virtual bool saveState(Settings& settings) const;
        virtual bool loadState(const Settings& settings);
        virtual void zoomIn() override;
        virtual void zoomOut() override;

    private slots:
        void on_actionPlay_triggered();
        void on_actionPause_triggered();      
        void on_actionStop_triggered();  
        void on_textureAdd_clicked();
        void on_textureDel_clicked();
        void on_textures_currentRowChanged(int row);
        void on_imgRect_clicked(bool checked);
        void on_rectX_valueChanged(double value);
        void on_rectY_valueChanged(double value);
        void on_rectW_valueChanged(double value);
        void on_rectH_valueChanged(double value);

    private:
        void paintScene(gfx::Painter& painter, double sec);
        QString getCurrentTextureFilename() const;
    private:
        Ui::MaterialWidget mUI;
    private:
        struct TextureRect {
            bool enabled = false;
            float x = 0.0f;
            float y = 0.0f;
            float w = 0.0f;
            float h = 0.0f;
        };
        QMap<QString, TextureRect> mTextureRects;
        enum PlayState {
            Playing, Paused, Stopped
        };
        PlayState mState = PlayState::Stopped;

        float mTime = 0.0f;        
    };
} // namespace
