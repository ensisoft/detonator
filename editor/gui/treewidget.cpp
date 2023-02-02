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

#include "config.h"

#include "warnpush.h"
#  include <QCoreApplication>
#  include <QKeyEvent>
#  include <QPaintEvent>
#  include <QMouseEvent>
#  include <QResizeEvent>
#  include <QScrollBar>
#  include <QStyle>
#  include <QStyleOptionFocusRect>
#  include <QStylePainter>
#  include <QPainter>
#  include <QPalette>
#  include <QBrush>
#  include <QFont>
#  include <QPen>
#  include <QDebug>
#include "warnpop.h"

#include "treewidget.h"

namespace {

void RenderTreeItem(const gui::TreeWidget::TreeItem& item, const QRect& box, const QPalette& palette, QPainter& painter,
    bool selected, bool hovered)
{
    const unsigned kBaseLevel = 3;
    const unsigned kLevelOffset = 16; // px
    const auto offset = kBaseLevel * kLevelOffset + item.GetLevel() * kLevelOffset;

    if (selected)
    {
        QPen pen;
        pen.setColor(palette.color(QPalette::HighlightedText));
        painter.setPen(pen);
        painter.fillRect(box, palette.color(QPalette::Highlight));
        painter.drawText(box.translated(offset, 0), Qt::AlignVCenter | Qt::AlignLeft, item.GetText());
    }
    else if (hovered)
    {
        QPen pen;
        painter.setPen(pen);
        pen.setColor(QColor(0x14, 0x8C, 0xD2)); //palette.color(QPalette::BrightText));
        painter.fillRect(box.translated(offset, 0), QColor(50, 65, 75, 255));
        painter.drawText(box.translated(offset, 0), Qt::AlignVCenter | Qt::AlignLeft, item.GetText());
    }
    else
    {
        QPen pen;
        pen.setColor(palette.color(QPalette::Text));
        painter.setPen(pen);
        painter.fillRect(box, palette.color(QPalette::Base));
        painter.drawText(box.translated(offset, 0), Qt::AlignVCenter | Qt::AlignLeft, item.GetText());
    }
    const QIcon& ico = item.GetIcon();
    if (!ico.isNull())
    {
        ico.paint(&painter, box.translated(0, 0), Qt::AlignLeft, item.GetIconMode());
    }
}


} // namespace

namespace gui
{

TreeWidget::TreeWidget(QWidget* parent) : QAbstractScrollArea(parent)
{
    // itemheight is determined by the default font height.
    QFont font;
    QFontMetrics fm(font);
    mItemHeight = fm.height();

    // need to set the focus policy in order to receive keyboard events.
    setFocusPolicy(Qt::StrongFocus);

    // need to enable mouse tracking in order to get mouse move events.
    setMouseTracking(true);

    // looks like it doesn't appear automatically.. ?
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
}

void TreeWidget::Rebuild()
{
    if (!mModel)
        return;

    const QString id = mSelected ? mSelected->GetId() : "";

    mSelected = nullptr;
    mHovered  = nullptr;

    mItems.clear();
    mModel->Flatten(mItems);

    for (auto& item : mItems)
    {
        if (item.GetId() == id)
        {
            mSelected = &item;
            break;
        }
    }

    const auto num_rows = mItems.size();
    const auto num_rows_visible = viewport()->height() / mItemHeight;
    if (num_rows > num_rows_visible)
    {
        const auto num_scroll_steps = num_rows - num_rows_visible;
        verticalScrollBar()->setVisible(true);
        verticalScrollBar()->setMaximum(num_scroll_steps);
    }
    else
    {
        verticalScrollBar()->setVisible(false);
        mYOffset = 0;
    }
    // not used right now
    horizontalScrollBar()->setVisible(false);
    viewport()->update();
}

void TreeWidget::Update()
{
    viewport()->update();
}

void TreeWidget::SelectItemById(const QString& id)
{
    for (auto& item : mItems)
    {
        if (item.GetId() == id)
        {
            mSelected = &item;
            break;
        }
    }

    viewport()->update();

    emit currentRowChanged();
}

void TreeWidget::ClearSelection()
{
    mSelected = nullptr;

    viewport()->update();

    emit currentRowChanged();
}

void TreeWidget::paintEvent(QPaintEvent* event)
{
    const QPalette& palette = this->palette();

    const unsigned kBaseLevel    = 1;
    const unsigned kLevelOffset  = 15; // px
    const unsigned window_width  = viewport()->width();
    const unsigned window_height = viewport()->height();

    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), palette.color(QPalette::Base));

    for (size_t i=0; i<mItems.size(); ++i)
    {
        const auto& item = mItems[i];
        const int ypos   = mYOffset * mItemHeight + i * mItemHeight;
        const int xpos   = mXOffset + item.GetLevel() * kLevelOffset + kBaseLevel * kLevelOffset;
        const bool selected = &item == mSelected;
        const bool hovered  = &item == mHovered;
        const QRect box(0, ypos, window_width, mItemHeight);
        RenderTreeItem(item, box, palette, painter, selected, hovered);

        // draw the connecting lines between child/parent items.
        QPen line;
        line.setColor(palette.color(QPalette::Shadow));
        painter.setPen(line);
        painter.drawLine(32 + xpos - 15, ypos + mItemHeight/2,
                         32 + xpos - 3,  ypos + mItemHeight/2);
        if (item.GetLevel() > 0)
        {
            painter.drawLine(32 + xpos - 15, ypos + mItemHeight/2,
                             32 + xpos - 15, ypos - 1);
        }
    }

    // widget coordinates but the units match those of
    // our backbuffer so we can use these offsets
    // directly in the backbuffer space
    const auto& drag_offset = mDragPoint - mDragStart;

    // filter out some unwanted accidental mouse moves when clicking
    // on an item.
    if (mDragging && std::abs(drag_offset.y()) >= 1)
    {
        for (size_t i = 0; i < mItems.size(); ++i)
        {
            const auto &item = mItems[i];
            if (&item != mSelected)
                continue;

            // compute the ypos of the item being dragged in widget coordinate
            // offset by the drag offset
            const int ypos = mYOffset * mItemHeight + i * mItemHeight + drag_offset.y();
            const int xpos = 0; //drag_offset.x(); looks silly since we're clipping the item visually so limit to y axis

            // figure out where the item being dragged to would land
            // and then indicate it.
            const auto landing_index = (ypos + (mItemHeight / 2)) / mItemHeight;
            if (landing_index < mItems.size())
            {
                QPen line;
                line.setWidth(1);
                line.setColor(QColor(0xff, 0xff, 0xff)); // QColorConstants::White);

                // hightlight the potential new parent
                const auto &parent = mItems[landing_index];
                const int ypos = landing_index * mItemHeight;
                const int xpos = 0;
                const QRect rc(xpos, ypos, window_width, mItemHeight);
                //RenderTreeItem(item, rc, palette, painter, false, true);
                painter.setPen(line);
                painter.drawRect(rc);
            }
            painter.setOpacity(0.5f);
            // render the item being dragged.
            RenderTreeItem(item, QRect(xpos, ypos, window_width, mItemHeight),
                           palette, painter, true, false);
        }
    }
}

