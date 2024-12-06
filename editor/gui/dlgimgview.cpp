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

#include "warnpush.h"
#  include <QFileInfo>
#  include <QFileDialog>
#  include <QPixmap>
#  include <QMessageBox>
#  include <QFile>
#  include <QFileInfo>
#  include <QImageWriter>
#  include <QEventLoop>
#  include <QInputDialog>
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <algorithm>

#include "base/json.h"
#include "graphics/painter.h"
#include "graphics/drawing.h"
#include "graphics/bitmap.h"
#include "graphics/simple_shape.h"
#include "graphics/texture_file_source.h"
#include "graphics/material_instance.h"
#include "editor/app/resource-uri.h"
#include "editor/app/utility.h"
#include "editor/app/eventlog.h"
#include "editor/app/workspace.h"
#include "editor/app/packing.h"
#include "editor/gui/dlgimgview.h"
#include "editor/gui/utility.h"
#include "editor/gui/drawing.h"

namespace {
    enum class TilePackVerticalAlignment {
        Top, Center, Bottom
    };
    enum class TilePackHorizontalAlignment {
        Left, Center, Right
    };
}

namespace gui
{

DlgImgView::DlgImgView(QWidget* parent) : QDialog(parent)
{
    mUI.setupUi(this);
    mUI.widget->onPaintScene   = std::bind(&DlgImgView::OnPaintScene, this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onMouseMove    = std::bind(&DlgImgView::OnMouseMove, this, std::placeholders::_1);
    mUI.widget->onMousePress   = std::bind(&DlgImgView::OnMousePress, this, std::placeholders::_1);
    mUI.widget->onMouseRelease = std::bind(&DlgImgView::OnMouseRelease, this, std::placeholders::_1);
    mUI.widget->onMouseDoubleClick = std::bind(&DlgImgView::OnMouseDoubleClick, this, std::placeholders::_1);
    mUI.widget->onKeyPress     = std::bind(&DlgImgView::OnKeyPress, this, std::placeholders::_1);
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

    connect(this, &QDialog::finished, this, &DlgImgView::finished);
    connect(&mTimer, &QTimer::timeout, this, &DlgImgView::timer);
    PopulateFromEnum<gfx::TextureFileSource::ColorSpace>(mUI.cmbColorSpace);
    PopulateFromEnum<gfx::MaterialClass::MinTextureFilter>(mUI.cmbMinFilter);
    PopulateFromEnum<gfx::MaterialClass::MagTextureFilter>(mUI.cmbMagFilter);

    SetVisible(mUI.btnCancel, false);
    SetVisible(mUI.btnAccept, false);
    SetVisible(mUI.imageCutterProgress, false);
    SetVisible(mUI.tilePackerProgress, false);
    SetValue(mUI.zoom, 1.0f);
    SetValue(mUI.cmbColorSpace, gfx::TextureFileSource::ColorSpace::sRGB);
    PopulateFromEnum<TilePackVerticalAlignment>(mUI.tilePackerVerticalAlign);
    PopulateFromEnum<TilePackHorizontalAlignment>(mUI.tilePackerHorizontalAlign);
    PopulateFromEnum<ToolMode>(mUI.cmbToolMode);
    SetValue(mUI.tilePackerVerticalAlign, TilePackVerticalAlignment::Center);
    SetValue(mUI.tilePackerHorizontalAlign, TilePackHorizontalAlignment::Center);
    SetValue(mUI.cmbToolMode, ToolMode::DefineMode);

    mUI.zoom->installEventFilter(this);
    mUI.cmbColorSpace->installEventFilter(this);
    mUI.cmbMinFilter->installEventFilter(this);
    mUI.cmbMagFilter->installEventFilter(this);
    mUI.listWidget->installEventFilter(this);
    mUI.renameTemplate->installEventFilter(this);
    mUI.tagTemplate->installEventFilter(this);
    mUI.imageCutterSelection->installEventFilter(this);
    mUI.imageCutterFormat->installEventFilter(this);
    mUI.imageCutterQuality->installEventFilter(this);
    mUI.tilePackerQuality->installEventFilter(this);
    mUI.imageCutterTopPadding->installEventFilter(this);
    mUI.imageCutterLeftPadding->installEventFilter(this);
    mUI.imageCutterRightPadding->installEventFilter(this);
    mUI.imageCutterBottomPadding->installEventFilter(this);
    mUI.imageCutterNameTemplate->installEventFilter(this);
    mUI.imageCutterOutputFolder->installEventFilter(this);
    mUI.tileWidth->installEventFilter(this);
    mUI.tileHeight->installEventFilter(this);
    mUI.tilePadding->installEventFilter(this);
}

void DlgImgView::LoadImage(const QString& file)
{
    auto source = std::make_unique<gfx::TextureFileSource>();
    source->SetFileName(app::ToUtf8(file));
    source->SetName(app::ToUtf8(file));
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

    mWidth  = bitmap->GetWidth();
    mHeight = bitmap->GetHeight();
    mDepth  = bitmap->GetDepthBits();
    mClass = std::make_shared<gfx::TextureMap2DClass>(gfx::MaterialClass::Type::Texture);
    mClass->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
    mClass->SetTexture(std::move(source));
    mClass->SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
    mClass->SetTextureMinFilter(GetValue(mUI.cmbMinFilter));
    mClass->SetTextureMagFilter(GetValue(mUI.cmbMagFilter));
    mMaterial = gfx::CreateMaterialInstance(mClass);
    SetValue(mUI.imageFile, file);
}

void DlgImgView::LoadJson(const QString& file)
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
    ClearTable(mUI.listWidget);
    ResizeTable(mUI.listWidget, pack.images.size(), 8);
    mUI.listWidget->setHorizontalHeaderLabels({"Name", "Char", "Tag", "Width", "Height", "X Pos", "Y Pos", "Index"});

    for (unsigned row=0; row<pack.images.size(); ++row)
    {
        const auto& img = pack.images[row];
        SetTableItem(mUI.listWidget, row, 0, img.name);
        SetTableItem(mUI.listWidget, row, 1, img.character);
        SetTableItem(mUI.listWidget, row, 2, img.tag);
        SetTableItem(mUI.listWidget, row, 3, img.width);
        SetTableItem(mUI.listWidget, row, 4, img.height);
        SetTableItem(mUI.listWidget, row, 5, img.xpos);
        SetTableItem(mUI.listWidget, row, 6, img.ypos);
        SetTableItem(mUI.listWidget, row, 7, img.index);
    }
    mPack = std::move(pack);
    SetValue(mUI.jsonFile, file);
    SetEnabled(mUI.btnSave, false);

    SetValue(mUI.cmbColorSpace, mPack.color_space);
    // this setting applies to the visualization on the main tab.
    SetValue(mUI.cmbMagFilter, mPack.mag_filter);
    SetValue(mUI.cmbMinFilter, mPack.min_filter);
    // apply the values to the material class if it exists.
    // ignore the combo box selection indices.
    on_cmbColorSpace_currentIndexChanged(0);
    on_cmbMinFilter_currentIndexChanged(0);
    on_cmbMagFilter_currentIndexChanged(0);

    SetValue(mUI.cmbToolMode, ToolMode::SelectMode);
}
void DlgImgView::SetDialogMode()
{
    SetVisible(mUI.btnClose, false);
    SetVisible(mUI.btnAccept, true);
    SetVisible(mUI.btnCancel, true);
    SetEnabled(mUI.btnAccept, false);
    SetVisible(mUI.btnSave, false);
    SetVisible(mUI.rename, false);
    SetVisible(mUI.retag,  false);
    SetValue(mUI.cmbToolMode, ToolMode::SelectMode);
    SetEnabled(mUI.cmbToolMode, false);

    QSignalBlocker s(mUI.tabWidget);
    mUI.tabWidget->removeTab(2); // cutter tab
    mUI.tabWidget->removeTab(2); // tile packer tab YES THE INDEX REPEATS NOW


    mUI.listWidget->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
    mDialogMode = true;
}

void DlgImgView::LoadState()
{
    if (!mWorkspace)
        return;

    int xpos = 0;
    int ypos = 0;

    // The order in which data is loaded matters here. First load the previous
    // image and the JSON files (if any). Then we load the rest of the state
    // which can then override some of the colorspace / texture filtering settings
    // that were initially loaded from the JSON.
    QString imagefile;
    QString jsonfile;
    GetUserProperty(*mWorkspace, "dlg-img-view-image-file", &imagefile);
    GetUserProperty(*mWorkspace, "dlg-img-view-json-file",  &jsonfile);
    if (!imagefile.isEmpty())
        LoadImage(imagefile);
    if (!jsonfile.isEmpty())
        LoadJson(jsonfile);

    GetUserProperty(*mWorkspace, "dlg-img-view-tool", mUI.cmbToolMode);
    GetUserProperty(*mWorkspace, "dlg-img-view-draw-rects", mUI.chkShowRects);
    GetUserProperty(*mWorkspace, "dlg-img-view-image-file", mUI.imageFile);
    GetUserProperty(*mWorkspace, "dlg-img-view-json-file", mUI.jsonFile);
    GetUserProperty(*mWorkspace, "dlg-img-view-widget", mUI.widget);
    GetUserProperty(*mWorkspace, "dlg-img-view-color-space", mUI.cmbColorSpace);
    GetUserProperty(*mWorkspace, "dlg-img-view-min-filter", mUI.cmbMinFilter);
    GetUserProperty(*mWorkspace, "dlg-img-view-mag-filter", mUI.cmbMagFilter);
    GetUserProperty(*mWorkspace, "dlg-img-view-zoom", mUI.zoom);
    GetUserProperty(*mWorkspace, "dlg-img-view-cut-selection-selector", mUI.imageCutterSelection);
    GetUserProperty(*mWorkspace, "dlg-img-view-cut-format", mUI.imageCutterFormat);
    GetUserProperty(*mWorkspace, "dlg-img-view-cut-quality", mUI.imageCutterQuality);
    GetUserProperty(*mWorkspace, "dlg-img-view-cut-top-padding", mUI.imageCutterTopPadding);
    GetUserProperty(*mWorkspace, "dlg-img-view-cut-left-padding", mUI.imageCutterLeftPadding);
    GetUserProperty(*mWorkspace, "dlg-img-view-cut-right-padding", mUI.imageCutterRightPadding);
    GetUserProperty(*mWorkspace, "dlg-img-view-cut-bottom-padding", mUI.imageCutterBottomPadding);
    GetUserProperty(*mWorkspace, "dlg-img-view-cut-out-folder", mUI.imageCutterOutputFolder);
    GetUserProperty(*mWorkspace, "dlg-img-view-cut-out-name-template", mUI.imageCutterNameTemplate);
    GetUserProperty(*mWorkspace, "dlg-img-view-cut-overwrite", mUI.imageCutterOverwrite);
    GetUserProperty(*mWorkspace, "dlg-img-view-cut-pot", mUI.imageCutterPOT);
    GetUserProperty(*mWorkspace, "dlg-img-view-xpos", &xpos);
    GetUserProperty(*mWorkspace, "dlg-img-view-ypos", &ypos);
    GetUserProperty(*mWorkspace, "dlg-img-view-tile-packer-selection-selector", mUI.tilePackerSelection);
    GetUserProperty(*mWorkspace, "dlg-img-view-tile-packer-format", mUI.tilePackerFormat);
    GetUserProperty(*mWorkspace, "dlg-img-view-tile-packer-quality", mUI.tilePackerQuality);
    GetUserProperty(*mWorkspace, "dlg-img-view-tile-packer-resize-pot", mUI.tilePackerPOT);
    GetUserProperty(*mWorkspace, "dlg-img-view-tile-packer-write-json", mUI.tilePackerJson);
    GetUserProperty(*mWorkspace, "dlg-img-view-tile-packer-tile-width", mUI.tileWidth);
    GetUserProperty(*mWorkspace, "dlg-img-view-tile-packer-tile-height", mUI.tileHeight);
    GetUserProperty(*mWorkspace, "dlg-img-view-tile-packer-tile-padding", mUI.tilePadding);
    GetUserProperty(*mWorkspace, "dlg-img-view-tile-packer-tile-content-valign", mUI.tilePackerVerticalAlign);
    GetUserProperty(*mWorkspace, "dlg-img-view-tile-packer-tile-content-halign", mUI.tilePackerHorizontalAlign);
    GetUserProperty(*mWorkspace, "dlg-img-view-tile-packer-tile-content-resize", mUI.tilePackerResize);
    GetUserProperty(*mWorkspace, "dlg-img-view-tile-packer-last-image-file", &mLastTileWriteFile);
    mTrackingOffset = QPoint(xpos, ypos);

    on_cmbColorSpace_currentIndexChanged(0);
    on_cmbMinFilter_currentIndexChanged(0);
    on_cmbMagFilter_currentIndexChanged(0);
}

void DlgImgView::LoadGeometry()
{
    if (!mWorkspace)
        return;

    QByteArray geometry;
    if (GetUserProperty(*mWorkspace, "dlg-img-view-geometry", &geometry))
        restoreGeometry(geometry);
}

void DlgImgView::SaveState() const
{
    if (!mWorkspace)
        return;

    SetUserProperty(*mWorkspace, "dlg-img-view-tool", mUI.cmbToolMode);
    SetUserProperty(*mWorkspace, "dlg-img-view-draw-rects", mUI.chkShowRects);
    SetUserProperty(*mWorkspace, "dlg-img-view-geometry", saveGeometry());
    SetUserProperty(*mWorkspace, "dlg-img-view-image-file", mUI.imageFile);
    SetUserProperty(*mWorkspace, "dlg-img-view-json-file", mUI.jsonFile);
    SetUserProperty(*mWorkspace, "dlg-img-view-widget", mUI.widget);
    SetUserProperty(*mWorkspace, "dlg-img-view-color-space", mUI.cmbColorSpace);
    SetUserProperty(*mWorkspace, "dlg-img-view-min-filter", mUI.cmbMinFilter);
    SetUserProperty(*mWorkspace, "dlg-img-view-mag-filter", mUI.cmbMagFilter);
    SetUserProperty(*mWorkspace, "dlg-img-view-zoom", mUI.zoom);
    SetUserProperty(*mWorkspace, "dlg-img-view-cut-selection-selector", mUI.imageCutterSelection);
    SetUserProperty(*mWorkspace, "dlg-img-view-cut-format", mUI.imageCutterFormat);
    SetUserProperty(*mWorkspace, "dlg-img-view-cut-quality", mUI.imageCutterQuality);
    SetUserProperty(*mWorkspace, "dlg-img-view-cut-top-padding", mUI.imageCutterTopPadding);
    SetUserProperty(*mWorkspace, "dlg-img-view-cut-left-padding", mUI.imageCutterLeftPadding);
    SetUserProperty(*mWorkspace, "dlg-img-view-cut-right-padding", mUI.imageCutterRightPadding);
    SetUserProperty(*mWorkspace, "dlg-img-view-cut-bottom-padding", mUI.imageCutterBottomPadding);
    SetUserProperty(*mWorkspace, "dlg-img-view-cut-out-folder", mUI.imageCutterOutputFolder);
    SetUserProperty(*mWorkspace, "dlg-img-view-cut-out-name-template", mUI.imageCutterNameTemplate);
    SetUserProperty(*mWorkspace, "dlg-img-view-cut-overwrite", mUI.imageCutterOverwrite);
    SetUserProperty(*mWorkspace, "dlg-img-view-cut-pot", mUI.imageCutterPOT);
    SetUserProperty(*mWorkspace, "dlg-img-view-xpos", mTrackingOffset.x());
    SetUserProperty(*mWorkspace, "dlg-img-view-ypos", mTrackingOffset.y());
    SetUserProperty(*mWorkspace, "dlg-img-view-tile-packer-selection-selector", mUI.tilePackerSelection);
    SetUserProperty(*mWorkspace, "dlg-img-view-tile-packer-format", mUI.tilePackerFormat);
    SetUserProperty(*mWorkspace, "dlg-img-view-tile-packer-quality", mUI.tilePackerQuality);
    SetUserProperty(*mWorkspace, "dlg-img-view-tile-packer-resize-pot", mUI.tilePackerPOT);
    SetUserProperty(*mWorkspace, "dlg-img-view-tile-packer-write-json", mUI.tilePackerJson);
    SetUserProperty(*mWorkspace, "dlg-img-view-tile-packer-tile-width", mUI.tileWidth);
    SetUserProperty(*mWorkspace, "dlg-img-view-tile-packer-tile-height", mUI.tileHeight);
    SetUserProperty(*mWorkspace, "dlg-img-view-tile-packer-tile-padding", mUI.tilePadding);
    SetUserProperty(*mWorkspace, "dlg-img-view-tile-packer-tile-content-valign", mUI.tilePackerVerticalAlign);
    SetUserProperty(*mWorkspace, "dlg-img-view-tile-packer-tile-content-halign", mUI.tilePackerHorizontalAlign);
    SetUserProperty(*mWorkspace, "dlg-img-view-tile-packer-tile-content-resize", mUI.tilePackerResize);
    SetUserProperty(*mWorkspace, "dlg-img-view-tile-packer-last-image-file", mLastTileWriteFile);
}

QString DlgImgView::GetImageFileName() const
{
    return GetValue(mUI.imageFile);
}
QString DlgImgView::GetJsonFileName() const
{
    return GetValue(mUI.jsonFile);
}
QString DlgImgView::GetImageName() const
{
    for (auto& img : mPack.images)
    {
        if (img.selected)
            return img.name;
    }
    return "";
}

QRectF DlgImgView::GetImageRectF() const
{
    for (auto& img : mPack.images)
    {
        if (!img.selected)
            continue;

        const auto x = (float)img.xpos / (float)mWidth;
        const auto y = (float)img.ypos / (float)mHeight;
        const auto w = (float)img.width / (float)mWidth;
        const auto h = (float)img.height / (float)mHeight;
        return QRectF(x, y, w, h);
    }
    return QRectF{};
}

void DlgImgView::ResetTransform()
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

void DlgImgView::on_btnSelectImage_clicked()
{
    const auto& file = QFileDialog::getOpenFileName(this,
        tr("Select Image File"), "",
        tr("Images (*.png *.jpg *.jpeg)"));
    if (file.isEmpty())
        return;

    LoadImage(file);
    ResetTransform();

    QString json = app::FindImageJsonFile(file);
    if (!json.isEmpty())
        LoadJson(json);
}

void DlgImgView::on_btnSelectJson_clicked()
{
    const auto& file = QFileDialog::getOpenFileName(this,
        tr("Select Json File"), "",
        tr("Json (*.json)"));
    if (file.isEmpty())
        return;

    LoadJson(file);

    QString img =  app::FindJsonImageFile(file);
    if (!img.isEmpty())
    {
        LoadImage(img);
        ResetTransform();
    }
}

void DlgImgView::on_btnResetImage_clicked()
{
    mMaterial.reset();
    SetValue(mUI.imageFile, QString(""));
}

void DlgImgView::on_btnResetJson_clicked()
{
    mPack = ImagePack {};

    ClearTable(mUI.listWidget);
    SetValue(mUI.jsonFile, QString(""));
}

void DlgImgView::on_btnClose_clicked()
{
    if (mUI.btnSave->isEnabled())
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Question);
        msg.setText(tr("Save changes?"));
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        const auto ret = msg.exec();
        if (ret == QMessageBox::Cancel)
            return;
        else if (ret == QMessageBox::Yes)
        {
            if (!WriteImagePack(GetValue(mUI.jsonFile), mPack))
            {
                QMessageBox msg(this);
                msg.setStandardButtons(QMessageBox::Ok);
                msg.setIcon(QMessageBox::Critical);
                msg.setText(tr("There was a problem saving the file.\n"
                               "Please see the log for details."));
                msg.exec();
                return;
            }
        }
    }

    mClosed =  true;
    SaveState();
    close();
}

