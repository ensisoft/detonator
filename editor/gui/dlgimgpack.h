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
#  include "ui_dlgimgpack.h"
#  include <QImage>
#  include <QPixmap>
#  include <QString>
#  include <nlohmann/json.hpp>
#include "warnpop.h"

namespace gui
{
    class DlgImgPack : public QDialog
    {
        Q_OBJECT
    public:
        DlgImgPack(QWidget* parent);

    private slots:
        void on_btnDeleteImage_clicked();
        void on_btnBrowseImage_clicked();
        void on_btnSaveAs_clicked();
        void on_btnClose_clicked();
        void on_listWidget_currentRowChanged(int index);
        void on_colorChanged();

    private:
        void repack();

    private:
        Ui::DlgImgPack mUI;
    private:
        QPixmap mPackedImage;
        nlohmann::json mJson;
        //QString mPackedImageJson;
    };

} // namespace
