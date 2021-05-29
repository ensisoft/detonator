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
