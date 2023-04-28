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
#  include <QImageWriter>
#  include <QEventLoop>
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <algorithm>

#include "base/json.h"
#include "graphics/painter.h"
#include "graphics/drawing.h"
#include "editor/app/utility.h"
#include "editor/app/eventlog.h"
#include "editor/app/workspace.h"
#include "editor/gui/dlgimgview.h"
#include "editor/gui/utility.h"
#include "editor/gui/drawing.h"

namespace {
    bool ReadTexturePack(const QString& file, std::vector<gui::DlgImgView::Image>* out)
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

                gui::DlgImgView::Image img;
                std::string name;
                std::string character;
                std::string tag;
                unsigned width = 0;
                unsigned height = 0;
                unsigned index = 0;

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
                if (base::JsonReadSafe(obj, "tag", &tag))
                    img.tag = app::FromUtf8(tag);
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

                    gui::DlgImgView::Image img;
                    img.width  = tile_width;
                    img.height = tile_height;
                    img.xpos   = tile_xpos;
                    img.ypos   = tile_ypos;
                    out->push_back(std::move(img));
                }
            }
        }

        // finally, sort based on the image index.
        std::stable_sort(std::begin(*out), std::end(*out), [&](const auto& a, const auto& b) {
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
    SetVisible(mUI.progressBar, false);
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
    mClass = std::make_shared<gfx::TextureMap2DClass>(gfx::MaterialClass::Type::Texture);
    mClass->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
    mClass->SetTexture(std::move(source));
    mClass->SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
    mClass->SetGamma(1.0f);
    mMaterial = gfx::CreateMaterialInstance(mClass);
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
        msg.setText(tr("There was a problem reading the file.\n"
                       "Perhaps the image is not a valid image descriptor JSON file?\n"
                       "Please see the log for details."));
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
        SetTableItem(mUI.listWidget, row, 2, img.width);
        SetTableItem(mUI.listWidget, row, 3, img.height);
        SetTableItem(mUI.listWidget, row, 4, img.xpos);
        SetTableItem(mUI.listWidget, row, 5, img.ypos);
        if (img.index.has_value())
            SetTableItem(mUI.listWidget, row, 6, img.index.value());
        else SetTableItem(mUI.listWidget, row, 6, QString(""));
    }

    mList = std::move(image_list);
    SetValue(mUI.jsonFile, file);
}
void DlgImgView::SetDialogMode(app::Workspace* workspace)
{
    SetVisible(mUI.btnClose, false);
    SetVisible(mUI.btnAccept, true);
    SetVisible(mUI.btnCancel, true);
    SetEnabled(mUI.btnAccept, false);
    QSignalBlocker s(mUI.tabWidget);
    mUI.tabWidget->removeTab(2); // cutter tab


    mDialogMode = true;
    mWorkspace  = workspace;
    if (mWorkspace)
    {
        QByteArray geometry;
        if (GetUserProperty(*mWorkspace, "dlg_img_view_geometry", &geometry))
            restoreGeometry(geometry);
        GetUserProperty(*mWorkspace, "dlg_img_view_widget", mUI.widget);
    }

    mUI.listWidget->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
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
    for (auto& img : mList)
    {
        if (img.selected)
            return img.name;
    }
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
        LoadImage(img);
}

void DlgImgView::on_btnClose_clicked()
{
    mClosed =  true;
    close();
}

