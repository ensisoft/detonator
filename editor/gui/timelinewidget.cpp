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
#include "editor/gui/timelinewidget.h"

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

void TimelineWidget::paintEvent(QPaintEvent* event)
{
    const QPalette& palette = this->palette();

    const unsigned window_width  = viewport()->width();
    const unsigned window_height = viewport()->height();

    const int timeline_start = mXOffset + 5;
    const int timeline_width = window_width - 5 - 5; // leave 5px gap at both ends
    const float num_seconds_on_timeline = timeline_width / 20.0f;
    const auto pixels_per_one_second = (num_seconds_on_timeline > mDuration
        ? timeline_width / mDuration : 20) * mZoomFactor;
    const auto num_second_ticks = timeline_width / pixels_per_one_second;

    const auto margin = 5;
    const auto item_height = 15;
    const unsigned render_height = mTimelines.size() * item_height + 40;
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
        const auto y = 40 + i * item_height + i * margin;
        const QRect box(x, y, render_width, item_height);
        p.fillRect(box, QColor(50, 50, 50));

        for (size_t i=0; i<timeline.GetNumItems(); ++i)
        {
            const auto& item = timeline.GetItem(i);
            const auto x0 = x + item.starttime * pixels_per_one_second;
            const auto y0 = y;
            const QRect box(x0, y0, item.duration * pixels_per_one_second, item_height);

            QPainterPath path;
            path.addRoundedRect(box, 5, 5);
            p.fillPath(path, item.color);
        }
        p.drawText(box, Qt::AlignVCenter | Qt::AlignHCenter, timeline.GetName());
    }

    // visualize the current time
    QPixmap bullet("icons:bullet.png");
    p.drawPixmap(timeline_start + mCurrentTime * pixels_per_one_second - 8, 25, bullet);

    // visualize mouse position
    /*
    if (underMouse())
    {
        p.drawLine(mMouseX, 0, mMouseX, window_height);
    }
    */

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

    mMouseX = pos.x();

    viewport()->update();
}
void TimelineWidget::mousePressEvent(QMouseEvent* mickey)
{

}
void TimelineWidget::mouseReleaseEvent(QMouseEvent* mickey)
{

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
    DEBUG("Scroll event dx=%1 dy=%2", dx, dy);

    mXOffset += dx;
    mYOffset += dy;
    viewport()->update();
}


} // namespace