void DlgImgView::on_btnAccept_clicked()
{
    if (!MustHaveInput(mUI.imageFile))
        return;
    if (!MustHaveInput(mUI.jsonFile))
        return;

    bool have_selection = false;
    for (auto& img : mPack.images)
    {
        if (img.selected)
        {
            have_selection = true;
            break;
        }
    }
    if (!have_selection)
        return;

    SaveState();
    accept();
}
void DlgImgView::on_btnCancel_clicked()
{
    SaveState();
    reject();
}

void DlgImgView::on_btnSave_clicked()
{
    if (!MustHaveInput(mUI.imageFile))
        return;
    if (!MustHaveInput(mUI.jsonFile))
        return;
    if (!WriteImagePack(GetValue(mUI.jsonFile), mPack))
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("There was a problem saving the file.\n"
                       "Please see the log for details."));
        msg.exec();
        return;
    }
    SetEnabled(mUI.btnSave, false);
}
void DlgImgView::on_btnExport_clicked()
{
    if (mPack.images.empty())
        return;

    const QString image_file = GetValue(mUI.imageFile);
    if (image_file.isEmpty())
        return;

    const QFileInfo image_file_info(image_file);
    QString json_file = image_file;
    json_file.remove(image_file_info.suffix());
    json_file.append("json");

    json_file = QFileDialog::getSaveFileName(this,
        tr("Select Save File"), json_file, "JSON (*.json)");
    if (json_file.isEmpty())
        return;

    mPack.color_space  = GetValue(mUI.cmbColorSpace);
    mPack.min_filter   = GetValue(mUI.cmbMinFilter);
    mPack.mag_filter   = GetValue(mUI.cmbMagFilter);
    mPack.image_width  = mWidth;
    mPack.image_height = mHeight;
    mPack.padding      = 0;
    mPack.image_file   = image_file_info.fileName();
    mPack.app_name     = APP_TITLE;
    mPack.app_version  = APP_VERSION;

    if (!WriteImagePack(json_file, mPack))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr("Failed to write the JSON description file."));
        msg.exec();
        return;
    }
    INFO("Wrote JSON file '%1'.", json_file);
}

