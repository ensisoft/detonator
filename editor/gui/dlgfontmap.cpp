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
#  include <QFileDialog>
#  include <QFileInfo>
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <algorithm>

#include "base/json.h"
#include "graphics/material.h"
#include "graphics/painter.h"
#include "graphics/drawing.h"
#include "editor/app/eventlog.h"
#include "editor/gui/drawing.h"
#include "editor/gui/utility.h"
#include "editor/gui/dlgfontmap.h"

namespace gui
{

DlgFontMap::DlgFontMap(QWidget* parent) : QDialog(parent)
{
    mUI.setupUi(this);
    mUI.widget->onPaintScene   = std::bind(&DlgFontMap::OnPaintScene,   this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onMouseMove    = std::bind(&DlgFontMap::OnMouseMove,    this, std::placeholders::_1);
    mUI.widget->onMousePress   = std::bind(&DlgFontMap::OnMousePress,   this, std::placeholders::_1);
    mUI.widget->onMouseRelease = std::bind(&DlgFontMap::OnMouseRelease, this, std::placeholders::_1);
    mUI.widget->onKeyPress     = std::bind(&DlgFontMap::OnKeyPress,     this, std::placeholders::_1);
    mUI.widget->onZoomOut = [this]() {
        float zoom = GetValue(mUI.zoom);
        SetValue(mUI.zoom, zoom - 0.1);
    };
    mUI.widget->onZoomIn = [this]() {
        float zoom = GetValue(mUI.zoom);
        SetValue(mUI.zoom, zoom + 0.2);
    };
    mUI.widget->onInitScene = [&](unsigned, unsigned) {
        mTimer.setInterval(1000.0/60.0);
        mTimer.start();
    };

    connect(this, &QDialog::finished, this,  &DlgFontMap::finished);
    connect(&mTimer, &QTimer::timeout, this, &DlgFontMap::timer);

    PopulateFromEnum<gfx::detail::TextureFileSource::ColorSpace>(mUI.cmbColorSpace);
    SetEnabled(mUI.btnExport, false);
    SetValue(mUI.zoom, 1.0f);
}

DlgFontMap::~DlgFontMap()
{}

void DlgFontMap::LoadImage(const QString& file)
{
    const QFileInfo info(file);
    const auto& name = info.baseName();

    std::string file_uri  = app::ToUtf8(file);
    std::string file_name = app::ToUtf8(name);
    auto source = std::make_unique<gfx::detail::TextureFileSource>();
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
    mWidth  = img_width;
    mHeight = img_height;
    mClass = std::make_shared<gfx::TextureMap2DClass>();
    mClass->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
    mClass->SetTexture(std::move(source));
    mClass->SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
    mClass->SetGamma(1.0f);
    mMaterial = gfx::CreateMaterialInstance(mClass);
    SetValue(mUI.imageFile, info.absoluteFilePath());
    SetValue(mUI.zoom, scale);
    SetEnabled(mUI.btnExport, true);

    SplitIntoTiles();
}

void DlgFontMap::on_cmbColorSpace_currentIndexChanged(int)
{
    if (!mClass)
        return;
    auto* source = mClass->GetTextureSource();
    auto* file_source = dynamic_cast<gfx::detail::TextureFileSource*>(source);
    file_source->SetColorSpace(GetValue(mUI.cmbColorSpace));
}

void DlgFontMap::on_widgetColor_colorChanged(QColor color)
{
    mUI.widget->SetClearColor(ToGfx(color));
}

void DlgFontMap::on_tileWidth_valueChanged(int)
{
    SplitIntoTiles();
}
void DlgFontMap::on_tileHeight_valueChanged(int)
{
    SplitIntoTiles();
}

void DlgFontMap::on_offsetX_valueChanged(int)
{
    SplitIntoTiles();
}
void DlgFontMap::on_offsetY_valueChanged(int)
{
    SplitIntoTiles();
}

void DlgFontMap::on_btnSelectImage_clicked()
{
    const auto ret = QFileDialog::getOpenFileName(this,
        tr("Select Image File"), "",
        tr("Images (*.png *.jpg *.jpeg)"));
    if (ret.isEmpty())
        return;
    LoadImage(ret);
}

void DlgFontMap::on_btnExport_clicked()
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

    // we're cranking out JSON that is compatible with the
    // JSON expected by the custom bitmap font implementation
    // in gfx/text. Also, the image packer can produce compatible
    // JSOn. (See DlgImgPack)
    nlohmann::json json;
    base::JsonWrite(json, "json_version", 1);
    base::JsonWrite(json, "made_with_app", APP_TITLE);
    base::JsonWrite(json, "made_with_ver", APP_VERSION);
    base::JsonWrite(json, "image_file", app::ToUtf8(image_file_info.fileName()));
    base::JsonWrite(json, "image_width", mWidth);
    base::JsonWrite(json, "image_height", mHeight);
    base::JsonWrite(json, "font_width", tile_width);
    base::JsonWrite(json, "font_height", tile_height);
    base::JsonWrite(json, "xoffset", tile_xoffset);
    base::JsonWrite(json, "yoffset", tile_yoffset);
    base::JsonWrite(json, "premultiply_alpha_hint", (bool)GetValue(mUI.chkAlpha));
    base::JsonWrite(json, "case_sensitive", (bool)GetValue(mUI.chkCaseSensitive));
    base::JsonWrite(json, "color_space", (gfx::detail::TextureFileSource::ColorSpace)GetValue(mUI.cmbColorSpace));
;
    const auto max_rows = (mHeight-tile_yoffset) / tile_height;
    const auto max_cols = (mWidth-tile_xoffset) / tile_width;

    for (unsigned row=0; row<max_rows; ++row)
    {
        for (unsigned col=0; col<max_cols; ++col)
        {
            const auto index = row * max_cols + col;
            ASSERT(index < mTiles.size());
            const auto& tile = mTiles[index];
            const auto tile_xpos = tile_xoffset + col * tile_width;
            const auto tile_ypos = tile_yoffset + row * tile_height;

            nlohmann::json tile_json;
            base::JsonWrite(tile_json, "xpos", tile_xpos);
            base::JsonWrite(tile_json, "ypos", tile_ypos);
            base::JsonWrite(tile_json, "char", tile.key); // utf8 encoded Unicode character
            json["images"].push_back(std::move(tile_json));
        }
    }
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

void DlgFontMap::on_btnClose_clicked()
{
    mClosed = true;
    close();
}

void DlgFontMap::finished()
{
    mClosed = true;
    mUI.widget->dispose();
}

void DlgFontMap::timer()
{
    mUI.widget->triggerPaint();
}

void DlgFontMap::SelectTile()
{
    mSelectedIndex.reset();

    const float width  = mUI.widget->width();
    const float height = mUI.widget->height();
    const float zoom   = GetValue(mUI.zoom);
    const unsigned tile_xoffset = GetValue(mUI.offsetX);
    const unsigned tile_yoffset = GetValue(mUI.offsetY);
    const unsigned tile_width  = GetValue(mUI.tileWidth);
    const unsigned tile_height = GetValue(mUI.tileHeight);
    const auto max_rows = (mHeight-tile_yoffset) / tile_height;
    const auto max_cols = (mWidth-tile_xoffset) / tile_width;
    const float img_width  = mWidth * zoom;
    const float img_height = mHeight * zoom;
    const auto xpos = (width - img_width) * 0.5f;
    const auto ypos = (height - img_height) * 0.5f;

    const int mouse_posx = (mCurrentPoint.x() - mTrackingOffset.x() - xpos) / zoom;
    const int mouse_posy = (mCurrentPoint.y() - mTrackingOffset.y() - ypos) / zoom;
    const int current_row = (mouse_posy-tile_yoffset) / tile_height;
    const int current_col = (mouse_posx-tile_xoffset) / tile_width;
    if (current_row >= max_rows || current_row < 0)
        return;
    else if (current_col >= max_cols || current_col < 0)
        return;

    mSelectedIndex = current_row * max_cols + current_col;
}

void DlgFontMap::SplitIntoTiles()
{
    const unsigned xoffset = GetValue(mUI.offsetX);
    const unsigned yoffset = GetValue(mUI.offsetY);
    const unsigned tile_width  = GetValue(mUI.tileWidth);
    const unsigned tile_height = GetValue(mUI.tileHeight);
    const auto rows = (mHeight - yoffset) / tile_height;
    const auto cols = (mWidth -xoffset) / tile_width;
    mTiles.resize(rows * cols);
}

void DlgFontMap::OnPaintScene(gfx::Painter& painter, double secs)
{
    SetValue(mUI.widgetColor, mUI.widget->GetCurrentClearColor());

    const float width  = mUI.widget->width();
    const float height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    if (!mMaterial)
    {
        ShowInstruction(
            "Assign font glyphs to Unicode characters.\n\n"
            "INSTRUCTIONS\n"
            "1. Select pre-generated font character texture map.\n"
            "2. Adjust the image offset and glyph sizes.\n"
            "3. Click on any font glyph.\n"
            "4. Press keys to assign a character value.\n"
            "5. When done, click on 'Export' to export the font JSON.\n",
            gfx::FRect(0, 0, width, height),
            painter);
        return;
    }

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
    const auto max_rows = (mHeight-tile_yoffset) / tile_height;
    const auto max_cols = (mWidth-tile_xoffset) / tile_width;
    const int mouse_posx = (mCurrentPoint.x() - mTrackingOffset.x() - xpos) / zoom;
    const int mouse_posy = (mCurrentPoint.y() - mTrackingOffset.y() - ypos) / zoom;
    const int current_row = (mouse_posy-tile_yoffset) / tile_height;
    const int current_col = (mouse_posx-tile_xoffset) / tile_width;

    const gfx::Color4f grid_color(gfx::Color::HotPink, 0.2);
    auto selection_material_class = gfx::CreateMaterialClassFromImage("app://textures/accept_icon.png");
    selection_material_class.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
    selection_material_class.SetBaseColor(gfx::Color4f(1.0f, 1.0f, 1.0f, 1.0f));
    auto selection_material = gfx::MaterialClassInst(selection_material_class);

    for (unsigned row=0; row<max_rows; ++row)
    {
        for (unsigned col=0; col<max_cols; ++col)
        {
            const auto tile_index  = row * max_cols + col;
            const auto& tile_value = base::SafeIndex(mTiles, tile_index);

            gfx::FRect tile(0.0f, 0.0f, tile_width*zoom, tile_height*zoom);
            tile.Translate(xpos, ypos);
            tile.Translate(mTrackingOffset.x(), mTrackingOffset.y());
            tile.Translate(tile_xoffset * zoom, tile_yoffset * zoom);
            tile.Translate(col * tile_width * zoom, row * tile_height * zoom);
            if (show_grid)
            {
                gfx::DrawRectOutline(painter, tile, gfx::CreateMaterialFromColor(grid_color));
            }
            if (row == current_row && col == current_col)
            {
                gfx::DrawRectOutline(painter, tile, gfx::CreateMaterialFromColor(gfx::Color::HotPink));
            }
            if (mSelectedIndex.value_or(mTiles.size()) == tile_index)
            {
                gfx::DrawRectOutline(painter, tile, gfx::CreateMaterialFromColor(gfx::Color::Green));
                ShowMessage(base::FormatString("Assigned key: '%1'", tile_value.key), painter);
            }

            if (!tile_value.key.empty())
            {
                tile.SetWidth(tile_width*0.5);
                tile.SetHeight(tile_height*0.5);
                gfx::FillShape(painter, tile, gfx::Circle(), selection_material);
            }
        }
    }
}
void DlgFontMap::OnMousePress(QMouseEvent* mickey)
{
    if (mickey->button() == Qt::LeftButton)
    {
        SelectTile();
        mMode = Mode::Selecting;
    }
    else if (mickey->button() == Qt::RightButton)
        mMode = Mode::Tracking;

    mStartPoint = mickey->pos();
}
void DlgFontMap::OnMouseMove(QMouseEvent* mickey)
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
void DlgFontMap::OnMouseRelease(QMouseEvent* mickey)
{
    mMode = Mode::Nada;
}

bool DlgFontMap::OnKeyPress(QKeyEvent* event)
{
    const auto key   = event->key();
    const auto shift = event->modifiers() & Qt::ShiftModifier;
    if (mSelectedIndex.has_value())
    {
        auto& tile = mTiles[mSelectedIndex.value()];
        if (key == Qt::Key_Backspace)
            tile.key.clear();
        else tile.key = app::ToUtf8(event->text());
        return true;
    }

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
