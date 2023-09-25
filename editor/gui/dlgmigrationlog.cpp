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

#define LOGTAG "gui"

#include "config.h"

#include "warnpush.h"
#  include <QString>
#  include <QTextStream>
#include "warnpop.h"

#include <unordered_map>
#include <vector>

#include "editor/gui/dlgmigrationlog.h"
#include "editor/app/utility.h"
#include "editor/app/format.h"
#include "editor/app/resource.h"

namespace gui
{

DlgMigrationLog::DlgMigrationLog(QWidget* parent, const app::MigrationLog& log)
  : QDialog(parent)
{
    mUI.setupUi(this);

    QString str;
    QTextStream ss(&str);

    struct Foobar {
        QString name;
        QString type;
        std::vector<QString> messages;
    };

    std::unordered_map<QString, Foobar> messages;

    for (size_t i=0; i<log.GetNumActions(); ++i)
    {
        const auto& action = log.GetAction(i);
        messages[action.id].name = action.name;
        messages[action.id].type = action.type;
        messages[action.id].messages.push_back(action.message);
    }
    ss << "The following resources have been migrated/updated to the current version.\n\n";
    for (const auto& [key, foo] : messages)
    {
        ss << foo.type << " | " << foo.name << "\n";
        ss << "--------------------------------------------------------------\n";
        for (const auto& msg : foo.messages)
        {
            ss << "* " << msg;
            ss << "\n";
        }
        ss << "\n";
    }
    mUI.text->setPlainText(str);
}

void DlgMigrationLog::on_btnClose_clicked()
{
    close();
}

} // namespace
