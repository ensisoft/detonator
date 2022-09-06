
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

#define LOGTAG "gui"

#include "config.h"

#include "warnpush.h"
#  include <QFileDialog>
#  include <QFileInfo>
#  include <Qt-Color-Widgets/include/ColorDialog>
#  include <neargye/magic_enum.hpp>
#include "warnpop.h"

#include "editor/gui/dlgstylestring.h"
#include "editor/app/utility.h"
namespace gui
{
DlgWidgetStyleString::DlgWidgetStyleString(QWidget* parent, const QString& style_string)
  : QDialog(parent)
{
    mUI.setupUi(this);
    mUI.text->setPlainText(style_string);
}
QString DlgWidgetStyleString::GetStyleString() const
{
    return mUI.text->toPlainText();
}

void DlgWidgetStyleString::on_btnAccept_clicked()
{
     accept();
}
void DlgWidgetStyleString::on_btnCancel_clicked()
{
     reject();
}

} // namespace
