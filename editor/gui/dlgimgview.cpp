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
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <algorithm>

#include "base/json.h"
#include "graphics/painter.h"
#include "graphics/drawing.h"
#include "editor/app/eventlog.h"
#include "editor/app/workspace.h"
#include "editor/gui/dlgimgview.h"
#include "editor/gui/utility.h"
#include "editor/gui/drawing.h"

namespace {
    bool ReadTexturePack(const QString& file, std::vector<gui::DlgImgView::Image>* out)
    {
        QFile io(file);
        if (!io.open(QIODevice::ReadOnly))
        {
            ERROR("Failed to open file for reading. [file='%1', error=%2]", file, io.error());
            return false;
        }
        const auto& buff = io.readAll();
        if (buff.isEmpty())
        {
            ERROR("JSON file contains no JSON content. [file='%1']", file);
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
        if (!json.contains("images") || !json["images"].is_array())
        {
            ERROR("JSON file doesn't contain images array. [file='%1']", file);
            return false;
        }
        for (const auto& img_json : json["images"].items())
        {
            const auto& obj = img_json.value();

            gui::DlgImgView::Image img;

            std::string name;
            std::string character;
            unsigned width  = 0;
            unsigned height = 0;
            unsigned index  = 0;

            if (base::JsonReadSafe(obj, "name", &name))
                img.name = app::FromUtf8(name);
            if (base::JsonReadSafe(obj, "char", &character))
                img.character = app::FromUtf8(character);
            if (base::JsonReadSafe(obj, "width", &width))
                img.width = width;
            if (base::JsonReadSafe(obj, "height", &height))
                img.height = height;
            if (base::JsonReadSafe(obj, "index", &index))
                img.index = index;
            if (!base::JsonReadSafe(obj, "xpos", &img.xpos))
                WARN("Image is missing 'xpos' attribute. [file='%1'", file);
            if (!base::JsonReadSafe(obj, "ypos", &img.ypos))
                WARN("Image is missing 'ypos' attribute. [file='%1']", file);

            out->push_back(std::move(img));
        }

        // finally sort based on the image index.
        std::sort(std::begin(*out), std::end(*out), [&](const auto& a, const auto& b) {
            return a.index < b.index;
        });

        INFO("Successfully parsed '%1'. %2 images found.", file, out->size());
        return true;
    }
} // namespace

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
    PopulateFromEnum<gfx::detail::TextureFileSource::ColorSpace>(mUI.cmbColorSpace);

    SetVisible(mUI.btnCancel, false);
    SetVisible(mUI.btnAccept, false);
    SetValue(mUI.zoom, 1.0f);
}

void DlgImgView::LoadImage(const QString& file)
{
    auto source = std::make_unique<gfx::detail::TextureFileSource>();
    source->SetFileName(app::ToUtf8(file));
    source->SetName(app::ToUtf8(file));
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

    mWidth    = img_width;
    mHeight   = img_height;
    mClass = std::make_shared<gfx::TextureMap2DClass>();
    mClass->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
    mClass->SetTexture(std::move(source));
    mClass->SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
    mClass->SetGamma(1.0f);
    mMaterial = gfx::CreateMaterialInstance(mClass);
    mSelectedIndex = mList.size(); // clear selection from image.
    SetValue(mUI.imageFile, file);
    SetValue(mUI.zoom, scale);
}

void DlgImgView::LoadJson(const QString& file)
{
    std::vector<Image> image_list;
    if (!ReadTexturePack(file, &image_list))
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("There was a problem reading the file.\n'%1'\n"
                       "Perhaps the image is not a valid JSON file?").arg(file));
        msg.exec();
        return;
    }
    ClearTable(mUI.listWidget);
    ResizeTable(mUI.listWidget, image_list.size(), 7);
    mUI.listWidget->setHorizontalHeaderLabels({"Name", "Char", "Width", "Height", "X Pos", "Y Pos", "Index"});

    for (unsigned row=0; row<image_list.size(); ++row)
    {
        const auto& img = image_list[row];
        SetTableItem(mUI.listWidget, row, 0, img.name);
        SetTableItem(mUI.listWidget, row, 1, img.character);

        if (img.width.has_value())
            SetTableItem(mUI.listWidget, row, 2, img.width.value());
        if (img.height.has_value())
            SetTableItem(mUI.listWidget, row, 3, img.height.value());

        SetTableItem(mUI.listWidget, row, 4, img.xpos);
        SetTableItem(mUI.listWidget, row, 5, img.ypos);

        if (img.index.has_value())
            SetTableItem(mUI.listWidget, row, 6, img.index.value());
    }

    mList = std::move(image_list);
    mSelectedIndex = mList.size();
    SetValue(mUI.jsonFile, file);
}
void DlgImgView::SetDialogMode(app::Workspace* workspace)
{
    SetVisible(mUI.btnClose, false);
    SetVisible(mUI.btnAccept, true);
    SetVisible(mUI.btnCancel, true);
    SetEnabled(mUI.btnAccept, false);
    mDialogMode = true;
    mWorkspace  = workspace;
    if (mWorkspace)
    {
        QByteArray geometry;
        if (GetUserProperty(*mWorkspace, "dlg_img_view_geometry", &geometry))
            restoreGeometry(geometry);
        GetUserProperty(*mWorkspace, "dlg_img_view_widget", mUI.widget);
    }
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
    if (mUI.tabWidget->currentIndex() == 0)
    {
        if (mSelectedIndex < mList.size())
            return mList[mSelectedIndex].name;
    }
    else
    {
        const auto row = mUI.listWidget->currentRow();
        if (row > 0 && row < mList.size())
            return mList[row].name;
    }
    BUG("Image index is not properly set.");
    return "";
}