void DlgImgView::on_btnCutImages_clicked()
{
    if (!MustHaveInput(mUI.imageFile))
        return;
    if (!MustHaveInput(mUI.imageCutterNameTemplate))
        return;
    if (!MustHaveInput(mUI.imageCutterOutputFolder))
        return;

    QString out_path = GetValue(mUI.imageCutterOutputFolder);
    if (!app::MakePath(out_path))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("Failed to create folder. [%1]").arg(out_path));
        msg.setStandardButtons(QMessageBox::Ok);
        msg.exec();
        return;
    }

    QPixmap source_image;
    if (!source_image.load(GetValue(mUI.imageFile)) || source_image.isNull())
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("Failed to load image file. [%1]").arg(GetValue(mUI.imageFile)));
        msg.setStandardButtons(QMessageBox::Ok);
        msg.exec();
        return;
    }

    SetValue(mUI.imageCutterProgress, 0);
    SetRange(mUI.imageCutterProgress, 0, mPack.images.size());
    AutoHider hider(mUI.imageCutterProgress);
    QEventLoop footgun;

    const unsigned left_padding   = GetValue(mUI.imageCutterLeftPadding);
    const unsigned right_padding  = GetValue(mUI.imageCutterRightPadding);
    const unsigned top_padding    = GetValue(mUI.imageCutterTopPadding);
    const unsigned bottom_padding = GetValue(mUI.imageCutterBottomPadding);
    const bool power_of_two = GetValue(mUI.imageCutterPOT);
    const bool overwrite    = GetValue(mUI.imageCutterOverwrite);
    const QString selection = GetValue(mUI.imageCutterSelection);
    const bool all_images   = selection == QString("All images");
    unsigned counter = 0;
    for (unsigned i=0; i<mPack.images.size(); ++i)
    {
        if (Increment(mUI.imageCutterProgress))
            footgun.processEvents();

        const auto& img = mPack.images[i];
        if (!img.selected && !all_images)
            continue;

        if (img.width == 0 || img.height == 0)
        {
            WARN("Image has no size specified. Skipping image cutting. [name=%1]", img.name);
            continue;
        }
        const QPixmap& copy = source_image.copy(img.xpos, img.ypos, img.width, img.height);
        if (copy.isNull())
        {
            WARN("Source image copy failed.");
            continue;
        }
        QString img_name = img.name;
        // hack here, we might have the .file extension in the image name.
        // don't want to repeat that in the output name.
        if (img_name.endsWith(".png", Qt::CaseInsensitive))
            img_name.chop(4);
        else if (img_name.endsWith(".jpg", Qt::CaseInsensitive))
            img_name.chop(4);
        else if (img_name.endsWith(".jpeg", Qt::CaseInsensitive))
            img_name.chop(5);
        else if (img_name.endsWith(".bmp", Qt::CaseInsensitive))
            img_name.chop(4);

        QString out_name = GetValue(mUI.imageCutterNameTemplate);
        out_name.replace("%c", QString::number(counter++));
        out_name.replace("%i", QString::number(img.index));
        out_name.replace("%w", QString::number(img.width));
        out_name.replace("%h", QString::number(img.height));
        out_name.replace("%x", QString::number(img.xpos));
        out_name.replace("%y", QString::number(img.ypos));
        out_name.replace("%n", img_name);
        out_name.replace("%t", img.tag);
        if (out_name.isEmpty())
            continue;
        out_name.append(".");
        out_name.append(mUI.imageCutterFormat->currentText().toLower());
        QString out_file = app::JoinPath(out_path, out_name);
        if (app::FileExists(out_file) && !overwrite)
        {
            DEBUG("Skipping output file since it already exists. [file='%1']", out_file);
            continue;
        }

        const auto total_width = img.width  + left_padding + right_padding;
        const auto total_height = img.height + top_padding + bottom_padding;
        const auto buffer_width = power_of_two ?  base::NextPOT(total_width) : total_width;
        const auto buffer_height = power_of_two ? base::NextPOT(total_height) : total_height;
        const auto buffer_offset_x = (buffer_width - total_width) / 2;
        const auto buffer_offset_y = (buffer_height - total_height) / 2;
        QImage buffer(buffer_width, buffer_height, QImage::Format_ARGB32);
        buffer.fill(QColor(0x00, 0x00, 0x00, 0x00)); // transparent

        QPainter painter(&buffer);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawPixmap(buffer_offset_x,
                           buffer_offset_y,
                           total_width,
                           total_height,
                           copy);
        QImageWriter writer;
        writer.setFileName(out_file);
        writer.setQuality(GetValue(mUI.imageCutterQuality));
        writer.setFormat(mUI.imageCutterFormat->currentText().toLocal8Bit());
        if (!writer.write(buffer))
        {
            ERROR("Failed to write image file. [file='%1', error='%2']", out_file, writer.errorString());
        } else DEBUG("Wrote new image files. [file='%1']", out_file);
    }

    QMessageBox msg(this);
    msg.setIcon(QMessageBox::Information);
    msg.setText(tr("All done!"));
    msg.setWindowTitle("Image Cutting");
    msg.exec();
}

