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
#  include "ui_dlgimgview.h"
#  include <QDialog>
#  include <QString>
#  include <QPixmap>
#  include <QTimer>
#include "warnpop.h"

#include <memory>
#include <optional>

#include "graphics/fwd.h"

namespace gui
{
    class DlgImgView : public QDialog
    {
        Q_OBJECT

    public:
        struct Image {
            QString name;
            QString character;
            unsigned xpos   = 0;
            unsigned ypos   = 0;
            std::optional<unsigned> index;
            std::optional<unsigned> width;
            std::optional<unsigned> height;
        };

        DlgImgView(QWidget* parent);

        void LoadImage(const QString& file);
        void LoadJson(const QString& file);
        void SetDialogMode();

        QString GetImageFileName() const;
        QString GetJsonFileName() const;
        QString GetImageName() const;

    private slots:
        void on_btnSelectImage_clicked();
        void on_btnSelectJson_clicked();
        void on_btnClose_clicked();
        void on_btnAccept_clicked();
        void on_btnCancel_clicked();
        void on_cmbColorSpace_currentIndexChanged(int);
        void on_widgetColor_colorChanged(QColor color);
        void on_listWidget_itemSelectionChanged();
        void on_tabWidget_currentChanged(int);
        void finished();
        void timer();
    private:
        void OnPaintScene(gfx::Painter& painter, double secs);
        void OnMousePress(QMouseEvent* mickey);
        void OnMouseMove(QMouseEvent* mickey);
        void OnMouseRelease(QMouseEvent* mickey);
        void OnMouseDoubleClick(QMouseEvent* mickey);
        bool OnKeyPress(QKeyEvent* event);
    private:
        Ui::DlgImgView mUI;
    private:
        std::vector<Image> mList;
        std::size_t mSelectedIndex = 0;
        std::shared_ptr<gfx::TextureMap2DClass> mClass;
        std::unique_ptr<gfx::Material> mMaterial;
        unsigned mWidth  = 0;
        unsigned mHeight = 0;
        QPixmap mImage;
        QTimer mTimer;
        QPoint mTrackingOffset;
        QPoint mCurrentPoint;
        QPoint mStartPoint;
        bool mTracking = false;
        bool mDialogMode = false;
    };
} // namespace