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
#  include <QPainterPath>
#  include <QPalette>
#  include <QBrush>
#  include <QFont>
#  include <QPen>
#  include <QDebug>
#include "warnpop.h"

#include <algorithm>

#include "base/math.h"
#include "editor/app/eventlog.h"
#include "editor/gui/timelinewidget.h"

namespace {
    const auto HorizontalMargin = 5;
    const auto VerticalMargin = 5;
    const auto TimelineHeight = 15;
    const auto RulerHeight = 40;
    const auto PixelsToSecond = 20;
} // namespace
namespace  gui
{

TimelineWidget::TimelineWidget(QWidget* parent) : QAbstractScrollArea(parent)
{
    // need to enable mouse tracking in order to get mouse move events.
    setMouseTracking(true);

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

void TimelineWidget::Rebuild()
{
    const QString& id = mSelected ? mSelected->id : "";

    mHovered  = nullptr;
    mSelected = nullptr;

    mTimelines.clear();
    mModel->Fetch(&mTimelines);

    for (auto& timeline : mTimelines)
    {
        for (size_t i=0; i<timeline.GetNumItems(); ++i)
        {
            auto& item = timeline.GetItem(i);
            if (item.id == id)
            {
                mSelected = &item;
                break;
            }
        }
    }

    viewport()->update();
}

void TimelineWidget::Update()
{
    viewport()->update();
}

void TimelineWidget::Repaint()
{
    viewport()->repaint();
}

float TimelineWidget::MapToSeconds(const QPoint& pos) const
{
    const unsigned window_width  = viewport()->width();
    const unsigned window_height = viewport()->height();
    // only interested in the x position which should be
    // within the widget's width.
    const float x = pos.x() - mXOffset - VerticalMargin;

    const int timeline_width = window_width - 2 * VerticalMargin; // leave 5px gap at both ends
    const float num_seconds_on_timeline = timeline_width / float(PixelsToSecond);
    const auto pixels_per_one_second = (num_seconds_on_timeline > mDuration
                                        ? timeline_width / mDuration : PixelsToSecond) * mZoomFactor;
    const float seconds = x / pixels_per_one_second;
    return seconds;
}

void TimelineWidget::paintEvent(QPaintEvent* event)
{
    const QPalette& palette = this->palette();

    const unsigned window_width  = viewport()->width();
    const unsigned window_height = viewport()->height();

    const int timeline_start = mXOffset + VerticalMargin;
    const int timeline_width = window_width - 2 * VerticalMargin; // leave 5px gap at both ends
    const float num_seconds_on_timeline = timeline_width / float(PixelsToSecond);
    const auto pixels_per_one_second = (num_seconds_on_timeline > mDuration
        ? timeline_width / mDuration : PixelsToSecond) * mZoomFactor;
    const auto num_second_ticks = timeline_width / pixels_per_one_second;

    const unsigned render_height = mTimelines.size() * TimelineHeight + RulerHeight;
    const unsigned render_width  = mDuration * pixels_per_one_second;

    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing);

    // draw the timeline meter at the top row.
    {
        QFont small;
        small.setPixelSize(8);
        QFontMetrics fm(small);

        QPen line;
        line.setColor(palette.color(QPalette::Text));

        p.setPen(line);
        p.drawLine(timeline_start, 20, timeline_start + render_width, 20);
        p.setFont(small);

        for (unsigned i=0; i<=num_second_ticks; ++i)
        {
            const auto x = timeline_start + i * pixels_per_one_second;
            const auto y = 20;
            p.drawLine(x, y, x, y+10);

            const auto& legend = QString("%1s").arg(i);
            const auto len = fm.horizontalAdvance(legend);
            p.drawText(x-len*0.5, 15, legend);

            const auto num_fractions = 10;
            const auto pixels_per_frac_second = pixels_per_one_second / num_fractions;
            if (pixels_per_one_second == 0)
                continue;

            for (int i=0; i<num_fractions; ++i)
            {
                const auto x0 = x + i * pixels_per_frac_second;
                const auto y0 = y;
                p.drawLine(x0, y0, x0, y0+5);
            }
        }
    }

    QFont font;
    p.setFont(font);

    // visualize the timelines and their items
    for (size_t i=0; i<mTimelines.size(); ++i)
    {
        const auto& timeline = mTimelines[i];
        const auto x = timeline_start;
        const auto y = RulerHeight + i * TimelineHeight + i * HorizontalMargin;
        const QRect box(x, y, render_width, TimelineHeight);

        if (i == mHoveredTimeline)
            p.fillRect(box, QColor(50, 50, 50));

        p.drawText(box, Qt::AlignVCenter | Qt::AlignHCenter, timeline.GetName());

        for (size_t i=0; i<timeline.GetNumItems(); ++i)
        {
            const auto& item = timeline.GetItem(i);
            const auto x0 = x + item.starttime * pixels_per_one_second * mDuration;
            const auto y0 = y;
            const QRect box(x0, y0, item.duration * pixels_per_one_second * mDuration, TimelineHeight);

            const auto group = mFreezeItems ? QPalette::ColorGroup::Disabled :
                    QPalette::ColorGroup::Active;
            QColor box_color;
            QColor pen_color;
            if (&item == mSelected)
            {
                pen_color = palette.color(group, QPalette::HighlightedText);
                box_color = palette.color(group, QPalette::Highlight);
            }
            else if (&item == mHovered && !mFreezeItems)
            {
                pen_color = palette.color(group, QPalette::HighlightedText);
                box_color = item.color;
                box_color.setAlpha(255);
            }
            else
            {
                pen_color = palette.color(group, QPalette::Text);
                box_color = mFreezeItems ? Qt::lightGray : item.color;
            }
            QPen pen;
            QPainterPath path;
            path.addRoundedRect(box, 5, 5);
            pen.setColor(pen_color);
            p.setPen(pen);
            p.fillPath(path, box_color);
            p.drawText(box, Qt::AlignVCenter | Qt::AlignHCenter, item.text);
        }
    }

    // visualize the current time
    QPixmap bullet("icons:bullet.png");
    p.drawPixmap(timeline_start + mCurrentTime * pixels_per_one_second - 8, 25, bullet);

    const auto vertical_excess = render_height - window_height;
    const auto horizontal_excess = render_width - window_width;
    verticalScrollBar()->setRange(0, vertical_excess);
    verticalScrollBar()->setPageStep(window_height);
    horizontalScrollBar()->setRange(0, horizontal_excess);
    horizontalScrollBar()->setPageStep(horizontal_excess);
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* mickey)
{
    const auto& pos = mickey->pos();
    const unsigned window_width  = viewport()->width();
    const int timeline_start = mXOffset + VerticalMargin;
    const int timeline_width = window_width - 2 * VerticalMargin; // leave 5px gap at both ends
    const float num_seconds_on_timeline = timeline_width / float(PixelsToSecond);
    const auto pixels_per_one_second = (num_seconds_on_timeline > mDuration
                                        ? timeline_width / mDuration : PixelsToSecond) * mZoomFactor;
    if (mDragging)
    {
        // convert drag coordinates to normalized positions.
        const auto& drag_offset = pos - mDragPoint;
        const auto drag_seconds = drag_offset.x() / (float)pixels_per_one_second;
        const auto drag_normalized = drag_seconds / mDuration;
        float moved_start = 0.0f;
        float moved_end   = 0.0f;
        if (mDraggingFromStart)
        {
            // move the start time only. (grows to the left)
            moved_start = mSelected->starttime + drag_normalized;
            moved_end = mSelected->starttime + mSelected->duration;
        }
        else if (mDraggingFromEnd)
        {
            // move the end time only. ( grows to the right)
            moved_start = mSelected->starttime;
            moved_end   = mSelected->starttime + mSelected->duration + drag_normalized;
        }
        else
        {
            // move start and end time, i.e. the whole item.
            moved_start = mSelected->starttime + drag_normalized;
            moved_end = moved_start + mSelected->duration;
        }
        moved_start = math::clamp(0.0f, 1.0f, moved_start);
        moved_end   = math::clamp(moved_start, 1.0f, moved_end);

        float lo_bound = 0.0f;
        float hi_bound = 1.0f;
        // find the lower and upper (left and right bounds)
        // based on the items to the left and to the right right
        // of the item we're currently moving.
        // the item we're currently moving/resizing cannot extend beyond
        // these boundary values.
        const auto& timeline = mTimelines[mSelectedTimeline];
        for (size_t i=0; i<timeline.GetNumItems(); ++i)
        {
            const auto& item = timeline.GetItem(i);
            if (&item == mSelected)
                continue;
            const auto start = item.starttime;
            const auto end   = item.starttime + item.duration;
            if (start >= mSelected->starttime)
                hi_bound = std::min(hi_bound, start);
            if (end <= mSelected->starttime)
                lo_bound = std::max(lo_bound, end);
        }
        const bool resizing  = mDraggingFromStart || mDraggingFromEnd;
        if (resizing)
        {
            const auto start = std::max(lo_bound, moved_start);
            const auto end   = std::min(hi_bound, moved_end);
            mSelected->starttime = start;
            mSelected->duration  = end - start;
        }
        else
        {
            const auto duration = mSelected->duration;
            const auto wiggle_room = (hi_bound - lo_bound) - duration;
            const auto start = math::clamp(lo_bound, lo_bound + wiggle_room, moved_start);
            mSelected->starttime = start;
        }
        emit SelectedItemDragged(mSelected);

        //DEBUG("Dragged %1 seconds, (start = %2, end = %3)", drag_seconds,
        //      mSelected->starttime * mDuration,
        //      (mSelected->starttime + mSelected->duration) * mDuration);

        mDragPoint = pos;
        // update immediately.
        viewport()->repaint();
        return;
    }

    // schedule update
    viewport()->update();

    mHovered = nullptr;
    mHoveredTimeline = mTimelines.size();
    setCursor(Qt::ArrowCursor);

    const auto mouse_y = pos.y() - mYOffset;
    const auto mouse_x = pos.x() - mXOffset;
    if (mouse_y <= RulerHeight)
        return;
    const auto height = TimelineHeight + HorizontalMargin;
    const auto index  = (mouse_y - RulerHeight) / height;
    if (index >= mTimelines.size())
        return;

    unsigned selected_item_left = 0;
    unsigned selected_item_right = 0;

    auto& timeline = mTimelines[index];
    for (size_t i=0; i<timeline.GetNumItems(); ++i)
    {
        auto& item = timeline.GetItem(i);
        const auto start = timeline_start + item.starttime * pixels_per_one_second * mDuration;
        const auto end   = start + item.duration * pixels_per_one_second * mDuration;
        if (mouse_x >= start && mouse_x <= end)
        {
            mHovered = &item;
            selected_item_left = start;
            selected_item_right = end;
            break;
        }
    }
    if (mHovered)
    {
        const bool on_left_edge = mouse_x >= selected_item_left &&
                mouse_x <= selected_item_left + 10;
        const bool on_right_edge = mouse_x >= selected_item_right - 10 &&
                mouse_x < selected_item_right;
        if (on_left_edge || on_right_edge)
            setCursor(Qt::SizeHorCursor);
        else setCursor(Qt::DragMoveCursor);
    }
    mHoveredTimeline = index;
}
void TimelineWidget::mousePressEvent(QMouseEvent* mickey)
{
    if (mFreezeItems)
        return;

    if (!(mickey->button() == Qt::LeftButton ||
          mickey->button() == Qt::RightButton))
        return;

    // schedule an update
    viewport()->update();

    mSelected = nullptr;
    emit SelectedItemChanged(nullptr);

    const auto& pos = mickey->pos();
    const unsigned window_width  = viewport()->width();
    const int timeline_start = mXOffset + VerticalMargin;
    const int timeline_width = window_width - 2 * VerticalMargin; // leave 5px gap at both ends
    const float num_seconds_on_timeline = timeline_width / float(PixelsToSecond);
    const auto pixels_per_one_second = (num_seconds_on_timeline > mDuration
                                        ? timeline_width / mDuration : PixelsToSecond) * mZoomFactor;
    const auto mouse_y = pos.y() - mYOffset;
    const auto mouse_x = pos.x() - mXOffset;

    if (mouse_y <= RulerHeight)
        return;
    const auto height = TimelineHeight + HorizontalMargin;
    const auto index  = (mouse_y - RulerHeight) / height;

    if (index >= mTimelines.size())
        return;

    unsigned selected_item_left = 0;
    unsigned selected_item_right = 0;

    auto &timeline = mTimelines[index];
    for (size_t i = 0; i < timeline.GetNumItems(); ++i)
    {
        auto &item = timeline.GetItem(i);
        const auto start = timeline_start + item.starttime * pixels_per_one_second * mDuration;
        const auto end = start + item.duration * pixels_per_one_second * mDuration;
        if (mouse_x >= start && mouse_x <= end)
        {
            mSelected = &item;
            mSelectedTimeline = index;
            selected_item_left = start;
            selected_item_right = end;
            break;
        }
    }

    if (mickey->button() == Qt::LeftButton && mSelected)
    {
        mDraggingFromStart = false;
        mDraggingFromEnd   = false;
        mDragging  = true;
        mDragStart = mickey->pos();
        mDragPoint = mickey->pos();
        if (mouse_x >= selected_item_left && mouse_x <= selected_item_left + 10)
            mDraggingFromStart = true;
        else if (mouse_x >= selected_item_right - 10 && mouse_x <= selected_item_right)
            mDraggingFromEnd = true;
    }
    if (mSelected)
        emit SelectedItemChanged(mSelected);
}
void TimelineWidget::mouseReleaseEvent(QMouseEvent* mickey)
{
    mDragging = false;
}
void TimelineWidget::wheelEvent(QWheelEvent* wheel)
{
    const auto mods = wheel->modifiers();
    if (mods != Qt::ControlModifier)
        return;

    const QPoint& num_degrees = wheel->angleDelta() / 8;
    const QPoint& num_steps = num_degrees / 15;
    // only consider the wheel scroll steps on the vertical
    // axis for zooming.
    // if steps are positive the wheel is scrolled away from the user
    // and if steps are negative the wheel is scrolled towards the user.
    const int num_zoom_steps = num_steps.y();

    //DEBUG("Zoom steps: %1", num_zoom_steps);

    for (int i=0; i<std::abs(num_zoom_steps); ++i)
    {
        if (num_zoom_steps > 0)
            mZoomFactor += 0.1;
        else if (num_zoom_steps < 0 && mZoomFactor > 0.1)
            mZoomFactor -= 0.1;
    }
    viewport()->update();
}

void TimelineWidget::scrollContentsBy(int dx, int dy)
{
    mXOffset += dx;
    mYOffset += dy;
    DEBUG("Scroll event dx=%1 dy=%2 xoffset=%3 yoffset=%4",
          dx, dy, mXOffset, mYOffset);
    viewport()->update();
}

void TimelineWidget::enterEvent(QEvent*)
{

}
void TimelineWidget::leaveEvent(QEvent*)
{
    mHovered = nullptr;
    // disabled for now as a hack solution to making
    // context menu work in animationtrackwidget.
    //mHoveredTimeline = mTimelines.size();

    viewport()->update();
}

} // namespace