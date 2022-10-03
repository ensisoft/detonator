// Copyright (C) 2020-2022 Sami Väisänen
// Copyright (C) 2020-2022 Ensisoft http://www.ensisoft.com
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
#  include "ui_dlgtileimport.h"
#  include "ui_importedtile.h"
#  include <QTimer>
#  include <QDialog>
#  include <QPoint>
#include "warnpop.h"

#include <memory>
#include <vector>
#include <set>

#include "graphics/fwd.h"


namespace app {
    class Workspace;
} // namespace

namespace gui
{
    class ImportedTile : public QWidget
    {
        Q_OBJECT

    public:
        ImportedTile(QWidget* parent);

        void SetPreview(QPixmap pix);

        QString GetName() const;
        void SetName(const QString& name);

    private:
        Ui::ImportedTile mUI;
    private:
    };

    class DlgTileImport : public QDialog
    {
        Q_OBJECT

    public:
        DlgTileImport(QWidget* parent, app::Workspace* workspace);
       ~DlgTileImport();

    private slots:
        void on_btnSelectFile_clicked();
        void on_btnSelectAll_clicked();
        void on_btnSelectNone_clicked();
        void on_btnClose_clicked();
        void on_btnImport_clicked();
        void on_tabWidget_currentChanged(int);
        void on_tileWidth_valueChanged(int);
        void on_tileHeight_valueChanged(int);
        void on_offsetX_valueChanged(int);
        void on_offsetY_valueChanged(int);
        void on_renameTiles_textChanged(const QString& name);
        void on_widgetColor_colorChanged(QColor color);
        void finished();
        void timer();
    private:
        void ToggleSelection();
        void SplitIntoTiles();
        void LoadFile(const QString& file);
        void LoadState();
        void SaveState();
        void OnPaintScene(gfx::Painter& painter, double secs);
        void OnMousePress(QMouseEvent* mickey);
        void OnMouseMove(QMouseEvent* mickey);
        void OnMouseRelease(QMouseEvent* mickey);
        bool OnKeyPress(QKeyEvent* event);
    private:
        Ui::DlgTileImport mUI;
    private:
        app::Workspace* mWorkspace = nullptr;
        std::shared_ptr<gfx::TextureMap2DClass> mClass;
        std::unique_ptr<gfx::Material> mMaterial;
        std::string mFileUri;
        std::string mFileName;
        QTimer mTimer;
        QPoint mStartPoint;
        QPoint mCurrentPoint;
        QPoint mTrackingOffset;
        unsigned mWidth  = 0;
        unsigned mHeight = 0;

        struct Tile {
            bool selected = false;
            ImportedTile* widget = nullptr;
        };
        std::vector<Tile> mTiles;
        enum class Mode {
            Nada, Tracking, Selecting
        };
        Mode mMode = Mode::Nada;
        std::set<unsigned> mTilesTouched;
    };

} // namespace