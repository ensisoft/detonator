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
#  include "ui_dlgtilemap.h"
#  include <QDialog>
#  include <QString>
#  include <QPixmap>
#  include <QTimer>
#include "warnpop.h"

#include <memory>
#include <string>
#include <vector>
#include <optional>

#include "graphics/fwd.h"

namespace gui
{
    class DlgTilemap : public QDialog
    {
        Q_OBJECT

    public:
        DlgTilemap(QWidget* parent);
       ~DlgTilemap();

        void LoadImage(const QString& file);

        bool IsClosed() const
        { return mClosed; }

    private slots:
        void on_widgetColor_colorChanged(const QColor& color);
        void on_tileWidth_valueChanged(int);
        void on_tileHeight_valueChanged(int);
        void on_offsetX_valueChanged(int);
        void on_offsetY_valueChanged(int);
        void on_btnSelectImage_clicked();
        void on_btnExport_clicked();
        void on_btnClose_clicked();
        void finished();
        void timer();

    private:
        void SelectTile();
        void SplitIntoTiles();
        void OnPaintScene(gfx::Painter& painter, double secs);
        void OnMousePress(QMouseEvent* mickey);
        void OnMouseMove(QMouseEvent* mickey);
        void OnMouseRelease(QMouseEvent* mickey);
        bool OnKeyPress(QKeyEvent* event);

    private:
        Ui::DlgTilemap mUI;

    private:
        std::shared_ptr<gfx::TextureMap2DClass> mClass;
        std::unique_ptr<gfx::Material> mMaterial;

        QTimer mTimer;
        QPoint mStartPoint;
        QPoint mCurrentPoint;
        QPoint mTrackingOffset;
        unsigned mWidth  = 0;
        unsigned mHeight = 0;

        enum class Mode {
            Nada, Tracking, Selecting
        };
        Mode mMode = Mode::Nada;

        bool mClosed = false;
    };

} // namespace
