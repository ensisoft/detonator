
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