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

#include <vector>

namespace gui
{
    class DlgImgPack : public QDialog
    {
        Q_OBJECT
    public:
        DlgImgPack(QWidget* parent);

        bool IsClosed() const
        { return mClosed; }

    private slots:
        void on_btnDeleteImage_clicked();
        void on_btnBrowseImage_clicked();
        void on_btnSaveAs_clicked();
        void on_btnClose_clicked();
        void on_listWidget_currentRowChanged(int index);
        void on_colorChanged();
        void on_padding_valueChanged(int);
        void on_chkPot_stateChanged(int);
        void on_glyphIndex_textChanged(const QString& text);
        void on_imgName_textChanged(const QString& text);
    private:
        struct SourceImage;
        static bool ReadJson(const QString& file, std::vector<SourceImage>* sources);
        static bool ReadList(const QString& file, std::vector<SourceImage>* sources);
        static bool ReadImage(const QString& file, std::vector<SourceImage>* sources);
        void repack();
    private:
        Ui::DlgImgPack mUI;
    private:
        struct SourceImage {
            QString name;
            QString file;
            QString glyph;
            unsigned width  = 0;
            unsigned height = 0;
            unsigned xpos = 0;
            unsigned ypos = 0;
            unsigned index = 0;
            bool sub_image = false;
        };
        std::vector<SourceImage> mSources;

        QImage mPackedImage;
        nlohmann::json mJson;
        bool mClosed = false;
        QString mLastSaveFile;
    };

} // namespace
