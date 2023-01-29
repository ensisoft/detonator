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
#  include <QFileDialog>
#  include <QFileInfo>
#  include <QEventLoop>
#include "warnpop.h"

#include <algorithm>

#include "base/assert.h"
#include "base/utility.h"
#include "editor/app/resource.h"
#include "editor/app/eventlog.h"
#include "editor/app/workspace.h"
#include "editor/app/utility.h"
#include "editor/gui/utility.h"
#include "editor/gui/drawing.h"
#include "editor/gui/dlgtileimport.h"
#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/drawing.h"

namespace {
    enum class MaterialType {
        Texture, Sprite
    };
    enum class TextureCutting {
        UseOriginal,
        CutNewTexture
    };

    enum class ImageFormat {
        PNG, JPG
    };
}// namespace

namespace gui
{
ImportedTile::ImportedTile(QWidget* parent)
  : QWidget(parent)
{
    mUI.setupUi(this);
}

void ImportedTile::SetPreview(QPixmap pix)
{
    mUI.preview->setPixmap(pix);
}

QString ImportedTile::GetName() const
{
    return GetValue(mUI.name);
}

void ImportedTile::SetName(const QString& name)
{
    SetValue(mUI.name, name);
}

DlgTileImport::DlgTileImport(QWidget* parent, app::Workspace* workspace)
  : QDialog(parent)
  , mWorkspace(workspace)
{
    mUI.setupUi(this);
    mUI.widget->onPaintScene   = std::bind(&DlgTileImport::OnPaintScene,   this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onMouseMove    = std::bind(&DlgTileImport::OnMouseMove,    this, std::placeholders::_1);
    mUI.widget->onMousePress   = std::bind(&DlgTileImport::OnMousePress,   this, std::placeholders::_1);
    mUI.widget->onMouseRelease = std::bind(&DlgTileImport::OnMouseRelease, this, std::placeholders::_1);
    mUI.widget->onKeyPress     = std::bind(&DlgTileImport::OnKeyPress,     this, std::placeholders::_1);
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
    connect(this, &QDialog::finished, this, &DlgTileImport::finished);
    connect(&mTimer, &QTimer::timeout, this, &DlgTileImport::timer);

    PopulateFromEnum<gfx::MaterialClass::SurfaceType>(mUI.surfaceType);
    PopulateFromEnum<gfx::MaterialClass::MinTextureFilter>(mUI.minFilter);
    PopulateFromEnum<gfx::MaterialClass::MagTextureFilter>(mUI.magFilter);
    PopulateFromEnum<gfx::detail::TextureFileSource::ColorSpace>(mUI.cmbColorSpace);
    PopulateFromEnum<MaterialType>(mUI.materialType);
    PopulateFromEnum<TextureCutting>(mUI.cmbCutting);
    PopulateFromEnum<ImageFormat>(mUI.cmbImageFormat);
    SetVisible(mUI.progressBar, false);
    SetValue(mUI.zoom, 1.0f);

    on_cmbCutting_currentIndexChanged(0);
    on_materialType_currentIndexChanged(0);

    LoadState();
}
DlgTileImport::~DlgTileImport()
{
    for (auto& tile : mTiles)
    {
        delete tile.widget;
    }
}

void DlgTileImport::on_btnSelectFile_clicked()
{
    const auto ret = QFileDialog::getOpenFileName(this,
        tr("Select Image File"), "",
        tr("Images (*.png *.jpg *.jpeg)"));
    if (ret.isEmpty()) return;

    LoadFile(ret);
}

void DlgTileImport::on_btnSelectAll_clicked()
{
    for (auto& tile : mTiles)
        tile.selected = true;
}

void DlgTileImport::on_btnSelectNone_clicked()
{
    for (auto& tile : mTiles)
        tile.selected = false;
}

void DlgTileImport::on_btnClose_clicked()
{
    SaveState();

    close();
}

void DlgTileImport::on_btnImport_clicked()
{
    const unsigned tile_xoffset = GetValue(mUI.offsetX);
    const unsigned tile_yoffset = GetValue(mUI.offsetY);
    const unsigned tile_width  = GetValue(mUI.tileWidth);
    const unsigned tile_height = GetValue(mUI.tileHeight);
    const auto rows = (mHeight-tile_yoffset) / tile_height;
    const auto cols = (mWidth-tile_xoffset) / tile_width;

    const unsigned tile_margin = math::clamp(0u, std::min(tile_width, tile_height)/2u, (unsigned)GetValue(mUI.tileMargin));
    const bool premul_alpha = GetValue(mUI.chkPremulAlpha);
    const bool premul_alpha_blend = GetValue(mUI.chkPremulAlphaBlend);
    const float img_height = mHeight;
    const float img_width = mWidth;

    std::vector<std::string> texture_uris;

    const MaterialType type  = GetValue(mUI.materialType);
    const ImageFormat format = GetValue(mUI.cmbImageFormat);
    const TextureCutting cutting = GetValue(mUI.cmbCutting);

    AutoHider hider(mUI.progressBar);
    AutoEnabler close(mUI.btnClose);
    AutoEnabler import(mUI.btnImport);
    QEventLoop footgun;

    // compute how much work there's to do
    unsigned work_tasks = 0;
    for (auto& tile : mTiles)
        if (tile.selected) ++work_tasks;

    SetValue(mUI.progressBar, 0);
    SetRange(mUI.progressBar, 0, work_tasks);

    if (cutting == TextureCutting::CutNewTexture)
    {
        SetRange(mUI.progressBar, 0, work_tasks * 2);
        SetValue(mUI.progressBar, "Cutting textures ... %p% ");

        QImage img;
        auto* src = mClass->GetTextureSource();
        // the bitmap must outlive the QImage object that is constructed
        // QImage will not take ownership / copy the data!
        auto bitmap = src->GetData();

        if (bitmap)
        {
            const auto width  = bitmap->GetWidth();
            const auto height = bitmap->GetHeight();
            const auto depth  = bitmap->GetDepthBits();
            if (depth == 0) img = QImage((const uchar*)bitmap->GetDataPtr(), width, height, width, QImage::Format_Grayscale8);
            else if (depth == 24) img = QImage((const uchar*)bitmap->GetDataPtr(), width, height, width * 3, QImage::Format_RGB888);
            else if (depth == 32) img = QImage((const uchar*)bitmap->GetDataPtr(), width, height, width * 4, QImage::Format_RGBA8888);
            else ERROR("Failed to load texture into QImage. Unexpected bit depth. depth=[%1']", depth);
        }

        if (img.isNull())
        {
            QMessageBox msg(this);
            msg.setIcon(QMessageBox::Critical);
            msg.setStandardButtons(QMessageBox::StandardButton::Ok);
            msg.setText("Failed to load the source texture.");
            msg.exec();
            return;
        }
        QString folder = GetValue(mUI.folder);
        if (folder.isEmpty())
            folder = "textures";
        QString dir = mWorkspace->GetSubDir(folder);
        QString ext;
        if (format == ImageFormat::PNG)
            ext = ".png";
        else if (format == ImageFormat::JPG)
            ext = ".jpg";
        else BUG("Missing image format case.");

        bool errors = false;

        for (unsigned row=0; row<rows; ++row)
        {
            for (unsigned col=0; col<cols; ++col)
            {
                const auto index = row * cols + col;
                ASSERT(index < mTiles.size());
                if (!mTiles[index].selected)
                    continue;
                QString name = mTiles[index].widget->GetName();
                QString filename = app::toString("%1_%2x%3_@_r%4_c%5", name, tile_width, tile_height, row, col);
                QString filepath = app::JoinPath(dir, filename);
                filepath += ext;
                QString uri = mWorkspace->MapFileToWorkspace(filepath);

                const auto tile_xpos = col * tile_width + tile_xoffset;
                const auto tile_ypos = row * tile_height + tile_yoffset;
                QImage tile = img.copy(tile_xpos, tile_ypos, tile_width, tile_height);
                QImageWriter writer;
                writer.setFileName(filepath);
                writer.setQuality(GetValue(mUI.imageQuality));
                if (format == ImageFormat::PNG)
                    writer.setFormat("PNG");
                else if (format == ImageFormat::JPG)
                    writer.setFormat("JPG");
                else BUG("Missing image format case.");
                if (!writer.write(tile))
                {
                    ERROR("Failed to write image file. [file='%1', error='%2']", filepath, writer.errorString());
                    errors = true;
                    break;
                }
                if (texture_uris.size() <= index)
                    texture_uris.resize(index + 1);
                texture_uris[index] = app::ToUtf8(uri);

                if (!Increment(mUI.progressBar) % 10)
                    footgun.processEvents();
            }
        }
        if (errors)
        {
            QMessageBox msg(this);
            msg.setIcon(QMessageBox::Critical);
            msg.setStandardButtons(QMessageBox::Ok);
            msg.setText("There were errors while writing the tile image files.");
            msg.exec();
            return;
        }
    }

    if (type == MaterialType::Texture)
    {
        SetValue(mUI.progressBar, "Making materials ... %p% ");

        for (unsigned row=0; row<rows; ++row)
        {
            for (unsigned col = 0; col < cols; ++col)
            {
                const auto index = row * cols + col;
                ASSERT(index < mTiles.size());
                if (!mTiles[index].selected)
                    continue;

                auto* widget = mTiles[index].widget;

                QString name = widget->GetName();

                gfx::detail::TextureFileSource texture;
                texture.SetColorSpace(GetValue(mUI.cmbColorSpace));
                texture.SetFlag(gfx::detail::TextureFileSource::Flags::PremulAlpha, premul_alpha);
                if (cutting == TextureCutting::UseOriginal)
                {
                    texture.SetFileName(mFileUri);
                    texture.SetName(mFileName);
                }
                else if(cutting == TextureCutting::CutNewTexture)
                {
                    texture.SetFileName(texture_uris[index]);
                    texture.SetName("Tile");
                } else BUG("Missing texture cut case.");

                gfx::TextureMap2DClass klass;
                klass.SetSurfaceType(GetValue(mUI.surfaceType));
                klass.SetTexture(texture.Copy());
                klass.SetTextureMinFilter(GetValue(mUI.minFilter));
                klass.SetTextureMagFilter(GetValue(mUI.magFilter));
                klass.SetFlag(gfx::TextureMap2DClass::Flags::PremultipliedAlpha, premul_alpha_blend);

                if (cutting == TextureCutting::UseOriginal)
                {
                    const auto rect = gfx::FRect(
                            (col * tile_width + tile_margin + tile_xoffset) / img_width,
                            (row * tile_height + tile_margin + tile_yoffset) / img_height,
                            (tile_width - tile_margin * 2) / img_width,
                            (tile_height - tile_margin * 2) / img_height);
                    klass.SetTextureRect(rect);
                }
                else if (cutting == TextureCutting::CutNewTexture)
                {
                    klass.SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
                } else BUG("Missing texture cut case.");

                app::MaterialResource res(klass, name);
                mWorkspace->SaveResource(res);

                if (!Increment(mUI.progressBar) % 10)
                    footgun.processEvents();
            }
        }
    }
    else if (type == MaterialType::Sprite)
    {
        gfx::SpriteClass klass;
        klass.SetSurfaceType(GetValue(mUI.surfaceType));
        klass.SetTextureMinFilter(GetValue(mUI.minFilter));
        klass.SetTextureMagFilter(GetValue(mUI.magFilter));
        klass.SetFlag(gfx::TextureMap2DClass::Flags::PremultipliedAlpha, premul_alpha_blend);
        klass.SetName(GetValue(mUI.spriteName));

        SetValue(mUI.progressBar, "Making sprite ... %p% ");

        for (unsigned row=0; row<rows; ++row)
        {
            for (unsigned col=0; col<cols; ++col)
            {
                const auto index = row * cols + col;
                ASSERT(index < mTiles.size());
                if (!mTiles[index].selected)
                    continue;

                auto* widget = mTiles[index].widget;

                QString name = widget->GetName();

                gfx::detail::TextureFileSource texture;
                texture.SetColorSpace(GetValue(mUI.cmbColorSpace));
                texture.SetFlag(gfx::detail::TextureFileSource::Flags::PremulAlpha, premul_alpha);
                if (cutting == TextureCutting::UseOriginal)
                {
                    texture.SetFileName(mFileUri);
                    texture.SetName(mFileName);
                    const auto rect = gfx::FRect(
                            (col * tile_width + tile_margin + tile_xoffset) / img_width,
                            (row * tile_height + tile_margin + tile_yoffset) / img_height,
                            (tile_width - tile_margin*2) / img_width,
                            (tile_height - tile_margin*2) / img_height);
                    klass.AddTexture(texture.Copy(), rect);
                }
                else if (cutting == TextureCutting::CutNewTexture)
                {
                    texture.SetFileName(texture_uris[index]);
                    texture.SetName("Tile");
                    klass.AddTexture(texture.Copy(), gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
                } else BUG("Missing texture cut case.");

                if (!Increment(mUI.progressBar) % 10)
                    footgun.processEvents();
            }
        }
        app::MaterialResource res(klass, GetValue(mUI.spriteName));
        mWorkspace->SaveResource(res);

    } else BUG("Missing material type handling.");
}

void DlgTileImport::on_tabWidget_currentChanged(int tab)
{
    if (tab != 1)
        return;

    if (!mMaterial)
        return;

    for (auto& tile : mTiles)
    {
        delete tile.widget;
        tile.widget = nullptr;
    }

    const unsigned tile_xoffset = GetValue(mUI.offsetX);
    const unsigned tile_yoffset = GetValue(mUI.offsetY);
    const unsigned tile_width  = GetValue(mUI.tileWidth);
    const unsigned tile_height = GetValue(mUI.tileHeight);
    const auto rows = (mHeight-tile_yoffset) / tile_height;
    const auto cols = (mWidth-tile_xoffset) / tile_width;

    QPixmap pixmap;

    auto* src = mClass->GetTextureSource();

    if (auto bitmap = src->GetData())
    {
        const auto width  = bitmap->GetWidth();
        const auto height = bitmap->GetHeight();
        const auto depth  = bitmap->GetDepthBits();
        QImage img;
        if (depth == 0)
            img = QImage((const uchar*)bitmap->GetDataPtr(), width, height, width, QImage::Format_Grayscale8);
        else if (depth == 24)
            img = QImage((const uchar*)bitmap->GetDataPtr(), width, height, width * 3, QImage::Format_RGB888);
        else if (depth == 32)
            img = QImage((const uchar*)bitmap->GetDataPtr(), width, height, width * 4, QImage::Format_RGBA8888);
        else ERROR("Failed to load texture preview. Unexpected bit depth. [depth=%1]", depth);
        pixmap.convertFromImage(img);
    }

    for (unsigned row=0; row<rows; ++row)
    {
        for (unsigned col=0; col<cols; ++col)
        {
            const auto index = row * cols + col;
            ASSERT(index < mTiles.size());
            if (!mTiles[index].selected)
                continue;

            auto*  widget = new ImportedTile(this);
            mTiles[index].widget = widget;
            mUI.layout->addWidget(widget);

            const auto x = col * tile_width + tile_xoffset;
            const auto y = row * tile_height + tile_yoffset;
            widget->SetPreview(pixmap.copy(x, y, tile_width, tile_height));
        }
    }

    SetValue(mUI.renameTiles, QString(""));
}

void DlgTileImport::on_tileWidth_valueChanged(int)
{
    SplitIntoTiles();
}
void DlgTileImport::on_tileHeight_valueChanged(int)
{
    SplitIntoTiles();
}

void DlgTileImport::on_offsetX_valueChanged(int)
{
    SplitIntoTiles();
}
void DlgTileImport::on_offsetY_valueChanged(int)
{
    SplitIntoTiles();
}

void DlgTileImport::on_renameTiles_textChanged(const QString& name)
{
    for (auto& tile : mTiles)
    {
        if (tile.widget)
            tile.widget->SetName(name);
    }
}

void DlgTileImport::on_widgetColor_colorChanged(QColor color)
{
    mUI.widget->SetClearColor(ToGfx(color));
}

void DlgTileImport::on_materialType_currentIndexChanged(int)
{
    const MaterialType type = GetValue(mUI.materialType);
    if(type == MaterialType::Sprite)
    {
        SetEnabled(mUI.spriteName, true);
    }
    else
    {
        SetEnabled(mUI.spriteName, false);
    }
}

void DlgTileImport::on_cmbColorSpace_currentIndexChanged(int)
{
    if (!mClass)
        return;
    auto* source = mClass->GetTextureSource();
    auto* file_source = dynamic_cast<gfx::detail::TextureFileSource*>(source);
    file_source->SetColorSpace(GetValue(mUI.cmbColorSpace));
}

void DlgTileImport::on_cmbCutting_currentIndexChanged(int)
{
    const TextureCutting cutting = GetValue(mUI.cmbCutting);
    if (cutting == TextureCutting::UseOriginal)
    {
        SetEnabled(mUI.cmbImageFormat, false);
        SetEnabled(mUI.imageQuality, false);
        SetEnabled(mUI.folder, false);
        SetEnabled(mUI.tileMargin, true);
    }
    else
    {
        SetEnabled(mUI.cmbImageFormat, true);
        SetEnabled(mUI.imageQuality, true);
        SetEnabled(mUI.folder, true);
        SetEnabled(mUI.tileMargin, false);
    }
}

void DlgTileImport::finished()
{
    mUI.widget->dispose();
}
void DlgTileImport::timer()
{
    mUI.widget->triggerPaint();
}

void DlgTileImport::ToggleSelection()
{
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

    const auto tile_index = current_row * max_cols + current_col;
    if (base::Contains(mTilesTouched, tile_index))
        return;

    ASSERT(tile_index < mTiles.size());
    mTiles[tile_index].selected = !mTiles[tile_index].selected;
    mTilesTouched.insert(tile_index);
}

void DlgTileImport::SplitIntoTiles()
{
    const unsigned xoffset = GetValue(mUI.offsetX);
    const unsigned yoffset = GetValue(mUI.offsetY);
    const unsigned tile_width  = GetValue(mUI.tileWidth);
    const unsigned tile_height = GetValue(mUI.tileHeight);
    const auto rows = (mHeight - yoffset) / tile_height;
    const auto cols = (mWidth -xoffset) / tile_width;
    mTiles.resize(rows * cols);
}

void DlgTileImport::LoadFile(const QString& ret)
{
    const QFileInfo info(ret);
    const auto& name = info.baseName();
    const auto& file = mWorkspace->MapFileToWorkspace(info.absoluteFilePath());

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

    mWidth    = bitmap->GetWidth();
    mHeight   = bitmap->GetHeight();
    mFileUri  = std::move(file_uri);
    mFileName = std::move(file_name);

    mClass = std::make_shared<gfx::TextureMap2DClass>();
    mClass->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
    mClass->SetTexture(std::move(source));
    mClass->SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
    mClass->SetGamma(1.0f);
    mMaterial = gfx::CreateMaterialInstance(mClass);
    SetValue(mUI.fileSource, info.absoluteFilePath());

    SplitIntoTiles();
}

void DlgTileImport::LoadState()
{
    QByteArray geometry;
    if (GetUserProperty(*mWorkspace, "dlg-tile-import-geometry", &geometry))
        restoreGeometry(geometry);

    int xpos = 0;
    int ypos = 0;


    GetUserProperty(*mWorkspace, "dlg-tile-import-color-space", mUI.cmbColorSpace);
    GetUserProperty(*mWorkspace, "dlg-tile-import-zoom", mUI.zoom);
    GetUserProperty(*mWorkspace, "dlg-tile-import-color", mUI.widget);
    GetUserProperty(*mWorkspace, "dlg-tile-import-material-type", mUI.materialType);
    GetUserProperty(*mWorkspace, "dlg-tile-import-material-surface", mUI.surfaceType);
    SetUserProperty(*mWorkspace, "dlg-tile-import-sprite-name", mUI.spriteName);
    GetUserProperty(*mWorkspace, "dlg-tile-import-min-filter", mUI.minFilter);
    GetUserProperty(*mWorkspace, "dlg-tile-import-mag-filter", mUI.magFilter);
    GetUserProperty(*mWorkspace, "dlg-tile-import-tile-width", mUI.tileWidth);
    GetUserProperty(*mWorkspace, "dlg-tile-import-tile-height", mUI.tileHeight);
    GetUserProperty(*mWorkspace, "dlg-tile-import-tile-margin", mUI.tileMargin);
    GetUserProperty(*mWorkspace, "dlg-tile-import-premul-alpha", mUI.chkPremulAlpha);
    GetUserProperty(*mWorkspace, "dlg-tile-import-premul-alpha-blend", mUI.chkPremulAlphaBlend);
    GetUserProperty(*mWorkspace, "dlg-tile-import-cut-texture", mUI.cmbCutting);
    GetUserProperty(*mWorkspace, "dlg-tile-import-img-format", mUI.cmbImageFormat);
    GetUserProperty(*mWorkspace, "dlg-tile-import-img-quality", mUI.imageQuality);
    GetUserProperty(*mWorkspace, "dlg-tile-import-img-folder", mUI.folder);
    GetUserProperty(*mWorkspace, "dlg-tile-import-offset-x", mUI.offsetX);
    GetUserProperty(*mWorkspace, "dlg-tile-import-offset-y", mUI.offsetY);
    GetUserProperty(*mWorkspace, "dlg-tile-import-xpos", &xpos);
    GetUserProperty(*mWorkspace, "dlg-tile-import-ypos", &ypos);

    QString file;
    if (GetUserProperty(*mWorkspace, "dlg-tile-import-file", &file))
        LoadFile(file);

    mTrackingOffset = QPoint(xpos, ypos);

    on_cmbCutting_currentIndexChanged(0);
    on_materialType_currentIndexChanged(0);
}
void DlgTileImport::SaveState()
{
    QString file = GetValue(mUI.fileSource);

    SetUserProperty(*mWorkspace, "dlg-tile-import-geometry", saveGeometry());
    SetUserProperty(*mWorkspace, "dlg-tile-import-color-space", mUI.cmbColorSpace);
    SetUserProperty(*mWorkspace, "dlg-tile-import-zoom", mUI.zoom);
    SetUserProperty(*mWorkspace, "dlg-tile-import-color", mUI.widget);
    SetUserProperty(*mWorkspace, "dlg-tile-import-material-type", mUI.materialType);
    SetUserProperty(*mWorkspace, "dlg-tile-import-material-surface", mUI.surfaceType);
    SetUserProperty(*mWorkspace, "dlg-tile-import-sprite-name", mUI.spriteName);
    SetUserProperty(*mWorkspace, "dlg-tile-import-min-filter", mUI.minFilter);
    SetUserProperty(*mWorkspace, "dlg-tile-import-mag-filter", mUI.magFilter);
    SetUserProperty(*mWorkspace, "dlg-tile-import-tile-width", mUI.tileWidth);
    SetUserProperty(*mWorkspace, "dlg-tile-import-tile-height", mUI.tileHeight);
    SetUserProperty(*mWorkspace, "dlg-tile-import-tile-margin", mUI.tileMargin);
    SetUserProperty(*mWorkspace, "dlg-tile-import-premul-alpha", mUI.chkPremulAlpha);
    SetUserProperty(*mWorkspace, "dlg-tile-import-premul-alpha-blend", mUI.chkPremulAlphaBlend);
    SetUserProperty(*mWorkspace, "dlg-tile-import-cut-texture", mUI.cmbCutting);
    SetUserProperty(*mWorkspace, "dlg-tile-import-img-format", mUI.cmbImageFormat);
    SetUserProperty(*mWorkspace, "dlg-tile-import-img-quality", mUI.imageQuality);
    SetUserProperty(*mWorkspace, "dlg-tile-import-img-folder", mUI.folder);
    SetUserProperty(*mWorkspace, "dlg-tile-import-offset-x", mUI.offsetX);
    SetUserProperty(*mWorkspace, "dlg-tile-import-offset-y", mUI.offsetY);
    SetUserProperty(*mWorkspace, "dlg-tile-import-xpos", mTrackingOffset.x());
    SetUserProperty(*mWorkspace, "dlg-tile-import-ypos", mTrackingOffset.y());
    SetUserProperty(*mWorkspace, "dlg-tile-import-file", file);
}

void DlgTileImport::OnPaintScene(gfx::Painter& painter, double secs)
{
    SetValue(mUI.widgetColor, mUI.widget->GetCurrentClearColor());

    if (!mMaterial)
        return;

    const float width  = mUI.widget->width();
    const float height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    const float zoom   = GetValue(mUI.zoom);
    const float img_width  = mWidth * zoom;
    const float img_height = mHeight * zoom;
    const auto xpos = (width - img_width) * 0.5f;
    const auto ypos = (height - img_height) * 0.5f;

    gfx::FRect img(0.0f, 0.0f, img_width, img_height);
    img.Translate(xpos, ypos);
    img.Translate(mTrackingOffset.x(), mTrackingOffset.y());
    gfx::FillRect(painter, img, *mMaterial);

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

    const bool grid = GetValue(mUI.chkGrid);

    auto selection_material_class = gfx::CreateMaterialClassFromTexture("app://textures/accept_icon.png");
    selection_material_class.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
    selection_material_class.SetBaseColor(gfx::Color4f(1.0f, 1.0f, 1.0f, 1.0f));
    auto selection_material = gfx::MaterialClassInst(selection_material_class);

    for (unsigned row=0; row<max_rows; ++row)
    {
        for (unsigned col=0; col<max_cols; ++col)
        {
            const auto tile_index  = row * max_cols + col;
            const auto& tile_value = mTiles[tile_index];
            ASSERT(tile_index < mTiles.size());

            const gfx::Color4f grid_color(gfx::Color::HotPink, 0.2);

            gfx::FRect tile(0.0f, 0.0f, tile_width*zoom, tile_height*zoom);
            tile.Translate(xpos, ypos);
            tile.Translate(mTrackingOffset.x(), mTrackingOffset.y());
            tile.Translate(tile_xoffset * zoom, tile_yoffset * zoom);
            tile.Translate(col * tile_width * zoom, row * tile_height * zoom);
            if (grid)
            {
                gfx::DrawRectOutline(painter, tile, gfx::CreateMaterialFromColor(grid_color));
            }
            if (row == current_row && col == current_col)
            {
                gfx::DrawRectOutline(painter, tile, gfx::CreateMaterialFromColor(gfx::Color::HotPink));
            }
            if (tile_value.selected)
            {
                tile.SetWidth(tile_width*0.5);
                tile.SetHeight(tile_height*0.5);
                gfx::FillShape(painter, tile, gfx::Circle(), selection_material);
            }
        }
    }
}
void DlgTileImport::OnMousePress(QMouseEvent* mickey)
{
    if (mickey->button() == Qt::LeftButton)
    {
        ToggleSelection();
        mMode = Mode::Selecting;
    }
    else if (mickey->button() == Qt::RightButton)
        mMode = Mode::Tracking;

    mStartPoint = mickey->pos();
}
void DlgTileImport::OnMouseMove(QMouseEvent* mickey)
{
    mCurrentPoint = mickey->pos();

    if (mMode == Mode::Selecting)
    {
        ToggleSelection();
    }
    else if (mMode == Mode::Tracking)
    {
        mTrackingOffset += (mCurrentPoint - mStartPoint);
        mStartPoint = mCurrentPoint;
    }
}
void DlgTileImport::OnMouseRelease(QMouseEvent* mickey)
{
    mMode = Mode::Nada;
    mTilesTouched.clear();
}

bool DlgTileImport::OnKeyPress(QKeyEvent* event)
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

