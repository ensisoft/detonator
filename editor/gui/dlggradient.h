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
#  include "ui_dlggradient.h"
#  include <QDialog>
#include "warnpop.h"

#include "base/assert.h"

namespace gui
{
    class DlgGradient : public QDialog
    {
        Q_OBJECT
    public:
        DlgGradient(QWidget* parent) : QDialog(parent)
        {
            mUI.setupUi(this);
        }
        void SetColor(const QColor& color, int index)
        {
            if (index == 0)
                mUI.colorMap0->setColor(color);
            else if (index == 1)
                mUI.colorMap1->setColor(color);
            else if (index == 2)
                mUI.colorMap2->setColor(color);
            else if (index == 3)
                mUI.colorMap3->setColor(color);
            else BUG("Incorrect color index.");
        }
        QColor GetColor(int index) const
        {
            if (index == 0)
                return mUI.colorMap0->color();
            else if (index == 1)
                return mUI.colorMap1->color();
            else if (index == 2)
                return mUI.colorMap2->color();
            else if (index == 3)
                return mUI.colorMap3->color();
            else BUG("Incorrect color index.");
            return QColor();
        }
    private slots:
        void on_btnAccept_clicked()
        {
            accept();
        };
        void on_btnCancel_clicked()
        {
            reject();
        }
    private:
        Ui::DlgGradient mUI;
    };
} // namespace
