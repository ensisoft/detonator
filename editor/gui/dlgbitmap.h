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

#include "warnpush.h"
#  include "ui_dlgbitmap.h"
#  include <QTimer>
#  include <QDialog>
#include "warnpop.h"

#include <memory>

#include "graphics/material.h"
#include "graphics/painter.h"
#include "graphics/bitmap.h"
#include "graphics/bitmap_generator.h"
#include "graphics/bitmap_noise.h"
#include "graphics/material_class.h"

namespace gui
{
    class DlgBitmap : public QDialog
    {
        Q_OBJECT

    public:
        DlgBitmap(QWidget* parent, std::unique_ptr<gfx::IBitmapGenerator> generator);

        std::unique_ptr<gfx::IBitmapGenerator> GetResult()
        { return std::move(mGenerator); }
    private slots:
        void on_btnRandomize_clicked();
        void on_btnAccept_clicked();
        void on_btnCancel_clicked();
        void on_btnAddNoiseLayer_clicked();
        void on_btnDelNoiseLayer_clicked();
        void on_noisePrime0_valueChanged();
        void on_noiseAmplitude_valueChanged();
        void on_noiseFrequency_valueChanged();
        void on_cmbNoiseLayers_currentIndexChanged(int);
    private:
        void PaintScene(gfx::Painter& painter, double secs);
    private:
        Ui::DlgBitmap mUI;
    private:
        QTimer mTimer;
        std::unique_ptr<gfx::IBitmapGenerator> mGenerator;
        std::unique_ptr<gfx::Material> mMaterial;
        std::shared_ptr<gfx::TextureMap2DClass> mClass;
        gfx::NoiseBitmapGenerator* mNoise = nullptr;
    };
} // namespace