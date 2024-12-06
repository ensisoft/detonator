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
#  include <QImage>
#  include <QImageWriter>
#  include <QMessageBox>
#  include <QPixmap>
#include "warnpop.h"

#include "graphics/drawing.h"
#include "graphics/material.h"
#include "graphics/material_instance.h"
#include "graphics/material_class.h"
#include "graphics/text_buffer.h"
#include "graphics/texture_text_buffer_source.h"
#include "graphics/painter.h"
#include "editor/app/workspace.h"
#include "editor/gui/utility.h"
#include "editor/gui/dlgtext.h"
#include "editor/gui/dlgfont.h"

namespace gui
{

DlgText::DlgText(QWidget* parent, const app::Workspace* workspace, gfx::TextBuffer& text)
    : QDialog(parent)
    , mWorkspace(workspace)
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

bool DlgText::DidExport() const
{
    if (mExportFile.isEmpty())
        return false;
    if (mExportHash != mText.GetHash())
        return false;

    return true;
}

void DlgText::on_btnAccept_clicked()
{
    accept();
}
void DlgText::on_btnCancel_clicked()
{
    reject();
}
void DlgText::on_btnSelectFont_clicked()
{
    DlgFont::DisplaySettings disp;
    disp.font_size  = GetValue(mUI.fontSize);
    disp.underline  = GetValue(mUI.underline);
    disp.text_color = Qt::darkGray;

    QString current_font = GetValue(mUI.cmbFont);
    DlgFont dlg(this, mWorkspace, current_font, disp);
    if (dlg.exec() == QDialog::Rejected)
        return;

    SetValue(mUI.cmbFont, dlg.GetSelectedFontURI());
}

void DlgText::on_btnBrowseFont_clicked()
{
    const auto& font = QFileDialog::getOpenFileName(this,
        tr("Select Font File"), "", tr("Font (*.ttf *.otf)"));
    if (font.isEmpty())
        return;
    const auto& uri = mWorkspace->MapFileToWorkspace(font);

    SetValue(mUI.cmbFont, uri);
}

void DlgText::on_btnAdjust_clicked()
{
    mAdjustOnce = true;
}
void DlgText::on_btnSaveAs_clicked()
{
    const auto& png = QFileDialog::getSaveFileName(this,
        tr("Select Save File"), "text.png", tr("PNG (*.png)"));
    if (png.isEmpty())
        return;

    const auto* source = mClass->GetTextureMap(0)->GetTextureSource(0);
    const auto& bitmap = source->GetData();
    if (bitmap == nullptr || !bitmap->IsValid())
    {
        return;
    }

    const auto width  = bitmap->GetWidth();
    const auto height = bitmap->GetHeight();
    const auto depth  = bitmap->GetDepthBits();
    QImage image;
    if (depth == 8)
        image = QImage((const uchar*)bitmap->GetDataPtr(), width, height, width, QImage::Format_Alpha8);
    else if (depth == 24)
        image = QImage((const uchar*)bitmap->GetDataPtr(), width, height, width * 3, QImage::Format_RGB888);
    else if (depth == 32)
        image = QImage((const uchar*)bitmap->GetDataPtr(), width, height, width * 4, QImage::Format_RGBA8888);
    else return;

    QImageWriter writer;
    writer.setFormat("PNG");
    writer.setQuality(100);
    writer.setFileName(png);
    if (!writer.write(image))
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("Failed to write the image.\n%1").arg(writer.errorString()));
        msg.exec();
    }
    mExportFile = png;
    mExportHash = mText.GetHash();
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

    // currently we can't rasterize "texture" based text since that requires
    // HW composition and the GetData() API cannot offer that.
    if (mText.GetRasterFormat() == gfx::TextBuffer::RasterFormat::Bitmap)
    {
        SetEnabled(mUI.btnSaveAs, true);
    }
    else
    {
        SetEnabled(mUI.btnSaveAs, true);
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
    auto* bitmap = dynamic_cast<gfx::TextureTextBufferSource*>(source);
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