void DlgImgView::on_btnPackTiles_clicked()
{
    if (!MustHaveInput(mUI.imageFile))
        return;

    QPixmap source_image;
    if (!source_image.load(GetValue(mUI.imageFile)) || source_image.isNull())
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("Failed to load image file. [%1]").arg(GetValue(mUI.imageFile)));
        msg.setStandardButtons(QMessageBox::Ok);
        msg.exec();
        return;
    }

    SetValue(mUI.imageCutterProgress, 0);
    SetRange(mUI.imageCutterProgress, 0, mPack.images.size());
    AutoHider hider(mUI.imageCutterProgress);
    QEventLoop footgun;

    const unsigned tile_width  = GetValue(mUI.tileWidth);
    const unsigned tile_height = GetValue(mUI.tileHeight);
    const unsigned tile_padding = GetValue(mUI.tilePadding);
    const unsigned tile_box_width  = tile_width + 2 * tile_padding;
    const unsigned tile_box_height = tile_height + 2 * tile_padding;

    const QString selection = GetValue(mUI.tilePackerSelection);
    const bool all_images = selection == QString("All images");
    unsigned counter = 0;

    std::vector<app::PackingRectangle> images_for_packing;

    for (unsigned i=0; i<mPack.images.size(); ++i)
    {
        const auto& img = mPack.images[i];
        if (!img.selected && !all_images)
            continue;

        app::PackingRectangle rect;
        rect.width  = tile_box_width;
        rect.height = tile_box_height;
        rect.index  = i;
        images_for_packing.push_back(rect);
    }

    if (images_for_packing.empty())
        return;

    const bool power_of_two = GetValue(mUI.tilePackerPOT);
    const bool write_json = GetValue(mUI.tilePackerJson);
    const bool resample_images = GetValue(mUI.tilePackerResize);

    const auto ret = app::PackFixedSizeRectangles(images_for_packing, power_of_two);
    if (ret.width == 0 || ret.height == 0)
        return;

    QImage buffer(ret.width, ret.height, QImage::Format_ARGB32);
    buffer.fill(QColor(0x00, 0x00, 0x00, 0x00)); // transparent,

    QPainter painter(&buffer);
    painter.setCompositionMode(QPainter::CompositionMode_Source);

    const auto valign = (TilePackVerticalAlignment) GetValue(mUI.tilePackerVerticalAlign);
    const auto halign = (TilePackHorizontalAlignment) GetValue(mUI.tilePackerHorizontalAlign);

    for (const auto& packed_img : images_for_packing)
    {
        const auto& src_img = mPack.images[packed_img.index];
        const auto dst_tile_xpos   = packed_img.xpos + tile_padding;
        const auto dst_tile_ypos   = packed_img.ypos + tile_padding;
        const auto dst_tile_width  = packed_img.width - 2*tile_padding;
        const auto dst_tile_height = packed_img.height - 2*tile_padding;

        unsigned copy_width  = 0;
        unsigned copy_height = 0;

        auto src_img_xpos   = src_img.xpos;
        auto src_img_ypos   = src_img.ypos;
        auto src_img_width  = src_img.width;
        auto src_img_height = src_img.height;

        if (resample_images)
        {
            const auto scale_factor = std::min((double)dst_tile_width/(double)src_img_width,
                                               (double)dst_tile_height/(double)src_img_height);
            copy_width  = src_img_width * scale_factor;
            copy_height = src_img_height * scale_factor;
        }
        else
        {
            copy_width  = std::min(src_img.width, dst_tile_width);
            copy_height = std::min(src_img.height, dst_tile_height);

            if (src_img.width > dst_tile_width)
                src_img_xpos += ((src_img.width - dst_tile_width) / 2);
            if (src_img.height > dst_tile_height)
                src_img_ypos += ((src_img.height - dst_tile_height) / 2);

        }

        auto copy_xpos = dst_tile_xpos;
        auto copy_ypos = dst_tile_ypos;

        if (copy_width < dst_tile_width) {
            if (halign == TilePackHorizontalAlignment::Center)
                copy_xpos += ((dst_tile_width - copy_width) / 2);
            else if (halign == TilePackHorizontalAlignment::Left)
                copy_xpos += 0;
            else if (halign == TilePackHorizontalAlignment::Right)
                copy_xpos += (dst_tile_width - copy_width);
        }
        if (copy_height < dst_tile_height) {
            if (valign == TilePackVerticalAlignment::Center)
                copy_ypos += ((dst_tile_height - copy_height) / 2);
            else if (valign == TilePackVerticalAlignment::Top)
                copy_ypos += 0;
            else if (valign == TilePackVerticalAlignment::Bottom)
                copy_ypos += (dst_tile_height - copy_height);
        }

        painter.drawPixmap(QRectF(copy_xpos, copy_ypos, copy_width, copy_height), source_image,
                           QRectF(src_img_xpos, src_img_ypos, src_img_width, src_img_height));

        if (Increment(mUI.tilePackerProgress))
            footgun.processEvents();
    }

    QString filter;
    QString filename;
    if (mUI.tilePackerFormat->currentText() == "JGP")
        filter = "Images (*.jpg)";
    else if (mUI.tilePackerFormat->currentText() == "PNG")
        filter = "Images (*.png)";
    else if (mUI.tilePackerFormat->currentText() == "BMP")
        filter = "Images (*.bmp)";

    filename = mLastTileWriteFile;
    if (filename.isEmpty())
        filename = "tilemap" + mUI.tilePackerFormat->currentText().toLower();

    const auto& file = QFileDialog::getSaveFileName(this,
        tr("Select Save File"), filename, filter);
    if (file.isEmpty())
        return;

    QImageWriter writer;
    writer.setFormat(mUI.tilePackerFormat->currentText().toLatin1());
    writer.setQuality(mUI.tilePackerQuality->value());
    writer.setFileName(file);
    if (!writer.write(buffer))
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("Failed to write the image.\n%1").arg(writer.errorString()));
        msg.exec();
        return;
    }

    if (write_json)
    {
        ImagePack::Tilemap tilemap;
        tilemap.tile_width  = tile_width;
        tilemap.tile_height = tile_height;
        tilemap.xoffset     = 0;
        tilemap.yoffset     = 0;

        ImagePack pack;
        pack.image_file   = QFileInfo(filename).fileName();
        pack.padding      = tile_padding;
        pack.image_width  = ret.width;
        pack.image_height = ret.height;
        pack.mag_filter   = GetValue(mUI.cmbMagFilter);
        pack.min_filter   = GetValue(mUI.cmbMinFilter);
        pack.color_space  = gfx::TextureFileSource::ColorSpace::sRGB;
        pack.tilemap      = tilemap;
        pack.power_of_two_hint = power_of_two;

        if (!WriteImagePack(filename + ".json", pack))
        {
            QMessageBox msg(this);
            msg.setIcon(QMessageBox::Critical);
            msg.setStandardButtons(QMessageBox::Ok);
            msg.setText(tr("Failed to write the JSON description file."));
            msg.exec();
            return;
        }
    }
    mLastTileWriteFile = filename;
    INFO("Wrote tilemap image to '%1'", file);
}

