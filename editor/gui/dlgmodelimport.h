// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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
#  include "ui_dlgmodelimport.h"
#  include <QElapsedTimer>
#  include <QTimer>
#  include <QDialog>
#  include <QString>
#  include <QColor>
#include "warnpop.h"

#include <unordered_map>

#include "editor/app/import.h"

namespace app {
    class Workspace;
} // app

namespace gui
{
    class DlgModelImport : public QDialog
    {
        Q_OBJECT

    public:
        DlgModelImport(QWidget* parent, app::Workspace* workspace);

        void LoadGeometry();
        void LoadState();
        void SaveState();

    private slots:
        void on_btnFile_clicked();
        void on_btnClose_clicked();
        void on_widgetColor_colorChanged(const QColor& color);
    private:
        void PaintScene(gfx::Painter& painter, double secs);
        void MousePress(QMouseEvent* mickey);
        void MouseDoubleClick(QMouseEvent* mickey);
        void MouseWheel(QWheelEvent* wheel);
        bool KeyPress(QKeyEvent* key);

        void LoadModel(const QString& file);

    private:
        Ui::DlgModelImport mUI;

        QTimer mTimer;

    private:
        app::Workspace* mWorkspace = nullptr;
        app::ModelImporter mImporter;

        struct DrawablePair {
            std::unique_ptr<gfx::PolygonMeshInstance> drawable;
            std::unique_ptr<gfx::MaterialInstance> material;
        };
        std::vector<DrawablePair> mDrawState;
    };

} // namespace