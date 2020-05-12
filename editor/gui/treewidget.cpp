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

#define LOGTAG "widget"

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

#include "editor/app/eventlog.h"
#include "treewidget.h"

namespace {

void RenderTreeItem(const gui::TreeWidget::TreeItem& item, const QRect& box, const QPalette& palette, QPainter& painter,
    bool selected, bool hovered)
{
    const unsigned kBaseLevel = 1;
    const unsigned kLevelOffset = 15; // px
    const auto offset = kLevelOffset + item.GetLevel() * kLevelOffset;

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

    // when using the qdarkstyle the stylesheet uses a "highlight" on the
    // frame border when hovering.
    // Theoretically it should be possible to set this property on
    // the custom widget in the style.qss file but good fucking luck making it work...
    // https://wiki.qt.io/Qt_Style_Sheets_and_Custom_Painting_Example
    // https://forum.qt.io/topic/27948/solved-custom-widget-with-custom-stylesheet
    bool dark_style = true;
    const QStringList& args = QCoreApplication::arguments();
    for (const QString& arg : args)
    {
        if (arg == "--no-dark-style")
        {
            dark_style = false;
            break;
        }
    }
    if (dark_style)
    {
        setStyleSheet(
            "QFrame:hover {\n"
            "  border: 1px solid #148CD2;\n"
            "  color: #F0F0F0;\n"
            "}");
    }
}

void TreeWidget::Rebuild()
{
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

    viewport()->update();
}

void TreeWidget::Update()
{
    viewport()->update();
}

void TreeWidget::paintEvent(QPaintEvent* event)
{
    const QPalette& palette = this->palette();

    const unsigned kBaseLevel = 1;
    const unsigned kLevelOffset = 15; // px

    const unsigned window_width  = viewport()->width();
    const unsigned window_height = viewport()->height();

    QFont font;
    QFontMetrics fm(font);

    // compute the extents of the whole tree.
    unsigned render_width  = 0;
    unsigned render_height = mItems.size() * mItemHeight;
    // compute maximum width
    for (const auto& item : mItems)
    {
        const unsigned text_width = fm.width(item.GetText());
        const unsigned text_offset = item.GetLevel() * kLevelOffset + kBaseLevel * kLevelOffset;
        render_width = std::max(text_width + text_offset, render_width);
    }
    render_width  = std::max(window_width, render_width);
    render_height = std::max(window_height, render_height);

    QPixmap buffer(render_width, render_height);

    buffer.fill(palette.color(QPalette::Base));

    // render to this offscreen buffer.
    QPainter painter(&buffer);

    for (size_t i=0; i<mItems.size(); ++i)
    {
        const auto& item = mItems[i];

        const int ypos = i * mItemHeight;
        const int xpos = item.GetLevel() * kLevelOffset + kBaseLevel * kLevelOffset;
        const bool selected = &item == mSelected;
        const bool hovered  = &item == mHovered;
        const QRect box(0, ypos, window_width, mItemHeight);
        RenderTreeItem(item, box, palette, painter, selected, hovered);

        // draw the connecting lines between child/parent items.
        QPen line;
        line.setColor(palette.color(QPalette::Shadow));
        painter.setPen(line);
        painter.drawLine(xpos - 15, ypos + mItemHeight/2,
                         xpos - 3,  ypos + mItemHeight/2);
        if (item.GetLevel() > 0)
        {
            painter.drawLine(xpos - 15, ypos + mItemHeight/2,
                             xpos - 15, ypos - 1);
        }
    }

    // widget coordinates but the units match those of
    // our backbuffer so we can use these offsets
    // directly in the backbuffer space
    const auto& drag_offset = mDragPoint - mDragStart;

    if (mDragging && std::abs(drag_offset.y()) >= 1)
    {
        for (size_t i=0; i<mItems.size(); ++i)
        {
            const auto& item = mItems[i];
            if (&item != mSelected)
                continue;

            // compute the original position for the item's row in the backbuffer.
            // then translate that to the viewport and offset by the current
            // drag amount in order to have the position of the drag item
            // in the widgdet's coordinate space.
            const int ypos  = drag_offset.y() + i * mItemHeight;
            const int xpos  = 0; //drag_offset.x(); looks silly since we're clipping the item visually so limit to y axis

            // figure out where the item being dragged to would land
            // and then indicate it.
            const auto landing_index = (ypos + (mItemHeight / 2)) / mItemHeight;
            if (landing_index < mItems.size())
            {
                QPen line;
                line.setWidth(1);
                line.setColor(QColorConstants::White);

                // hightlight the potential new parent
                const auto& parent = mItems[landing_index];
                const int ypos = landing_index * mItemHeight;
                const int xpos = 0;
                const QRect rc(xpos, ypos, window_width, mItemHeight);
                //RenderTreeItem(item, rc, palette, painter, false, true);
                painter.setPen(line);
                painter.drawRect(rc);
            }
            painter.setOpacity(0.5f);
            RenderTreeItem(item, QRect (xpos, ypos, window_width, mItemHeight),
                palette, painter, true, false);
        }

    }

    // blit the offscreen buffer to the actual viewport widget
    // offset by the current scroll delta.
    {
        QPainter painter(viewport());
        const QRect dst(viewport()->rect());
        const QRect src(-mXOffset, -mYOffset, window_width, window_height);
        painter.drawPixmap(dst, buffer, src);

        // todo: this should probably be done elsewhere..
        const auto vertical_excess = render_height - window_height;
        const auto horizontal_excess = render_width - window_width;

        verticalScrollBar()->setRange(0, vertical_excess);
        verticalScrollBar()->setPageStep(window_height);
        horizontalScrollBar()->setRange(0, horizontal_excess);
        horizontalScrollBar()->setPageStep(window_width);
    }
}

void TreeWidget::mouseMoveEvent(QMouseEvent* mickey)
{
    if (mDragging)
    {
        mDragPoint = mickey->pos();
        viewport()->update();
        return;
    }

    const auto mouse_y = mickey->pos().y() - mYOffset;
    const auto index   = mouse_y / mItemHeight;

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

    const auto mouse_y = mickey->pos().y() - mYOffset;
    const auto mouse_x = mickey->pos().x() - mXOffset;
    //DEBUG("Mouse press ət %1,%2", mouse_x, mouse_y);

    // every row item has the same fixed height
    // so to determine the row that is clicked is easy
    const auto index = mouse_y / mItemHeight;
    if (index >= mItems.size())
    {
        // select nada.
        mSelected = nullptr;
    }
    else
    {
        mSelected  = &mItems[index];
        mDragging  = true;
        mDragStart = mickey->pos();
        mDragPoint = mickey->pos();
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
        return;

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
        DEBUG("Item '%1' dragged onto '%2'", item.GetText(), target.GetText());
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
    const QSize& size = resize->size();

    DEBUG("Resize event: %1x%2", size.width(), size.height());

    // todo: compute the scroll bars again
}

void TreeWidget::scrollContentsBy(int dx, int dy)
{
    DEBUG("Scroll event dx=%1 dy=%2", dx,dy);
    mXOffset += dx;
    mYOffset += dy;

    viewport()->update();
}

} // namespace