void DlgImgView::on_btnSelectOut_clicked()
{
    const auto& dir = QFileDialog::getExistingDirectory(this,
        tr("Select Output Directory"), GetValue(mUI.imageCutterOutputFolder));
    if (dir.isEmpty())
        return;
    SetValue(mUI.imageCutterOutputFolder, dir);
}

void DlgImgView::on_cmbColorSpace_currentIndexChanged(int)
{
    if (!mClass)
        return;
    auto* source = mClass->GetTextureMap(0)->GetTextureSource(0);
    auto* file_source = dynamic_cast<gfx::TextureFileSource*>(source);
    file_source->SetColorSpace(GetValue(mUI.cmbColorSpace));
}

void DlgImgView::on_cmbMinFilter_currentIndexChanged(int)
{
    if (!mClass)
        return;

    mClass->SetTextureMinFilter(GetValue(mUI.cmbMinFilter));
}
void DlgImgView::on_cmbMagFilter_currentIndexChanged(int)
{
    if (!mClass)
        return;

    mClass->SetTextureMagFilter(GetValue(mUI.cmbMagFilter));
}

void DlgImgView::on_widgetColor_colorChanged(QColor color)
{
    mUI.widget->SetClearColor(ToGfx(color));
}

void DlgImgView::on_listWidget_itemSelectionChanged()
{
    for (auto& img : mPack.images)
        img.selected = false;

    const auto& selection = GetSelection(mUI.listWidget);
    for (int i=0; i<selection.size(); ++i)
    {
        const auto item = selection[i];
        const auto index = static_cast<size_t>(item.row());
        base::SafeIndex(mPack.images, index).selected = true;
    }

    SetEnabled(mUI.btnAccept, false);

    if (mDialogMode)
    {
        for (const auto& img : mPack.images)
        {
            if (img.selected)
            {
                SetEnabled(mUI.btnAccept, true);
                break;
            }
        }
    }
}

