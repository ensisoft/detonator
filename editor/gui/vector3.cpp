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

#include "config.h"

#include "warnpush.h"
#  include "ui_vector3.h"
#include "warnpop.h"

#include "editor/gui/utility.h"
#include "editor/gui/vector3.h"

namespace gui
{

Vector3::Vector3(QWidget* parent) : QWidget(parent)
{
    mUI = new Ui::Vector3();
    mUI->setupUi(this);
}

Vector3::~Vector3() = default;

void Vector3::SetX(float x)
{
    gui::SetValue(mUI->X, x);
}
void Vector3::SetY(float y)
{
    gui::SetValue(mUI->Y, y);
}
void Vector3::SetZ(float z)
{
    gui::SetValue(mUI->Z, z);
}

float Vector3::GetX() const
{
    return gui::GetValue(mUI->X);
}
float Vector3::GetY() const
{
    return gui::GetValue(mUI->Y);
}
float Vector3::GetZ() const
{
    return gui::GetValue(mUI->Z);
}

glm::vec3 Vector3::GetVec3() const
{
    const float x = GetX();
    const float y = GetY();
    const float z = GetZ();
    return glm::vec3 { x, y, z };
}

glm::vec3 Vector3::GetValue() const
{
    return GetVec3();
}

void Vector3::SetVec3(const glm::vec3& value)
{
    gui::SetValue(mUI->X, value.x);
    gui::SetValue(mUI->Y, value.y);
    gui::SetValue(mUI->Z, value.z);
}
void Vector3::SetValue(const glm::vec3& value)
{
    gui::SetValue(mUI->X, value.x);
    gui::SetValue(mUI->Y, value.y);
    gui::SetValue(mUI->Z, value.z);
}

qreal Vector3::minimum() const
{
    return mUI->X->minimum();
}
qreal Vector3::maximum() const
{
    return mUI->X->maximum();
}

void Vector3::setMinimum(qreal min)
{
    mUI->X->setMinimum(min);
    mUI->Y->setMinimum(min);
    mUI->Z->setMinimum(min);
}
void Vector3::setMaximum(qreal max)
{
    mUI->X->setMaximum(max);
    mUI->Y->setMaximum(max);
    mUI->Z->setMaximum(max);
}

QSize Vector3::sizeHint() const
{
    return mUI->horizontalLayout->sizeHint();
}

QSize Vector3::minimumSizeHint() const
{
    return mUI->horizontalLayout->minimumSize();
}

bool Vector3::getShowLabels() const
{
    return mLabelsVisible;
}
void Vector3::setShowLabels(bool val)
{
    mLabelsVisible = val;
    mUI->label->setVisible(mLabelsVisible);
    mUI->label_2->setVisible(mLabelsVisible);
    mUI->label_3->setVisible(mLabelsVisible);
}

QString Vector3::getSuffix() const
{
    return mUI->X->suffix();
}
void Vector3::setSuffix(QString suffix)
{
    mUI->X->setSuffix(suffix);
    mUI->Y->setSuffix(suffix);
    mUI->Z->setSuffix(suffix);
}

void Vector3::on_X_valueChanged(double)
{
    emit ValueChanged(this);
}
void Vector3::on_Y_valueChanged(double)
{
    emit ValueChanged(this);
}
void Vector3::on_Z_valueChanged(double)
{
    emit ValueChanged(this);
}

} // namespace