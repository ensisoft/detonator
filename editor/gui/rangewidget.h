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
    class RangeWidget : public QWidget
    {
        Q_OBJECT

    public:
        RangeWidget(QWidget* parent);

        float GetLo() const;
        float GetHi() const;
        void SetLo(float value);
        void SetHi(float value);

        void SetScale(float scale)
        { mScale = scale; }
        void SetExponent(float exponent)
        { mExponent = exponent; }
    signals:
        void RangeChanged(float lo, float hi);

    private:
        virtual void paintEvent(QPaintEvent* event) override;
        virtual void mouseMoveEvent(QMouseEvent* mickey) override;
        virtual void mousePressEvent(QMouseEvent* mickey) override;
        virtual void mouseReleaseEvent(QMouseEvent* mickey) override;
        virtual void enterEvent(QEvent*) override;
        virtual void leaveEvent(QEvent*) override;
        virtual void keyPressEvent(QKeyEvent* key) override;
        virtual void resizeEvent(QResizeEvent* resize) override;
        virtual void focusInEvent(QFocusEvent* focus) override;
        virtual void focusOutEvent(QFocusEvent* focus) override;
    private:
        float mScale = 0.0f;
        float mExponent = 1.0f;
        float mLo = 0.0f;
        float mHi = 1.0f;
        enum class Dragging {
            None, Lo, Hi, Range
        };
        QPoint mDragStart;
        Dragging mDragging = Dragging::None;
        bool mHovered = false;
        bool mFocused = false;
    };

} // namespace
