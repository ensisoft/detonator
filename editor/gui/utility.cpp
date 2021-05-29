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
#  include <QFontDatabase>
#  include <QStringList>
#  include <QDir>
#  include <QCoreApplication>
#include "warnpop.h"

#include "editor/app/utility.h"
#include "editor/gui/utility.h"

namespace gui {

void PopulateFontNames(QComboBox* cmb)
{
    QSignalBlocker s(cmb);
    cmb->clear();

    QStringList filters;
    filters << "*.ttf" << "*.otf";
    const auto& appdir = QCoreApplication::applicationDirPath();
    const auto& fontdir = app::JoinPath(appdir , "fonts");
    QDir dir;
    dir.setPath(fontdir);
    dir.setNameFilters(filters);
    const QStringList& font_files = dir.entryList();
    for (const auto& font_file : font_files)
    {
        const QFileInfo info(font_file);
        cmb->addItem("app://fonts/" + info.fileName());
    }
}

void PopulateFontSizes(QComboBox* cmb)
{
    QSignalBlocker s(cmb);
    cmb->clear();

    const auto font_sizes = QFontDatabase::standardSizes();
    for (int size : font_sizes)
        cmb->addItem(QString::number(size));
}

void PopulateUIStyles(QComboBox* cmb)
{
    QSignalBlocker s(cmb);
    cmb->clear();

    QStringList filters;
    filters << "*.json";
    const auto& appdir = QCoreApplication::applicationDirPath();
    const auto& styledir = app::JoinPath(appdir, "ui");
    QDir dir;
    dir.setPath(styledir);
    dir.setNameFilters(filters);
    const QStringList& style_files = dir.entryList();
    for (const auto& style_file : style_files)
    {
        const QFileInfo info(style_file);
        cmb->addItem("app://ui/" + info.fileName());
    }
}

} // gui