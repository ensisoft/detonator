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

#include "base/math.h"
#include "editor/gui/rangewidget.h"
#include "editor/app/eventlog.h"

namespace {
    constexpr auto Margin = 5.0f;
} // namespace

namespace gui
{
RangeWidget::RangeWidget(QWidget* parent) : QWidget(parent)
{
    setFocusPolicy(Qt::TabFocus);

    // need to enable mouse tracking in order to get mouse move events.
    setMouseTracking(true);
}

float RangeWidget::GetLo() const
{
    return mScale * std::pow(mLo, mExponent);
}
float RangeWidget::GetHi() const
{
    return mScale * std::pow(mHi, mExponent);
}
void RangeWidget::SetLo(float value)
{
    mLo = std::pow(value/mScale, 1.0f/mExponent);
}
void RangeWidget::SetHi(float value)
{
    mHi = std::pow(value/mScale, 1.0f/mExponent);
}

void RangeWidget::paintEvent(QPaintEvent* event)
{
    const QPalette& palette = this->palette();

    const auto width = this->width();
    const auto height = this->height();
    const auto left = Margin;
    const auto top = Margin;
    const auto handle_size = height - 2.0f*Margin;
    const auto handle_half  = handle_size * 0.5;

    const auto range_width = width - 2.0f*Margin - handle_size;
    const auto range_start = (width - range_width) * 0.5f;

    QRectF lo_handle(range_start + mLo * range_width - handle_half,
                           top, handle_size, handle_size);
    QRectF hi_handle(range_start + mHi * range_width - handle_half,
                           top, handle_size, handle_size);

    const QRectF range(range_start + mLo * range_width - handle_half,
                       top, range_width * (mHi - mLo), handle_size);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // fill background with some color
    //painter.fillRect(rect(), palette.color(QPalette::Window));
    // draw the line segment that illustrates the range for the slider knobs
    painter.drawLine(range_start, height*0.5,
                     range_start + range_width, height*0.5f);

    // draw the line segment between the knobs/handles
    QPen p;
    p.setWidth(4.0f);
    p.setColor(palette.color(QPalette::Active, QPalette::Highlight));
    painter.setPen(p);
    painter.drawLine(range_start + mLo * range_width, height*0.5,
                     range_start + mHi * range_width, height*0.5);

    // draw both handles, first drop shadow
    painter.fillRect(lo_handle, palette.color(QPalette::Active, QPalette::Shadow));
    painter.fillRect(hi_handle, palette.color(QPalette::Active, QPalette::Shadow));
    lo_handle.adjust(1.0f, 1.0f, -1.0f, -1.0f);
    hi_handle.adjust(1.0f, 1.0f, -1.0f, -1.0f);
    painter.fillRect(lo_handle, palette.color(QPalette::Active, QPalette::Light));
    painter.fillRect(hi_handle, palette.color(QPalette::Active, QPalette::Light));
}

void RangeWidget::mouseMoveEvent(QMouseEvent* mickey)
{
    if (mDragging == Dragging::None)
        return;

    const auto width = this->width();
    const auto height = this->height();
    const auto left = Margin;
    const auto top = Margin;
    const auto handle_size = height - 2.0f*Margin;
    const auto handle_half  = handle_size * 0.5;

    const auto range_width = width - 2.0f*Margin - handle_size;
    const auto range_start = (width - range_width) * 0.5f;

    const auto pos = mickey->pos();
    const auto delta = pos - mDragStart;
    const auto dx = delta.x()  / range_width;

    // lo cannot go higher than hi and hi cannot go lower than lo
    if (mDragging == Dragging::Lo)
        mLo = math::clamp(0.0f, mHi, mLo + dx);
    else if (mDragging == Dragging::Hi)
        mHi = math::clamp(mLo, 1.0f, mHi + dx);
    else if (mDragging == Dragging::Range)
    {
        if (mLo + dx > 0.0f && mHi + dx < 1.0f)
        {
            mLo += dx;
            mHi += dx;
        }
    }
    else if (mDragging == Dragging::NotSure)
    {
        if (dx > 0.0f)
            mDragging = Dragging::Hi;
        else mDragging = Dragging::Lo;
    }

    mDragStart = pos;

    update();

    emit RangeChanged(GetLo(), GetHi());

}
void RangeWidget::mousePressEvent(QMouseEvent* mickey)
{
    const auto width = this->width();
    const auto height = this->height();
    const auto left = Margin;
    const auto top = Margin;
    const auto handle_size = height - 2.0f*Margin;
    const auto handle_half  = handle_size * 0.5;

    const auto range_width = width - 2.0f*Margin - handle_size;
    const auto range_start = (width - range_width) * 0.5f;

    const QRectF lo_handle(range_start + mLo * range_width - handle_half,
                           top, handle_size, handle_size);
    const QRectF hi_handle(range_start + mHi * range_width - handle_half,
                           top, handle_size, handle_size);
    const QRectF range(range_start + mLo * range_width - handle_half,
                       top, range_width * (mHi - mLo), handle_size);

    if (math::equals(mLo, mHi))
    {
        if (math::equals(mLo, 0.0f))
            mDragging = Dragging::Hi;
        else if (math::equals(mHi, 1.0f))
            mDragging = Dragging::Lo;
        else mDragging = Dragging::NotSure;
    }
    else if (lo_handle.contains(mickey->pos()))
        mDragging = Dragging::Lo;
    else if (hi_handle.contains(mickey->pos()))
        mDragging = Dragging::Hi;
    else if (range.contains(mickey->pos()))
        mDragging = Dragging::Range;

    mDragStart = mickey->pos();

    update();

    //DEBUG("Dragging %1", mDragging);
}

void RangeWidget::mouseReleaseEvent(QMouseEvent* mickey)
{
    mDragging = Dragging::None;
}
void RangeWidget::enterEvent(QEvent*)
{
    mHovered = true;
}
void RangeWidget::leaveEvent(QEvent*)
{
    mHovered = false;
}
void RangeWidget::keyPressEvent(QKeyEvent* key)
{

}
void RangeWidget::resizeEvent(QResizeEvent* resize)
{

}

void RangeWidget::focusInEvent(QFocusEvent* focus)
{
    mFocused = true;
}
void RangeWidget::focusOutEvent(QFocusEvent* focus)
{
    mFocused = false;
}

} // namespace