void DlgImgView::on_btnSelectImage_clicked()
{
    const auto& file = QFileDialog::getOpenFileName(this,
        tr("Select Image File"), "",
        tr("Images (*.png *.jpg *.jpeg)"));
    if (file.isEmpty())
        return;

    LoadImage(file);
    if (FileExists(file + ".json"))
        LoadJson(file + ".json");
}

void DlgImgView::on_btnSelectJson_clicked()
{
    const auto& file = QFileDialog::getOpenFileName(this,
        tr("Select Json File"), "",
        tr("Json (*.json)"));
    if (file.isEmpty())
        return;

    LoadJson(file);

    QString img = file;
    img.replace(".json", ".png", Qt::CaseInsensitive);
    if (FileExists(img))
        LoadImage(img);

}

void DlgImgView::on_btnClose_clicked()
{
    mClosed =  true;
    close();
}

void DlgImgView::on_btnAccept_clicked()
{
    if (!FileExists(mUI.imageFile))
    {
        mUI.imageFile->setFocus();
        return;
    }
    if (!FileExists(mUI.jsonFile))
    {
        mUI.jsonFile->setFocus();
        return;
    }
    if (mUI.tabWidget->currentIndex() == 0)
    {
        if (mSelectedIndex == mList.size())
            return;
    }
    else
    {
        const auto row = mUI.listWidget->currentRow();
        if (row == -1)
            return;
    }

     if (mWorkspace)
     {
         SetUserProperty(*mWorkspace, "dlg_img_view_geometry", saveGeometry());
         SetUserProperty(*mWorkspace, "dlg_img_view_widget", mUI.widget);
     }

    accept();
}
void DlgImgView::on_btnCancel_clicked()
{
    if (mWorkspace)
    {
        SetUserProperty(*mWorkspace, "dlg_img_view_geometry", saveGeometry());
        SetUserProperty(*mWorkspace, "dlg_img_view_widget", mUI.widget);
    }

    reject();
}

void DlgImgView::on_cmbColorSpace_currentIndexChanged(int)
{
    if (!mClass)
        return;
    auto* source = mClass->GetTextureSource();
    auto* file_source = dynamic_cast<gfx::detail::TextureFileSource*>(source);
    file_source->SetColorSpace(GetValue(mUI.cmbColorSpace));
}

void DlgImgView::on_widgetColor_colorChanged(QColor color)
{
    mUI.widget->SetClearColor(ToGfx(color));
}

void DlgImgView::on_listWidget_itemSelectionChanged()
{
    const auto row = mUI.listWidget->currentRow();
    SetEnabled(mUI.btnAccept, row != -1);
}

void DlgImgView::on_tabWidget_currentChanged(int)
{
    if (mUI.tabWidget->currentIndex() == 0)
    {
        if (mSelectedIndex == mList.size())
            SetEnabled(mUI.btnAccept, false);
        else SetEnabled(mUI.btnAccept, true);
    }
    else
    {
        const auto row = mUI.listWidget->currentRow();
        SetEnabled(mUI.btnAccept, row != -1);
    }
}

void DlgImgView::finished()
{
    mUI.widget->dispose();
}
void DlgImgView::timer()
{
    mUI.widget->triggerPaint();
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

    if (mList.empty() || mSelectedIndex == mList.size())
        return;

    const auto& img = mList[mSelectedIndex];

    gfx::FRect sel_rect(0.0f, 0.0f, img.width.value_or(0.0f)*zoom, img.height.value_or(0.0f)*zoom);
    sel_rect.Translate(xpos, ypos);
    sel_rect.Translate(mTrackingOffset.x(), mTrackingOffset.y());
    sel_rect.Translate(img.xpos*zoom, img.ypos*zoom);
    gfx::DrawRectOutline(painter, sel_rect, gfx::CreateMaterialFromColor(gfx::Color::Green));
}
void DlgImgView::OnMousePress(QMouseEvent* mickey)
{
    mStartPoint = mickey->pos();

    if (mickey->button() == Qt::RightButton)
        mTracking = true;
    else if (mickey->button() == Qt::LeftButton)
    {
        if (mList.empty() || !mMaterial)
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

        for (mSelectedIndex=0; mSelectedIndex<mList.size(); ++mSelectedIndex)
        {
            const auto& img = mList[mSelectedIndex];
            if (mouse_posx < img.xpos || mouse_posx > img.xpos+img.width.value_or(0.0f))
                continue;
            if (mouse_posy < img.ypos || mouse_posy > img.ypos+img.height.value_or(0.0f))
                continue;

            break;
        }
        if (mSelectedIndex == mList.size())
            SetEnabled(mUI.btnAccept, false);
        else SetEnabled(mUI.btnAccept, true);
    }
}
void DlgImgView::OnMouseMove(QMouseEvent* mickey)
{
    mCurrentPoint = mickey->pos();

    if (!mTracking)
        return;

    mTrackingOffset += (mCurrentPoint - mStartPoint);
    mStartPoint = mCurrentPoint;
}
void DlgImgView::OnMouseRelease(QMouseEvent* mickey)
{
    mTracking = false;
}

void DlgImgView::OnMouseDoubleClick(QMouseEvent* mickey)
{
    if (!mDialogMode)
        return;

    OnMousePress(mickey);
    if (mSelectedIndex < mList.size())
        accept();
}

bool DlgImgView::OnKeyPress(QKeyEvent* event)
{
    return false;
}

} // namespace
