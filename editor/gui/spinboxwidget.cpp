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
#  include "ui_doublespinbox.h"
#include "warnpop.h"

#include "spinboxwidget.h"


namespace gui
{

DoubleSpinBox::DoubleSpinBox(QWidget* parent)
  : QWidget(parent)
{
    mUI = std::make_unique<Ui::DoubleSpinBox>();
    mUI->setupUi(this);
    SetSpecialValueText("No Value Set");

    mUI->spinBox->installEventFilter(this);
}

DoubleSpinBox::~DoubleSpinBox() = default;

QString DoubleSpinBox::GetSpecialValueText() const
{
    return mSpecialValueText;
}
QString DoubleSpinBox::GetSuffix() const
{
    return mUI->spinBox->suffix();
}

void DoubleSpinBox::SetSpecialValueText(const QString& text)
{
    mSpecialValueText = text;
    if (!mHasValue)
        mUI->spinBox->setSpecialValueText(text);
}
void DoubleSpinBox::SetSuffix(const QString& suffix)
{
    mUI->spinBox->setSuffix(suffix);
}

int DoubleSpinBox::GetDecimals() const
{
    return mUI->spinBox->decimals();
}
void DoubleSpinBox::SetDecimals(int decimals)
{
    mUI->spinBox->setDecimals(decimals);
}
double DoubleSpinBox::GetMin() const
{
    return mUI->spinBox->minimum();
}
double DoubleSpinBox::GetMax() const
{
    return mUI->spinBox->maximum();
}
void DoubleSpinBox::SetMin(double value)
{
    mUI->spinBox->setMinimum(value);
}
void DoubleSpinBox::SetMax(double value)
{
    mUI->spinBox->setMaximum(value);
}

QDoubleSpinBox::StepType DoubleSpinBox::GetStepType() const
{
    return mUI->spinBox->stepType();
}

void DoubleSpinBox::SetStepType(QDoubleSpinBox::StepType type)
{
    mUI->spinBox->setStepType(type);
}

double DoubleSpinBox::GetValueForDesigner() const
{
    return mHasValue ? mUI->spinBox->value() : 0.0;
}

void DoubleSpinBox::SetValueFromDesigner(double value)
{
    mUI->spinBox->setValue(value);
    mUI->spinBox->setSpecialValueText("");
    mHasValue = true;
}

double DoubleSpinBox::GetSingleStep() const
{
    return mUI->spinBox->singleStep();
}

void DoubleSpinBox::SetSingleStep(double step)
{
    QSignalBlocker s(mUI->spinBox);
    mUI->spinBox->setSingleStep(step);
}

std::optional<double> DoubleSpinBox::GetValue() const
{
    if (!mHasValue)
        return {};
    return mUI->spinBox->value();
}

bool DoubleSpinBox::GetValue(double* value) const
{
    if (!mHasValue)
        return false;

    *value = mUI->spinBox->value();
    return true;
}

double DoubleSpinBox::GetValueOr(double backup) const
{
    if (!mHasValue)
        return backup;
    return mUI->spinBox->value();
}

bool DoubleSpinBox::HasValue() const
{
    return mHasValue;
}

void DoubleSpinBox::ClearValue()
{
    QSignalBlocker s(mUI->spinBox);
    mUI->spinBox->setValue(mUI->spinBox->minimum());
    mUI->spinBox->setSpecialValueText(mSpecialValueText);
    mHasValue = false;
}

void DoubleSpinBox::SetValue(double value)
{
    QSignalBlocker s(mUI->spinBox);
    mUI->spinBox->setValue(value);
    mUI->spinBox->setSpecialValueText("");
    mHasValue = true;
}

void DoubleSpinBox::EnableValueFromDesigner(bool has_value)
{
    if (!has_value)
        ClearValue();
    else SetValue(mInitialValue);
}

bool DoubleSpinBox::GetShowClearButton() const
{
    return !mUI->btnClear->isHidden();
}

void DoubleSpinBox::SetShowClearButton(bool show)
{
    mUI->btnClear->setVisible(show);
}

QIcon DoubleSpinBox::GetClearButtonIcon() const
{
    return mUI->btnClear->icon();
}
void DoubleSpinBox::SetClearButtonIcon(const QIcon& jesus)
{
    mUI->btnClear->setIcon(jesus);
}

void DoubleSpinBox::ResetClearButtonIcon()
{
    mUI->btnClear->setIcon(QIcon());
}

QString DoubleSpinBox::GetClearButtonText() const
{
    return mUI->btnClear->text();
}
void DoubleSpinBox::SetClearButtonText(const QString& text)
{
    mUI->btnClear->setText(text);
}

void DoubleSpinBox::on_spinBox_valueChanged(double value)
{
    mHasValue = true;
    QSignalBlocker s(mUI->spinBox);
    mUI->spinBox->setSpecialValueText("");
    emit valueChanged(true, value);
}

void DoubleSpinBox::on_btnClear_clicked()
{
    ClearValue();
    emit valueChanged(false, mInitialValue);
}

bool DoubleSpinBox::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::Type::FocusIn)
    {
        if (!mHasValue)
        {
            QSignalBlocker s(mUI->spinBox);
            mUI->spinBox->setSpecialValueText("");
            mUI->spinBox->setValue(mInitialValue);
        }
    }
    else if (event->type() == QEvent::Type::FocusOut)
    {
        if (!mHasValue)
        {
            QSignalBlocker s(mUI->spinBox);
            mUI->spinBox->setSpecialValueText(mSpecialValueText);
            mUI->spinBox->setValue(mUI->spinBox->minimum());
        }
    }
    return false;
}

} // namespace