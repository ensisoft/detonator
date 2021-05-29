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

#define LOGTAG "settings"

#include "config.h"
#include "warnpush.h"
#  include <QJsonDocument>
#  include <QJsonArray>
#  include <QFile>
#include "warnpop.h"

#include "editor/app/eventlog.h"
#include "settings.h"

namespace gui
{

bool Settings::JsonFileSettingsStorage::Load()
{
    QFile file(mFilename);
    if (!file.open(QIODevice::ReadOnly))
    {
        ERROR("Failed to open settings file: '%1'", mFilename);
        return false;
    }

    const QJsonDocument doc(QJsonDocument::fromJson(file.readAll()));
    if (doc.isNull())
    {
        ERROR("JSON parse error in file: '%1'", mFilename);
        return false;
    }
    mValues = doc.object().toVariantMap();
    return true;
}

bool Settings::JsonFileSettingsStorage::Save()
{
    QFile file(mFilename);
    if (!file.open(QIODevice::WriteOnly))
    {
        ERROR("Failed to open settings file: '%1'", mFilename);
        return false;
    }
    const QJsonObject& json = QJsonObject::fromVariantMap(mValues);
    const QJsonDocument docu(json);

    file.write(docu.toJson());
    file.close();
    return true;
}

} // namespace