void DlgImgView::on_tabWidget_currentChanged(int)
{
    const auto& index = GetSelectedIndex(mUI.listWidget);
    if (index.isValid())
        mUI.listWidget->scrollTo(index);
}

void DlgImgView::on_renameTemplate_returnPressed()
{
    std::vector<QString> original_names;
    for (const auto& img : mPack.images)
        original_names.push_back(img.name);

    const auto& selection = GetSelection(mUI.listWidget);
    if (selection.isEmpty())
    {
        QMessageBox msg(this);
        msg.setText("You have nothing selected!");
        msg.setIcon(QMessageBox::Information);
        msg.exec();
        return;
    }

    unsigned counter = 0;
    for (int i=0; i<selection.size(); ++i)
    {
        QString out_name = GetValue(mUI.renameTemplate);

        const auto item = selection[i];
        const auto index = static_cast<size_t>(item.row());
        auto& img = mPack.images[index];

        out_name.replace("%c", QString::number(counter++));
        out_name.replace("%i", QString::number(img.index));
        out_name.replace("%w", QString::number(img.width));
        out_name.replace("%h", QString::number(img.height));
        out_name.replace("%x", QString::number(img.xpos));
        out_name.replace("%y", QString::number(img.ypos));
        out_name.replace("%t", img.tag);
        img.name = out_name;
        SetTableItem(mUI.listWidget, item.row(), 0, out_name);
    }

    QMessageBox msg(this);
    msg.setWindowTitle("Confirm Rename");
    msg.setText("Do you want to keep these changes?");
    msg.setIcon(QMessageBox::Question);
    msg.setStandardButtons(QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No);
    if (msg.exec() == QMessageBox::StandardButton::Yes)
    {
        SetEnabled(mUI.btnSave, true);
        return;
    }

    ASSERT(mPack.images.size() == original_names.size());
    for (size_t i=0; i<original_names.size(); ++i)
    {
        mPack.images[i].name = original_names[i];
        SetTableItem(mUI.listWidget, i, 0, original_names[i]);
    }
}

