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
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <algorithm>

#include "base/assert.h"
#include "base/utility.h"
#include "base/json.h"
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

    // this is copy/paste from DlgImgView. refactor to a single place since the
    // functionality is the same.
    bool ReadTexturePack(const QString& file, std::vector<gui::DlgTileImport::Image>* out)
    {
        QString err_str;
        QFile::FileError err_val = QFile::FileError::NoError;
        const auto& buff = app::ReadBinaryFile(file, &err_val, &err_str);
        if (err_val != QFile::FileError::NoError)
        {
            ERROR("Failed to read file. [file='%1', error='%2']", file, err_str);
            return false;
        }

        const auto* beg  = buff.data();
        const auto* end  = buff.data() + buff.size();
        const auto& json = nlohmann::json::parse(beg, end, nullptr, false);
        if (json.is_discarded())
        {
            ERROR("Failed to parse JSON file. [file='%1']", file);
            return false;
        }

        if (json.contains("images") && json["images"].is_array())
        {
            for (const auto& img_json: json["images"].items())
            {
                const auto& obj = img_json.value();

                gui::DlgTileImport::Image img;
                std::string name;
                std::string character;
                unsigned width = 0;
                unsigned height = 0;

                // optional
                base::JsonReadSafe(obj, "name", &name);
                base::JsonReadSafe(obj, "index", &img.index);

                if (!base::JsonReadSafe(obj, "width", &img.width))
                    WARN("Image is missing 'width' attribute. [file='%1']", file);
                if (!base::JsonReadSafe(obj, "height", &img.height))
                    WARN("Image is missing 'height' attribute. [file='%1']", file);
                if (!base::JsonReadSafe(obj, "xpos", &img.xpos))
                    WARN("Image is missing 'xpos' attribute. [file='%1']", file);
                if (!base::JsonReadSafe(obj, "ypos", &img.ypos))
                    WARN("Image is missing 'ypos' attribute. [file='%1']", file);

                out->push_back(std::move(img));
            }
        }
        else
        {
            unsigned image_width  = 0;
            unsigned image_height = 0;
            unsigned tile_width  = 0;
            unsigned tile_height = 0;
            unsigned xoffset = 0;
            unsigned yoffset = 0;
            bool error = true;

            if (!base::JsonReadSafe(json, "image_width", &image_width))
                ERROR("Missing image_width property. [file='%1']", file);
            else if (!base::JsonReadSafe(json, "image_height", &image_height))
                ERROR("Missing image_height property. [file='%1']", file);
            else if (!base::JsonReadSafe(json, "tile_width", &tile_width))
                ERROR("Missing tile_width property. [file='%1']", file);
            else if (!base::JsonReadSafe(json, "tile_height", &tile_height))
                ERROR("Missing tile_height property. [file='%1']", file);
            else if (!base::JsonReadSafe(json, "xoffset", &xoffset))
                ERROR("Missing xoffset property.[file='%1']", file);
            else if (!base::JsonReadSafe(json, "yoffset", &yoffset))
                ERROR("Missing yoffset property. [file='%1']", file);
            else error = false;
            if (error) return false;

            const auto max_rows = (image_height - yoffset) / tile_height;
            const auto max_cols = (image_width - xoffset) / tile_width;
            for (unsigned row=0; row<max_rows; ++row)
            {
                for (unsigned col=0; col<max_cols; ++col)
                {
                    const auto index = row * max_cols + col;
                    const auto tile_xpos = xoffset + col * tile_width;
                    const auto tile_ypos = yoffset + row * tile_height;

                    gui::DlgTileImport::Image img;
                    img.width  = tile_width;
                    img.height = tile_height;
                    img.xpos   = tile_xpos;
                    img.ypos   = tile_ypos;
                    out->push_back(std::move(img));
                }
            }
        }
        INFO("Successfully parsed '%1'. %2 images found.", file, out->size());
        return true;
    }

}// namespace

