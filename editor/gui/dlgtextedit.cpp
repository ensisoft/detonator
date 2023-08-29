
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
#include "warnpop.h"

#include "editor/gui/dlgtextedit.h"
#include "editor/app/utility.h"
namespace gui
{

DlgTextEdit::DlgTextEdit(QWidget* parent)
  : QDialog(parent)
{
    mUI.setupUi(this);
}

void DlgTextEdit::SetTitle(const QString& str)
{
    setWindowTitle(str);
    mUI.groupBox->setTitle(str);
}

void DlgTextEdit::SetText(const app::AnyString& str)
{
    mUI.text->setPlainText(str);
}

app::AnyString DlgTextEdit::GetText() const
{
    return mUI.text->toPlainText();
}

void DlgTextEdit::on_btnAccept_clicked()
{
     accept();
}
void DlgTextEdit::on_btnCancel_clicked()
{
     reject();
}

} // namespace
