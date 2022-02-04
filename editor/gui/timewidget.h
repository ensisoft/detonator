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
#include "warnpop.h"

namespace Ui {
    class TimeWidget;
}

namespace gui
{
    class TimeWidget : public QWidget
    {
        Q_OBJECT

    public:
        enum class Format {
            Milliseconds,
            Seconds,
            Minutes
        };

        TimeWidget(QWidget* parent);
       ~TimeWidget();

        void SetFormat(Format format);
        // Get the time in milliseconds.
        unsigned GetTime() const;
        // Set the time from milliseconds.
        void SetTime(unsigned value);

        void SetEditable(bool on_off);

    signals:
        void valueChanged(int);

    private slots:
        void on_format_currentIndexChanged(int);
        void on_value_valueChanged(double);
    private:
        void ShowValue();
        void SetSuffix();
    private:
        Ui::TimeWidget* mUI = nullptr;
    private:
        unsigned mMilliseconds = 0;
    };
} // namespace