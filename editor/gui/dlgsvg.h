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
#  include "ui_dlgsvg.h"
#  include <QDialog>
#  include <QStringList>
#include "warnpop.h"

namespace gui
{
    class DlgSvgView : public QDialog
    {
    Q_OBJECT

    public:
        explicit DlgSvgView(QWidget* parent);

        bool IsClosed() const
        { return mClosed; }

    private slots:
        void on_btnClose_clicked();
        void on_btnSelectImage_clicked();
        void on_chkShowBackground_stateChanged(int);
        void on_chkShowOutline_stateChanged(int);
        void on_viewX_valueChanged(int);
        void on_viewY_valueChanged(int);
        void on_viewW_valueChanged(int);
        void on_viewH_valueChanged(int);
        void on_rasterWidth_valueChanged(int);
        void on_rasterHeight_valueChanged(int);
        void on_btnDoubleSize_clicked();
        void on_btnHalveSize_clicked();
        void on_btnSaveAs_clicked();
        void on_cmbElement_currentIndexChanged(const QString&);

    private:
        void SetViewBox();
        void SetRasterSize(const QSize& size);
    private:
        Ui::DlgSvg mUI;
    private:
        bool mClosed = false;
        QStringList mElements;
        QString mLastSaveFile;
        float mViewAspect = 1.0f;
    };

} // namespace
