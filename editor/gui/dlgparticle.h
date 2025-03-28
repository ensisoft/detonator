// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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
#  include "ui_dlgparticle.h"
#  include <QElapsedTimer>
#  include <QTimer>
#  include <QDialog>
#include "warnpop.h"

#include <vector>
#include <memory>

#include "graphics/fwd.h"
#include "editor/app/types.h"
#include "editor/app/resource.h"
#include "editor/gui/types.h"

namespace app {
    class Workspace;
}

namespace gui
{
    class DlgParticle : public QDialog
    {
        Q_OBJECT

    public:
        DlgParticle(QWidget* parent, const app::Workspace* workspace);
        ~DlgParticle() override;

        auto GetParticleClass() const
        { return mParticleClass; }

        auto GetMaterialClass() const
        { return mMaterialClass; }

    private slots:
        void on_btnAccept_clicked();
        void on_btnCancel_clicked();
        void on_cmbParticle_currentIndexChanged(int);

    private:
        void PaintScene(gfx::Painter& painter, double secs);
        void MousePress(QMouseEvent* mickey);
        void MouseDoubleClick(QMouseEvent* mickey);
        void MouseWheel(QWheelEvent* wheel);
        bool KeyPress(QKeyEvent* key);
    private:
        Ui::DlgParticle mUI;
    private:
        const app::Workspace* mWorkspace = nullptr;
        std::shared_ptr<const gfx::MaterialClass> mMaterialClass;
        std::shared_ptr<const gfx::ParticleEngineClass> mParticleClass;
        std::unique_ptr<gfx::ParticleEngineInstance> mEngine;
        std::unique_ptr<gfx::Material> mMaterial;
        float mVizWidth = 100.0f;
        float mVizHeight = 100.0f;
        bool mEmitOnce = false;
    };
}
