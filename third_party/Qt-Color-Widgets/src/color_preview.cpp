/**
 * \file
 *
 * \author Mattia Basaglia
 *
 * \copyright Copyright (C) 2013-2017 Mattia Basaglia
 * \copyright Copyright (C) 2014 Calle Laakkonen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "color_preview.hpp"

#include <QStylePainter>
#include <QStyleOptionFrame>
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>

namespace color_widgets {

class ColorPreview::Private
{
public:
    QColor col; ///< color to be viewed
    QColor comparison; ///< comparison color
    QBrush back;///< Background brush, visible on a transparent color
    DisplayMode display_mode; ///< How the color(s) are to be shown
    bool has_color = true;
    bool sRGB = false;
    QString placeholder_text;
    Private() : col(Qt::red), back(Qt::darkGray, Qt::DiagCrossPattern), display_mode(NoAlpha)
    {}
};

ColorPreview::ColorPreview(QWidget *parent) :
    QWidget(parent), p(new Private)
{
    p->back.setTexture(QPixmap(QStringLiteral(":/color_widgets/alphaback.png")));
}

ColorPreview::~ColorPreview()
{
    delete p;
}

void ColorPreview::setBackground(const QBrush &bk)
{
    p->back = bk;
    update();
}

QBrush ColorPreview::background() const
{
    return p->back;
}

ColorPreview::DisplayMode ColorPreview::displayMode() const
{
    return p->display_mode;
}

void ColorPreview::setDisplayMode(DisplayMode m)
{
    p->display_mode = m;
    update();
}

void ColorPreview::setPlaceholderText(QString text)
{
    p->placeholder_text = text;
    update();
}

bool ColorPreview::hasColor() const
{
    return p->has_color;
}

void ColorPreview::setHasColor(bool on_off)
{
    p->has_color = on_off;
    update();
}

void ColorPreview::clearColor()
{
    p->has_color = false;
    update();
}


bool ColorPreview::get_sRGB_flag() const
{
    return p->sRGB;
}

void ColorPreview::set_sRGB_flag(bool on_off)
{
    p->sRGB = on_off;
}

QColor ColorPreview::color() const
{
    return p->col;
}

QColor ColorPreview::comparisonColor() const
{
    return p->comparison;
}
QString ColorPreview::placeholderText() const
{
    return p->placeholder_text;
}

QSize ColorPreview::sizeHint() const
{
    return QSize(24,24);
}

void ColorPreview::paint(QPainter &painter, QRect rect) const
{
    QColor c1, c2;
    switch(p->display_mode) {
    case DisplayMode::NoAlpha:
        c1 = c2 = p->col.rgb();
        break;
    case DisplayMode::AllAlpha:
        c1 = c2 = p->col;
        break;
    case DisplayMode::SplitAlpha:
        c1 = p->col.rgb();
        c2 = p->col;
        break;
    case DisplayMode::SplitColor:
        c1 = p->comparison;
        c2 = p->col;
        break;
    }

    QStyleOptionFrame panel;
    panel.initFrom(this);
    panel.lineWidth = 2;
    panel.midLineWidth = 0;
    panel.state |= QStyle::State_Sunken;
    style()->drawPrimitive(QStyle::PE_Frame, &panel, &painter);
    QRect r = style()->subElementRect(QStyle::SE_FrameContents, &panel);

    if (hasFocus())
    {
        QStyleOptionFocusRect opt;
        opt.initFrom(this);
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &opt, &painter);
    }

    r.adjust(3, 3, -3, -3);
    painter.setClipRect(r);

    if (!p->has_color)
    {
        style()->drawItemText(&painter, r, Qt::AlignLeft | Qt::AlignVCenter, palette(), isEnabled(), p->placeholder_text);
        return;
    }

    if ( c1.alpha() < 255 || c2.alpha() < 255 )
        painter.fillRect(r, p->back);

    int w = r.width() / 2;
    int h = r.height();
    painter.fillRect(r.x() + 0, r.y(), w, h, c1);
    painter.fillRect(r.x() + w, r.y(), w, h, c2);
}

void ColorPreview::setColor(const QColor &c)
{
    p->col = c;
    p->has_color = true;
    update();
    Q_EMIT colorChanged(c);
}

void ColorPreview::setComparisonColor(const QColor &c)
{
    p->comparison = c;
    update();
}

void ColorPreview::paintEvent(QPaintEvent *)
{
    QStylePainter painter(this);

    paint(painter, geometry());
}

void ColorPreview::resizeEvent(QResizeEvent *)
{
    update();
}

void ColorPreview::mouseReleaseEvent(QMouseEvent * ev)
{
    if ( QRect(QPoint(0,0),size()).contains(ev->pos()) )
        Q_EMIT clicked();
}

void ColorPreview::mouseMoveEvent(QMouseEvent *ev)
{

    if ( ev->buttons() &Qt::LeftButton && !QRect(QPoint(0,0),size()).contains(ev->pos()) )
    {
        QMimeData *data = new QMimeData;

        data->setColorData(p->col);

        QDrag* drag = new QDrag(this);
        drag->setMimeData(data);

        QPixmap preview(24,24);
        preview.fill(p->col);
        drag->setPixmap(preview);

        drag->exec();
    }
}

} // namespace color_widgets
