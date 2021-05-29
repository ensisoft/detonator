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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <QWidget>
#  include <QObject>
#  include <QString>
#  include <QIcon>
#  include <QAbstractScrollArea>
#include "warnpop.h"

#include <vector>
#include <memory>

class QPalette;
class QPainter;
class QPaintEvent;
class QMouseEvent;
class QResizeEvent;
class QKeyEvent;

namespace gui
{
    // Thanks to the brain damaged shit that is QTreeView (seriously, how in the fuck
    // do you even make it support drag&drop within the widget ??)
    // we implement our own tree widget that makes world sane again.
    class TreeWidget : public QAbstractScrollArea
    {
        Q_OBJECT

    public:
        // TreeItem contains the data of each item row displayed
        // by thee TreeWidget.
        class TreeItem
        {
        public:
            // Set the Id that identifies this item
            void SetId(const QString& id)
            { mId = id;}
            // Set the item text.
            void SetText(const QString& text)
            { mText = text; }
            // Set the item icon.
            void SetIcon(const QIcon& icon)
            { mIcon = icon; }
            void SetIconMode(QIcon::Mode mode)
            { mIconMode = mode; }
            // Set the item user data.
            void SetUserData(void* user)
            { mUser = user;}
            // Set the indentation level that is used to visually express
            // parent-child relationship in the widget's UI.
            void SetLevel(unsigned level)
            { mLevel = level; }

            void* GetUserData()
            { return mUser; }
            const void* GetUserData() const
            { return mUser; }
            QString GetId() const
            { return mId; }
            QString GetText() const
            { return mText; }
            QIcon GetIcon() const
            { return mIcon; }
            unsigned GetLevel() const
            { return mLevel; }
            QIcon::Mode GetIconMode() const
            { return mIconMode; }

        private:
            QString mId;
            QString mText;
            QIcon   mIcon;
            QIcon::Mode mIconMode = QIcon::Normal;
            void*   mUser = nullptr;
            unsigned mLevel  = 0;
        };
        // Abstract interface for getting a list of TreeItems.
        // The model is expected to traverse the underlying tree
        // hiearchy, visit each node and provide a list of items.
        // Preorder tree traversal will produce the expected UI
        // hierarchy. To indicate that an item is a child item
        // identation can be set.
        class TreeModel
        {
        public:
            virtual ~TreeModel() = default;
            // Walk the tree model and fill out the render node hiearchy
            // of stuff to render and manage.
            //virtual void BuildTree(RenderTree& root) = 0;
            virtual void Flatten(std::vector<TreeItem>& list) = 0;
        private:
        };

        // ctor.
        TreeWidget(QWidget* parent);

        // Rebuild the widget's item tree. This will ask the
        // TreeModel to provide a new flattened list of TreeItems
        // and then trigger a repaint.
        void Rebuild();

        // Ask the widget to do a repaint. You should call this
        // if you change some TreeItems' properties and want to
        // have them reflected in the widget.
        void Update();

        // Get the currently selected item if any.
        TreeItem* GetSelectedItem()
        { return mSelected; }
        const TreeItem* GetSelectedItem() const
        { return mSelected; }
        // Get the current treewidget's data model.
        TreeModel* GetModel()
        { return mModel; }
        // Set the data model that provides access to the tree data.
        void SetModel(TreeModel* model)
        { mModel = model; }

        void SelectItemById(const QString& id);

        void ClearSelection();

        // QWidget
        virtual QSize sizeHint() const override
        { return QSize(200, 200); }
        virtual QSize minimumSizeHint() const override
        { return QSize(200, 200); }

    signals:
        void currentRowChanged();
        void dragEvent(TreeItem* item, TreeItem* target);
        void clickEvent(TreeItem* item);

    private:
        virtual void paintEvent(QPaintEvent* event) override;
        virtual void mouseMoveEvent(QMouseEvent* mickey) override;
        virtual void mousePressEvent(QMouseEvent* mickey) override;
        virtual void mouseReleaseEvent(QMouseEvent* mickey) override;
        virtual void enterEvent(QEvent*) override;
        virtual void leaveEvent(QEvent*) override;
        virtual void keyPressEvent(QKeyEvent* key) override;
        virtual void resizeEvent(QResizeEvent* resize) override;
        virtual void scrollContentsBy(int dx, int dy) override;
    private:
        QPoint MapPoint(const QPoint& widget) const;
    private:
        // the provider of the tree widget data i.e. TreeItems
        TreeModel* mModel = nullptr;
        // The currently selected item if any.
        TreeItem* mSelected = nullptr;
        // The currently hovered item if any.
        TreeItem* mHovered  = nullptr;
        // The current list of tree items.
        std::vector<TreeItem> mItems;
        // item height (in pixels) for each item
        unsigned mItemHeight = 0;
        // current scrolling offsets, changed when the scroll
        // bars are moved by the user.
        int mXOffset = 0;
        int mYOffset = 0;
        // true if doing drag & drop
        bool mDragging = false;
        QPoint mDragStart;
        QPoint mDragPoint;
    };

} // namespace
