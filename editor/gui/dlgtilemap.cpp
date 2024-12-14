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

#define LOGTAG "gui"

#include "config.h"

#include "warnpush.h"
#  include <QMessageBox>
#  include <QFileDialog>
#  include <QFileInfo>
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <algorithm>

#include "base/json.h"
#include "graphics/material.h"
#include "graphics/material_class.h"
#include "graphics/painter.h"
#include "graphics/drawing.h"
#include "graphics/texture_file_source.h"
#include "editor/app/eventlog.h"
#include "editor/gui/drawing.h"
#include "editor/gui/utility.h"
#include "editor/gui/dlgtilemap.h"
#include "editor/gui/imgpack.h"

namespace gui
{

DlgTilemap::DlgTilemap(QWidget* parent) : QDialog(parent)
{
    mUI.setupUi(this);
    mUI.widget->onPaintScene   = std::bind(&DlgTilemap::OnPaintScene, this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onMouseMove    = std::bind(&DlgTilemap::OnMouseMove, this, std::placeholders::_1);
    mUI.widget->onMousePress   = std::bind(&DlgTilemap::OnMousePress, this, std::placeholders::_1);
    mUI.widget->onMouseRelease = std::bind(&DlgTilemap::OnMouseRelease, this, std::placeholders::_1);
    mUI.widget->onKeyPress     = std::bind(&DlgTilemap::OnKeyPress, this, std::placeholders::_1);
    mUI.widget->onZoomOut = [this]() {
        float zoom = GetValue(mUI.zoom);
        SetValue(mUI.zoom, zoom - 0.1);
    };
    mUI.widget->onZoomIn = [this]() {
        float zoom = GetValue(mUI.zoom);
        SetValue(mUI.zoom, zoom + 0.2);
    };
    mUI.widget->onInitScene = [this](unsigned, unsigned) {
        mTimer.setInterval(1000.0/60.0);
        mTimer.start();
    };

    connect(this, &QDialog::finished, this,  &DlgTilemap::finished);
    connect(&mTimer, &QTimer::timeout, this, &DlgTilemap::timer);

    PopulateFromEnum<gfx::TextureFileSource::ColorSpace>(mUI.cmbColorSpace);
    PopulateFromEnum<gfx::MaterialClass::MinTextureFilter>(mUI.cmbMinFilter);
    PopulateFromEnum<gfx::MaterialClass::MagTextureFilter>(mUI.cmbMagFilter);
    SetEnabled(mUI.btnExport, false);
    SetValue(mUI.zoom, 1.0f);
    SetValue(mUI.cmbColorSpace, gfx::TextureFileSource::ColorSpace::sRGB);
}

DlgTilemap::~DlgTilemap()
{}

void DlgTilemap::LoadImage(const QString& file)
{
    const QFileInfo info(file);
    const auto& name = info.baseName();

    std::string file_uri  = app::ToUtf8(file);
    std::string file_name = app::ToUtf8(name);
    auto source = std::make_unique<gfx::TextureFileSource>();
    source->SetFileName(file_uri);
    source->SetName(file_name);
    source->SetColorSpace(GetValue(mUI.cmbColorSpace));
    auto bitmap = source->GetData();
    if (!bitmap)
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Critical);
        msg.setText("The selected image file could not be loaded.");
        msg.exec();
        return;
    }

    const auto img_width = bitmap->GetWidth();
    const auto img_height = bitmap->GetHeight();
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto scale = std::min((float)width/(float)img_width,
                                (float)height/(float)img_height);
    mTrackingOffset = QPoint(0, 0);
    mWidth  = img_width;
    mHeight = img_height;
    mClass = std::make_shared<gfx::TextureMap2DClass>(gfx::MaterialClass::Type::Texture);
    mClass->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
    mClass->SetTexture(std::move(source));
    mClass->SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
    mMaterial = gfx::CreateMaterialInstance(mClass);
    SetValue(mUI.imageFile, info.absoluteFilePath());
    SetValue(mUI.zoom, scale);
    SetEnabled(mUI.btnExport, true);

    const auto& json = app::FindImageJsonFile(file);
    if (json.isEmpty())
        return;

    ImagePack pack;
    if (!ReadImagePack(json, &pack) || !pack.tilemap.has_value())
        return;

    const auto& tilemap = pack.tilemap.value();
    SetValue(mUI.offsetX, tilemap.xoffset);
    SetValue(mUI.offsetY, tilemap.yoffset);
    SetValue(mUI.tileWidth, tilemap.tile_width);
    SetValue(mUI.tileHeight, tilemap.tile_height);
    SetValue(mUI.padding, pack.padding);
}

void DlgTilemap::on_widgetColor_colorChanged(const QColor& color)
{
    mUI.widget->SetClearColor(ToGfx(color));
}

void DlgTilemap::on_tileWidth_valueChanged(int)
{
}
void DlgTilemap::on_tileHeight_valueChanged(int)
{
}

void DlgTilemap::on_offsetX_valueChanged(int)
{
}
void DlgTilemap::on_offsetY_valueChanged(int)
{
}

