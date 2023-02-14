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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <QWidget>
#include "warnpop.h"

#include <memory>

namespace Ui {
    class DoubleSlider;
} // namespace

namespace gui
{
    class DoubleSlider : public QWidget
    {
        Q_OBJECT

        Q_PROPERTY(double minimum READ GetMinimum WRITE SetMinimum DESIGNABLE true)
        Q_PROPERTY(double maximum READ GetMaximum WRITE SetMaximum DESIGNABLE true)
        Q_PROPERTY(double singleStep READ GetSingleStep WRITE SetSingleStep DESIGNABLE true)
        Q_PROPERTY(double value READ GetValue WRITE SetValue DESIGNABLE true)
    public:
        explicit DoubleSlider(QWidget* parent);
        ~DoubleSlider();

        inline double GetMinimum() const
        { return mMinimum; }
        inline double GetMaximum() const
        { return mMaximum; }
        inline double GetSingleStep() const
        { return  mStep; }
        inline double GetValue() const
        { return mValue; }

        void SetMinimum(double min);
        void SetMaximum(double max);
        void SetSingleStep(double step);
        void SetValue(double value);
    signals:
        void valueChanged(double value);

    private slots:
        void on_slider_valueChanged(int value);

    private:
        void AdjustSlider();
    private:
        std::unique_ptr<Ui::DoubleSlider> mUI;
        double mMinimum = 0.0;
        double mMaximum = 99.0;
        double mStep    = 1.0;
        double mValue   = 0.0;
    };
} // namespace
