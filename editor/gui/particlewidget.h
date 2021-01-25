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
#  include "ui_particlewidget.h"
#  include <QStringList>
#  include <QString>
#include "warnpop.h"

#include <memory>
#include <vector>

#include "graphics/painter.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "mainwidget.h"

namespace app {
    class Resource;
    class Workspace;
} // app

namespace gui
{
    class ParticleEditorWidget : public MainWidget
    {
        Q_OBJECT
    public:
        ParticleEditorWidget(app::Workspace* workspace);
        ParticleEditorWidget(app::Workspace* workspace, const app::Resource& resource);
       ~ParticleEditorWidget();

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
        virtual bool ConfirmClose() override;
        virtual bool GetStats(Stats* stats) const override;
    private slots:
        void on_actionPlay_triggered();
        void on_actionStop_triggered();
        void on_actionPause_triggered();
        void on_actionSave_triggered();
        void on_resetTransform_clicked();
        void on_plus90_clicked();
        void on_minus90_clicked();
        void on_motion_currentIndexChanged(int);
        void PaintScene(gfx::Painter& painter, double secs);
        void NewResourceAvailable(const app::Resource* resource);
        void ResourceToBeDeleted(const app::Resource* resource);

    private:
        void FillParams(gfx::KinematicsParticleEngineClass::Params& params) const;

    private:
        Ui::ParticleWidget mUI;
    private:
        app::Workspace* mWorkspace = nullptr;
        gfx::KinematicsParticleEngineClass mClass;
        std::unique_ptr<gfx::KinematicsParticleEngine> mEngine;
        std::unique_ptr<gfx::Material> mMaterial;
        bool mPaused = false;
        float mTime  = 0.0f;
    private:
        // cache the name of the current material so that we can
        // compare and detect if the user has selected different material
        // from the material combo and then recreate the material instance.
        QString mMaterialName;
        // the original hash value that is used to
        // check against if there are unsaved changes.
        std::size_t mOriginalHash = 0;
    };

} // namespace