void DlgTilemap::on_padding_valueChanged(int)
{
}

void DlgTilemap::on_btnSelectImage_clicked()
{
    const auto ret = QFileDialog::getOpenFileName(this,
        tr("Select Image File"), "",
        tr("Images (*.png *.jpg *.jpeg)"));
    if (ret.isEmpty())
        return;
    LoadImage(ret);
}

void DlgTilemap::on_btnExport_clicked()
{
    const QString image_file = GetValue(mUI.imageFile);
    const QFileInfo image_file_info(GetValue(mUI.imageFile));

    QString json_file = image_file;
    json_file.remove(image_file_info.suffix());
    json_file.append("json");
    json_file = QFileDialog::getSaveFileName(this,
        tr("Select Save File"), json_file, ".json");
    if (json_file.isEmpty())
        return;

    const unsigned tile_xoffset = GetValue(mUI.offsetX);
    const unsigned tile_yoffset = GetValue(mUI.offsetY);
    const unsigned tile_width  = GetValue(mUI.tileWidth);
    const unsigned tile_height = GetValue(mUI.tileHeight);
    const unsigned tile_padding = GetValue(mUI.padding);

    // we're cranking out JSON that is compatible with the
    // JSON expected by the custom bitmap font implementation
    // in gfx/text. Also, the image packer can produce compatible
    // JSOn. (See DlgImgPack)
    nlohmann::json json;
    base::JsonWrite(json, "json_version", 1);
    base::JsonWrite(json, "made_with_app", APP_TITLE);
    base::JsonWrite(json, "made_with_ver", APP_VERSION);
    base::JsonWrite(json, "image_type",    "tilemap");
    base::JsonWrite(json, "image_file",    app::ToUtf8(image_file_info.fileName()));
    base::JsonWrite(json, "image_width",   mWidth);
    base::JsonWrite(json, "image_height",  mHeight);
    base::JsonWrite(json, "tile_width",    tile_width);
    base::JsonWrite(json, "tile_height",   tile_height);
    base::JsonWrite(json, "padding",       tile_padding);
    base::JsonWrite(json, "xoffset",       tile_xoffset);
    base::JsonWrite(json, "yoffset",       tile_yoffset);
    base::JsonWrite(json, "color_space",   (gfx::TextureFileSource::ColorSpace)GetValue(mUI.cmbColorSpace));
    base::JsonWrite(json, "min_filter",    (gfx::MaterialClass::MinTextureFilter)GetValue(mUI.cmbMinFilter));
    base::JsonWrite(json, "mag_filter",    (gfx::MaterialClass::MagTextureFilter)GetValue(mUI.cmbMagFilter));
    base::JsonWrite(json, "premultiply_alpha", (bool)GetValue(mUI.chkPremulAlpha));
    base::JsonWrite(json, "premulalpha_blend", (bool)GetValue(mUI.chkPremulAlphaBlend));
    // going to skip the writing of the images array for now since it's just
    // repeated information that can be created out of the tile size, offset and
    // img size information.
#if 0
    const auto max_rows = (mHeight-tile_yoffset) / tile_height;
    const auto max_cols = (mWidth-tile_xoffset) / tile_width;
    for (unsigned row=0; row<max_rows; ++row)
    {
        for (unsigned col=0; col<max_cols; ++col)
        {
            const auto index = row * max_cols + col;
            const auto tile_xpos = tile_xoffset + col * tile_width;
            const auto tile_ypos = tile_yoffset + row * tile_height;

            nlohmann::json tile_json;
            base::JsonWrite(tile_json, "xpos", tile_xpos);
            base::JsonWrite(tile_json, "ypos", tile_ypos);
            json["images"].push_back(std::move(tile_json));
        }
    }
#endif

    QFile file;
    file.setFileName(json_file);
    if (!file.open(QIODevice::WriteOnly))
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("Failed to write the JSON description file.\n"
                       "File error '%1'").arg(file.errorString()));
        msg.exec();
        return;
    }
    const auto& json_string = json.dump(2);
    file.write(&json_string[0], json_string.size());
    file.close();
    DEBUG("Wrote font map JSON file. [file='%1']", json_file);
}

void DlgTilemap::on_btnClose_clicked()
{
    mClosed = true;
    close();
}

void DlgTilemap::finished()
{
    mClosed = true;
    mUI.widget->dispose();
}

void DlgTilemap::timer()
{
    mUI.widget->triggerPaint();
}

