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
#include <QtDesigner/QExtensionFactory>
#include <QtDesigner/QExtensionManager>
#include <QtDesigner/QDesignerFormEditorInterface>
#include <QtDesigner/QDesignerFormWindowInterface>
#include <QtDesigner/QDesignerContainerExtension>
#include <QtDesigner/QDesignerPropertySheetExtension>

#include "plugin.h"
#include "../spinboxwidget.h"
#include "../rangewidget.h"
#include "../treewidget.h"
#include "../collapsible_widget.h"

DoubleSpinBoxWidgetPlugin::DoubleSpinBoxWidgetPlugin(QObject* parent)
  : QObject(parent)
{}

bool DoubleSpinBoxWidgetPlugin::isContainer() const
{
    return false;
}
bool DoubleSpinBoxWidgetPlugin::isInitialized() const
{
    return initialized;
}
QIcon DoubleSpinBoxWidgetPlugin::icon() const
{
    return QIcon();
}
QString DoubleSpinBoxWidgetPlugin::domXml() const
{
    return "<ui language=\"c++\">\n"
           " <widget class=\"gui::DoubleSpinBox\" name=\"spinBox\">\n"
           "  <property name=\"geometry\">\n"
           "   <rect>\n"
           "    <x>0</x>\n"
           "    <y>0</y>\n"
           "    <width>100</width>\n"
           "    <height>30</height>\n"
           "   </rect>\n"
           "  </property>\n"
           " </widget>\n"
           "</ui>\n";
}
QString DoubleSpinBoxWidgetPlugin::group() const
{
    return "DETONATOR2D";
}

QString DoubleSpinBoxWidgetPlugin::includeFile() const
{
    return "spinboxwidget.h";
}

QString DoubleSpinBoxWidgetPlugin::name() const
{
    return "gui::DoubleSpinBox";
}
QString DoubleSpinBoxWidgetPlugin::toolTip() const
{
    return "";
}
QString DoubleSpinBoxWidgetPlugin::whatsThis() const
{
    return toolTip();
}
QWidget* DoubleSpinBoxWidgetPlugin::createWidget(QWidget *parent)
{
    return new gui::DoubleSpinBox(parent);
}
void DoubleSpinBoxWidgetPlugin::initialize(QDesignerFormEditorInterface *core)
{
    initialized = true;
}


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

CollapsibleWidgetPlugin::CollapsibleWidgetPlugin(QObject* parent)
  : QObject(parent)
{
    //qDebug("CollapsibleWidgetPlugin::CollapsibleWidgetPlugin");
}

bool CollapsibleWidgetPlugin::isContainer() const
{
    return true;
}
bool CollapsibleWidgetPlugin::isInitialized() const
{
    return initialized;
}
QIcon CollapsibleWidgetPlugin::icon() const
{
    return QIcon();
}
QString CollapsibleWidgetPlugin::domXml() const
{
    return "<ui language=\"c++\">\n"
           " <widget class=\"gui::CollapsibleWidget\" name=\"fooWidget\">\n"
           "  <widget class=\"QWidget\" name=\"page\" />\n"
           "  <property name=\"geometry\">\n"
           "   <rect>\n"
           "    <x>0</x>\n"
           "    <y>0</y>\n"
           "    <width>200</width>\n"
           "    <height>300</height>\n"
           "   </rect>\n"
           "  </property>\n"
           " </widget>\n"
           "<customwidgets>\n"
               "<customwidget>\n"
                 "<class>\"gui::CollapsibleWidget\"</class>\n"
                 "<extends>QWidget</extends>\n"
                 "<addpagemethod>AddPage</addpagemethod>\n"
               "</customwidget>\n"
           "</customwidgets>\n"
           "</ui>\n";
}
QString CollapsibleWidgetPlugin::group() const
{
    return "DETONATOR2D";
}

QString CollapsibleWidgetPlugin::includeFile() const
{
    return "collapsible_widget.h";
}

