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
