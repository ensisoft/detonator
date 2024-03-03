// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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
#  include <QFrame>
#  include <QObject>
#  include <QString>
#  include <QIcon>
#  include <QColor>
#  include <QtUiPlugin/QDesignerExportWidget>
#include "warnpop.h"

#include <memory>

#include "base/math.h"
#include "base/format.h"

namespace math {
    Q_NAMESPACE
    Q_ENUM_NS(Interpolation)
}

namespace gui
{
    class DESIGNER_PLUGIN_EXPORT CurveWidget : public QFrame
    {
        Q_OBJECT
        Q_PROPERTY(int method READ GetInterpolation WRITE SetInterpolation DESIGNABLE true)

    public:
        class Function {
        public:
            virtual ~Function() = default;
            virtual float SampleFunction(float x) const = 0;
            virtual float SampleDerivative(float x) const = 0;
            virtual std::string GetName() const = 0;
        private:
        };

        explicit CurveWidget(QWidget* parent);

        int GetInterpolation() const noexcept;
        void SetInterpolation(int);

        void SetFunction(math::Interpolation);

        inline void ClearFunction() noexcept
        { mFunction.reset(); }
        inline void SetFunction(std::unique_ptr<Function> function) noexcept
        { mFunction = std::move(function); }
    private:
        virtual void paintEvent(QPaintEvent* paint) override;

    private:
        class MathInterpolationFunction : public Function {
        public:
            explicit MathInterpolationFunction(math::Interpolation method) noexcept
              : mMethod(method)
            {}
            virtual float SampleFunction(float x) const override
            {
                return math::interpolate(x, mMethod);
            }
            virtual float SampleDerivative(float x) const override
            { return 0.0f; }
            virtual std::string GetName() const override
            { return base::ToString(mMethod); }

            inline math::Interpolation GetInterpolation() const noexcept
            { return mMethod; }

        private:
            const math::Interpolation mMethod;
        };
        std::unique_ptr<Function> mFunction;
    };
}