// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

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
        gfx::NoiseBitmapGenerator* mNoise = nullptr;
    };
} // namespace