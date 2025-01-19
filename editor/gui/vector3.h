// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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
#  include <QObject>
#  include <QWidget>
#  include <QString>
#  include <QtUiPlugin/QDesignerExportWidget>
#  include <glm/vec3.hpp>
#include "warnpop.h"

namespace Ui {
    class Vector3;
}

namespace gui
{
    class DESIGNER_PLUGIN_EXPORT Vector3 : public QWidget
    {
        Q_OBJECT
        Q_PROPERTY(qreal min    READ minimum WRITE setMinimum DESIGNABLE true)
        Q_PROPERTY(qreal max    READ maximum WRITE setMaximum DESIGNABLE true)
        Q_PROPERTY(qreal x      READ x       WRITE setX       DESIGNABLE true)
        Q_PROPERTY(qreal y      READ y       WRITE setY       DESIGNABLE true)
        Q_PROPERTY(qreal z      READ z       WRITE setZ       DESIGNABLE true)
        Q_PROPERTY(bool    labels READ getShowLabels WRITE setShowLabels DESIGNABLE true)
        Q_PROPERTY(QString suffix READ getSuffix     WRITE setSuffix     DESIGNABLE true)

    public:
        Vector3(QWidget* parent);
       ~Vector3();

        qreal x() const
        { return GetX(); }
        qreal y() const
        { return GetY(); }
        qreal z() const
        { return GetZ(); }

        void setX(qreal x)
        { SetX(x); }
        void setY(qreal y)
        { SetY(y); }
        void setZ(qreal z)
        { SetZ(z); }

        void SetX(float x);
        void SetY(float y);
        void SetZ(float z);

        float GetX() const;
        float GetY() const;
        float GetZ() const;

        glm::vec3 GetVec3() const;
        glm::vec3 GetValue() const;
        void SetVec3(const glm::vec3& value);
        void SetValue(const glm::vec3& value);

        qreal minimum() const;
        qreal maximum() const;
        void setMinimum(qreal min);
        void setMaximum(qreal max);

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

        bool getShowLabels() const;
        void setShowLabels(bool val);

        QString getSuffix() const;
        void setSuffix(QString suffix);

    signals:
        void ValueChanged(const Vector3* vector);
    private slots:
        void on_X_valueChanged(double);
        void on_Y_valueChanged(double);
        void on_Z_valueChanged(double);

    private:
        Ui::Vector3* mUI = nullptr;
        bool mLabelsVisible = true;
    };
} // namespace