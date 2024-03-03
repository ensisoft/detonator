// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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
#  include <QPaintEvent>
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
#  include <glm/vec2.hpp>
#include "warnpop.h"

#include <algorithm>


#include "curvewidget.h"

namespace gui
{

CurveWidget::CurveWidget(QWidget* parent)
  : QFrame(parent)
{}

int CurveWidget::GetInterpolation() const noexcept
{
    if (mFunction)
    {
        if (auto* ptr = dynamic_cast<MathInterpolationFunction*>(mFunction.get()))
            return static_cast<int>(ptr->GetInterpolation());
    }
    return -1;
}
void CurveWidget::SetInterpolation(int method)
{
    if (method == -1)
        ClearFunction();
    SetFunction(static_cast<math::Interpolation>(method));
}

void CurveWidget::SetFunction(math::Interpolation method)
{
    mFunction = std::make_unique<MathInterpolationFunction>(method);
    update();
}

void CurveWidget::paintEvent(QPaintEvent* paint)
{
    auto rect = this->rect();
    const auto& palette = this->palette();

    const auto padding = 1;
    rect.translate(padding, padding);
    rect.setHeight(rect.height()-2*padding);
    rect.setWidth(rect.width()-2*padding);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.fillRect(rect, QBrush(QColor(35, 35, 35, 255)));
    //painter.fillRect(rect, palette.color(QPalette::ColorGroup::Active, QPalette::Dark));

    const float width = rect.width();
    const float height = rect.height();
    const auto grid_rows = 10;
    const auto grid_cols = 10;

    QPen pen;
    pen.setColor(QColor(227, 227, 227, 53));
    painter.setPen(pen);

    // draw grid columns (vertical lines)
    for (int i=1; i<grid_cols; ++i)
    {
        const auto step = width / grid_cols;
        const auto xpos = padding + i * step;
        painter.drawLine(xpos, padding, xpos, padding+height);
    }

    // draw grid rows (horizontal lines)
    for (int i=1; i<grid_rows; ++i)
    {
        const auto step = height / grid_cols;
        const auto ypos = padding + i * step;
        painter.drawLine(padding, ypos, padding+width, ypos);
    }
    if (!mFunction)
    {
        QFrame::paintEvent(paint);
        return;
    }

    // looping with i<=samples -> 100 samples
    const auto samples = 99.0f;

    // plot with line segments.
    // todo: fix incorrect rendering on discontinuity
    // todo: use derivative to determine non-constant sampling rate.
    // todo: scale the curve more nicely in the window.

    /*
    float min = 0.0f;
    float max = 1.0f;
    for (int i=0; i<=samples; ++i)
    {
        const float t = i/samples;
        const float x = t;
        const float y = mFunction->SampleFunction(x);
        min = std::min(min, y);
        max = std::max(max, y);
    }
    const auto amplitude = max - min;
    // amp * scale = 1.0
    // scale = 1.0 / amp
    const auto scale = 1.0f / amplitude;
     */

    QFont small;
    small.setPixelSize(10);
    //pen.setColor(palette.color(QPalette::Text));
    pen.setColor(QColor(227, 227, 227, 200));
    painter.setPen(pen);
    painter.setFont(small);
    painter.drawText(15, 15, QString::fromStdString(mFunction->GetName()));

#if 0
    pen.setColor(QColor(227, 227, 227, 200));
    painter.setPen(pen);
    painter.drawLine(padding+width*0.2f,       padding+height-height*0.2f,
                     padding+width*0.8f,       padding+height-height*0.2f);
    painter.drawLine(padding+width*0.2f,       padding+height*0.2f,
                     padding+width*0.2f,       padding+height*0.8f);
#endif

    glm::vec2 previous;
    pen.setColor(QColor(227, 227, 227, 100));
    pen.setWidthF(2.0f);
    painter.setPen(pen);

    for (int i=0; i<=samples; ++i) {
        const float t = i/samples;
        const float x = t;
        const float y = mFunction->SampleFunction(x);

        glm::vec2 next;
        next.x = x;
        next.y = y;

        if (i >= 1)
        {
        painter.drawLine(padding+width*0.2f         + previous.x * width*0.6f,
                         padding+height-height*0.2f - previous.y * height*0.6f,
                         padding+width*0.2f         + next.x * width*0.6f,
                         padding+height-height*0.2f - next.y * height*0.6f);
        }
        previous = next;
    }

    QFrame::paintEvent(paint);
}

} // namespace