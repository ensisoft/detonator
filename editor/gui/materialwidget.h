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
#  include <QMap>
#  include <QString>
#include "warnpop.h"

#include "editor/gui/mainwidget.h"
#include "graphics/types.h"
#include "graphics/material.h"

namespace gfx {
    class Material;
    class Painter;
} // gfx

namespace app {
    class Resource;
    class Workspace;
} // app

namespace gui
{
    class MaterialWidget : public MainWidget
    {
        Q_OBJECT
    public:
        MaterialWidget(app::Workspace* workspace);
        MaterialWidget(app::Workspace* workspace, const app::Resource& resource);
       ~MaterialWidget();
        virtual void AddActions(QToolBar& bar) override;
        virtual void AddActions(QMenu& menu) override;
        virtual bool SaveState(Settings& settings) const;
        virtual bool LoadState(const Settings& settings);
        virtual bool CanTakeAction(Actions action, const Clipboard* clipboard) const override;
        virtual void ZoomIn() override;
        virtual void ZoomOut() override;
        virtual void ReloadShaders() override;
        virtual void ReloadTextures() override;
        virtual void Shutdown() override;
        virtual void Update(double dt) override;
        virtual void Render() override;
        virtual void Save() override;
        virtual bool HasUnsavedChanges() const override;
        virtual bool ConfirmClose() override;
        virtual bool GetStats(Stats* stats) const override;
    private slots:
        void on_actionPlay_triggered();
        void on_actionPause_triggered();
        void on_actionStop_triggered();
        void on_actionSave_triggered();
        void on_btnAddTextureMap_clicked();
        void on_btnDelTextureMap_clicked();
        void on_btnEditTextureMap_clicked();
        void on_btnEditShader_clicked();
        void on_btnResetTextureRect_clicked();
        void on_btnSelectTextureRect_clicked();
        void on_browseShader_clicked();
        void on_textures_currentRowChanged(int row);
        void on_materialType_currentIndexChanged(const QString& text);
        void on_rectX_valueChanged(double value);
        void on_rectY_valueChanged(double value);
        void on_rectW_valueChanged(double value);
        void on_rectH_valueChanged(double value);
        void AddNewTextureMapFromFile();
        void AddNewTextureMapFromText();
        void AddNewTextureMapFromBitmap();

    private:
        void SetMaterialProperties() const;
        void PaintScene(gfx::Painter& painter, double sec);

    private:
        Ui::MaterialWidget mUI;
    private:
        // current workspace object
        app::Workspace* mWorkspace = nullptr;
        // the material class we're editing
        mutable gfx::MaterialClass mMaterial;
        // play state
        enum PlayState {
            Playing, Paused, Stopped
        };
        PlayState mState = PlayState::Stopped;
        // current playback time (if playing)
        float mTime = 0.0f;
        // the original material hash that is used to
        // check against if there are unsaved changes.
        std::size_t mOriginalHash = 0;

    };
} // namespace
