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

#define LOGTAG "gui"

#include "config.h"

#include "warnpush.h"
#  include <QCoreApplication>
#  include <QFileDialog>
#  include <boost/math/special_functions/prime.hpp>
#include "warnpop.h"

#include "editor/app/eventlog.h"
#include "editor/gui/dlgbitmap.h"
#include "editor/gui/utility.h"
#include "graphics/bitmap.h"
#include "graphics/text.h"
#include "graphics/drawing.h"
#include "graphics/material.h"
#include "graphics/transform.h"

namespace gui
{
DlgBitmap::DlgBitmap(QWidget* parent, std::unique_ptr<gfx::IBitmapGenerator> generator)
    : QDialog(parent)
    , mGenerator(std::move(generator))
{
    mUI.setupUi(this);
    // do the graphics dispose in finished handler which is triggered
    // regardless whether we do accept/reject or the user clicks the X
    // or presses Esc.
    connect(this, &QDialog::finished, mUI.widget, &GfxWidget::dispose);
    // render on timer
    connect(&mTimer, &QTimer::timeout, mUI.widget, &GfxWidget::triggerPaint);

    mUI.widget->onPaintScene = std::bind(&DlgBitmap::PaintScene,
                                         this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onInitScene = [&](unsigned, unsigned) {
        mTimer.setInterval(1000.0/60.0);
        mTimer.start();
    };

    PopulateFromEnum<gfx::IBitmapGenerator::Function>(mUI.cmbFunction);
    SetValue(mUI.cmbFunction, mGenerator->GetFunction());
    SetValue(mUI.bmpWidth, mGenerator->GetWidth());
    SetValue(mUI.bmpHeight, mGenerator->GetHeight());

    if (mGenerator->GetFunction() == gfx::IBitmapGenerator::Function::Noise)
    {
        QSignalBlocker s(mUI.cmbNoiseLayers);

        mNoise = dynamic_cast<gfx::NoiseBitmapGenerator *>(mGenerator.get());
        for (size_t i=0; i<mNoise->GetNumLayers(); ++i)
            mUI.cmbNoiseLayers->addItem(QString::number(i));

        if (mNoise->HasLayers())
            mUI.cmbNoiseLayers->setCurrentIndex(0);

        on_cmbNoiseLayers_currentIndexChanged(mUI.cmbNoiseLayers->currentIndex());
    }
}

void DlgBitmap::on_btnRandomize_clicked()
{
    const auto min_prime_index = mUI.noisePrime0->minimum();
    const auto max_prime_index = mUI.noisePrime0->maximum();
    const auto layers = 3;
    mNoise->Randomize(min_prime_index, max_prime_index, 3);
    on_cmbNoiseLayers_currentIndexChanged(0);
}

void DlgBitmap::on_btnAccept_clicked()
{
    accept();
}
void DlgBitmap::on_btnCancel_clicked()
{
    reject();
}

void DlgBitmap::on_btnAddNoiseLayer_clicked()
{
    gfx::NoiseBitmapGenerator::Layer layer;
    layer.prime0 = boost::math::prime(mUI.noisePrime0->value());
    //layer.prime1 = boost::math::prime(mUI.noisePrime1->value());
    //layer.prime2 = boost::math::prime(mUI.noisePrime2->value());
    layer.amplitude = mUI.noiseAmplitude->value();
    layer.frequency = mUI.noiseFrequency->value();
    const auto num = mNoise->GetNumLayers();
    mNoise->AddLayer(layer);

    QSignalBlocker s(mUI.cmbNoiseLayers);
    mUI.cmbNoiseLayers->addItem(QString::number(num));
    mUI.cmbNoiseLayers->setCurrentIndex(num);
    mUI.btnDelNoiseLayer->setEnabled(true);
    mUI.noisePrime0->setEnabled(true);
    mUI.noiseAmplitude->setEnabled(true);
    mUI.noiseFrequency->setEnabled(true);
}
void DlgBitmap::on_btnDelNoiseLayer_clicked()
{
    const auto index = mUI.cmbNoiseLayers->currentIndex();
    if (index == -1)
        return;

    mNoise->DelLayer(index);

    QSignalBlocker block(mUI.cmbNoiseLayers);
    mUI.cmbNoiseLayers->clear();
    for (size_t i=0; i<mNoise->GetNumLayers(); ++i)
        mUI.cmbNoiseLayers->addItem(QString::number(i));

    on_cmbNoiseLayers_currentIndexChanged(mUI.cmbNoiseLayers->currentIndex());
}

void DlgBitmap::on_noisePrime0_valueChanged()
{
    const auto index = mUI.cmbNoiseLayers->currentIndex();
    if (index == -1)
        return;
    auto& layer = mNoise->GetLayer(index);
    layer.prime0 = boost::math::prime(mUI.noisePrime0->value());
}

void DlgBitmap::on_noiseAmplitude_valueChanged()
{
    const auto index = mUI.cmbNoiseLayers->currentIndex();
    if (index == -1)
        return;
    auto& layer = mNoise->GetLayer(index);
    layer.amplitude = mUI.noiseAmplitude->value();
}
void DlgBitmap::on_noiseFrequency_valueChanged()
{
    const auto index = mUI.cmbNoiseLayers->currentIndex();
    if (index == -1)
        return;
    auto& layer = mNoise->GetLayer(index);
    layer.frequency = mUI.noiseFrequency->value();
}

void DlgBitmap::on_cmbNoiseLayers_currentIndexChanged(int index)
{
    DEBUG("Selected noise layer %1", index);

    mUI.noisePrime0->setEnabled(index != -1);
    mUI.noiseAmplitude->setEnabled(index != -1);
    mUI.noiseFrequency->setEnabled(index != -1);
    mUI.btnDelNoiseLayer->setEnabled(index != -1);
    SetValue(mUI.noisePrime0, mUI.noisePrime0->minimum());
    SetValue(mUI.noiseFrequency, mUI.noiseFrequency->minimum());
    SetValue(mUI.noiseAmplitude, mUI.noiseAmplitude->minimum());
    if (index == -1)
        return;

    const auto& layer = mNoise->GetLayer(index);
    SetValue(mUI.noiseAmplitude, layer.amplitude);
    SetValue(mUI.noiseFrequency, layer.frequency);
    // scan the list of boost primes to find the slider index (value)
    // for the prime
    for (int i=mUI.noisePrime0->minimum(); i<=mUI.noisePrime0->maximum(); ++i)
    {
        const auto prime = boost::math::prime(i);
        if (prime == layer.prime0)
        {
            SetValue(mUI.noisePrime0, i);
            break;
        }
    }
}

void DlgBitmap::PaintScene(gfx::Painter &painter, double secs)
{
    const auto widget_width = mUI.widget->width();
    const auto widget_height = mUI.widget->height();
    painter.SetViewport(0, 0, widget_width, widget_height);

    const unsigned bmp_width  = GetValue(mUI.bmpWidth);
    const unsigned bmp_height = GetValue(mUI.bmpHeight);
    mNoise->SetWidth(bmp_width);
    mNoise->SetHeight(bmp_height);

    if (mMaterial == nullptr)
    {
        mClass = std::make_shared<gfx::TextureMap2DClass>();
        mClass->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
        mClass->SetBaseColor(gfx::Color::White);
        mClass->SetTexture(gfx::GenerateNoiseTexture(*mNoise));
        mMaterial = gfx::CreateMaterialInstance(mClass);
    }
    auto* source = mClass->AsTexture()->GetTextureSource();
    auto* bitmap = dynamic_cast<gfx::detail::TextureBitmapGeneratorSource*>(source);
    bitmap->SetGenerator(mGenerator->Clone());

    if ((bool)GetValue(mUI.chkScale))
    {
        const auto scale = std::min((float)widget_width / (float)bmp_width,
                                    (float)widget_height / (float)bmp_height);
        const auto render_width = bmp_width * scale;
        const auto render_height = bmp_height * scale;
        const auto x = (widget_width - render_width) / 2.0f;
        const auto y = (widget_height - render_height) / 2.0f;
        gfx::FillRect(painter, gfx::FRect(x, y, render_width, render_height), *mMaterial);
        gfx::DrawRectOutline(painter, gfx::FRect(x, y, render_width, render_height),
                             gfx::CreateMaterialFromColor(gfx::Color::DarkGreen), 1.0f);
    }
    else
    {
        const auto x = (widget_width - bmp_width) / 2;
        const auto y = (widget_height - bmp_height) / 2;
        gfx::FillRect(painter, gfx::FRect(x, y, bmp_width, bmp_height), *mMaterial);
    }

}

} // namespace