void TreeWidget::mouseMoveEvent(QMouseEvent* mickey)
{
    if (mDragging)
    {
        mDragPoint = MapPoint(mickey->pos());
        viewport()->update();
        return;
    }

    const auto point = MapPoint(mickey->pos());
    const auto index = point.y() / mItemHeight;

    mHovered = nullptr;

    if (index >= mItems.size())
        return;

    mHovered = &mItems[index];

    // trigger paint
    viewport()->update();
}

void TreeWidget::mousePressEvent(QMouseEvent* mickey)
{
    // need to find the node that we're hitting.
    if (!(mickey->button() == Qt::LeftButton ||
          mickey->button() == Qt::RightButton))
        return;

    const auto point = MapPoint(mickey->pos());
    //DEBUG("Mouse press ət %1,%2", mouse_x, mouse_y);

    // every row item has the same fixed height
    // so to determine the row that is clicked is easy
    const auto index = point.y() / mItemHeight;
    if (index >= mItems.size())
    {
        // select nada.
        mSelected = nullptr;
    }
    else
    {
        mSelected = &mItems[index];
        if (mickey->button() == Qt::LeftButton)
        {
            mDragging  = true;
            mDragStart = MapPoint(mickey->pos());
            mDragPoint = MapPoint(mickey->pos());
        }
    }

    // trigger paint
    viewport()->update();

    emit currentRowChanged();
}

void TreeWidget::mouseReleaseEvent(QMouseEvent* mickey)
{
    const bool was_dragging = mDragging;
    mDragging = false;

    if (!was_dragging)
    {
        const auto pos  = MapPoint(mickey->pos());
        const auto xpos = pos.x();
        const auto ypos = pos.y();
        if (xpos >= 16)
            return;

        const unsigned index = ypos / mItemHeight;
        if (index >= mItems.size())
            return;
        emit clickEvent(&mItems[index]);
        viewport()->update();
        return;
    }

    const auto& drag_offset = mDragPoint - mDragStart;
    if (std::abs(drag_offset.y()) < 1)
        return;

    for (size_t i=0; i<mItems.size(); ++i)
    {
        auto& item = mItems[i];
        if (&item != mSelected)
            continue;

        const unsigned xpos = 0;
        const unsigned ypos = drag_offset.y() + i * mItemHeight;
        const unsigned landing_index = (ypos + (mItemHeight/2)) / mItemHeight;
        if (landing_index >= mItems.size())
            return;
        // no point to drag onto itself
        if (landing_index == i)
            return;

        auto& target = mItems[landing_index];
        //DEBUG("Item '%1' dragged onto '%2'", item.GetText(), target.GetText());
        emit dragEvent(&item, &target);

        break;
    }
    Rebuild();

    viewport()->update();
}

void TreeWidget::enterEvent(QEvent*)
{
}

void TreeWidget::leaveEvent(QEvent*)
{
    mHovered  = nullptr;

    viewport()->update();
}

void TreeWidget::keyPressEvent(QKeyEvent* press)
{
    //DEBUG("Keypress event: %1", press->key());

    if (!mSelected)
        return;

    const auto key = press->key();

    unsigned index = 0;
    for (unsigned i=0; i<mItems.size(); ++i)
    {
        if (mSelected == &mItems[i])
        {
            index = i;
            break;
        }
    }

    if (key == Qt::Key_Up)
        if (index > 0) index--;
    if (key == Qt::Key_Down)
        if (index < mItems.size()-1) index++;

    mSelected = &mItems[index];

    viewport()->update();

    emit currentRowChanged();
}

void TreeWidget::resizeEvent(QResizeEvent* resize)
{
    // rebuild the tree and re-compute the extents for the
    // scroll bars.
    Rebuild();
}

void TreeWidget::scrollContentsBy(int dx, int dy)
{
    //DEBUG("Scroll event dx=%1 dy=%2", dx,dy);
    mXOffset += dx;
    mYOffset += dy;

    viewport()->update();
}

QPoint TreeWidget::MapPoint(const QPoint& widget) const
{
    // go from widget coordinate to a coordinate in the data
    // buffer (still in pixels)
    return QPoint(widget.x(), widget.y() - mItemHeight * mYOffset);
}

} // namespace
