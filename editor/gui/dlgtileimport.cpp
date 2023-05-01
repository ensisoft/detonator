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

app::AnyString ImportedTile::GetName() const
{
    return GetValue(mUI.name);
}

app::AnyString ImportedTile::GetTag() const
{
    return GetValue(mUI.tag);
}

void ImportedTile::SetName(const app::AnyString& name)
{
    SetValue(mUI.name, name);
}

void ImportedTile::SetTag(const app::AnyString& tag)
{
    SetValue(mUI.tag, tag);
}

void ImportedTile::InstallEventFilter(QObject* receiver)
{
    mUI.name->installEventFilter(receiver);
    mUI.tag->installEventFilter(receiver);
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
    SetValue(mUI.zoom, 1.0f);

    on_materialType_currentIndexChanged(0);
    mUI.renameTemplate->installEventFilter(this);
    mUI.spriteName->installEventFilter(this);
}

DlgTileImport::~DlgTileImport()
{
    for (auto& tile : mPack.images)
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
    ResetTransform();

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
    {
        LoadImageFile(img);
        ResetTransform();
    }
}

void DlgTileImport::on_btnSelectAll_clicked()
{
    for (auto& tile : mPack.images)
        tile.selected = true;
}

void DlgTileImport::on_btnSelectNone_clicked()
{
    for (auto& tile : mPack.images)
        tile.selected = false;
}

void DlgTileImport::on_btnClose_clicked()
{
    SaveState();

    close();
}