namespace gui
{
ImportedTile::ImportedTile(QWidget* parent)
  : QWidget(parent)
{
    mUI.setupUi(this);
}

void ImportedTile::SetPreview(const QPixmap& pix)
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

void ImportedTile::InstallEventFilter(QObject* receiver)
{
    mUI.name->installEventFilter(receiver);
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
    mUI.widget->onInitScene = [this](unsigned, unsigned) {
        mTimer.setInterval(1000.0/60.0);
        mTimer.start();
    };
    connect(this, &QDialog::finished, this, &DlgTileImport::finished);
    connect(&mTimer, &QTimer::timeout, this, &DlgTileImport::timer);

    PopulateFromEnum<gfx::MaterialClass::SurfaceType>(mUI.surfaceType);
    PopulateFromEnum<gfx::MaterialClass::MinTextureFilter>(mUI.minFilter);
    PopulateFromEnum<gfx::MaterialClass::MagTextureFilter>(mUI.magFilter);
    PopulateFromEnum<gfx::detail::TextureFileSource::ColorSpace>(mUI.cmbColorSpace);
    PopulateFromEnum<gfx::MaterialClass::MinTextureFilter>(mUI.cmbMinFilter);
    PopulateFromEnum<gfx::MaterialClass::MagTextureFilter>(mUI.cmbMagFilter);
    PopulateFromEnum<MaterialType>(mUI.materialType);
    PopulateFromEnum<TextureCutting>(mUI.cmbCutting);
    PopulateFromEnum<ImageFormat>(mUI.cmbImageFormat);
    SetVisible(mUI.progressBar, false);
    SetValue(mUI.zoom, 1.0f);

    on_cmbCutting_currentIndexChanged(0);
    on_materialType_currentIndexChanged(0);

    mUI.renameTiles->installEventFilter(this);
    mUI.spriteName->installEventFilter(this);
    mUI.textureFolder->installEventFilter(this);
}

DlgTileImport::~DlgTileImport()
{
    for (auto& tile : mImages)
    {
        delete tile.widget;
    }
}

void DlgTileImport::on_btnSelectImage_clicked()
{
    const auto& file = QFileDialog::getOpenFileName(this,
        tr("Select Image File"), "",
        tr("Images (*.png *.jpg *.jpeg)"));
    if (file.isEmpty()) return;

    LoadImageFile(file);

    QString json = file;
    json.replace(".png", ".json", Qt::CaseInsensitive);
    if (app::FileExists(json))
        LoadJsonFile(json);
}

void DlgTileImport::on_btnSelectJson_clicked()
{
    const auto& file = QFileDialog::getOpenFileName(this,
        tr("Select Json File"), "",
        tr("Json (*.json)"));
    if (file.isEmpty())
        return;

    LoadJsonFile(file);

    QString img = file;
    img.replace(".json", ".png", Qt::CaseInsensitive);
    if (app::FileExists(img))
        LoadImageFile(img);
}

void DlgTileImport::on_btnSelectAll_clicked()
{
    for (auto& tile : mImages)
        tile.selected = true;
}

void DlgTileImport::on_btnSelectNone_clicked()
{
    for (auto& tile : mImages)
        tile.selected = false;
}

void DlgTileImport::on_btnClose_clicked()
{
    SaveState();

    close();
}

void DlgTileImport::on_btnImport_clicked()
{
    const auto tile_margin_top  = (unsigned)GetValue(mUI.tileMarginTop);
    const auto tile_margin_left = (unsigned)GetValue(mUI.tileMarginLeft);
    const auto tile_margin_right = (unsigned)GetValue(mUI.tileMarginRight);
    const auto tile_margin_bottom = (unsigned)GetValue(mUI.tileMarginBottom);
    const auto premul_alpha = (bool)GetValue(mUI.chkPremulAlpha);
    const auto premul_alpha_blend = (bool)GetValue(mUI.chkPremulAlphaBlend);
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
    int work_tasks = 0;
    for (auto& tile : mImages)
    {
        if (tile.selected)
            ++work_tasks;
    }
    if (work_tasks == 0)
        return;

    SetValue(mUI.progressBar, 0);
    SetRange(mUI.progressBar, 0, work_tasks);

    if (cutting == TextureCutting::CutNewTexture)
    {
        SetRange(mUI.progressBar, 0, work_tasks * 2);
        SetValue(mUI.progressBar, "Cutting textures ... %p% ");

        QImage source_img;
        auto* map = mClass->GetTextureMap(0);
        auto* src = map->GetTextureSource(0);
        // the bitmap must outlive the QImage object that is constructed
        // QImage will not take ownership / copy the data!
        auto bitmap = src->GetData();

        if (bitmap)
        {
            const auto width  = bitmap->GetWidth();
            const auto height = bitmap->GetHeight();
            const auto depth  = bitmap->GetDepthBits();
            if (depth == 0) source_img = QImage((const uchar*)bitmap->GetDataPtr(), width, height, width, QImage::Format_Grayscale8);
            else if (depth == 24) source_img = QImage((const uchar*)bitmap->GetDataPtr(), width, height, width * 3, QImage::Format_RGB888);
            else if (depth == 32) source_img = QImage((const uchar*)bitmap->GetDataPtr(), width, height, width * 4, QImage::Format_RGBA8888);
            else ERROR("Failed to load texture into QImage. Unexpected bit depth. depth=[%1']", depth);
        }

        if (source_img.isNull())
        {
            QMessageBox msg(this);
            msg.setIcon(QMessageBox::Critical);
            msg.setStandardButtons(QMessageBox::StandardButton::Ok);
            msg.setText("Failed to load the source texture.");
            msg.exec();
            return;
        }
        QString folder = GetValue(mUI.textureFolder);
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
        for (size_t index=0; index<mImages.size(); ++index)
        {
            const auto& img = mImages[index];
            if (!img.selected)
                continue;

            QString name = img.widget->GetName();
            QString filename = app::toString("%1_%2x%3", name, img.width, img.height);
            QString filepath = app::JoinPath(dir, filename);
            filepath += ext;
            QString uri = mWorkspace->MapFileToWorkspace(filepath);

            QImage tile = source_img.copy(img.xpos, img.ypos, img.width, img.height);
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

            if (Increment(mUI.progressBar))
                footgun.processEvents();
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

        for (size_t index=0; index<mImages.size(); ++index)
        {
            const auto& img  = mImages[index];
            if (!img.selected)
                continue;

            const auto& name = img.widget->GetName();
            gfx::detail::TextureFileSource texture;
            texture.SetColorSpace(GetValue(mUI.cmbColorSpace));
            texture.SetFlag(gfx::detail::TextureFileSource::Flags::PremulAlpha, premul_alpha);
            if (cutting == TextureCutting::UseOriginal)
            {
                texture.SetFileName(mFileUri);
                texture.SetName(app::ToUtf8(name));
            }
            else if (cutting == TextureCutting::CutNewTexture)
            {
                texture.SetFileName(texture_uris[index]);
                texture.SetName(app::ToUtf8(name));
            } else BUG("Missing texture cut case.");

            gfx::TextureMap2DClass klass(gfx::MaterialClass::Type::Texture, base::RandomString(10));
            klass.SetSurfaceType(GetValue(mUI.surfaceType));
            klass.SetTexture(texture.Copy());
            klass.SetTextureMinFilter(GetValue(mUI.minFilter));
            klass.SetTextureMagFilter(GetValue(mUI.magFilter));
            klass.SetName(app::ToUtf8(img.name));
            klass.SetFlag(gfx::TextureMap2DClass::Flags::PremultipliedAlpha, premul_alpha_blend);

            if (cutting == TextureCutting::UseOriginal)
            {
                const auto rect = gfx::FRect(
                  float(img.xpos + tile_margin_left) / img_width,
                  float(img.ypos + tile_margin_top) / img_height,
                  float(img.width - tile_margin_left - tile_margin_right) / img_width,
                  float(img.height - tile_margin_top - tile_margin_bottom) / img_height);
                klass.SetTextureRect(rect);
            }
            else if (cutting == TextureCutting::CutNewTexture)
            {
                klass.SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
            } else BUG("Missing texture cut case.");

            app::MaterialResource res(klass, name);
            mWorkspace->SaveResource(res);

            if (Increment(mUI.progressBar))
                footgun.processEvents();
        }
    }
    else if (type == MaterialType::Sprite)
    {
        auto map = std::make_unique<gfx::TextureMap>();
        map->SetName("Sprite");
        map->SetType(gfx::TextureMap::Type::Sprite);
        map->SetFps(GetValue(mUI.spriteFps));

        gfx::SpriteClass klass(gfx::MaterialClass::Type::Sprite, base::RandomString(10));
        klass.SetSurfaceType(GetValue(mUI.surfaceType));
        klass.SetTextureMinFilter(GetValue(mUI.minFilter));
        klass.SetTextureMagFilter(GetValue(mUI.magFilter));
        klass.SetFlag(gfx::TextureMap2DClass::Flags::PremultipliedAlpha, premul_alpha_blend);
        klass.SetName(GetValue(mUI.spriteName));
        klass.SetBlendFrames(GetValue(mUI.chkBlendFrames));
        klass.SetNumTextureMaps(1);
        klass.SetTextureMap(0, std::move(map));

        SetValue(mUI.progressBar, "Making sprite ... %p% ");

        // if the selected images / tiles are in one contiguous region
        // with regular size then the sprite can be optimized to use a single
        // sprite sheet instead of multiple different images, but only if
        // texture cutting is not being done.  Texture cutting forces the use
        // of separate image source files.
        if (cutting == TextureCutting::UseOriginal)
        {
            unsigned tile_width  = 0;
            unsigned tile_height = 0;
            unsigned tile_xpos   = 0;
            unsigned tile_ypos   = 0;
            unsigned tile_count  = 0;
            bool irregular_size  = false;
            bool disjoint_selection = false;
            base::URect rect;

            for (size_t index=0; index<mImages.size(); ++index)
            {
                const auto& img = mImages[index];
                if (!img.selected)
                    continue;

                base::URect tile;
                tile.Resize(img.width, img.height);
                tile.Translate(img.xpos, img.ypos);
                rect = base::Union(rect, tile);

                if (tile_width == 0 && tile_height == 0)
                {
                    tile_width  = img.width;
                    tile_height = img.height;
                    tile_xpos   = img.xpos;
                    tile_ypos   = img.ypos;
                    rect        = tile;
                    ++tile_count;
                    continue;
                }
                if ((img.width != tile_width) || (img.height != tile_height))
                {
                    irregular_size = true;
                    break;
                }
                if ((img.xpos != tile_xpos) && (img.ypos != tile_ypos))
                {
                    disjoint_selection = true;
                    break;
                }
                if (img.xpos == tile_xpos)
                {
                    if (img.ypos != tile_height * tile_count)
                    {
                        disjoint_selection = true;
                        break;
                    }
                }
                if (img.ypos == tile_ypos)
                {
                    if (img.xpos != tile_width * tile_count)
                    {
                        disjoint_selection = true;
                        break;
                    }
                }
                ++tile_count;
            }
            if (!irregular_size && !disjoint_selection)
            {
                ASSERT(tile_width && tile_height);
                gfx::TextureMap::SpriteSheet sprite;
                sprite.cols = rect.GetWidth() / tile_width;
                sprite.rows = rect.GetHeight() / tile_height;
                ASSERT(sprite.cols && sprite.rows);
                DEBUG("Using optimized single spritesheet with regular tile size %1x%2", tile_width, tile_height);

                const auto width  = rect.GetWidth();
                const auto height = rect.GetHeight();
                rect.Translate(tile_margin_left, tile_margin_top);
                rect.SetWidth(width - tile_margin_left - tile_margin_right);
                rect.SetHeight(height - tile_margin_top - tile_margin_bottom);

                auto texture = std::make_unique<gfx::detail::TextureFileSource>();
                texture->SetColorSpace(GetValue(mUI.cmbColorSpace));
                texture->SetFlag(gfx::detail::TextureFileSource::Flags::PremulAlpha, premul_alpha);
                texture->SetFileName(mFileUri);
                texture->SetName("Spritesheet");

                klass.GetTextureMap(0)->SetSpriteSheet(sprite);
                klass.GetTextureMap(0)->SetNumTextures(1);
                klass.GetTextureMap(0)->SetTextureSource(0, std::move(texture));
                klass.GetTextureMap(0)->SetTextureRect(0, rect.Normalize(base::FSize(mWidth, mHeight)));
            }
        }

        if (klass.GetTextureMap(0)->GetNumTextures() == 0)
        {
            for (size_t index = 0; index < mImages.size(); ++index)
            {
                const auto& img = mImages[index];
                if (!img.selected)
                    continue;

                const auto& name = img.widget->GetName();

                gfx::FRect rect;
                auto texture = std::make_unique<gfx::detail::TextureFileSource>();
                texture->SetColorSpace(GetValue(mUI.cmbColorSpace));
                texture->SetFlag(gfx::detail::TextureFileSource::Flags::PremulAlpha, premul_alpha);
                if (cutting == TextureCutting::UseOriginal)
                {
                    texture->SetFileName(mFileUri);
                    texture->SetName(app::ToUtf8(name));
                    rect.Move(img.xpos, img.ypos);
                    rect.Translate(tile_margin_left, tile_margin_top);
                    rect.SetWidth(img.width - tile_margin_left - tile_margin_right);
                    rect.SetHeight(img.height - tile_margin_top - tile_margin_bottom);
                    rect = rect.Normalize(base::FSize(mWidth, mHeight));
                }
                else if (cutting == TextureCutting::CutNewTexture)
                {
                    texture->SetFileName(texture_uris[index]);
                    texture->SetName("Tile");
                    rect = gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f);
                } else BUG("Missing texture cut case.");

                auto* map = klass.GetTextureMap(0);
                auto count = map->GetNumTextures();
                map->SetNumTextures(count+1);
                map->SetTextureSource(count, std::move(texture));
                map->SetTextureRect(count, rect);

                if (Increment(mUI.progressBar))
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

    QPixmap pixmap;

    auto* map = mClass->GetTextureMap(0);
    auto* src = map->GetTextureSource(0);

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

    unsigned counter = 0;
    for (auto& img : mImages)
    {
        if (img.widget)
        {
            delete img.widget;
            img.widget = nullptr;
        }
        if (!img.selected)
            continue;

        img.widget = new ImportedTile(this);
        img.widget->InstallEventFilter(this);
        img.widget->SetPreview(pixmap.copy(img.xpos, img.ypos, img.width, img.height));
        if (img.name.isEmpty())
            img.widget->SetName(app::toString("Tile %1", counter));
        else img.widget->SetName(img.name);
        mUI.layout->addWidget(img.widget);
        ++counter;
    }
    SetValue(mUI.renameTiles, QString(""));
}

void DlgTileImport::on_renameTiles_textChanged(const QString& name)
{
    size_t counter = 0;

    for (auto& tile : mImages)
    {
        if (!tile.widget)
            continue;

        QString str = name;
        str.replace("$c", QString::number(counter));
        str.replace("$i", QString::number(tile.index));
        str.replace("$n", tile.name);
        tile.widget->SetName(str);
        counter++;
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
        SetEnabled(mUI.chkBlendFrames, true);
        SetEnabled(mUI.spriteFps, true);
        SetEnabled(mUI.spriteName, true);
    }
    else
    {
        SetEnabled(mUI.chkBlendFrames, false);
        SetEnabled(mUI.spriteFps, false);
        SetEnabled(mUI.spriteName, false);
    }
}

void DlgTileImport::on_cmbColorSpace_currentIndexChanged(int)
{
    if (!mClass)
        return;
    auto* map = mClass->GetTextureMap(0);
    auto* src = map->GetTextureSource(0);
    auto* file_source = dynamic_cast<gfx::detail::TextureFileSource*>(src);
    file_source->SetColorSpace(GetValue(mUI.cmbColorSpace));
}

void DlgTileImport::on_cmbMinFilter_currentIndexChanged(int)
{
    if (!mClass)
        return;

    mClass->SetTextureMinFilter(GetValue(mUI.cmbMinFilter));
}

void DlgTileImport::on_cmbMagFilter_currentIndexChanged(int)
{
    if (!mClass)
        return;

    mClass->SetTextureMagFilter(GetValue(mUI.cmbMagFilter));
}

void DlgTileImport::on_cmbCutting_currentIndexChanged(int)
{
    const TextureCutting cutting = GetValue(mUI.cmbCutting);
    if (cutting == TextureCutting::UseOriginal)
    {
        SetEnabled(mUI.cmbImageFormat,   false);
        SetEnabled(mUI.imageQuality,     false);
        SetEnabled(mUI.textureFolder,    false);
        SetEnabled(mUI.tileMarginTop,    true);
        SetEnabled(mUI.tileMarginLeft,   true);
        SetEnabled(mUI.tileMarginRight,  true);
        SetEnabled(mUI.tileMarginBottom, true);
    }
    else
    {
        SetEnabled(mUI.cmbImageFormat,   true);
        SetEnabled(mUI.imageQuality,     true);
        SetEnabled(mUI.textureFolder,    true);
        SetEnabled(mUI.tileMarginTop,    false);
        SetEnabled(mUI.tileMarginLeft,   false);
        SetEnabled(mUI.tileMarginRight,  false);
        SetEnabled(mUI.tileMarginBottom, false);
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

void DlgTileImport::keyPressEvent(QKeyEvent* event)
{
    if (!OnKeyPress(event))
        QDialog::keyPressEvent(event);
}

bool DlgTileImport::eventFilter(QObject* destination, QEvent* event)
{
    if (event->type() != QEvent::KeyPress)
        return false;

    const auto* key = static_cast<const QKeyEvent*>(event);
    const bool ctrl = key->modifiers() & Qt::ControlModifier;
    const bool alt  = key->modifiers() & Qt::AltModifier;

    if (alt && key->key() == Qt::Key_1)
        mUI.tabWidget->setCurrentIndex(0);
    else if (alt && key->key() == Qt::Key_2)
        mUI.tabWidget->setCurrentIndex(1);
    else return false;

    return true;
}

void DlgTileImport::ToggleSelection()
{
    if (mIndexUnderMouse >= mImages.size())
        return;

    if (base::Contains(mTilesTouched, mIndexUnderMouse))
        return;

    mImages[mIndexUnderMouse].selected = !mImages[mIndexUnderMouse].selected;
    mTilesTouched.insert(mIndexUnderMouse);
}

void DlgTileImport::LoadImageFile(const QString& ret)
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
    const auto& bitmap = source->GetData();
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
    mWidth    = img_width;
    mHeight   = img_height;
    mFileUri  = std::move(file_uri);
    mFileName = std::move(file_name);
    mTrackingOffset = QPoint(0, 0);

    mClass = std::make_shared<gfx::TextureMap2DClass>(gfx::MaterialClass::Type::Texture);
    mClass->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
    mClass->SetTexture(std::move(source));
    mClass->SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
    mClass->SetGamma(1.0f);
    mClass->SetTextureMinFilter(GetValue(mUI.cmbMinFilter));
    mClass->SetTextureMagFilter(GetValue(mUI.cmbMagFilter));
    mMaterial = gfx::CreateMaterialInstance(mClass);
    SetValue(mUI.imageFile, info.absoluteFilePath());
    SetValue(mUI.zoom, scale);
}

void DlgTileImport::LoadJsonFile(const QString& file)
{
    std::vector<Image> image_list;
    if (!ReadTexturePack(file, &image_list))
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("There was a problem reading the file.\n"
                       "Perhaps the image is not a valid image descriptor JSON file?\n"
                       "Please see the log for details."));
        msg.exec();
        return;
    }
    mImages = std::move(image_list);
    SetValue(mUI.jsonFile, file);
}

void DlgTileImport::LoadState()
{
    QByteArray geometry;
    if (GetUserProperty(*mWorkspace, "dlg-tile-import-geometry", &geometry))
        restoreGeometry(geometry);

    int xpos = 0;
    int ypos = 0;
    QString imagefile;
    QString jsonfile;
    GetUserProperty(*mWorkspace, "dlg-tile-import-color-space", mUI.cmbColorSpace);
    GetUserProperty(*mWorkspace, "dlg-tile-import-zoom", mUI.zoom);
    GetUserProperty(*mWorkspace, "dlg-tile-import-color", mUI.widget);
    GetUserProperty(*mWorkspace, "dlg-tile-import-material-type", mUI.materialType);
    GetUserProperty(*mWorkspace, "dlg-tile-import-material-surface", mUI.surfaceType);
    GetUserProperty(*mWorkspace, "dlg-tile-import-sprite-name", mUI.spriteName);
    GetUserProperty(*mWorkspace, "dlg-tile-import-sprite-fps", mUI.spriteFps);
    GetUserProperty(*mWorkspace, "dlg-tile-import-import-min-filter", mUI.minFilter);
    GetUserProperty(*mWorkspace, "dlg-tile-import-import-mag-filter", mUI.magFilter);
    GetUserProperty(*mWorkspace, "dlg-tile-import-view-min-filter", mUI.cmbMinFilter);
    GetUserProperty(*mWorkspace, "dlg-tile-import-view-mag-filter", mUI.cmbMagFilter);
    GetUserProperty(*mWorkspace, "dlg-tile-import-tile-margin-top", mUI.tileMarginTop);
    GetUserProperty(*mWorkspace, "dlg-tile-import-tile-margin-left", mUI.tileMarginLeft);
    GetUserProperty(*mWorkspace, "dlg-tile-import-tile-margin-right", mUI.tileMarginRight);
    GetUserProperty(*mWorkspace, "dlg-tile-import-tile-margin-bottom", mUI.tileMarginBottom);
    GetUserProperty(*mWorkspace, "dlg-tile-import-premul-alpha", mUI.chkPremulAlpha);
    GetUserProperty(*mWorkspace, "dlg-tile-import-premul-alpha-blend", mUI.chkPremulAlphaBlend);
    GetUserProperty(*mWorkspace, "dlg-tile-import-blend-frames", mUI.chkBlendFrames);
    GetUserProperty(*mWorkspace, "dlg-tile-import-cut-texture", mUI.cmbCutting);
    GetUserProperty(*mWorkspace, "dlg-tile-import-img-format", mUI.cmbImageFormat);
    GetUserProperty(*mWorkspace, "dlg-tile-import-img-quality", mUI.imageQuality);
    GetUserProperty(*mWorkspace, "dlg-tile-import-img-folder", mUI.textureFolder);
    GetUserProperty(*mWorkspace, "dlg-tile-import-xpos", &xpos);
    GetUserProperty(*mWorkspace, "dlg-tile-import-ypos", &ypos);
    GetUserProperty(*mWorkspace, "dlg-tile-import-image-file", &imagefile);
    GetUserProperty(*mWorkspace, "dlg-tile-import-json-file", &jsonfile);
    if (!imagefile.isEmpty())
        LoadImageFile(imagefile);
    if (!jsonfile.isEmpty())
        LoadJsonFile(jsonfile);

    mTrackingOffset = QPoint(xpos, ypos);

    on_cmbCutting_currentIndexChanged(0);
    on_materialType_currentIndexChanged(0);
}
void DlgTileImport::SaveState()
{
    QString imagefile = GetValue(mUI.imageFile);
    QString jsonfile  = GetValue(mUI.jsonFile);

    SetUserProperty(*mWorkspace, "dlg-tile-import-geometry", saveGeometry());
    SetUserProperty(*mWorkspace, "dlg-tile-import-color-space", mUI.cmbColorSpace);
    SetUserProperty(*mWorkspace, "dlg-tile-import-zoom", mUI.zoom);
    SetUserProperty(*mWorkspace, "dlg-tile-import-color", mUI.widget);
    SetUserProperty(*mWorkspace, "dlg-tile-import-material-type", mUI.materialType);
    SetUserProperty(*mWorkspace, "dlg-tile-import-material-surface", mUI.surfaceType);
    SetUserProperty(*mWorkspace, "dlg-tile-import-sprite-name", mUI.spriteName);
    SetUserProperty(*mWorkspace, "dlg-tile-import-sprite-fps", mUI.spriteFps);
    SetUserProperty(*mWorkspace, "dlg-tile-import-import-min-filter", mUI.minFilter);
    SetUserProperty(*mWorkspace, "dlg-tile-import-import-mag-filter", mUI.magFilter);
    SetUserProperty(*mWorkspace, "dlg-tile-import-view-min-filter", mUI.cmbMinFilter);
    SetUserProperty(*mWorkspace, "dlg-tile-import-view-mag-filter", mUI.cmbMagFilter);
    SetUserProperty(*mWorkspace, "dlg-tile-import-tile-margin-top", mUI.tileMarginTop);
    SetUserProperty(*mWorkspace, "dlg-tile-import-tile-margin-left", mUI.tileMarginLeft);
    SetUserProperty(*mWorkspace, "dlg-tile-import-tile-margin-right", mUI.tileMarginRight);
    SetUserProperty(*mWorkspace, "dlg-tile-import-tile-margin-bottom", mUI.tileMarginBottom);
    SetUserProperty(*mWorkspace, "dlg-tile-import-premul-alpha", mUI.chkPremulAlpha);
    SetUserProperty(*mWorkspace, "dlg-tile-import-premul-alpha-blend", mUI.chkPremulAlphaBlend);
    SetUserProperty(*mWorkspace, "dlg-tile-import-blend-frames", mUI.chkBlendFrames);
    SetUserProperty(*mWorkspace, "dlg-tile-import-cut-texture", mUI.cmbCutting);
    SetUserProperty(*mWorkspace, "dlg-tile-import-img-format", mUI.cmbImageFormat);
    SetUserProperty(*mWorkspace, "dlg-tile-import-img-quality", mUI.imageQuality);
    SetUserProperty(*mWorkspace, "dlg-tile-import-img-folder", mUI.textureFolder);
    SetUserProperty(*mWorkspace, "dlg-tile-import-xpos", mTrackingOffset.x());
    SetUserProperty(*mWorkspace, "dlg-tile-import-ypos", mTrackingOffset.y());
    SetUserProperty(*mWorkspace, "dlg-tile-import-image-file", imagefile);
    SetUserProperty(*mWorkspace, "dlg-tile-import-json-file", jsonfile);
}

void DlgTileImport::OnPaintScene(gfx::Painter& painter, double secs)
{
    SetValue(mUI.widgetColor, mUI.widget->GetCurrentClearColor());

    const float width  = mUI.widget->width();
    const float height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    if (!mMaterial)
    {
        ShowInstruction(
            "Import tilemap data as materials and textures.\n\n"
            "INSTRUCTIONS\n"
            "1. Select a tilemap image file.\n"
            "2. Click on any tile to toggle selection.\n"
            "3. Go to 'Review Tiles' and select options.\n"
            "4. Click 'Import' to import the tiles into project.\n",
            gfx::FRect(0, 0, width, height), painter);
        return;
    }

    const float zoom   = GetValue(mUI.zoom);
    const float img_width  = mWidth * zoom;
    const float img_height = mHeight * zoom;
    const auto xpos = (width - img_width) * 0.5f;
    const auto ypos = (height - img_height) * 0.5f;

    gfx::FRect img_rect(0.0f, 0.0f, img_width, img_height);
    img_rect.Translate(xpos, ypos);
    img_rect.Translate(mTrackingOffset.x(), mTrackingOffset.y());
    gfx::FillRect(painter, img_rect, *mMaterial);

    if (mImages.empty())
        return;

    static auto selection_material_class = gfx::CreateMaterialClassFromImage("app://textures/accept_icon.png");
    selection_material_class.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
    selection_material_class.SetBaseColor(gfx::Color4f(1.0f, 1.0f, 1.0f, 1.0f));
    static auto selection_material = gfx::MaterialClassInst(selection_material_class);

    for (size_t index=0; index<mImages.size(); ++index)
    {
        const auto& img = mImages[index];
        if (!img.selected && index != mIndexUnderMouse)
            continue;

        gfx::FRect rect(0.0f, 0.0f, img.width*zoom, img.height*zoom);
        rect.Translate(xpos, ypos);
        rect.Translate(mTrackingOffset.x(), mTrackingOffset.y());
        rect.Translate(img.xpos * zoom, img.ypos * zoom);

        if (index == mIndexUnderMouse)
        {
            gfx::DrawRectOutline(painter, rect, gfx::CreateMaterialFromColor(gfx::Color::HotPink));
        }
        if (img.selected)
        {
            rect.SetWidth(32.0f);
            rect.SetHeight(32.0f);
            gfx::FillShape(painter, rect, gfx::Circle(), selection_material);
        }
    }
}
void DlgTileImport::OnMousePress(QMouseEvent* mickey)
{
    mStartPoint = mickey->pos();

    if (mickey->button() == Qt::RightButton)
        mMode = Mode::Tracking;
    else if (mickey->button() == Qt::LeftButton)
    {
        mMode = Mode::Selecting;
        ToggleSelection();
    }
}

void DlgTileImport::OnMouseMove(QMouseEvent* mickey)
{
    mCurrentPoint = mickey->pos();

    if (mMode == Mode::Tracking)
    {
        mTrackingOffset += (mCurrentPoint - mStartPoint);
        mStartPoint = mCurrentPoint;
    }

    mIndexUnderMouse = mImages.size();
    if (mImages.empty() || !mMaterial)
        return;

    const float width = mUI.widget->width();
    const float height = mUI.widget->height();
    const float zoom   = GetValue(mUI.zoom);
    const float img_width = mWidth * zoom;
    const float img_height = mHeight * zoom;
    const auto xpos = (width - img_width) * 0.5f;
    const auto ypos = (height - img_height) * 0.5f;
    const int mouse_posx = (mCurrentPoint.x() - mTrackingOffset.x() - xpos) / zoom;
    const int mouse_posy = (mCurrentPoint.y() - mTrackingOffset.y() - ypos) / zoom;

    for (mIndexUnderMouse=0; mIndexUnderMouse<mImages.size(); ++mIndexUnderMouse)
    {
        const auto& img = mImages[mIndexUnderMouse];
        if (mouse_posx < img.xpos || mouse_posx > img.xpos+img.width)
            continue;
        if (mouse_posy < img.ypos || mouse_posy > img.ypos+img.height)
            continue;
        break;
    }

    if (mMode == Mode::Selecting)
    {
        ToggleSelection();
    }
}

void DlgTileImport::OnMouseRelease(QMouseEvent* mickey)
{
    mMode = Mode::Nada;
    mTilesTouched.clear();
}

bool DlgTileImport::OnKeyPress(QKeyEvent* key)
{
    const bool ctrl = key->modifiers() & Qt::ControlModifier;
    const bool alt  = key->modifiers() & Qt::AltModifier;

    if (alt && key->key() == Qt::Key_1)
        mUI.tabWidget->setCurrentIndex(0);
    else if (alt && key->key() == Qt::Key_2)
        mUI.tabWidget->setCurrentIndex(1);
    else if (ctrl && key->key() == Qt::Key_W)
        on_btnClose_clicked();
    else if (key->key() == Qt::Key_Escape)
    {
        bool had_selection = false;
        for (auto& tile : mImages)
        {
            had_selection = had_selection || tile.selected;
            tile.selected = false;
        }
        return had_selection;
    }

    else return false;

    return true;
}

} // namespace