QString CollapsibleWidgetPlugin::name() const
{
    return "gui::CollapsibleWidget";
}
QString CollapsibleWidgetPlugin::toolTip() const
{
    return "";
}
QString CollapsibleWidgetPlugin::whatsThis() const
{
    return "";
}
QWidget* CollapsibleWidgetPlugin::createWidget(QWidget *parent)
{
    //qDebug("CollapsibleWidgetPlugin::createWidget");
    return new gui::CollapsibleWidget(parent);
}
void CollapsibleWidgetPlugin::initialize(QDesignerFormEditorInterface* formEditor)
{
    if (initialized)
        return;

    QExtensionManager* manager = formEditor->extensionManager();
    QExtensionFactory* factory = new CollapsibleWidgetExtensionFactory(manager);

    Q_ASSERT(manager != 0);
    manager->registerExtensions(factory, Q_TYPEID(QDesignerContainerExtension));

    initialized = true;
    //qDebug("CollapsibleWidgetPlugin::initialize");
}

MyCustomWidgets::MyCustomWidgets(QObject* parent)
  : QObject(parent)
{
    widgets.append(new RangeWidgetPlugin(this));
    widgets.append(new TreeWidgetPlugin(this));
    widgets.append(new CollapsibleWidgetPlugin(this));
    widgets.append(new DoubleSpinBoxWidgetPlugin(this));
}

QList<QDesignerCustomWidgetInterface*> MyCustomWidgets::customWidgets() const
{
    return widgets;
}



CollapsibleWidgetContainerExtension::CollapsibleWidgetContainerExtension(gui::CollapsibleWidget *widget, QObject *parent)
    : QObject(parent)
    , myWidget(widget)
{
}
bool CollapsibleWidgetContainerExtension::canAddWidget() const
{
    // This should return false for containers that have a single, fixed page, for example QScrollArea or QDockWidget.

    return false;
}
void CollapsibleWidgetContainerExtension::addWidget(QWidget *widget)
{
    // Adds the given page to the container by appending it to the extension's list of pages.
    //qDebug("CollapsibleWidgetContainerExtension::addWidget, widget=%p");
    myWidget->AddPage(widget);
}
int CollapsibleWidgetContainerExtension::count() const
{
    // Returns the number of pages in the container.

    //qDebug("CollapsibleWidgetContainerExtension::count");
    return myWidget->GetCount();

}
int CollapsibleWidgetContainerExtension::currentIndex() const
{
    // Returns the index of the currently selected page in the container.
    //qDebug("CollapsibleWidgetContainer::currentIndex");
    return mCurrentIndex;
}


void CollapsibleWidgetContainerExtension::insertWidget(int index, QWidget *widget)
{
    //qDebug("CollapsibleWidgetContainer::insertWidget, index=%d, widget=%p", index, widget);
}
bool CollapsibleWidgetContainerExtension::canRemove(int index) const
{
    //qDebug("CollapsibleWidgetContainer::canRemove, index=%d", index);
    return false;
}

void CollapsibleWidgetContainerExtension::remove(int index)
{
    //qDebug("CollapsibleWidgetContainer::remove, index=%d", index);
}
void CollapsibleWidgetContainerExtension::setCurrentIndex(int index)
{
    //qDebug("CollapsibleWidgetContainer::setCurrentIndex, index=%d", index);
}
QWidget* CollapsibleWidgetContainerExtension::widget(int index) const
{
    //qDebug("CollapsibleWidgetContainer::widget, index=%d, myWidget=%p", index, myWidget);
    return myWidget->GetWidget(index);
}

CollapsibleWidgetExtensionFactory::CollapsibleWidgetExtensionFactory(QExtensionManager *parent)
  : QExtensionFactory(parent)
{}

QObject *CollapsibleWidgetExtensionFactory::createExtension(QObject *object,
                                                            const QString &iid,
                                                            QObject *parent) const
{
    auto *widget = qobject_cast<gui::CollapsibleWidget*>(object);

    //qDebug("CollapsibleWidgetExtensionFactory: object=%p, class='%s', widget=%p", object, object->metaObject()->className(), widget);

    if (widget && (iid == Q_TYPEID(QDesignerContainerExtension)))
        return new CollapsibleWidgetContainerExtension(widget, parent);

    //qDebug("CollapsibleWidgetExtensionFactory null return");
    return nullptr;
}
//! [1]