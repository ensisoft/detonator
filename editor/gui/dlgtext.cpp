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
#  include <QFileInfo>
#include "warnpop.h"

#include "graphics/text.h"
#include "graphics/drawing.h"
#include "graphics/material.h"
#include "editor/gui/utility.h"
#include "editor/gui/dlgtext.h"

namespace gui
{

DlgText::DlgText(QWidget* parent, gfx::TextBuffer& text)
    : QDialog(parent)
    , mText(text)
{
    mUI.setupUi(this);
    PopulateFromEnum<gfx::TextBuffer::HorizontalAlignment>(mUI.cmbHAlign);
    PopulateFromEnum<gfx::TextBuffer::VerticalAlignment>(mUI.cmbVAlign);
    SetValue(mUI.cmbVAlign, gfx::TextBuffer::VerticalAlignment::AlignCenter);
    SetValue(mUI.cmbHAlign, gfx::TextBuffer::HorizontalAlignment::AlignCenter);
    // do the graphics dispose in finished handler which is triggered
    // regardless whether we do accept/reject or the user clicks the X
    // or presses Esc.
    connect(this, &QDialog::finished, mUI.widget, &GfxWidget::dispose);
    // render on timer
    connect(&mTimer, &QTimer::timeout, mUI.widget, &GfxWidget::triggerPaint);

    mUI.widget->onPaintScene = std::bind(&DlgText::PaintScene,
        this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onInitScene = [&](unsigned, unsigned) {
        mTimer.setInterval(1000.0/60.0);
        mTimer.start();
    };

    PopulateFontNames(mUI.cmbFont);

    if (!text.IsEmpty())
    {
        const auto& text_and_style = text.GetText();
        SetValue(mUI.cmbFont, text_and_style.font);
        SetValue(mUI.fontSize, text_and_style.fontsize);
        SetValue(mUI.underline, text_and_style.underline);
        SetValue(mUI.lineHeight, text_and_style.lineheight);
        SetValue(mUI.text, text_and_style.text);
    }
    SetValue(mUI.bufferWidth, text.GetBufferWidth());
    SetValue(mUI.bufferHeight, text.GetBufferHeight());
    SetValue(mUI.cmbVAlign, text.GetVerticalAlignment());
    SetValue(mUI.cmbHAlign, text.GetHorizontalAligment());

    mUI.cmbFont->lineEdit()->setReadOnly(true);
}

void DlgText::on_btnAccept_clicked()
{
    accept();
}
void DlgText::on_btnCancel_clicked()
{
    reject();
}
void DlgText::on_btnFont_clicked()
{
    const auto& list = QFileDialog::getOpenFileNames(this,
        tr("Select Font File"), "", tr("Font (*.ttf *.otf)"));
    if (list.isEmpty())
        return;

    mUI.cmbFont->setCurrentText(list[0]);
}

void DlgText::on_btnAdjust_clicked()
{
    mAdjustOnce = true;
}

void DlgText::PaintScene(gfx::Painter& painter, double secs)
{
    const bool adjust = mAdjustOnce;
    mAdjustOnce = false;

    const auto widget_width = mUI.widget->width();
    const auto widget_height = mUI.widget->height();
    painter.SetViewport(0, 0, widget_width, widget_height);

    const QString& text = GetValue(mUI.text);
    const QString& font = GetValue(mUI.cmbFont);
    if (text.isEmpty() || font.isEmpty())
        return;

    unsigned buffer_width  = adjust ? 0 : GetValue(mUI.bufferWidth);
    unsigned buffer_height = adjust ? 0 : GetValue(mUI.bufferHeight);

    gfx::TextBuffer::Text text_and_style;
    text_and_style.text       = app::ToUtf8(text);
    text_and_style.font       = app::ToUtf8(font);
    text_and_style.fontsize   = GetValue(mUI.fontSize);
    text_and_style.underline  = GetValue(mUI.underline);
    text_and_style.lineheight = GetValue(mUI.lineHeight);

    mText.SetBufferSize(buffer_width, buffer_height);
    mText.SetText(std::move(text_and_style));
    mText.SetAlignment((gfx::TextBuffer::VerticalAlignment)GetValue(mUI.cmbVAlign));
    mText.SetAlignment((gfx::TextBuffer::HorizontalAlignment)GetValue(mUI.cmbHAlign));
    if (adjust)
    {
        const auto format = mText.GetRasterFormat();
        if (format == gfx::TextBuffer::RasterFormat::Bitmap)
        {
            if (const auto& bitmap = mText.RasterizeBitmap())
            {
                buffer_width = bitmap->GetWidth();
                buffer_height = bitmap->GetHeight();
                SetValue(mUI.bufferWidth, buffer_width);
                SetValue(mUI.bufferHeight, buffer_height);
                mText.SetBufferSize(buffer_width, buffer_height);
            }
        }
        else if (format == gfx::TextBuffer::RasterFormat::Texture)
        {
            if (auto* texture = mText.RasterizeTexture("TmpTextRaster", "TmpTextRaster", *painter.GetDevice()))
            {
                texture->SetTransient(true);
                texture->SetGarbageCollection(true);
                texture->SetName("TmpTextRaster");
                buffer_width = texture->GetWidth();
                buffer_height = texture->GetHeight();
                SetValue(mUI.bufferWidth, buffer_width);
                SetValue(mUI.bufferHeight, buffer_height);
                mText.SetBufferSize(buffer_width, buffer_height);
            }
        }
    }
    if (mMaterial == nullptr)
    {
        mClass = std::make_shared<gfx::TextureMap2DClass>(gfx::MaterialClass::Type::Texture);
        mClass->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
        mClass->SetBaseColor(gfx::Color::White);
        mClass->SetTexture(gfx::CreateTextureFromText(mText));
        mClass->GetTextureMap(0)->GetTextureSource(0)->SetName("DlgTextTexture");
        mMaterial = gfx::CreateMaterialInstance(mClass);
    }
    auto* source = mClass->GetTextureMap(0)->GetTextureSource(0);
    auto* bitmap = dynamic_cast<gfx::detail::TextureTextBufferSource*>(source);
    bitmap->SetTextBuffer(mText);

    if ((bool)GetValue(mUI.chkScale))
    {
        const auto scale = std::min((float)widget_width / (float)buffer_width,
                                    (float)widget_height / (float)buffer_height);
        const auto render_width = buffer_width * scale;
        const auto render_height = buffer_height * scale;
        const auto x = (widget_width - render_width) / 2.0f;
        const auto y = (widget_height - render_height) / 2.0f;
        gfx::FillRect(painter, gfx::FRect(x, y, render_width, render_height), *mMaterial);
        gfx::DrawRectOutline(painter, gfx::FRect(x, y, render_width, render_height),
                             gfx::CreateMaterialFromColor(gfx::Color::DarkGreen), 1.0f);
    }
    else
    {
        const auto x = ((float)widget_width - (float)buffer_width) / 2.0f;
        const auto y = ((float)widget_height - (float)buffer_height) / 2.0f;
        gfx::FillRect(painter, gfx::FRect(x, y, buffer_width, buffer_height), *mMaterial);
        gfx::DrawRectOutline(painter, gfx::FRect(x, y, buffer_width, buffer_height),
                             gfx::CreateMaterialFromColor(gfx::Color::DarkGreen), 1.0f);
    }
}

} // namespace
