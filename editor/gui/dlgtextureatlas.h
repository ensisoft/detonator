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
#  include "ui_dlgtextureatlas.h"
#  include <QDialog>
#  include <QString>
#  include <QPixmap>
#include "warnpop.h"

namespace gui
{
    class DlgTextureAtlas : public QDialog
    {
        Q_OBJECT

    public:
        struct Image {
            QString name;
            unsigned width  = 0;
            unsigned height = 0;
            unsigned xpos   = 0;
            unsigned ypos   = 0;
            unsigned index  = 0;
        };

        DlgTextureAtlas(QWidget* parent);

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
        void on_listWidget_currentRowChanged(int index);
    private:
        Ui::DlgTextureAtlas mUI;
    private:
        std::vector<Image> mList;
        QPixmap mImage;
    };
} // namespace