void DlgTilemap::OnPaintScene(gfx::Painter& painter, double secs)
{
    SetValue(mUI.widgetColor, mUI.widget->GetCurrentClearColor());

    const float width  = mUI.widget->width();
    const float height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    if (!mMaterial)
    {
        ShowInstruction(
            "Create a tilemap description based on an image.\n\n"
            "INSTRUCTIONS\n"
            "1. Select the tilemap image file.\n"
            "2. Adjust the tile region position.\n"
            "3. Adjust the tile dimensions.\n"
            "4. When the tiles align with the guide grid you're done.\n"
            "5. Click 'Export' to save the tile JSON.\n",
            gfx::FRect(0, 0, width, height),
            painter);
        return;
    }
    mClass->SetFlag(gfx::MaterialClass::Flags::PremultipliedAlpha, GetValue(mUI.chkPremulAlphaBlend));
    mClass->SetTextureMinFilter(GetValue(mUI.cmbMinFilter));
    mClass->SetTextureMagFilter(GetValue(mUI.cmbMagFilter));
    auto* map = mClass->GetTextureMap(0);
    auto* src = map->GetTextureSource(0);
    auto* texture_source = dynamic_cast<gfx::TextureFileSource*>(src);
    texture_source->SetColorSpace(GetValue(mUI.cmbColorSpace));
    texture_source->SetFlag(gfx::TextureFileSource::Flags::PremulAlpha, GetValue(mUI.chkPremulAlpha));

    const float zoom   = GetValue(mUI.zoom);
    const float img_width  = mWidth * zoom;
    const float img_height = mHeight * zoom;
    const auto xpos = (width - img_width) * 0.5f;
    const auto ypos = (height - img_height) * 0.5f;

    gfx::FRect img(0.0f, 0.0f, img_width, img_height);
    img.Translate(xpos, ypos);
    img.Translate(mTrackingOffset.x(), mTrackingOffset.y());
    gfx::FillRect(painter, img, *mMaterial);

    const bool show_grid = GetValue(mUI.chkGrid);
    const unsigned tile_xoffset = GetValue(mUI.offsetX);
    const unsigned tile_yoffset = GetValue(mUI.offsetY);
    const unsigned tile_width  = GetValue(mUI.tileWidth);
    const unsigned tile_height = GetValue(mUI.tileHeight);
    const unsigned tile_padding = GetValue(mUI.padding);

    const unsigned tile_box_width = tile_width + 2 * tile_padding;
    const unsigned tile_box_height = tile_height + 2 * tile_padding;

    const auto max_rows = (mHeight-tile_yoffset) / tile_box_height;
    const auto max_cols = (mWidth-tile_xoffset) / tile_box_width;
    const int mouse_posx = (mCurrentPoint.x() - mTrackingOffset.x() - xpos) / zoom;
    const int mouse_posy = (mCurrentPoint.y() - mTrackingOffset.y() - ypos) / zoom;
    const int current_row = (mouse_posy-tile_yoffset) / tile_box_height;
    const int current_col = (mouse_posx-tile_xoffset) / tile_box_width;

    const gfx::Color4f grid_color(gfx::Color::HotPink, 0.8);

    for (unsigned row=0; row<max_rows; ++row)
    {
        for (unsigned col=0; col<max_cols; ++col)
        {
            const auto tile_index  = row * max_cols + col;

            gfx::FRect tile(0.0f, 0.0f, tile_box_width*zoom, tile_box_height*zoom);
            tile.Translate(xpos, ypos);
            tile.Translate(mTrackingOffset.x(), mTrackingOffset.y());
            tile.Translate(tile_xoffset * zoom, tile_yoffset * zoom);
            tile.Translate(col * tile_box_width * zoom, row * tile_box_height * zoom);
            if (show_grid)
            {
                gfx::DrawRectOutline(painter, tile, gfx::CreateMaterialFromColor(grid_color));
            }
            if (row == current_row && col == current_col)
            {
                gfx::DrawRectOutline(painter, tile, gfx::CreateMaterialFromColor(gfx::Color::HotPink));
            }
        }
    }
}
void DlgTilemap::OnMousePress(QMouseEvent* mickey)
{
    if (mickey->button() == Qt::LeftButton)
    {
        mMode = Mode::Selecting;
    }
    else if (mickey->button() == Qt::RightButton)
        mMode = Mode::Tracking;

    mStartPoint = mickey->pos();
}
void DlgTilemap::OnMouseMove(QMouseEvent* mickey)
{
    mCurrentPoint = mickey->pos();

    if (mMode == Mode::Selecting)
    {
        //ToggleSelection();
    }
    else if (mMode == Mode::Tracking)
    {
        mTrackingOffset += (mCurrentPoint - mStartPoint);
        mStartPoint = mCurrentPoint;
    }
}
void DlgTilemap::OnMouseRelease(QMouseEvent* mickey)
{
    mMode = Mode::Nada;
}

bool DlgTilemap::OnKeyPress(QKeyEvent* event)
{
    const auto key   = event->key();
    const auto shift = event->modifiers() & Qt::ShiftModifier;

    if (shift && (key == Qt::Key_Up))
        Decrement(mUI.offsetY, 1);
    else if (shift && (key == Qt::Key_Down))
        Increment(mUI.offsetY, 1);
    else if (shift && (key == Qt::Key_Left))
        Decrement(mUI.offsetX, 1);
    else if (shift && (key == Qt::Key_Right))
        Increment(mUI.offsetX, 1);
    else return false;

    return true;
}


} // namespace
