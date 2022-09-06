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
#  include <QWidget>
#  include <QColor>
#  include <QDialog>
#  include "ui_dlgstylestring.h"
#include "warnpop.h"

#include <string>
#include <memory>
#include <vector>

namespace gui
{
    class DlgWidgetStyleString : public QDialog
    {
        Q_OBJECT
    public:
        DlgWidgetStyleString(QWidget* parent, const QString& style_string);

        QString GetStyleString() const;
    private slots:
        void on_btnAccept_clicked();
        void on_btnCancel_clicked();
    private:
        Ui::DlgStyleString mUI;
    };
} // namespace
