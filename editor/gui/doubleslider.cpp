// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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
#  include "ui_doubleslider.h"
#include "warnpop.h"

#include "base/math.h"

#include "doubleslider.h"

namespace gui
{

DoubleSlider::DoubleSlider(QWidget* parent)
  : QWidget(parent)
{
    mUI.reset(new Ui::DoubleSlider);
    mUI->setupUi(this);
    setFocusPolicy(Qt::StrongFocus);
}
DoubleSlider::~DoubleSlider() = default;

void DoubleSlider::SetMinimum(double min)
{
    mMinimum = min;
    AdjustSlider();
}
void DoubleSlider::SetMaximum(double max)
{
    mMaximum = max;
    AdjustSlider();
}
void DoubleSlider::SetSingleStep(double step)
{
    mStep = step;
    AdjustSlider();
}

void DoubleSlider::SetValue(double value)
{
    mValue = value;
    AdjustSlider();
}

void DoubleSlider::on_slider_valueChanged(int value)
{
    const auto range = mMaximum - mMinimum;
    const auto steps = range / mStep;

    const auto val = value / steps * range + mMinimum;

    emit valueChanged(val);
}


void DoubleSlider::AdjustSlider()
{
    mValue = math::clamp(mMinimum, mMaximum, mValue);

    const auto range = mMaximum - mMinimum;
    const auto steps = range / mStep;

    QSignalBlocker s(mUI->slider);
    mUI->slider->setMinimum(0);
    mUI->slider->setMaximum(steps);
    mUI->slider->setValue((mValue-mMinimum)/range * steps);
    mUI->slider->setSingleStep(1);
    mUI->slider->setPageStep(10);
}


} // namespace