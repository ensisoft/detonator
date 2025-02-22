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
#  include "ui_dlggradient.h"
#  include <QDialog>
#include "warnpop.h"

#include "base/assert.h"
#include "graphics/material_class.h"
#include "editor/gui/utility.h"
#include "editor/gui/translation.h"

namespace gui
{
    class DlgGradient : public QDialog
    {
        Q_OBJECT

    public:
        DlgGradient(QWidget* parent) : QDialog(parent)
        {
            mUI.setupUi(this);
            connect(mUI.colorMap0, &color_widgets::ColorSelector::colorChanged, this, &DlgGradient::ColorMapColorChanged);
            connect(mUI.colorMap1, &color_widgets::ColorSelector::colorChanged, this, &DlgGradient::ColorMapColorChanged);
            connect(mUI.colorMap2, &color_widgets::ColorSelector::colorChanged, this, &DlgGradient::ColorMapColorChanged);
            connect(mUI.colorMap3, &color_widgets::ColorSelector::colorChanged, this, &DlgGradient::ColorMapColorChanged);
            PopulateFromEnum<gfx::MaterialClass::GradientType>(mUI.cmbGradientType);
            SetValue(mUI.cmbGradientType, gfx::MaterialClass::GradientType::Bilinear);
        }
        void SetColor(const QColor& color, int index)
        {
            if (index == 0)
                SetValue(mUI.colorMap0, color);
            else if (index == 1)
                SetValue(mUI.colorMap1, color);
            else if (index == 2)
                SetValue(mUI.colorMap2, color);
            else if (index == 3)
                SetValue(mUI.colorMap3, color);
            else BUG("Incorrect color index.");
        }
        QColor GetColor(int index) const
        {
            if (index == 0)
                return mUI.colorMap0->color();
            else if (index == 1)
                return mUI.colorMap1->color();
            else if (index == 2)
                return mUI.colorMap2->color();
            else if (index == 3)
                return mUI.colorMap3->color();
            else BUG("Incorrect color index.");
            return QColor();
        }
        void SetGradientType(gfx::MaterialClass::GradientType type)
        {
            SetValue(mUI.cmbGradientType, type);
        }
        gfx::MaterialClass::GradientType GetGradientType()
        {
            return GetValue(mUI.cmbGradientType);
        }
        void SetGamma(float gamma)
        {
            SetValue(mUI.gamma, gamma);
        }
        float GetGamma() const
        {
            return GetValue(mUI.gamma);
        }
    signals:
        void GradientChanged(DlgGradient* dlg);

    private slots:
        void on_btnAccept_clicked()
        {
            accept();
        };
        void on_btnCancel_clicked()
        {
            reject();
        }
        void on_cmbGradientType_currentIndexChanged(int)
        {
            emit GradientChanged(this);
        }
        void ColorMapColorChanged(QColor color)
        {
            emit GradientChanged(this);
        }
        void on_gamma_valueChanged(double)
        {
            emit GradientChanged(this);
        }

        void on_btnSwap01_clicked()
        {
            const QColor color0 = GetValue(mUI.colorMap0);
            const QColor color1 = GetValue(mUI.colorMap1);
            SetValue(mUI.colorMap0, color1);
            SetValue(mUI.colorMap1, color0);
            emit GradientChanged(this);
        }
        void on_btnSwap23_clicked()
        {
            const QColor color2 = GetValue(mUI.colorMap2);
            const QColor color3 = GetValue(mUI.colorMap3);
            SetValue(mUI.colorMap2, color3);
            SetValue(mUI.colorMap3, color2);
            emit GradientChanged(this);
        }
        void on_btnSwap02_clicked()
        {
            const QColor color0 = GetValue(mUI.colorMap0);
            const QColor color2 = GetValue(mUI.colorMap2);
            SetValue(mUI.colorMap0, color2);
            SetValue(mUI.colorMap2, color0);
            emit GradientChanged(this);
        }
        void on_btnSwap13_clicked()
        {
            const QColor color1 = GetValue(mUI.colorMap1);
            const QColor color3 = GetValue(mUI.colorMap3);
            SetValue(mUI.colorMap1, color3);
            SetValue(mUI.colorMap3, color1);
            emit GradientChanged(this);
        }

    private:
        Ui::DlgGradient mUI;
    };
} // namespace