void DlgImgView::on_tagTemplate_returnPressed()
{
    std::vector<QString> original_tags;
    for (const auto& img: mPack.images)
        original_tags.push_back(img.tag);

    const auto& selection = GetSelection(mUI.listWidget);
    if (selection.isEmpty())
    {
        QMessageBox msg(this);
        msg.setText("You have nothing selected!");
        msg.setIcon(QMessageBox::Information);
        msg.exec();
        return;
    }

    unsigned counter = 0;
    for (int i=0; i<selection.size(); ++i)
    {
        QString out_tag = GetValue(mUI.tagTemplate);

        const auto item = selection[i];
        const auto index = static_cast<size_t>(item.row());
        auto& img = mPack.images[index];

        out_tag.replace("%c", QString::number(counter++));
        out_tag.replace("%i", QString::number(img.index));
        out_tag.replace("%w", QString::number(img.width));
        out_tag.replace("%h", QString::number(img.height));
        out_tag.replace("%x", QString::number(img.xpos));
        out_tag.replace("%y", QString::number(img.ypos));
        out_tag.replace("%t", img.tag);
        img.tag = out_tag;
        SetTableItem(mUI.listWidget, item.row(), 2, out_tag);
    }

    QMessageBox msg(this);
    msg.setWindowTitle("Confirm Rename");
    msg.setText("Do you want to keep these changes?");
    msg.setIcon(QMessageBox::Question);
    msg.setStandardButtons(QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No);
    if (msg.exec() == QMessageBox::StandardButton::Yes)
    {
        SetEnabled(mUI.btnSave, true);
        return;
    }

    ASSERT(mPack.images.size() == original_tags.size());
    for (size_t i=0; i<original_tags.size(); ++i)
    {
        mPack.images[i].name = original_tags[i];
        SetTableItem(mUI.listWidget, i, 2, original_tags[i]);
    }
}

void DlgImgView::finished()
{
    mClosed = true;
    mUI.widget->dispose();
}
void DlgImgView::timer()
{
    mUI.widget->triggerPaint();
}

void DlgImgView::keyPressEvent(QKeyEvent* event)
{
    if (!OnKeyPress(event))
        QDialog::keyPressEvent(event);
}

bool DlgImgView::eventFilter(QObject* destination, QEvent* event)
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
    else if (alt && key->key() == Qt::Key_3)
        mUI.tabWidget->setCurrentIndex(2);
    else return false;

    return true;
}

