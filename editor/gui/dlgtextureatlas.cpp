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
#include "editor/gui/dlgtextureatlas.h"
#include "editor/gui/utility.h"

namespace {
    bool ReadTexturePack(const QString& file, std::vector<gui::DlgTextureAtlas::Image>* out)
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
            std::string name;
            unsigned w, h, x, y, index;
            if (!base::JsonReadSafe(obj, "width", &w) ||
                !base::JsonReadSafe(obj, "height", &h) ||
                !base::JsonReadSafe(obj, "xpos", &x) ||
                !base::JsonReadSafe(obj, "ypos", &y) ||
                !base::JsonReadSafe(obj, "name", &name) ||
                !base::JsonReadSafe(obj, "index", &index))
            {
                WARN("Failed to read JSON image box data.");
                continue;
            }
            gui::DlgTextureAtlas::Image img;
            img.name   = app::FromUtf8(name);
            img.width  = w;
            img.height = h;
            img.xpos   = x;
            img.ypos   = y;
            img.index  = index;
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

DlgTextureAtlas::DlgTextureAtlas(QWidget* parent) : QDialog(parent)
{
    mUI.setupUi(this);
    mUI.widget->onPaintScene   = std::bind(&DlgTextureAtlas::OnPaintScene,   this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onMouseMove    = std::bind(&DlgTextureAtlas::OnMouseMove,    this, std::placeholders::_1);
    mUI.widget->onMousePress   = std::bind(&DlgTextureAtlas::OnMousePress,   this, std::placeholders::_1);
    mUI.widget->onMouseRelease = std::bind(&DlgTextureAtlas::OnMouseRelease, this, std::placeholders::_1);
    mUI.widget->onMouseDoubleClick = std::bind(&DlgTextureAtlas::OnMouseDoubleClick, this, std::placeholders::_1);
    mUI.widget->onKeyPress     = std::bind(&DlgTextureAtlas::OnKeyPress,     this, std::placeholders::_1);
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

    connect(this, &QDialog::finished, this, &DlgTextureAtlas::finished);
    connect(&mTimer, &QTimer::timeout, this, &DlgTextureAtlas::timer);

    SetVisible(mUI.btnCancel, false);
    SetVisible(mUI.btnAccept, false);
}

void DlgTextureAtlas::LoadImage(const QString& file)
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

void DlgTextureAtlas::LoadJson(const QString& file)
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
    ResizeTable(mUI.listWidget, image_list.size(), 5);
    mUI.listWidget->setHorizontalHeaderLabels({"Name", "Width", "Height", "X", "Y"});

    for (unsigned row=0; row<image_list.size(); ++row)
    {
        const auto& img = image_list[row];
        SetTableItem(mUI.listWidget, row, 0, img.name);
        SetTableItem(mUI.listWidget, row, 1, img.width);
        SetTableItem(mUI.listWidget, row, 2, img.height);
        SetTableItem(mUI.listWidget, row, 3, img.xpos);
        SetTableItem(mUI.listWidget, row, 4, img.ypos);
    }

    mList = std::move(image_list);
    mSelectedIndex = mList.size();
    SetValue(mUI.jsonFile, file);
}
void DlgTextureAtlas::SetDialogMode()
{
    SetVisible(mUI.btnClose, false);
    SetVisible(mUI.btnAccept, true);
    SetVisible(mUI.btnCancel, true);
    SetEnabled(mUI.btnAccept, false);
}

QString DlgTextureAtlas::GetImageFileName() const
{
    return GetValue(mUI.imageFile);
}
QString DlgTextureAtlas::GetJsonFileName() const
{
    return GetValue(mUI.jsonFile);
}
QString DlgTextureAtlas::GetImageName() const
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

void DlgTextureAtlas::on_btnSelectImage_clicked()
{
    const auto& file = QFileDialog::getOpenFileName(this,
        tr("Select Image File"), "",
        tr("Images (*.png *.jpg *.jpeg)"));
    if (file.isEmpty())
        return;

    LoadImage(file);
}

void DlgTextureAtlas::on_btnSelectJson_clicked()
{
    const auto& file = QFileDialog::getOpenFileName(this,
        tr("Select Json File"), "",
        tr("Json (*.json)"));
    if (file.isEmpty())
        return;

    LoadJson(file);
}

void DlgTextureAtlas::on_btnClose_clicked()
{
    close();
}

void DlgTextureAtlas::on_btnAccept_clicked()
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

    accept();
}
void DlgTextureAtlas::on_btnCancel_clicked()
{
    reject();
}

void DlgTextureAtlas::on_widgetColor_colorChanged(QColor color)
{
    mUI.widget->SetClearColor(ToGfx(color));
}

void DlgTextureAtlas::on_listWidget_itemSelectionChanged()
{
    const auto row = mUI.listWidget->currentRow();
    SetEnabled(mUI.btnAccept, row != -1);
}

void DlgTextureAtlas::on_tabWidget_currentChanged(int)
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

void DlgTextureAtlas::finished()
{
    mUI.widget->dispose();
}
void DlgTextureAtlas::timer()
{
    mUI.widget->triggerPaint();
}

void DlgTextureAtlas::OnPaintScene(gfx::Painter& painter, double secs)
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

    gfx::FRect img_rect(0.0f, 0.0f, img_width, img_height);
    img_rect.Translate(xpos, ypos);
    img_rect.Translate(mTrackingOffset.x(), mTrackingOffset.y());
    gfx::FillRect(painter, img_rect, *mMaterial);

    if (mList.empty() || mSelectedIndex == mList.size())
        return;

    const auto& img = mList[mSelectedIndex];

    gfx::FRect sel_rect(0.0f, 0.0f, img.width*zoom, img.height*zoom);
    sel_rect.Translate(xpos, ypos);
    sel_rect.Translate(mTrackingOffset.x(), mTrackingOffset.y());
    sel_rect.Translate(img.xpos*zoom, img.ypos*zoom);
    gfx::DrawRectOutline(painter, sel_rect, gfx::CreateMaterialFromColor(gfx::Color::Green));
}
void DlgTextureAtlas::OnMousePress(QMouseEvent* mickey)
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
            if (mouse_posx < img.xpos || mouse_posx > img.xpos+img.width)
                continue;
            if (mouse_posy < img.ypos || mouse_posy > img.ypos+img.height)
                continue;

            break;
        }
        if (mSelectedIndex == mList.size())
            SetEnabled(mUI.btnAccept, false);
        else SetEnabled(mUI.btnAccept, true);
    }
}
void DlgTextureAtlas::OnMouseMove(QMouseEvent* mickey)
{
    mCurrentPoint = mickey->pos();

    if (!mTracking)
        return;

    mTrackingOffset += (mCurrentPoint - mStartPoint);
    mStartPoint = mCurrentPoint;
}
void DlgTextureAtlas::OnMouseRelease(QMouseEvent* mickey)
{
    mTracking = false;
}

void DlgTextureAtlas::OnMouseDoubleClick(QMouseEvent* mickey)
{
    OnMousePress(mickey);
    if (mSelectedIndex < mList.size())
        accept();
}

bool DlgTextureAtlas::OnKeyPress(QKeyEvent* event)
{
    return false;
}

} // namespace