void DlgTileImport::on_btnImport_clicked()
{
    bool have_selection = false;
    for (const auto& img : mPack.images)
    {
        if (img.selected) {
            have_selection = true;
            break;
        }
    }
    if (!have_selection)
    {
        QMessageBox msg(this);
        msg.setText("You haven't selected any tiles!");
        msg.setIcon(QMessageBox::Information);
        msg.exec();
        return;
    }

    const auto tile_margin_top  = (unsigned)GetValue(mUI.topMargin);
    const auto tile_margin_left = (unsigned)GetValue(mUI.leftMargin);
    const auto tile_margin_right = (unsigned)GetValue(mUI.rightMargin);
    const auto tile_margin_bottom = (unsigned)GetValue(mUI.bottomMargin);
    const auto premul_alpha = (bool)GetValue(mUI.chkPremulAlpha);
    const auto premul_alpha_blend = (bool)GetValue(mUI.chkPremulAlphaBlend);
    const float img_height = mHeight;
    const float img_width = mWidth;

    const MaterialType type  = GetValue(mUI.materialType);

    AutoEnabler close(mUI.btnClose);
    AutoEnabler import(mUI.btnImport);

    if (type == MaterialType::Texture)
    {
        for (size_t index=0; index<mPack.images.size(); ++index)
        {
            const auto& img  = mPack.images[index];
            if (!img.selected)
                continue;

            const auto& name = img.widget->GetName();
            auto texture = std::make_unique<gfx::detail::TextureFileSource>();
            texture->SetColorSpace(GetValue(mUI.cmbColorSpace));
            texture->SetFlag(gfx::detail::TextureFileSource::Flags::PremulAlpha, premul_alpha);
            texture->SetFileName(mFileUri);
            texture->SetName(name);

            gfx::FRect rect;
            rect.Move(img.xpos, img.ypos);
            rect.Translate(tile_margin_left, tile_margin_top);
            rect.SetWidth(img.width - tile_margin_left - tile_margin_right);
            rect.SetHeight(img.height - tile_margin_top - tile_margin_bottom);
            rect = rect.Normalize(base::FSize(mWidth, mHeight));

            auto map = std::make_unique<gfx::TextureMap>();
            map->SetType(gfx::TextureMap::Type::Texture2D);
            map->SetName("Texture");
            map->SetNumTextures(1);
            map->SetTextureSource(0, std::move(texture));
            map->SetTextureRect(0, rect);

            gfx::TextureMap2DClass klass(gfx::MaterialClass::Type::Texture, base::RandomString(10));
            klass.SetSurfaceType(GetValue(mUI.surfaceType));
            klass.SetTextureMinFilter(GetValue(mUI.minFilter));
            klass.SetTextureMagFilter(GetValue(mUI.magFilter));
            klass.SetNumTextureMaps(1);
            klass.SetTextureMap(0, std::move(map));
            klass.SetName(app::ToUtf8(img.name));
            klass.SetFlag(gfx::TextureMap2DClass::Flags::PremultipliedAlpha, premul_alpha_blend);

            app::MaterialResource res(klass, name);
            mWorkspace->SaveResource(res);
        }
    }
    else if (type == MaterialType::Sprite)
    {
        if (!MustHaveInput(mUI.spriteName))
            return;

        auto map = std::make_unique<gfx::TextureMap>();
        map->SetName("Sprite");
        map->SetType(gfx::TextureMap::Type::Sprite);
        map->SetFps(GetValue(mUI.spriteFps));

        gfx::MaterialClass klass(gfx::MaterialClass::Type::Sprite, base::RandomString(10));
        klass.SetSurfaceType(GetValue(mUI.surfaceType));
        klass.SetTextureMinFilter(GetValue(mUI.minFilter));
        klass.SetTextureMagFilter(GetValue(mUI.magFilter));
        klass.SetFlag(gfx::TextureMap2DClass::Flags::PremultipliedAlpha, premul_alpha_blend);
        klass.SetName(GetValue(mUI.spriteName));
        klass.SetBlendFrames(GetValue(mUI.chkBlendFrames));
        klass.SetNumTextureMaps(1);
        klass.SetTextureMap(0, std::move(map));

        // if the selected images / tiles are in one contiguous region
        // with regular size then the sprite can be optimized to use a single
        // sprite sheet instead of multiple different images, but only if
        // texture cutting is not being done.  Texture cutting forces the use
        // of separate image source files.
        unsigned tile_width  = 0;
        unsigned tile_height = 0;
        unsigned tile_xpos   = 0;
        unsigned tile_ypos   = 0;
        unsigned tile_count  = 0;
        bool irregular_size  = false;
        bool disjoint_selection = false;
        base::URect rect;

        for (size_t index=0; index<mPack.images.size(); ++index)
        {
            const auto& img = mPack.images[index];
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

        if (irregular_size || disjoint_selection)
        {
            for (size_t index = 0; index < mPack.images.size(); ++index)
            {
                const auto& img = mPack.images[index];
                if (!img.selected)
                    continue;

                const auto& name = img.widget->GetName();

                auto texture = std::make_unique<gfx::detail::TextureFileSource>();
                texture->SetColorSpace(GetValue(mUI.cmbColorSpace));
                texture->SetFlag(gfx::detail::TextureFileSource::Flags::PremulAlpha, premul_alpha);
                texture->SetFileName(mFileUri);
                texture->SetName(name);

                gfx::FRect rect;
                rect.Move(img.xpos, img.ypos);
                rect.Translate(tile_margin_left, tile_margin_top);
                rect.SetWidth(img.width - tile_margin_left - tile_margin_right);
                rect.SetHeight(img.height - tile_margin_top - tile_margin_bottom);
                rect = rect.Normalize(base::FSize(mWidth, mHeight));

                auto* map = klass.GetTextureMap(0);
                auto count = map->GetNumTextures();
                map->SetNumTextures(count+1);
                map->SetTextureSource(count, std::move(texture));
                map->SetTextureRect(count, rect);
            }
        }
        else
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
        app::MaterialResource res(klass, GetValue(mUI.spriteName));
        mWorkspace->SaveResource(res);

    } else BUG("Missing material type handling.");
}

void DlgTileImport::on_tabWidget_currentChanged(int tab)
{
    SetEnabled(mUI.btnImport, false);

    if (tab != 1)
        return;

    if (!mMaterial)
        return;

    QPixmap pixmap;

    auto* map = mClass->GetTextureMap(0);
    auto* src = map->GetTextureSource(0);

    if (const auto& bitmap = src->GetData())
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
    for (auto& img : mPack.images)
    {
        if (!img.selected)
            continue;

        if (img.widget)
            delete img.widget;

        img.widget = new ImportedTile(this);
        img.widget->InstallEventFilter(this);
        img.widget->SetPreview(pixmap.copy(img.xpos, img.ypos, img.width, img.height));

        if (img.name.isEmpty())
            img.widget->SetName(app::toString("Tile %1", counter));
        else img.widget->SetName(img.name);
        img.widget->SetTag(img.tag);

        mUI.layout->addWidget(img.widget);
        ++counter;
        SetEnabled(mUI.btnImport, true);
    }
    SetValue(mUI.renameTemplate, QString(""));
}

void DlgTileImport::on_renameTemplate_returnPressed()
{
    bool have_selection = false;
    std::vector<QString> original_names;
    for (const auto& img : mPack.images)
    {
        if (img.widget)
            original_names.push_back(img.widget->GetName());
        else original_names.push_back(QString(""));
        if (img.selected)
            have_selection = true;
    }

    if (!have_selection)
    {
        QMessageBox msg(this);
        msg.setText("You haven't selected any tiles!");
        msg.setIcon(QMessageBox::Information);
        msg.exec();
        return;
    }

    size_t counter = 0;
    for (auto& img : mPack.images)
    {
        if (!img.widget)
            continue;

        QString out_name = GetValue(mUI.renameTemplate);
        out_name.replace("%c", QString::number(counter++));
        out_name.replace("%i", QString::number(img.index));
        out_name.replace("%w", QString::number(img.width));
        out_name.replace("%h", QString::number(img.height));
        out_name.replace("%x", QString::number(img.xpos));
        out_name.replace("%y", QString::number(img.ypos));
        out_name.replace("%n", img.name);
        out_name.replace("%t", img.tag);
        img.widget->SetName(out_name);
        counter++;
    }

    QMessageBox msg(this);
    msg.setWindowTitle("Confirm Rename");
    msg.setText("Do you want to keep these changes?");
    msg.setIcon(QMessageBox::Question);
    msg.setStandardButtons(QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No);
    if (msg.exec() == QMessageBox::StandardButton::Yes)
    {
        return;
    }

    ASSERT(mPack.images.size() == original_names.size());
    for (size_t i=0; i<original_names.size(); ++i)
    {
        mPack.images[i].name = original_names[i];
        if (mPack.images[i].widget)
            mPack.images[i].widget->SetName(original_names[i]);
    }
    SetValue(mUI.renameTemplate, QString(""));
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
        SetValue(mUI.spriteName, QString(""));
        SetPlaceholderText(mUI.spriteName, QString(""));
    }
    else
    {
        SetEnabled(mUI.chkBlendFrames, false);
        SetEnabled(mUI.spriteFps, false);
        SetEnabled(mUI.spriteName, false);
        SetValue(mUI.spriteName, QString(""));
        SetPlaceholderText(mUI.spriteName, QString("From tilename"));
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
    if (mIndexUnderMouse >= mPack.images.size())
        return;

    if (base::Contains(mTilesTouched, mIndexUnderMouse))
        return;

    mPack.images[mIndexUnderMouse].selected = !mPack.images[mIndexUnderMouse].selected;
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

    mWidth    = bitmap->GetWidth();
    mHeight   = bitmap->GetHeight();
    mFileUri  = std::move(file_uri);
    mFileName = std::move(file_name);

    mClass = std::make_shared<gfx::TextureMap2DClass>(gfx::MaterialClass::Type::Texture);
    mClass->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
    mClass->SetTexture(std::move(source));
    mClass->SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
    mClass->SetGamma(1.0f);
    mClass->SetTextureMinFilter(GetValue(mUI.cmbMinFilter));
    mClass->SetTextureMagFilter(GetValue(mUI.cmbMagFilter));
    mMaterial = gfx::CreateMaterialInstance(mClass);
    SetValue(mUI.imageFile, info.absoluteFilePath());
}

void DlgTileImport::LoadJsonFile(const QString& file)
{
    ImagePack pack;
    if (!ReadImagePack(file, &pack))
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
    mPack = std::move(pack);
    SetValue(mUI.jsonFile, file);
    SetValue(mUI.cmbColorSpace, mPack.color_space);
    // this setting applies to the visualization on the main tab.
    SetValue(mUI.cmbMagFilter, mPack.mag_filter);
    SetValue(mUI.cmbMinFilter, mPack.min_filter);
    // this setting applies to the new materials when importing
    SetValue(mUI.minFilter, mPack.min_filter);
    SetValue(mUI.magFilter, mPack.mag_filter);

    // apply the values to the material class if it exists.
    // ignore the combo box selection indices.
    on_cmbColorSpace_currentIndexChanged(0);
    on_cmbMinFilter_currentIndexChanged(0);
    on_cmbMagFilter_currentIndexChanged(0);
}

void DlgTileImport::ResetTransform()
{
    if (mWidth == 0 || mHeight == 0)
        return;

    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto scale = std::min((float)width/(float)mWidth,
                                (float)height/(float)mHeight);
    mTrackingOffset = QPoint(0, 0);
    SetValue(mUI.zoom, scale);
}

void DlgTileImport::LoadState()
{
    int xpos = 0;
    int ypos = 0;

    // The order in which data is loaded matters here. First load the previous
    // image and the JSON files (if any). Then we load the rest of the state
    // which can then override some of the colorspace / texture filtering settings
    // that were initially loaded from the JSON.

    QString imagefile;
    QString jsonfile;
    GetUserProperty(*mWorkspace, "dlg-tile-import-image-file", &imagefile);
    GetUserProperty(*mWorkspace, "dlg-tile-import-json-file", &jsonfile);
    if (!imagefile.isEmpty())
        LoadImageFile(imagefile);
    if (!jsonfile.isEmpty())
        LoadJsonFile(jsonfile);

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
    GetUserProperty(*mWorkspace, "dlg-tile-import-tile-margin-top", mUI.topMargin);
    GetUserProperty(*mWorkspace, "dlg-tile-import-tile-margin-left", mUI.leftMargin);
    GetUserProperty(*mWorkspace, "dlg-tile-import-tile-margin-right", mUI.rightMargin);
    GetUserProperty(*mWorkspace, "dlg-tile-import-tile-margin-bottom", mUI.bottomMargin);
    GetUserProperty(*mWorkspace, "dlg-tile-import-premul-alpha", mUI.chkPremulAlpha);
    GetUserProperty(*mWorkspace, "dlg-tile-import-premul-alpha-blend", mUI.chkPremulAlphaBlend);
    GetUserProperty(*mWorkspace, "dlg-tile-import-blend-frames", mUI.chkBlendFrames);
    GetUserProperty(*mWorkspace, "dlg-tile-import-xpos", &xpos);
    GetUserProperty(*mWorkspace, "dlg-tile-import-ypos", &ypos);
    mTrackingOffset = QPoint(xpos, ypos);

    on_materialType_currentIndexChanged(0);
    on_cmbMinFilter_currentIndexChanged(0);
    on_cmbMagFilter_currentIndexChanged(0);
    on_cmbColorSpace_currentIndexChanged(0);
}

void DlgTileImport::LoadGeometry()
{
    QByteArray geometry;
    if (GetUserProperty(*mWorkspace, "dlg-tile-import-geometry", &geometry))
        restoreGeometry(geometry);
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
    SetUserProperty(*mWorkspace, "dlg-tile-import-tile-margin-top", mUI.topMargin);
    SetUserProperty(*mWorkspace, "dlg-tile-import-tile-margin-left", mUI.leftMargin);
    SetUserProperty(*mWorkspace, "dlg-tile-import-tile-margin-right", mUI.rightMargin);
    SetUserProperty(*mWorkspace, "dlg-tile-import-tile-margin-bottom", mUI.bottomMargin);
    SetUserProperty(*mWorkspace, "dlg-tile-import-premul-alpha", mUI.chkPremulAlpha);
    SetUserProperty(*mWorkspace, "dlg-tile-import-premul-alpha-blend", mUI.chkPremulAlphaBlend);
    SetUserProperty(*mWorkspace, "dlg-tile-import-blend-frames", mUI.chkBlendFrames);
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

    if (mPack.images.empty())
        return;

    static auto selection_material_class = gfx::CreateMaterialClassFromImage("app://textures/accept_icon.png");
    selection_material_class.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
    selection_material_class.SetBaseColor(gfx::Color4f(1.0f, 1.0f, 1.0f, 1.0f));
    static auto selection_material = gfx::MaterialClassInst(selection_material_class);

    for (size_t index=0; index<mPack.images.size(); ++index)
    {
        const auto& img = mPack.images[index];
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

    mIndexUnderMouse = mPack.images.size();
    if (mPack.images.empty() || !mMaterial)
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

    for (mIndexUnderMouse=0; mIndexUnderMouse<mPack.images.size(); ++mIndexUnderMouse)
    {
        const auto& img = mPack.images[mIndexUnderMouse];
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
        for (auto& tile : mPack.images)
        {
            had_selection = had_selection || tile.selected;
            tile.selected = false;
        }
        return had_selection;
    } else return false;

    return true;
}

} // namespace

