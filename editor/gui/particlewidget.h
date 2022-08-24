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
#  include "ui_particlewidget.h"
#  include <QStringList>
#  include <QString>
#include "warnpop.h"

#include <memory>
#include <vector>

#include "graphics/painter.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "editor/gui/mainwidget.h"

namespace app {
    class Resource;
    class Workspace;
} // app

namespace gui
{
    class MouseTool;

    class ParticleEditorWidget : public MainWidget
    {
        Q_OBJECT

    public:
        ParticleEditorWidget(app::Workspace* workspace);
        ParticleEditorWidget(app::Workspace* workspace, const app::Resource& resource);
       ~ParticleEditorWidget();

        virtual void Initialize(const UISettings& settings) override;
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
        virtual void Update(double secs) override;
        virtual void Render() override;
        virtual void Save() override;
        virtual bool HasUnsavedChanges() const override;
        virtual bool GetStats(Stats* stats) const override;
    private slots:
        void on_actionPlay_triggered();
        void on_actionStop_triggered();
        void on_actionPause_triggered();
        void on_actionSave_triggered();
        void on_resetTransform_clicked();
        void on_btnViewPlus90_clicked();
        void on_btnViewMinus90_clicked();
        void on_btnSelectMaterial_clicked();
        void on_motion_currentIndexChanged(int);
        void on_boundary_currentIndexChanged(int);
        void on_when_currentIndexChanged(int);
        void on_simWidth_valueChanged(double);
        void on_simHeight_valueChanged(double);
        void on_gravityX_valueChanged(double);
        void on_gravityY_valueChanged(double);
        void on_numParticles_valueChanged(double);
        void on_initX_valueChanged(double);
        void on_initY_valueChanged(double);
        void on_initWidth_valueChanged(double);
        void on_initHeight_valueChanged(double);
        void on_dirStartAngle_valueChanged();
        void on_dirSizeAngle_valueChanged();
        void on_minVelocity_valueChanged(double);
        void on_maxVelocity_valueChanged(double);
        void on_minLifetime_valueChanged(double);
        void on_maxLifetime_valueChanged(double);
        void on_minPointsize_valueChanged(int);
        void on_maxPointsize_valueChanged(int);
        void on_minAlpha_valueChanged(double);
        void on_maxAlpha_valueChanged(double);
        void on_timeSizeDerivative_valueChanged(double);
        void on_distSizeDerivative_valueChanged(double);
        void on_timeAlphaDerivative_valueChanged(double);
        void on_distAlphaDerivative_valueChanged(double);
        void on_canExpire_stateChanged(int);

        void NewResourceAvailable(const app::Resource* resource);
        void ResourceUpdated(const app::Resource* resource);
        void ResourceToBeDeleted(const app::Resource* resource);
    private:
        void SetParams();
        void ShowParams();
        void PaintScene(gfx::Painter& painter, double secs);
        void MouseMove(QMouseEvent* mickey);
        void MousePress(QMouseEvent* mickey);
        void MouseRelease(QMouseEvent* mickey);
    private:
        Ui::ParticleWidget mUI;
    private:
        class MoveEmitterTool;
        class SizeEmitterTool;
        struct UIState;
        app::Workspace* mWorkspace = nullptr;
        std::shared_ptr<gfx::KinematicsParticleEngineClass> mClass;
        std::unique_ptr<gfx::KinematicsParticleEngine> mEngine;
        std::unique_ptr<gfx::Material> mMaterial;
        std::unique_ptr<MouseTool> mMouseTool;
        std::unique_ptr<UIState> mState;
        bool mPaused = false;
        float mTime  = 0.0f;
    private:
        // the original hash value that is used to
        // check against if there are unsaved changes.
        std::size_t mOriginalHash = 0;
    };

} // namespace
