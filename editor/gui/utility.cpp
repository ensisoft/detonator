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
#  include <QIcon>
#  include <QPixmap>
#  include <QStyleFactory>
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include "base/json.h"
#include "editor/app/eventlog.h"
#include "editor/app/utility.h"
#include "editor/gui/utility.h"

namespace gui {


void SetValue(QPlainTextEdit* edit, const app::AnyString& value, const app::AnyString& format)
{
    QSignalBlocker s(edit);
    if (format == "JSON")
    {
        const auto& [ok, json, error] = base::JsonParse(value);
        if (ok)
        {
            edit->setPlainText(app::FromUtf8(json.dump(2)));
        }
        return;
    }
    edit->setPlainText(value);
}


QPixmap ToGrayscale(QPixmap p)
{
    QImage img = p.toImage();
    const int width  = img.width();
    const int height = img.height();
    for (int i=0; i<width; ++i)
    {
        for (int j=0; j<height; ++j)
        {
            const auto pix = img.pixel(i, j);
            const auto val = qGray(pix);
            img.setPixel(i, j, qRgba(val, val, val, (pix >> 24 & 0xff)));
        }
    }
    return QPixmap::fromImage(img);
}

std::vector<QString> ListWSFonts(const QString& workspace_dir)
{
    std::vector<QString> ret;

    QStringList filters;
    filters << "*.ttf" << "*.otf" << "*.json";
    const auto& fontdir = app::JoinPath(workspace_dir , "fonts");
    QDir dir;
    dir.setPath(fontdir);
    dir.setNameFilters(filters);
    const QStringList& font_files = dir.entryList();
    for (const auto& font_file : font_files)
    {
        const QFileInfo info(font_file);
        ret.push_back(QString("ws://fonts/" + info.fileName()));
    }
    return ret;
}

std::vector<QString> ListAppFonts()
{
    std::vector<QString> ret;

    QStringList filters;
    filters << "*.ttf" << "*.otf" << "*.json";
    const auto& appdir = QCoreApplication::applicationDirPath();
    const auto& fontdir = app::JoinPath(appdir , "fonts");
    QDir dir;
    dir.setPath(fontdir);
    dir.setNameFilters(filters);
    const QStringList& font_files = dir.entryList();
    for (const auto& font_file : font_files)
    {
        const QFileInfo info(font_file);
        ret.push_back(QString("app://fonts/" + info.fileName()));
    }
    return ret;
}

std::vector<ResourceListItem> ListParticles()
{
    std::vector<ResourceListItem> ret;

    QStringList filters;
    filters << "*.png";
    const auto& appdir = QCoreApplication::applicationDirPath();
    const auto& fontdir = app::JoinPath(appdir , "textures/particles");
    QDir dir;
    dir.setPath(fontdir);
    dir.setNameFilters(filters);
    const QStringList& files = dir.entryList();
    for (const auto& file : files)
    {
        const QFileInfo info(file);

        ResourceListItem item;
        item.name = info.baseName();
        item.id   = app::toString("app://textures/particles/%1", info.fileName());
        item.icon = QIcon(dir.absoluteFilePath(file));
        ret.push_back(item);
    }
    return ret;
}

std::vector<ResourceListItem> ListShaders()
{
    std::vector<ResourceListItem> ret;

    QStringList filters;
    filters << "*.glsl";
    const auto& appdir = QCoreApplication::applicationDirPath();
    const auto& fontdir = app::JoinPath(appdir , "shaders/es2");
    QDir dir;
    dir.setPath(fontdir);
    dir.setNameFilters(filters);
    const QStringList& files = dir.entryList();
    for (const auto& file : files)
    {
        const QFileInfo info(file);

        ResourceListItem item;
        item.name = info.baseName();
        item.id   = app::toString("app://shaders/es2/%1", info.fileName());
        ret.push_back(item);
    }
    return ret;
}


std::vector<ResourceListItem> ListPresetParticles()
{
    std::vector<ResourceListItem> ret;

    QStringList filters;
    filters << "*.json";
    const auto& appdir = QCoreApplication::applicationDirPath();
    const auto& fontdir = app::JoinPath(appdir , "presets/particles");
    QDir dir;
    dir.setPath(fontdir);
    dir.setNameFilters(filters);
    const QStringList& files = dir.entryList();
    for (const auto& file : files)
    {
        const QFileInfo info(file);

        ResourceListItem item;
        item.name = info.baseName();
        item.id   = app::toString("app://presets/particles/%1", info.fileName());
        ret.push_back(item);
    }
    return ret;
}

void PopulatePresetParticleList(QComboBox* cmb)
{
    QSignalBlocker s(cmb);
    cmb->clear();
    cmb->setProperty("__is_id_list__", true);

    for (const auto& shader : ListPresetParticles())
    {
        cmb->addItem(shader.name, shader.id);
    }
}

void PopulateShaderList(QComboBox* cmb, const QString& filter)
{
    QSignalBlocker s(cmb);
    cmb->clear();
    cmb->setProperty("__is_id_list__", true);

    for (const auto& shader : ListShaders())
    {
        if (!filter.isEmpty() && !shader.id.Contains(filter))
            continue;

        cmb->addItem(shader.name, shader.id);
    }
}

void PopulateParticleList(QComboBox* cmb)
{
    QSignalBlocker s(cmb);
    cmb->clear();
    cmb->setProperty("__is_id_list__", true);

    for (const auto& particle : ListParticles())
    {
        cmb->addItem(particle.icon, particle.name, particle.id);
    }
}

void PopulateFontNames(QComboBox* cmb)
{
    QSignalBlocker s(cmb);
    cmb->clear();
    for (const auto& font : ListAppFonts())
    {
        cmb->addItem(font);
    }
    if (cmb->isEditable())
        cmb->lineEdit()->setCursorPosition(0);
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
    cmb->setProperty("__is_string_list__", true);

    QStringList filters;
    filters << "*.json";
    const auto& appdir = QCoreApplication::applicationDirPath();
    const auto& styledir = app::JoinPath(appdir, "ui/style");
    QDir dir;
    dir.setPath(styledir);
    dir.setNameFilters(filters);
    const QStringList& style_files = dir.entryList();
    for (const auto& style_file : style_files)
    {
        const QFileInfo info(style_file);
        cmb->addItem("app://ui/style/" + info.fileName());
    }
    if (cmb->isEditable())
        cmb->lineEdit()->setCursorPosition(0);
}

void PopulateUIKeyMaps(QComboBox* cmb)
{
    QSignalBlocker s(cmb);
    cmb->clear();
    cmb->setProperty("__is_string_list__", true);

    QStringList filters;
    filters << "*.json";
    const auto& appdir = QCoreApplication::applicationDirPath();
    const auto& styledir = app::JoinPath(appdir, "ui/keymap");
    QDir dir;
    dir.setPath(styledir);
    dir.setNameFilters(filters);
    const QStringList& style_files = dir.entryList();
    for (const auto& style_file : style_files)
    {
        const QFileInfo info(style_file);
        cmb->addItem("app://ui/keymap/" + info.fileName());
    }
    if (cmb->isEditable())
        cmb->lineEdit()->setCursorPosition(0);
}

void PopulateQtStyles(QComboBox* cmb)
{
    QSignalBlocker s(cmb);
    cmb->clear();

    cmb->addItem("DETONATOR"); //

    // add Qt's built-in / plugin styles.
    const auto& styles = QStyleFactory::keys();
    for (const auto& style : styles)
    {
        cmb->addItem(style);
    }

    cmb->addItem("Fusion-Dark"); // custom dark

    if (cmb->isEditable())
        cmb->lineEdit()->setCursorPosition(0);
}

} // gui