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
#  include <QString>
#  include <QDoubleSpinBox>
#  include <QIcon>
#include "warnpop.h"

#include <optional>
#include <memory>

namespace Ui {
    class DoubleSpinBox;
} // namespace

namespace gui
{
    // Wrapper for QDoubleSpinBox that properly deals with the case
    // of not having any value set.
    class DoubleSpinBox : public QWidget
    {
        Q_OBJECT
        Q_PROPERTY(QString specialValueText READ GetSpecialValueText WRITE SetSpecialValueText DESIGNABLE true)
        Q_PROPERTY(QString suffix READ GetSuffix WRITE SetSuffix DESIGNABLE true)
        Q_PROPERTY(QDoubleSpinBox::StepType stepType READ GetStepType WRITE SetStepType DESIGNABLE true)
        Q_PROPERTY(int decimals READ GetDecimals WRITE SetDecimals DESIGNABLE true)
        Q_PROPERTY(double minimum READ GetMin WRITE SetMin DESIGNABLE true)
        Q_PROPERTY(double maximum READ GetMax WRITE SetMax DESIGNABLE true)
        Q_PROPERTY(double value READ GetValueForDesigner WRITE SetValueFromDesigner RESET ClearValue DESIGNABLE true)
        Q_PROPERTY(double singleStep READ GetSingleStep WRITE SetSingleStep DESIGNABLE true)
        Q_PROPERTY(double initialValue READ GetInitialValue WRITE SetInitialValue DESIGNABLE true)
        Q_PROPERTY(bool hasValue READ HasValue WRITE EnableValueFromDesigner RESET ClearValue DESIGNABLE true)
        Q_PROPERTY(bool showClearButton READ GetShowClearButton WRITE SetShowClearButton DESIGNABLE true)
        Q_PROPERTY(QIcon clearButtonIcon READ GetClearButtonIcon WRITE SetClearButtonIcon RESET ResetClearButtonIcon DESIGNABLE true)
        Q_PROPERTY(QString clearButtonText READ GetClearButtonText WRITE SetClearButtonText DESIGNABLE true)

    public:
        explicit DoubleSpinBox(QWidget* parent);
        ~DoubleSpinBox();

        QString GetSpecialValueText() const;
        QString GetSuffix() const;

        void SetSpecialValueText(const QString& text);
        void SetSuffix(const QString& suffix);

        int GetDecimals() const;
        void SetDecimals(int decimals);

        double GetMin() const;
        double GetMax() const;

        void SetMin(double value);
        void SetMax(double value);

        QDoubleSpinBox::StepType GetStepType() const;
        void SetStepType(QDoubleSpinBox::StepType type);

        double GetValueForDesigner() const;
        void SetValueFromDesigner(double value);

        double GetSingleStep() const;
        void SetSingleStep(double step);

        inline double GetInitialValue() const
        { return mInitialValue; }
        inline void SetInitialValue(double value)
        { mInitialValue = value; }


        std::optional<double> GetValue() const;

        bool GetValue(double* val) const;

        double GetValueOr(double backup) const;

        bool HasValue() const;

        void ClearValue();
        void SetValue(double value);

        void EnableValueFromDesigner(bool has_value);

        bool GetShowClearButton() const;
        void SetShowClearButton(bool show);

        QIcon GetClearButtonIcon() const;
        void SetClearButtonIcon(const QIcon& jesus);
        void ResetClearButtonIcon();

        QString GetClearButtonText() const;
        void SetClearButtonText(const QString& text);

    signals:
        void valueChanged(bool has_value, double value);

    private slots:
        void on_spinBox_valueChanged(double value);
        void on_btnClear_clicked();

    private:
        virtual bool eventFilter(QObject* object, QEvent* event) override;

    private:
        std::unique_ptr<Ui::DoubleSpinBox> mUI;
    private:
        bool mHasValue = false;
        QString mSpecialValueText;
        double mInitialValue = 0.0;
    };
} // namespace
