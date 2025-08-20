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

#define LOGTAG "widget"

#include "config.h"

#include "warnpush.h"
#  include <QApplication>
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
    constexpr auto HorizontalMargin = 5; // left/right margin
    constexpr auto VerticalMargin   = 5; // top/bottom margin
    constexpr auto TimelineHeight   = 15;
    constexpr auto RulerHeight      = 40;
    constexpr auto PixelsToSecond   = 20;
    constexpr auto PointIconSize    = 40;

    QPixmap ToGrayscale(const QPixmap& pixmap)
    {
        QImage img = pixmap.toImage();
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


} // namespace
namespace  gui
{

TimelineWidget::TimelineWidget(QWidget* parent) : QAbstractScrollArea(parent)
{
    setFrameShape(QAbstractScrollArea::Shape::NoFrame);
    // need to enable mouse tracking in order to get mouse move events.
    setMouseTracking(true);
}

void TimelineWidget::Rebuild()
{
    const QString& id = mSelectedItem ? mSelectedItem->id : "";

    mHoveredItem  = nullptr;
    mSelectedItem = nullptr;

    mTimelines.clear();
    mModel->Fetch(&mTimelines);

    for (auto& timeline : mTimelines)
    {
        for (size_t i=0; i<timeline.GetNumItems(); ++i)
        {
            auto& item = timeline.GetItem(i);
            if (item.id == id)
            {
                mSelectedItem = &item;
                break;
            }
        }
    }

    viewport()->update();

    ComputeVerticalScrollbars();
}

void TimelineWidget::Update()
{
    viewport()->update();
}

void TimelineWidget::Repaint()
{
    viewport()->repaint();
}

float TimelineWidget::MapToSeconds(QPoint pos) const
{
    // only the horizontal position matters here.
    pos.setY(RulerHeight + 20);

    const auto hotspot = TestHotSpot(pos);
    if (hotspot == HotSpot::LeftMargin)
        return 0.0f;
    else if (hotspot == HotSpot::RightMargin)
        return 1.0f;

    pos = MapFromView(pos);

    const auto point = static_cast<float>(pos.x());
    const auto pixels_per_one_second = GetPixelsPerSecond();
    const auto seconds = point / pixels_per_one_second;

    return seconds;
}

const TimelineWidget::TimelineItem* TimelineWidget::SelectItem(const app::AnyString& itemId)
{
    for (auto& timeline : mTimelines)
    {
        for (size_t i=0; i<timeline.GetNumItems(); ++i)
        {
            auto& item = timeline.GetItem(i);
            if (item.id == itemId)
            {
                mSelectedItem = &item;
                return mSelectedItem;
            }
        }
    }
    return nullptr;
}

void TimelineWidget::paintEvent(QPaintEvent* event)
{
    //const QPalette& palette = this->palette();
    const auto& palette = QApplication::palette();

    const auto viewport_width  = viewport()->width();
    const auto viewport_height = viewport()->height();

    const auto timeline_row_height   = TimelineHeight + 2 * VerticalMargin;
    const auto pixels_per_one_second = GetPixelsPerSecond();
    const auto content_width_pixels  = std::abs(mXOffset) + viewport_width - 2 * HorizontalMargin;
    const auto content_width_seconds = content_width_pixels / pixels_per_one_second;
    const auto timeline_count        = mTimelines.size();
    const auto ruler_tick_count     = std::min(mDuration, content_width_seconds);

    auto color_group = QPalette::ColorGroup::Active;
    if (mFreezeItems)
        color_group = QPalette::ColorGroup::Disabled;
    else if (!hasFocus())
        color_group = QPalette::ColorGroup::Inactive;

    QPen selection_pen;
    selection_pen.setWidth(2.0f);
    selection_pen.setColor(QColor(0x0, 0xff, 0x00, 0xff));

    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(viewport()->rect(), palette.color(color_group, QPalette::Base));

    // paint order
    // draw timeline visualization
    // draw time spans
    // draw time points
    // draw ruler + time bullet

    for (size_t timeline_index=0; timeline_index<timeline_count; ++timeline_index)
    {
        const auto& timeline = mTimelines[timeline_index];

        QRect timeline_box(0, timeline_index * timeline_row_height, content_width_pixels, TimelineHeight);
        timeline_box.translate(0, VerticalMargin);
        timeline_box.translate(mXOffset, mYOffset);
        timeline_box.translate(HorizontalMargin, RulerHeight);

        // indicate the timeline the mouse is hovered on unless there's
        // an item we're hovering on.
        QColor color;
        if (timeline_index == mHoveredTimeline && !mHoveredItem)
            color = palette.color(color_group, QPalette::AlternateBase);
        else color = palette.color(color_group, QPalette::Base);

        p.fillRect(timeline_box, color);
    }

    // draw spans
    for (size_t timeline_index=0; timeline_index<timeline_count; ++timeline_index)
    {
        const auto& timeline = mTimelines[timeline_index];
        for (size_t item_index=0; item_index<timeline.GetNumItems(); ++item_index)
        {
            const auto& item = timeline.GetItem(item_index);
            if (item.type != TimelineItem::Type::Span)
                continue;

            QRect span_box(item.starttime * pixels_per_one_second * mDuration,
                timeline_index * timeline_row_height,
                item.duration * pixels_per_one_second * mDuration, TimelineHeight);
            span_box.translate(0, VerticalMargin);
            span_box.translate(mXOffset, mYOffset);
            span_box.translate(HorizontalMargin, RulerHeight);

            QColor box_color = item.color;
            if (mFreezeItems)
                box_color = Qt::lightGray;
            else if (mHoveredItem == &item && mSelectedItem != &item)
                box_color.setAlpha(255);

            if (mSelectedItem == &item && mDragging)
                span_box = span_box.adjusted(-2, -2, 4, 4);

            QPainterPath path;
            path.addRoundedRect(span_box, 5, 5);
            p.fillPath(path, box_color);

            if (mSelectedItem == &item && !mFreezeItems)
            {
                p.setPen(selection_pen);
                p.drawPath(path);
            }
        }
    }

    // draw timeline labels.
    for (size_t timeline_index=0; timeline_index < timeline_count; ++timeline_index)
    {
        QRect timeline_box(HorizontalMargin, timeline_index * timeline_row_height + mYOffset + RulerHeight,
                viewport_width, timeline_row_height);

        QPen pen;
        pen.setColor(palette.color(color_group, QPalette::HighlightedText));
        p.setPen(pen);
        p.drawText(timeline_box, Qt::AlignVCenter | Qt::AlignHCenter,
            mTimelines[timeline_index].GetName());
    }


    // draw points
    for (size_t timeline_index=0; timeline_index<timeline_count; ++timeline_index)
    {
        const auto& timeline = mTimelines[timeline_index];
        for (size_t item_index=0; item_index<timeline.GetNumItems(); ++item_index)
        {
            const auto& item = timeline.GetItem(item_index);
            if (item.type != TimelineItem::Type::Point)
                continue;

            QRect point_box(item.starttime * pixels_per_one_second * mDuration,
                timeline_index * timeline_row_height,
                PointIconSize, PointIconSize);
            point_box.translate(0, timeline_row_height * 0.5);
            point_box.translate(mXOffset, mYOffset);
            point_box.translate(HorizontalMargin, RulerHeight);
            point_box.translate(-PointIconSize*0.5, -PointIconSize*0.5);

            if (mSelectedItem == &item && mDragging)
                point_box = point_box.adjusted(-2, -2, 4, 4);

            p.fillRect(point_box, item.color);
            p.drawPixmap(point_box, mFreezeItems ? ToGrayscale(item.icon) : item.icon);

            if (mSelectedItem == &item)
            {
                p.setPen(selection_pen);
                p.drawRect(point_box);
            }
        }
    }

    QPen ruler_line_pen;
    ruler_line_pen.setColor(palette.color(color_group, QPalette::Text));

    QFont ruler_font;
    ruler_font.setPixelSize(8);
    QFontMetrics ruler_font_metrics(ruler_font);

    // draw the timeline ruler on top, if we have a yoffset
    // we must mask out the stuff underneath.
    if (std::abs(mYOffset))
    {
        p.fillRect(QRect(0, 0, viewport_width, RulerHeight),
            palette.color(color_group, QPalette::Base));
    }
    p.setFont(ruler_font);
    p.setPen(ruler_line_pen);
    p.drawLine(0 + mXOffset + HorizontalMargin, 20, content_width_pixels, 20);
    for (size_t tick=0; tick<=ruler_tick_count; ++tick)
    {
        const auto x = tick * pixels_per_one_second + mXOffset + HorizontalMargin;
        const auto y = 20;
        p.drawLine(x, y, x, y+10);

        const auto& legend = QString("%1s").arg(tick);
        const auto len = ruler_font_metrics.horizontalAdvance(legend);
        p.drawText(x-len*0.5, 15, legend);

        const auto tick_fraction_count = 10u;
        const auto pixels_per_frac_second = pixels_per_one_second / tick_fraction_count;
        if (pixels_per_one_second == 0)
            continue;
        if (tick == ruler_tick_count)
            continue;

        for (unsigned fraction_tick=0; fraction_tick<tick_fraction_count; ++fraction_tick)
        {
            const auto x0 = x + fraction_tick * pixels_per_frac_second;
            const auto y0 = y;
            p.drawLine(x0, y0, x0, y0+5);
        }
    }

    // visualize the current time
    QPixmap bullet("icons:bullet.png");
    constexpr auto BulletSize = 16; // px
    p.drawPixmap(mCurrentTime * pixels_per_one_second + mXOffset + HorizontalMargin - BulletSize/2, 25, bullet);

    if (mAlignmentBar)
    {
        const auto bar_position = mAlignmentBarTime * pixels_per_one_second * mDuration + mXOffset + HorizontalMargin;
        QPen pen;
        pen.setColor(palette.color(QPalette::Text));
        p.setPen(pen);
        p.drawLine(bar_position, 0, bar_position, viewport()->height());
    }


    if (hasFocus())
    {
        QPen pen;
        pen.setWidth(1.0);
        pen.setColor(palette.color(color_group, QPalette::Highlight));
        p.setPen(pen);

        auto rect = viewport()->rect();
        rect.translate(1, 1);
        rect.setWidth(rect.width() - 2);
        rect.setHeight(rect.height() -2);
        p.drawRect(rect);
    }
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* mickey)
{
    mAlignmentBar = false;

    if (mDragging)
    {
        const auto& pos = mickey->pos();
        const auto pixels_per_one_second = GetPixelsPerSecond();

        // convert drag coordinates to normalized positions.
        const auto& drag_offset = pos - mDragPoint;
        const auto drag_seconds = drag_offset.x() / (float)pixels_per_one_second;
        const auto drag_normalized = drag_seconds / mDuration;
        float moved_start = 0.0f;
        float moved_end   = 0.0f;
        if (mDraggingFromStart)
        {
            // move the start time only. (grows to the left)
            moved_start = mSelectedItem->starttime + drag_normalized;
            moved_end = mSelectedItem->starttime + mSelectedItem->duration;
        }
        else if (mDraggingFromEnd)
        {
            // move the end time only. ( grows to the right)
            moved_start = mSelectedItem->starttime;
            moved_end   = mSelectedItem->starttime + mSelectedItem->duration + drag_normalized;
        }
        else
        {
            // move start and end time, i.e. the whole item.
            moved_start = mSelectedItem->starttime + drag_normalized;
            moved_end = moved_start + mSelectedItem->duration;
        }
        moved_start = math::clamp(0.0f, 1.0f, moved_start);
        moved_end   = math::clamp(moved_start, 1.0f, moved_end);
        // compare against other timeline's items to see if there's an
        // alignment on the start or end time with some other item.
        for (size_t i=0; i<mTimelines.size(); ++i)
        {
            if (i == mSelectedTimeline)
                continue;
            const auto& timeline = mTimelines[i];
            for (size_t i=0; i<timeline.GetNumItems(); ++i)
            {
                const auto& item = timeline.GetItem(i);
                const auto start = item.starttime;
                const auto end   = item.starttime + item.duration;
                const auto epsilon = 0.0001f;
                if (mDraggingFromStart || mDraggingFromEnd)
                {
                    const auto drag_pos = mDraggingFromStart ? moved_start : moved_end;

                    if (math::equals(drag_pos, start, epsilon))
                    {
                        mAlignmentBar = true;
                        mAlignmentBarTime = start;
                    }
                    else if (math::equals(drag_pos, end, epsilon))
                    {
                        mAlignmentBar = true;
                        mAlignmentBarTime = end;
                    }
                }
                else
                {
                    if (math::equals(moved_start , start , epsilon) ||
                        math::equals(moved_end, start, epsilon))
                    {
                        mAlignmentBar = true;
                        mAlignmentBarTime = start;
                    }
                    else if (math::equals(moved_start , end , epsilon) ||
                            math::equals(moved_end, end, epsilon))
                    {
                        mAlignmentBar = true;
                        mAlignmentBarTime = end;
                    }
                }
                if (mAlignmentBar)
                    break;
            }
            if (mAlignmentBar)
                break;
        }

        // lo and hi bound for the drag operation.
        float lo_bound = 0.0f;
        float hi_bound = 1.0f;

        if (mSelectedItem->type == TimelineItem::Type::Span)
        {
            // find the lower and upper (left and right) bounds
            // based on the items to the left and to the right
            // of the item we're currently moving.
            // the item we're currently moving/resizing cannot extend beyond
            // these boundary values.
            const auto &timeline = mTimelines[mSelectedTimeline];
            for (size_t i = 0; i < timeline.GetNumItems(); ++i)
            {
                const auto &item = timeline.GetItem(i);
                if (&item == mSelectedItem || item.type == TimelineItem::Type::Point)
                    continue;
                const auto start = item.starttime;
                const auto end = item.starttime + item.duration;
                if (start >= mSelectedItem->starttime)
                    hi_bound = std::min(hi_bound, start);
                if (end <= mSelectedItem->starttime)
                    lo_bound = std::max(lo_bound, end);
            }
        }
        const bool resizing  = mDraggingFromStart || mDraggingFromEnd;
        if (resizing)
        {
            const auto start = std::max(lo_bound, moved_start);
            const auto end   = std::min(hi_bound, moved_end);
            mSelectedItem->starttime = start;
            mSelectedItem->duration  = end - start;
        }
        else
        {
            const auto duration = mSelectedItem->duration;
            const auto wiggle_room = (hi_bound - lo_bound) - duration;
            const auto start = math::clamp(lo_bound, lo_bound + wiggle_room, moved_start);
            mSelectedItem->starttime = start;
        }
        emit SelectedItemDragged(mSelectedItem);

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

    mHoveredItem = nullptr;
    mHoveredTimeline = mTimelines.size();
    setCursor(Qt::ArrowCursor);

    const auto& hover_pos = MapFromView(mickey->pos());
    const auto pixels_per_one_second = GetPixelsPerSecond();

    const auto mouse_x = hover_pos.x();
    const auto mouse_y = hover_pos.y();

    const auto timeline_row_height = TimelineHeight + 2 * VerticalMargin;
    const auto timeline_row_index  = mouse_y / timeline_row_height;
    if (timeline_row_index >= mTimelines.size())
        return;

    unsigned selected_item_left = 0;
    unsigned selected_item_right = 0;

    auto& timeline = mTimelines[timeline_row_index];
    for (size_t i=0; i<timeline.GetNumItems(); ++i)
    {
        auto& item = timeline.GetItem(i);
        if (item.type == TimelineItem::Type::Span)
        {
            const auto span_left = item.starttime * pixels_per_one_second * mDuration;
            const auto span_right = span_left + item.duration * pixels_per_one_second * mDuration;
            if (mouse_x >= span_left && mouse_x <= span_right)
            {
                mHoveredItem = &item;
                selected_item_left = span_left;
                selected_item_right = span_right;
                break;
            }
        }
        else if (item.type == TimelineItem::Type::Point)
        {
            const auto point = item.starttime * pixels_per_one_second * mDuration;
            const auto point_left = point - PointIconSize / 2;
            const auto point_right = point + PointIconSize / 2;
            if (mouse_x >= point_left && mouse_x <= point_right)
            {
                mHoveredItem = &item;
                break;
            }
        }
    }
    if (mHoveredItem && mHoveredItem->type == TimelineItem::Type::Span)
    {
        const bool on_left_edge = mouse_x >= selected_item_left &&
                mouse_x <= selected_item_left + 10;
        const bool on_right_edge = mouse_x >= selected_item_right - 10 &&
                mouse_x < selected_item_right;
        if (on_left_edge || on_right_edge)
            setCursor(Qt::SizeHorCursor);
        else setCursor(Qt::DragMoveCursor);
    }
    else if (mHoveredItem && mHoveredItem->type == TimelineItem::Type::Point)
    {
        setCursor(Qt::DragMoveCursor);
    }
    mHoveredTimeline = timeline_row_index;
}
void TimelineWidget::mousePressEvent(QMouseEvent* mickey)
{
    if (mFreezeItems)
        return;

    if (mickey->button() != Qt::LeftButton)
        return;

    const auto hotspot = TestHotSpot(mickey->pos());
    if (hotspot != HotSpot::Content)
    {
        if (hotspot == HotSpot::Ruler)
            VERBOSE("Timeline ruler click detected. Not implemented.");
        return;
    }

    // schedule an update
    viewport()->update();

    mSelectedItem = nullptr;
    emit SelectedItemChanged(nullptr);

    const auto& click_pos  = MapFromView(mickey->pos());
    const auto pixels_per_one_second = GetPixelsPerSecond();

    const auto mouse_y = click_pos.y();
    const auto mouse_x = click_pos.x();

    const auto timeline_row_height = TimelineHeight + 2 * VerticalMargin;
    const auto timeline_row_index  = mouse_y / timeline_row_height;
    if (timeline_row_index >= mTimelines.size())
        return;

    auto &timeline = mTimelines[timeline_row_index];

    // hit test points first
    for (size_t i=0; i<timeline.GetNumItems(); ++i)
    {
        auto& item = timeline.GetItem(i);
        if (item.type != TimelineItem::Type::Point)
            continue;

        const auto point = item.starttime * pixels_per_one_second * mDuration;
        const auto point_left  = point - PointIconSize / 2;
        const auto point_right = point + PointIconSize / 2;
        if (mouse_x >= point_left && mouse_x <= point_right)
        {
            mSelectedItem      = &item;
            mSelectedTimeline  = timeline_row_index;
            mDragging          = true;
            mDragStart = mickey->pos();
            mDragPoint = mickey->pos();
            emit SelectedItemChanged(mSelectedItem);
            return;
        }
    }
    // hit test spans
    for (size_t i = 0; i < timeline.GetNumItems(); ++i)
    {
        auto &item = timeline.GetItem(i);
        if (item.type != TimelineItem::Type::Span)
            continue;

        const auto span_left = item.starttime * pixels_per_one_second * mDuration;
        const auto span_right = span_left + item.duration * pixels_per_one_second * mDuration;
        if (mouse_x >= span_left && mouse_x <= span_right)
        {
            mSelectedItem     = &item;
            mSelectedTimeline = timeline_row_index;
            mDragging         = true;
            mDragStart = mickey->pos();
            mDragPoint = mickey->pos();
            if (mouse_x >= span_left && mouse_x <= span_left + 10)
                mDraggingFromStart = true;
            else if (mouse_x >= span_right - 10 && mouse_x <= span_right)
                mDraggingFromEnd = true;
            emit SelectedItemChanged(mSelectedItem);
            return;
        }
    }
}

void TimelineWidget::mouseReleaseEvent(QMouseEvent* mickey)
{
    viewport()->update();

    mDraggingFromStart = false;
    mDraggingFromEnd   = false;
    mDragging = false;
}
void TimelineWidget::wheelEvent(QWheelEvent* wheel)
{
    const auto mods = wheel->modifiers();
    const QPoint& num_degrees = wheel->angleDelta() / 8;
    const QPoint& num_steps = num_degrees / 15;
    // only consider the wheel scroll steps on the vertical
    // axis for zooming.
    // if steps are positive the wheel is scrolled away from the user
    // and if steps are negative the wheel is scrolled towards the user.
    const int num_wheel_steps = num_steps.y();

    //DEBUG("Mouse wheel event with steps: %1", num_wheel_steps);

    for (int i=0; i<std::abs(num_wheel_steps); ++i)
    {
        if (mods & Qt::ControlModifier)
        {
            if (num_wheel_steps > 0)
                mZoomFactor += 0.1;
            else if (num_wheel_steps < 0 && mZoomFactor > 0.1)
                mZoomFactor -= 0.1;

            ComputeHorizontalScrollbars();
        }
        else if (verticalScrollBar()->isVisible())
        {
            const int scroll_rows_count = verticalScrollBar()->maximum();
            const int scroll_row_index  = verticalScrollBar()->value();
            const int timeline_row_height = TimelineHeight + 2 * VerticalMargin;

            int scroll_value = scroll_row_index;

            // scrolling upwards
            if (num_wheel_steps > 0)
                scroll_value = math::clamp(0, scroll_rows_count, scroll_row_index - 1);
            else if (num_wheel_steps < 0)
                scroll_value = math::clamp(0, scroll_rows_count, scroll_row_index + 1);

            QSignalBlocker s(verticalScrollBar());
            verticalScrollBar()->setValue(scroll_value);
            mYOffset = (scroll_value * timeline_row_height) * -1;
        }
    }
    viewport()->update();
}

void TimelineWidget::scrollContentsBy(int dx, int dy)
{
    // vertical scroll is in timeline rows and horizontal scroll
    // is in pixels.

    const auto timeline_row_height = TimelineHeight + 2 * VerticalMargin;

    mXOffset += dx;
    mYOffset += (dy * timeline_row_height);
    //DEBUG("Scroll event dx=%1 dy=%2 xoffset=%3 yoffset=%4", dx, dy, mXOffset, mYOffset);
    viewport()->update();
}

void TimelineWidget::enterEvent(QEvent*)
{

}
void TimelineWidget::leaveEvent(QEvent*)
{
    mHoveredItem = nullptr;
    // disabled for now as a hack solution to making
    // context menu work in animationtrackwidget.
    //mHoveredTimeline = mTimelines.size();

    viewport()->update();
}

void TimelineWidget::resizeEvent(QResizeEvent* event)
{
    ComputeHorizontalScrollbars();
    ComputeVerticalScrollbars();
}

void TimelineWidget::keyPressEvent(QKeyEvent* event)
{
    if(auto* item = GetSelectedItem())
    {
        const auto key = event->key();
        if (key == Qt::Key_Delete)
            emit DeleteSelectedItem(item);
    }
    QWidget::keyPressEvent(event);
}

void TimelineWidget::ComputeVerticalScrollbars()
{
    const unsigned viewport_height = viewport()->height();
    const unsigned visible_area_height = viewport_height - RulerHeight;
    const unsigned timeline_row_height = TimelineHeight + 2 * VerticalMargin;
    const unsigned visible_rows = visible_area_height / timeline_row_height;

    const unsigned current_rows = mTimelines.size();
    const unsigned scroll_rows  = visible_rows >= current_rows ? 0 : current_rows - visible_rows;

    QSignalBlocker s(verticalScrollBar());

    // compute vertical scroll
    if (scroll_rows)
    {
        if (verticalScrollBar()->maximum() == scroll_rows)
            return;

        verticalScrollBar()->setSingleStep(1);
        verticalScrollBar()->setValue(0);
        verticalScrollBar()->setMinimum(0);
        verticalScrollBar()->setMaximum(scroll_rows);
        verticalScrollBar()->setVisible(true);
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        mYOffset = 0;
    }
    else
    {
        verticalScrollBar()->setVisible(false);
        mXOffset = 0;
    }
}

void TimelineWidget::ComputeHorizontalScrollbars()
{
    const unsigned viewport_width = viewport()->width() - 2 * HorizontalMargin;
    const auto pixels_per_second = GetPixelsPerSecond();
    const auto content_width = mDuration * pixels_per_second;

    QSignalBlocker s(horizontalScrollBar());

    if (content_width > viewport_width)
    {
        const auto horizontal_excess = content_width - viewport_width + 10;
        if (horizontalScrollBar()->maximum() == horizontal_excess)
            return;

        horizontalScrollBar()->setValue(0);
        horizontalScrollBar()->setMinimum(0);
        horizontalScrollBar()->setMaximum(horizontal_excess);
        horizontalScrollBar()->setPageStep(horizontal_excess);
        horizontalScrollBar()->setVisible(true);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        mXOffset = 0;
    }
    else
    {
        horizontalScrollBar()->setVisible(false);
        mXOffset = 0;
    }
}

float TimelineWidget::GetPixelsPerSecond() const
{
    // create a mapping from time in seconds to pixels based on the
    // current viewport size and zoom factor.
    const auto viewport_width  = viewport()->width();
    const auto viewport_height = viewport()->height();

    const int timeline_width = viewport_width - 2 * HorizontalMargin; // leave 5px gap at both ends
    const float num_seconds_on_timeline = timeline_width / float(PixelsToSecond);
    const auto pixels_per_one_second = (num_seconds_on_timeline > mDuration
                                    ? timeline_width / mDuration : PixelsToSecond) * mZoomFactor;
    return pixels_per_one_second;
}

QPoint TimelineWidget::MapFromView(QPoint click_pos) const
{
    click_pos -= QPoint(HorizontalMargin, RulerHeight);
    click_pos -= QPoint(mXOffset, mYOffset);
    return click_pos;
}

TimelineWidget::HotSpot TimelineWidget::TestHotSpot(const QPoint& click_pos) const
{
    if (click_pos.y() <= RulerHeight)
        return HotSpot::Ruler;

    const auto viewport_width  = viewport()->width();
    const auto viewport_height = viewport()->height();
    if (click_pos.x() <= HorizontalMargin)
        return HotSpot::LeftMargin;
    if (click_pos.y() >= viewport_width - HorizontalMargin)
        return HotSpot::RightMargin;

    return HotSpot::Content;
}


} // namespace
