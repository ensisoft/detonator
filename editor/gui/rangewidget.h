// Copyright (C) 2020-2026 Sami Väisänen
// Copyright (C) 2020-2026 Ensisoft http://www.ensisoft.com
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
#  include <QtUiPlugin/QDesignerExportWidget>
#include "warnpop.h"

class QPalette;
class QPainter;
class QPaintEvent;
class QMouseEvent;
class QResizeEvent;
class QKeyEvent;

namespace gui
{
    class DESIGNER_PLUGIN_EXPORT RangeWidget : public QWidget
    {
        Q_OBJECT

        Q_PROPERTY(qreal lo READ GetLo WRITE SetLo DESIGNABLE true)
        Q_PROPERTY(qreal hi READ GetHi WRITE SetHi DESIGNABLE true)
        Q_PROPERTY(qreal exponent READ GetExponent WRITE SetExponent DESIGNABLE true)
        Q_PROPERTY(qreal scale    READ GetScale    WRITE SetScale    DESIGNABLE true)
    public:
        explicit RangeWidget(QWidget* parent = nullptr);

        float GetLo() const;
        float GetHi() const;
        void SetLo(float value);
        void SetHi(float value);

        void SetScale(float scale)
        { mScale = scale; }
        float GetScale() const
        { return mScale; }
        void SetExponent(float exponent)
        { mExponent = exponent; }
        float GetExponent() const
        { return mExponent; }
    signals:
        void RangeChanged(float lo, float hi);

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mouseMoveEvent(QMouseEvent* mickey) override;
        void mousePressEvent(QMouseEvent* mickey) override;
        void mouseReleaseEvent(QMouseEvent* mickey) override;
        void enterEvent(QEvent*) override;
        void leaveEvent(QEvent*) override;
        void keyPressEvent(QKeyEvent* key) override;
        void resizeEvent(QResizeEvent* resize) override;
        void focusInEvent(QFocusEvent* focus) override;
        void focusOutEvent(QFocusEvent* focus) override;

    private:
        float mScale = 0.0f;
        float mExponent = 1.0f;
        float mLo = 0.0f;
        float mHi = 1.0f;
        enum class Dragging {
            None, Lo, Hi, Range, NotSure
        };
        QPoint mMousePos;
        QPoint mDragStart;
        Dragging mDragging = Dragging::None;
        bool mHovered = false;
        bool mFocused = false;
    };

} // namespace
