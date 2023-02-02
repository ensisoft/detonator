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

#include "config.h"

#include <QIcon>
#include <QString>
#include <QtPlugin>

#include "plugin.h"

#include "../rangewidget.h"
#include "../treewidget.h"

RangeWidgetPlugin::RangeWidgetPlugin(QObject* parent)
  : QObject(parent)
{}

bool RangeWidgetPlugin::isContainer() const
{
    return false;
}
bool RangeWidgetPlugin::isInitialized() const
{
    return initialized;
}
QIcon RangeWidgetPlugin::icon() const
{
    return QIcon();
}
QString RangeWidgetPlugin::domXml() const
{
    return "<ui language=\"c++\">\n"
           " <widget class=\"gui::RangeWidget\" name=\"rangeWidget\">\n"
           "  <property name=\"geometry\">\n"
           "   <rect>\n"
           "    <x>0</x>\n"
           "    <y>0</y>\n"
           "    <width>200</width>\n"
           "    <height>30</height>\n"
           "   </rect>\n"
           "  </property>\n"
           " </widget>\n"
           "</ui>\n";
}
QString RangeWidgetPlugin::group() const
{
    return "DETONATOR2D";
}

QString RangeWidgetPlugin::includeFile() const
{
    return "rangewidget.h";
}

QString RangeWidgetPlugin::name() const
{
    return "gui::RangeWidget";
}
QString RangeWidgetPlugin::toolTip() const
{
    return "";
}
QString RangeWidgetPlugin::whatsThis() const
{
    return toolTip();
}
QWidget* RangeWidgetPlugin::createWidget(QWidget *parent)
{
    return new gui::RangeWidget(parent);
}
void RangeWidgetPlugin::initialize(QDesignerFormEditorInterface *core)
{
    initialized = true;
}


TreeWidgetPlugin::TreeWidgetPlugin(QObject* parent)
  : QObject(parent)
{}

bool TreeWidgetPlugin::isContainer() const
{
    return false;
}
bool TreeWidgetPlugin::isInitialized() const
{
    return initialized;
}
QIcon TreeWidgetPlugin::icon() const
{
    return QIcon();
}
QString TreeWidgetPlugin::domXml() const
{
    return "<ui language=\"c++\">\n"
           " <widget class=\"gui::TreeWidget\" name=\"treeWidget\">\n"
           "  <property name=\"geometry\">\n"
           "   <rect>\n"
           "    <x>0</x>\n"
           "    <y>0</y>\n"
           "    <width>200</width>\n"
           "    <height>200</height>\n"
           "   </rect>\n"
           "  </property>\n"
           " </widget>\n"
           "</ui>\n";
}
QString TreeWidgetPlugin::group() const
{
    return "DETONATOR2D";
}

QString TreeWidgetPlugin::includeFile() const
{
    return "treewidget.h";
}

QString TreeWidgetPlugin::name() const
{
    return "gui::TreeWidget";
}
QString TreeWidgetPlugin::toolTip() const
{
    return "";
}
QString TreeWidgetPlugin::whatsThis() const
{
    return "";
}
QWidget* TreeWidgetPlugin::createWidget(QWidget *parent)
{
    return new gui::TreeWidget(parent);
}
void TreeWidgetPlugin::initialize(QDesignerFormEditorInterface *core)
{
    initialized = true;
}

MyCustomWidgets::MyCustomWidgets(QObject* parent)
  : QObject(parent)
{
    widgets.append(new RangeWidgetPlugin(this));
    widgets.append(new TreeWidgetPlugin(this));
}

QList<QDesignerCustomWidgetInterface*> MyCustomWidgets::customWidgets() const
{
    return widgets;
}