void DlgImgView::OnPaintScene(gfx::Painter& painter, double secs)
{
    SetValue(mUI.widgetColor, mUI.widget->GetCurrentClearColor());

    const float width  = mUI.widget->width();
    const float height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    if (!mMaterial)
    {
        ShowInstruction(
            "View the contents of a packed image file (atlas).\n"
            "The contents can be viewed visually and textually.\n\n"
            "INSTRUCTIONS\n"
            "1. Select a (packed) image file.\n"
            "2. Select an associated JSON file.\n",
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

    static auto selection_material_class = gfx::CreateMaterialClassFromImage(res::AcceptIcon);
    selection_material_class.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
    selection_material_class.SetBaseColor(gfx::Color4f(1.0f, 1.0f, 1.0f, 1.0f));
    static auto selection_material = gfx::MaterialInstance(selection_material_class);

    const bool draw_rects = GetValue(mUI.chkShowRects);

    for (size_t index=0; index<mPack.images.size(); ++index)
    {
        const auto& img = mPack.images[index];
        if (!img.selected && index != mIndexUnderMouse && !draw_rects)
            continue;

        gfx::FRect rect(0.0f, 0.0f, img.width*zoom, img.height*zoom);
        rect.Translate(xpos, ypos);
        rect.Translate(mTrackingOffset.x(), mTrackingOffset.y());
        rect.Translate(img.xpos * zoom, img.ypos * zoom);

        if (draw_rects)
        {
            gfx::DrawRectOutline(painter, rect, gfx::CreateMaterialFromColor(gfx::Color::HotPink));
        }

        if (index == mIndexUnderMouse)
        {
            gfx::DrawRectOutline(painter, rect, gfx::CreateMaterialFromColor(gfx::Color::Green));
        }
        if (img.selected)
        {
            rect.SetWidth(32.0f);
            rect.SetHeight(32.0f);
            gfx::FillShape(painter, rect, gfx::Circle(), selection_material);
        }
    }

    ShowMessage(app::toString("%1 x %2 @ %3bpp", mWidth, mHeight, mDepth), painter);
}

void DlgImgView::OnMousePress(QMouseEvent* mickey)
{
    mStartPoint = mickey->pos();

    if (mickey->button() == Qt::RightButton)
        mMode = Mode::Tracking;
    else if (mickey->button() == Qt::LeftButton)
    {
        if (!mMaterial)
            return;

        const ToolMode mode = GetValue(mUI.cmbToolMode);
        if (mode == ToolMode::DefineMode)
        {
            MagicMouseSelect();
        }
        else if (mode == ToolMode::SelectMode)
        {
            mMode = Mode::Selecting;
            ToggleMouseSelection();
        }
        else BUG("Bug on image tool mode.");
    }
}

void DlgImgView::OnMouseMove(QMouseEvent* mickey)
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

    const auto current_image_pixel = MapToImage(mCurrentPoint);
    const auto current_posx = current_image_pixel.x();
    const auto current_posy = current_image_pixel.y();

    // update the current index under mouse
    for (mIndexUnderMouse=0; mIndexUnderMouse<mPack.images.size(); ++mIndexUnderMouse)
    {
        const auto& img = mPack.images[mIndexUnderMouse];
        if (current_posx < img.xpos || current_posx > img.xpos+img.width)
            continue;
        if (current_posy < img.ypos || current_posy > img.ypos+img.height)
            continue;
        break;
    }

    if (mMode == Mode::Selecting)
    {
        ToggleMouseSelection();
    }
}

void DlgImgView::OnMouseRelease(QMouseEvent* mickey)
{
    mMode = Mode::Nada;
    mTilesTouched.clear();
}

void DlgImgView::OnMouseDoubleClick(QMouseEvent* mickey)
{
    if (!mDialogMode)
        return;

    OnMousePress(mickey);
    for (auto& img : mPack.images)
    {
        if (img.selected)
        {
            accept();
            return;
        }
    }
}

bool DlgImgView::OnKeyPress(QKeyEvent* key)
{
    const bool ctrl = key->modifiers() & Qt::ControlModifier;
    const bool alt  = key->modifiers() & Qt::AltModifier;

    if (alt && key->key() == Qt::Key_1)
        mUI.tabWidget->setCurrentIndex(0);
    else if (alt && key->key() == Qt::Key_2)
        mUI.tabWidget->setCurrentIndex(1);
    else if (alt && key->key() == Qt::Key_3)
        mUI.tabWidget->setCurrentIndex(2);
    else if (ctrl && key->key() == Qt::Key_W)
        on_btnClose_clicked();
    else if (key->key() == Qt::Key_Escape)
    {
        bool had_selection = false;
        for (auto& image : mPack.images)
        {
            had_selection = had_selection || image.selected;
            image.selected = false;
        }
        return had_selection;
    }
    else return false;

    return true;
}

void DlgImgView::ToggleMouseSelection()
{
    if (mIndexUnderMouse >= mPack.images.size())
        return;

    if (base::Contains(mTilesTouched, mIndexUnderMouse))
        return;

    if (mDialogMode)
    {
        for (auto& img : mPack.images)
            img.selected = false;

        SetEnabled(mUI.btnAccept, false);
    }

    mPack.images[mIndexUnderMouse].selected = !mPack.images[mIndexUnderMouse].selected;
    mTilesTouched.insert(mIndexUnderMouse);

    if (mDialogMode)
    {
        for (const auto& img : mPack.images)
        {
            if (img.selected)
            {
                SetEnabled(mUI.btnAccept, true);
                break;
            }
        }
    }

    for (size_t i = 0; i < mPack.images.size(); ++i)
    {
        SelectTableRow(mUI.listWidget, i, mPack.images[i].selected);
    }
}

void DlgImgView::MagicMouseSelect()
{
    const auto* texture_source = mClass->GetTextureMap(0)->GetTextureSource(0);
    const auto& bitmap = texture_source->GetData();
    const auto& view = bitmap->GetReadView();
    const auto& point = MapToImage(mStartPoint);
    const auto point_x = point.x();
    const auto point_y = point.y();
    auto ret = gfx::FindImageRectangle(*view, gfx::IPoint(point_x, point_y));
    if (ret.IsEmpty())
        return;

    // dupe?
    for (const auto& i : mPack.images)
    {
        if (i.width == ret.GetWidth() && i.height == ret.GetHeight() && i.xpos == ret.GetX() && i.ypos == ret.GetY())
            return;
    }

    bool accepted = false;
    const auto& name = QInputDialog::getText(this,
        tr("Rename Image"),
        tr("Image Name:"), QLineEdit::Normal, "", &accepted);
    if (!accepted)
        return;

    const auto row = mPack.images.size();

    ImagePack::Image img;
    img.height = ret.GetHeight();
    img.width  = ret.GetWidth();
    img.xpos   = ret.GetX();
    img.ypos   = ret.GetY();
    img.name   = name;
    mPack.images.push_back(img);

    ResizeTable(mUI.listWidget,  row+1, 8);
    mUI.listWidget->setHorizontalHeaderLabels({"Name", "Char", "Tag", "Width", "Height", "X Pos", "Y Pos", "Index"});

    SetTableItem(mUI.listWidget, row, 0, img.name);
    SetTableItem(mUI.listWidget, row, 1, img.character);
    SetTableItem(mUI.listWidget, row, 2, img.tag);
    SetTableItem(mUI.listWidget, row, 3, img.width);
    SetTableItem(mUI.listWidget, row, 4, img.height);
    SetTableItem(mUI.listWidget, row, 5, img.xpos);
    SetTableItem(mUI.listWidget, row, 6, img.ypos);
    SetTableItem(mUI.listWidget, row, 7, img.index);
}

QPoint DlgImgView::MapToImage(const QPoint& point) const
{
    const float width = mUI.widget->width();
    const float height = mUI.widget->height();
    const float zoom   = GetValue(mUI.zoom);
    const float img_width = mWidth * zoom;
    const float img_height = mHeight * zoom;
    const auto xpos = (width - img_width) * 0.5f;
    const auto ypos = (height - img_height) * 0.5f;
    const int mouse_posx = (point.x() - mTrackingOffset.x() - xpos) / zoom;
    const int mouse_posy = (point.y() - mTrackingOffset.y() - ypos) / zoom;

    return {mouse_posx, mouse_posy};

}


} // namespace
