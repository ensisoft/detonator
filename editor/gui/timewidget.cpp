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

namespace gui
{

TimeWidget::TimeWidget(QWidget* parent) : QWidget(parent)
{
    mUI = new Ui::TimeWidget();
    mUI->setupUi(this);
    PopulateFromEnum<Format>(mUI->format);
    SetValue(mUI->format, Format::Milliseconds);
    SetSuffix();
}
TimeWidget::~TimeWidget()
{
    delete mUI;
}

void TimeWidget::SetFormat(Format format)
{
    SetValue(mUI->format, format);
    SetSuffix();
    ShowValue();
}
unsigned TimeWidget::GetTime() const
{
    const auto format = (Format)gui::GetValue(mUI->format);
    const float value = gui::GetValue(mUI->value);
    if (format == Format::Milliseconds)
        return value;
    else if (format == Format::Seconds)
        return value * 1000.0;
    else if (format == Format::Minutes)
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
    const auto format = (Format)gui::GetValue(mUI->format);
    if (format == Format::Milliseconds)
        gui::SetValue(mUI->value, mMilliseconds);
    else if (format == Format::Seconds)
        gui::SetValue(mUI->value, mMilliseconds / (1000.0));
    else if (format == Format::Minutes)
        gui::SetValue(mUI->value, mMilliseconds / (1000.0 * 60.0));
    else BUG("???");
}

void TimeWidget::SetSuffix()
{
    const auto format = (Format)GetValue(mUI->format);
    if (format == Format::Milliseconds)
        mUI->value->setSuffix(" ms");
    else if (format == Format::Minutes)
        mUI->value->setSuffix(" min");
    else if (format == Format::Seconds)
        mUI->value->setSuffix(" s");
    else BUG("Unhandled time format.");
}

} // namespace