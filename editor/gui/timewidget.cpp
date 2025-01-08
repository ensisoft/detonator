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

#include "config.h"

#include "warnpush.h"
#  include "ui_timewidget.h"
#include "warnpop.h"

#include "base/assert.h"
#include "editor/gui/utility.h"
#include "editor/gui/timewidget.h"
#include "editor/gui/translation.h"

namespace gui
{

std::string TranslateEnum(const TimeWidget::Unit unit)
{
    if (unit == TimeWidget::Unit::Millisecs)
        return "ms";
    else if (unit == TimeWidget::Unit::Seconds)
        return "s";
    else if (unit == TimeWidget::Unit::Minutes)
        return "min";
    BUG("Missing time unit translation");
    return "???";
}

TimeWidget::TimeWidget(QWidget* parent) : QWidget(parent)
{
    mUI = new Ui::TimeWidget();
    mUI->setupUi(this);
    PopulateFromEnum<Unit>(mUI->format);
    SetValue(mUI->format, Unit::Millisecs);
    SetSuffix();
}
TimeWidget::~TimeWidget()
{
    delete mUI;
}

void TimeWidget::SetFormat(Unit unit)
{
    SetValue(mUI->format, unit);
    SetSuffix();
    ShowValue();
}
unsigned TimeWidget::GetTime() const
{
    const auto format = (Unit)gui::GetValue(mUI->format);
    const float value = gui::GetValue(mUI->value);
    if (format == Unit::Millisecs)
        return value;
    else if (format == Unit::Seconds)
        return value * 1000.0;
    else if (format == Unit::Minutes)
        return value * 1000.0 * 60.0;
    else BUG("Unhandled time format.");
    return 0;
}

void TimeWidget::SetTime(unsigned value)
{
    // store for simple conversion
    mMilliseconds = value;
    ShowValue();
}

void TimeWidget::SetEditable(bool on_off)
{
    SetEnabled(mUI->value, on_off);
}

void TimeWidget::on_format_currentIndexChanged(int)
{
    SetSuffix();
    ShowValue();
}
void TimeWidget::on_value_valueChanged(double)
{
    mMilliseconds = GetTime();
    emit valueChanged(mMilliseconds);
}

void TimeWidget::ShowValue()
{
    const auto format = (Unit)gui::GetValue(mUI->format);
    if (format == Unit::Millisecs)
        gui::SetValue(mUI->value, mMilliseconds);
    else if (format == Unit::Seconds)
        gui::SetValue(mUI->value, mMilliseconds / (1000.0));
    else if (format == Unit::Minutes)
        gui::SetValue(mUI->value, mMilliseconds / (1000.0 * 60.0));
    else BUG("???");
}

void TimeWidget::SetSuffix()
{
    const auto unit = (Unit)GetValue(mUI->format);
    if (unit == Unit::Millisecs)
        mUI->value->setSuffix(" ms");
    else if (unit == Unit::Minutes)
        mUI->value->setSuffix(" min");
    else if (unit == Unit::Seconds)
        mUI->value->setSuffix(" s");
    else BUG("Unhandled time format.");
}

} // namespace