void DlgImgView::on_btnAccept_clicked()
{
    if (!MustHaveInput(mUI.imageFile))
        return;

    if (!MustHaveInput(mUI.jsonFile))
        return;

    bool have_selection = false;
    for (auto& img : mList)
    {
        if (img.selected)
        {
            have_selection = true;
            break;
        }
    }
    if (!have_selection)
        return;

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

void DlgImgView::on_btnCutImages_clicked()
{
    if (!MustHaveInput(mUI.imageFile))
        return;
    if (!MustHaveInput(mUI.nameTemplate))
        return;
    if (!MustHaveInput(mUI.outputFolder))
        return;

    QString out_path = GetValue(mUI.outputFolder);
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

    SetValue(mUI.progressBar, 0);
    SetRange(mUI.progressBar, 0, mList.size());
    AutoHider hider(mUI.progressBar);
    QEventLoop footgun;

    const unsigned left_padding  = GetValue(mUI.leftPadding);
    const unsigned right_padding = GetValue(mUI.rightPadding);
    const unsigned top_padding   = GetValue(mUI.topPadding);
    const unsigned bottom_padding = GetValue(mUI.bottomPadding);
    const bool power_of_two = GetValue(mUI.chkPOT);
    const bool overwrite    = GetValue(mUI.chkOverwrite);
    const QString selection = GetValue(mUI.cmbSelection);
    const bool all_images   = selection == QString("All images");
    unsigned counter = 0;
    for (unsigned i=0; i<mList.size(); ++i)
    {
        if (Increment(mUI.progressBar))
            footgun.processEvents();

        const auto& img = mList[i];
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

        QString out_name = GetValue(mUI.nameTemplate);
        out_name.replace("%c", QString::number(counter++));
        out_name.replace("%i", QString::number(img.index.value_or(0)));
        out_name.replace("%w", QString::number(img.width));
        out_name.replace("%h", QString::number(img.height));
        out_name.replace("%x", QString::number(img.xpos));
        out_name.replace("%h", QString::number(img.height));
        out_name.replace("%n", img_name);
        out_name.replace("%t", img.tag);
        if (out_name.isEmpty())
            continue;
        out_name.append(".");
        out_name.append(mUI.cmbImageFormat->currentText().toLower());
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
        writer.setQuality(GetValue(mUI.imageQuality));
        writer.setFormat(mUI.cmbImageFormat->currentText().toLocal8Bit());
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

void DlgImgView::on_btnSelectOut_clicked()
{
    const auto& dir = QFileDialog::getExistingDirectory(this,
        tr("Select Output Directory"), GetValue(mUI.outputFolder));
    if (dir.isEmpty())
        return;
    SetValue(mUI.outputFolder, dir);
}

void DlgImgView::on_cmbColorSpace_currentIndexChanged(int)
{
    if (!mClass)
        return;
    auto* source = mClass->GetTextureMap(0)->GetTextureSource(0);
    auto* file_source = dynamic_cast<gfx::detail::TextureFileSource*>(source);
    file_source->SetColorSpace(GetValue(mUI.cmbColorSpace));
}

void DlgImgView::on_widgetColor_colorChanged(QColor color)
{
    mUI.widget->SetClearColor(ToGfx(color));
}

void DlgImgView::on_listWidget_itemSelectionChanged()
{
    for (auto& img : mList)
        img.selected = false;

    const auto& selection = GetSelection(mUI.listWidget);
    for (int i=0; i<selection.size(); ++i)
    {
        const auto item = selection[i];
        const auto index = static_cast<size_t>(item.row());
        base::SafeIndex(mList, index).selected = true;
    }

    SetEnabled(mUI.btnAccept, false);

    if (mDialogMode)
    {
        for (const auto& img : mList)
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
    if (mUI.tabWidget->currentIndex() == 1)
    {
        for (size_t i = 0; i < mList.size(); ++i)
        {
            SelectTableRow(mUI.listWidget, i, mList[i].selected);
        }
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

    if (mList.empty())
        return;

    static auto selection_material_class = gfx::CreateMaterialClassFromImage("app://textures/accept_icon.png");
    selection_material_class.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
    selection_material_class.SetBaseColor(gfx::Color4f(1.0f, 1.0f, 1.0f, 1.0f));
    static auto selection_material = gfx::MaterialClassInst(selection_material_class);

    for (size_t index=0; index<mList.size(); ++index)
    {
        const auto& img = mList[index];
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

void DlgImgView::OnMousePress(QMouseEvent* mickey)
{
    mStartPoint = mickey->pos();

    if (mickey->button() == Qt::RightButton)
        mMode = Mode::Tracking;
    else if (mickey->button() == Qt::LeftButton)
    {
        mMode = Mode::Selecting;
        ToggleMouseSelection();
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

    mIndexUnderMouse = mList.size();
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

    for (mIndexUnderMouse=0; mIndexUnderMouse<mList.size(); ++mIndexUnderMouse)
    {
        const auto& img = mList[mIndexUnderMouse];
        if (mouse_posx < img.xpos || mouse_posx > img.xpos+img.width)
            continue;
        if (mouse_posy < img.ypos || mouse_posy > img.ypos+img.height)
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
    for (auto& img : mList)
    {
        if (img.selected)
        {
            accept();
            return;
        }
    }
}

bool DlgImgView::OnKeyPress(QKeyEvent* event)
{
    return false;
}

void DlgImgView::ToggleMouseSelection()
{
    if (mIndexUnderMouse >= mList.size())
        return;

    if (base::Contains(mTilesTouched, mIndexUnderMouse))
        return;

    if (mDialogMode)
    {
        for (auto& img : mList)
            img.selected = false;

        SetEnabled(mUI.btnAccept, false);
    }

    mList[mIndexUnderMouse].selected = !mList[mIndexUnderMouse].selected;
    mTilesTouched.insert(mIndexUnderMouse);

    if (mDialogMode)
    {
        for (const auto& img : mList)
        {
            if (img.selected)
            {
                SetEnabled(mUI.btnAccept, true);
                break;
            }
        }
    }
}

} // namespace
