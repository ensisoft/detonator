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

#pragma once

#include "config.h"

#include <QObject>
#include <QtUiPlugin/QDesignerCustomWidgetInterface>
#include <QtUiPlugin/QDesignerCustomWidgetCollectionInterface>
#include <QtDesigner/QExtensionFactory>
#include <QtDesigner/QDesignerContainerExtension>

namespace gui {
    class CollapsibleWidget;
} // namespace

// to export a single plugin use
// Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QDesignerCustomWidgetInterface")

// to export multiple plugins from a single dll use
// Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QDesignerCustomWidgetCollectionInterface")

// https://doc.qt.io/qt-6/qdesignercustomwidgetcollectioninterface.html#details

class QtSvgViewWidgetPlugin : public QObject, public QDesignerCustomWidgetInterface
{
Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)

public:
    QtSvgViewWidgetPlugin(QObject* parent = nullptr);

    virtual bool isContainer() const override;
    virtual bool isInitialized() const override;
    virtual QIcon icon() const override;
    virtual QString domXml() const override;
    virtual QString group() const override;
    virtual QString includeFile() const override;
    virtual QString name() const override;
    virtual QString toolTip() const override;
    virtual QString whatsThis() const override;
    virtual QWidget *createWidget(QWidget *parent) override;
    virtual void initialize(QDesignerFormEditorInterface *core) override;

private:
    bool initialized = false;
};


class DoubleSliderWidgetPlugin : public QObject, public QDesignerCustomWidgetInterface
{
Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)

public:
    DoubleSliderWidgetPlugin(QObject* parent = nullptr);

    virtual bool isContainer() const override;
    virtual bool isInitialized() const override;
    virtual QIcon icon() const override;
    virtual QString domXml() const override;
    virtual QString group() const override;
    virtual QString includeFile() const override;
    virtual QString name() const override;
    virtual QString toolTip() const override;
    virtual QString whatsThis() const override;
    virtual QWidget *createWidget(QWidget *parent) override;
    virtual void initialize(QDesignerFormEditorInterface *core) override;

private:
    bool initialized = false;
};


class DoubleSpinBoxWidgetPlugin : public QObject, public QDesignerCustomWidgetInterface
{
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)

public:
    DoubleSpinBoxWidgetPlugin(QObject* parent = nullptr);

    virtual bool isContainer() const override;
    virtual bool isInitialized() const override;
    virtual QIcon icon() const override;
    virtual QString domXml() const override;
    virtual QString group() const override;
    virtual QString includeFile() const override;
    virtual QString name() const override;
    virtual QString toolTip() const override;
    virtual QString whatsThis() const override;
    virtual QWidget *createWidget(QWidget *parent) override;
    virtual void initialize(QDesignerFormEditorInterface *core) override;

private:
    bool initialized = false;
};


class RangeWidgetPlugin : public QObject, public QDesignerCustomWidgetInterface
{
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)

public:
    RangeWidgetPlugin(QObject* parent = nullptr);

    virtual bool isContainer() const override;
    virtual bool isInitialized() const override;
    virtual QIcon icon() const override;
    virtual QString domXml() const override;
    virtual QString group() const override;
    virtual QString includeFile() const override;
    virtual QString name() const override;
    virtual QString toolTip() const override;
    virtual QString whatsThis() const override;
    virtual QWidget *createWidget(QWidget *parent) override;
    virtual void initialize(QDesignerFormEditorInterface *core) override;

private:
    bool initialized = false;
};

class TreeWidgetPlugin : public QObject, public QDesignerCustomWidgetInterface
{
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)

public:
    TreeWidgetPlugin(QObject* parent = nullptr);

    virtual bool isContainer() const override;
    virtual bool isInitialized() const override;
    virtual QIcon icon() const override;
    virtual QString domXml() const override;
    virtual QString group() const override;
    virtual QString includeFile() const override;
    virtual QString name() const override;
    virtual QString toolTip() const override;
    virtual QString whatsThis() const override;
    virtual QWidget *createWidget(QWidget *parent) override;
    virtual void initialize(QDesignerFormEditorInterface *core) override;

private:
    bool initialized = false;
};

class CollapsibleWidgetPlugin : public QObject, public QDesignerCustomWidgetInterface
{
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)

public:
    CollapsibleWidgetPlugin(QObject* parent = nullptr);

    virtual bool isContainer() const override;
    virtual bool isInitialized() const override;
    virtual QIcon icon() const override;
    virtual QString domXml() const override;
    virtual QString group() const override;
    virtual QString includeFile() const override;
    virtual QString name() const override;
    virtual QString toolTip() const override;
    virtual QString whatsThis() const override;
    virtual QWidget *createWidget(QWidget *parent) override;
    virtual void initialize(QDesignerFormEditorInterface *core) override;
private:
    bool initialized = false;
};

// this class is the interface implementation for combining multiple widget plugins
// i.e. QDesignerCustomWidgetInterface implementations in a single library (.dll or .so)
class MyCustomWidgets: public QObject, public QDesignerCustomWidgetCollectionInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QDesignerCustomWidgetCollectionInterface")
    Q_INTERFACES(QDesignerCustomWidgetCollectionInterface)

public:
    explicit MyCustomWidgets(QObject *parent = 0);

    QList<QDesignerCustomWidgetInterface*> customWidgets() const override;

private:
    QList<QDesignerCustomWidgetInterface*> widgets;
};

QT_BEGIN_NAMESPACE
class QExtensionManager;
QT_END_NAMESPACE

class CollapsibleWidgetContainerExtension : public QObject,
                                            public QDesignerContainerExtension
{
    Q_OBJECT
    Q_INTERFACES(QDesignerContainerExtension)

public:
    explicit CollapsibleWidgetContainerExtension(gui::CollapsibleWidget *widget, QObject *parent);

    bool canAddWidget() const override;
    void addWidget(QWidget *widget) override;
    int count() const override;
    int currentIndex() const override;
    void insertWidget(int index, QWidget *widget) override;
    bool canRemove(int index) const override;
    void remove(int index) override;
    void setCurrentIndex(int index) override;
    QWidget *widget(int index) const override;

private:
    int mCurrentIndex = 0;
    gui::CollapsibleWidget *myWidget = nullptr;
};

class CollapsibleWidgetExtensionFactory : public QExtensionFactory
{
    Q_OBJECT

public:
    explicit CollapsibleWidgetExtensionFactory(QExtensionManager *parent = nullptr);

protected:
    QObject *createExtension(QObject *object, const QString &iid, QObject *parent) const override;
};
