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
#  include <QFileInfo>
#  include <QFileDialog>
#  include <QFile>
#  include <QPixmap>
#  include <QPainter>
#  include <QImage>
#  include <QImageWriter>
#  include <QMessageBox>
#  include <QTextStream>
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <vector>

#include "base/assert.h"
#include "base/json.h"
#include "base/utility.h"
#include "editor/app/eventlog.h"
#include "editor/app/packing.h"
#include "editor/gui/dlgimgpack.h"
#include "editor/gui/utility.h"

// todo: checking for dupes ?
// todo: output image format / bit depth ?
// todo: packing algorithm params, i.e. keep square or not (wastes more space)

namespace gui
{

DlgImgPack::DlgImgPack(QWidget* parent) : QDialog(parent)
{
    mUI.setupUi(this);

    connect(mUI.bgColor, &color_widgets::ColorSelector::colorChanged,
            this, &DlgImgPack::on_colorChanged);
}

void DlgImgPack::on_btnDeleteImage_clicked()
{
    const auto row = mUI.listWidget->currentRow();
    if (row == -1)
        return;
    delete mUI.listWidget->takeItem(row);
    repack();
}
void DlgImgPack::on_btnBrowseImage_clicked()
{
    const auto& list = QFileDialog::getOpenFileNames(this,
        tr("Select Image File(s)"), "",
        tr("Images (*.png *.jpg *.jpeg)"));
    for (int i=0; i<list.size(); ++i)
    {
        const QPixmap pix(list[i]);
        if (pix.isNull())
        {
            QMessageBox msg(this);
            msg.setStandardButtons(QMessageBox::Ok);
            msg.setIcon(QMessageBox::Critical);
            msg.setText(tr("There was a problem reading the image.\n'%1'\n"
                           "Perhaps the image is not a valid image?").arg(list[i]));
            msg.exec();
            return;
        }
        AddItem(mUI.listWidget, list[i]);
    }

    const auto index = mUI.listWidget->currentRow();
    mUI.listWidget->setCurrentRow(index == -1 ? 0 : index);

    repack();

}
void DlgImgPack::on_btnSaveAs_clicked()
{
    QString filter;
    if (mUI.cmbFormat->currentText() == "JPG")
        filter = "Images (*.jpg)";
    else if (mUI.cmbFormat->currentText() == "PNG")
        filter = "Images (*.png)";
    else if (mUI.cmbFormat->currentText() == "BMP")
        filter = "Images (*.bmp)";

    // where to save it ?

    QString filename = QString("untitled") + "." + mUI.cmbFormat->currentText().toLower();
    filename = QFileDialog::getSaveFileName(this,
        tr("Select Save File"), filename, filter);
    if (filename.isEmpty())
        return;

    QImageWriter writer;
    writer.setFormat(mUI.cmbFormat->currentText().toLatin1());
    writer.setQuality(mUI.quality->value());
    writer.setFileName(filename);
    if (!writer.write(mPackedImage))
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("Failed to write the image.\n%1").arg(writer.errorString()));
        msg.exec();
    }
    DEBUG("Wrote packaged image to '%1'.", filename);

    if (!GetValue(mUI.chkJson))
        return;

    const QFileInfo info(filename);
    base::JsonWrite(mJson, "image_file", app::ToUtf8(info.fileName()));
    base::JsonWrite(mJson, "json_version", 1);
    base::JsonWrite(mJson, "made_with_app", APP_TITLE);
    base::JsonWrite(mJson, "made_with_ver", APP_VERSION);

    // premultiplied alpha is here used only as a rendered hint that the
    // image should be converted into premultiplied format before being
    // used to render with. Why is this only a flag? The PNG specification
    // specifically says that PNG files are assumed to be in straight, i.e.
    // non-premultiplied format. Qt seems to follow this so that if the QImage
    // with _Premultiplied (such as Format_ARGB32_Premultiplied) as written
    // out to a file (using QImageWriter) the image is converted back to
    // straight alpha. Of course, it'd be possible to simply take those
    // image pixels and do a conversion manually (without changing the image
    // format to a Premultiplied) and then write it out but this would still
    // go against the PNG spec. In order to avoid such confusion we're going
    // to stick to straight alpha here and only provide the flag as a rendering
    // hint only.
    base::JsonWrite(mJson, "premultiply_alpha", (bool)GetValue(mUI.chkPremulAlpha));

    // todo: should we ask for the JSON filename too?
    // seems a bit excessive, but this could overwrite a file unexpectedly

    QFile file;
    file.setFileName(filename + ".json");
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

    const auto& json = mJson.dump(2);
    file.write(&json[0], json.size());
    file.close();
    DEBUG("Wrote package image json file to '%1'.", filename + ".json");
}

void DlgImgPack::on_btnClose_clicked()
{
    close();
}

void DlgImgPack::on_listWidget_currentRowChanged(int index)
{
    if (index == -1)
    {
        SetEnabled(mUI.grpImageProperties, false);
        SetEnabled(mUI.btnDeleteImage, false);
        SetValue(mUI.lblImageWidth, QString(""));
        SetValue(mUI.lblImageHeight, QString(""));
        SetValue(mUI.lblImageDepth, QString(""));
        SetImage(mUI.lblImagePreview, QPixmap(":texture.png"));
        SetValue(mUI.glyphIndex, QString(""));
        return;
    }

    const QListWidgetItem* item = mUI.listWidget->item(index);
    const QString& file  = item->text();
    const QString& glyph = item->data(Qt::UserRole).toString();
    SetValue(mUI.glyphIndex, glyph);
    SetEnabled(mUI.btnDeleteImage, true);
    SetEnabled(mUI.grpImageProperties, true);

    QPixmap pix(file);
    if (pix.isNull())
        return;

    SetValue(mUI.lblImageWidth, pix.width());
    SetValue(mUI.lblImageHeight, pix.height());
    SetValue(mUI.lblImageDepth, pix.depth());
    SetImage(mUI.lblImagePreview, pix);
}

void DlgImgPack::on_colorChanged()
{
    repack();
}

void DlgImgPack::on_padding_valueChanged(int)
{
    repack();
}

void DlgImgPack::on_chkPot_stateChanged(int)
{
    repack();
}

void DlgImgPack::on_glyphIndex_textChanged(const QString& text)
{
    if (auto* item = mUI.listWidget->currentItem())
    {
        item->setData(Qt::UserRole, text);
    }
}

void DlgImgPack::repack()
{
    std::vector<app::PackingRectangle> images;

    const unsigned padding  = GetValue(mUI.padding);
    const bool power_of_two = GetValue(mUI.chkPot);

    // take the files and build a list of "named images" for the algorithm
    // to work on.
    for (int i=0; i<mUI.listWidget->count(); ++i)
    {
        const auto* item = mUI.listWidget->item(i);
        const auto& file = item->text();

        QPixmap pix(file);
        if (pix.isNull())
        {
            QMessageBox msg(this);
            msg.setStandardButtons(QMessageBox::Ok);
            msg.setIcon(QMessageBox::Critical);
            msg.setText(tr("There was a problem reading the image.\n'%1'\n"
                            "Perhaps the image is not a valid image?").arg(file));
            msg.exec();
            continue;
        }
        app::PackingRectangle img;
        img.width  = pix.width() + 2 * padding;
        img.height = pix.height() + 2 * padding;
        img.index  = i; // index to the source list.
        images.push_back(img);
    }

    // nothing to pack ?
    if (images.empty())
    {
        SetEnabled(mUI.grpPackagedImage, false);
        SetValue(mUI.grpPackagedImage, "Packed image");
        SetImage(mUI.lblPackagedImage, QPixmap());
        return;
    }

    const auto ret = app::PackRectangles(images);
    DEBUG("Packaged image size %1x%2 pixels.", ret.width, ret.height);
    if (ret.width == 0 || ret.height == 0)
        return;

    unsigned dst_img_width = ret.width;
    unsigned dst_img_height = ret.height;
    if (power_of_two)
    {
        dst_img_width  = base::NextPOT(ret.width);
        dst_img_height = base::NextPOT(ret.height);
    }
    DEBUG("Destination image size %1x%2 pixels.", dst_img_width, dst_img_height);

    // after the pack images algorithm has completed the list of images
    // has been modified to include a x,y coordinate for each input image
    // now we need to render the input images in the final image.
    QImage buffer(dst_img_width, dst_img_height, QImage::Format_ARGB32);

    buffer.fill(GetValue(mUI.bgColor));

    QPainter painter(&buffer);
    painter.setCompositionMode(QPainter::CompositionMode_Source); // copy src pixel as is

    mJson.clear();
    base::JsonWrite(mJson, "image_width", dst_img_width);
    base::JsonWrite(mJson, "image_height", dst_img_height);
    base::JsonWrite(mJson, "power_of_two", power_of_two);
    base::JsonWrite(mJson, "padding", padding);

    // keep in mind that order of the images in the vector is no
    // longer the same as the input order (obviously).
    for (size_t i=0; i<images.size(); ++i)
    {
        const auto& img     = images[i];
        const auto index    = img.index;
        const auto* item    = mUI.listWidget->item(index);
        const QString& image_file = item->text();
        const QString& glyph_key  = item->data(Qt::UserRole).toString();

        const auto xpos   = img.xpos+padding;
        const auto ypos   = img.ypos+padding;
        const auto width  = img.width-2*padding;
        const auto height = img.height-2*padding;

        const QRectF dst(xpos, ypos, width, height);
        const QRectF src(0, 0, width, height);
        const QPixmap pix(image_file);
        painter.drawPixmap(dst, pix, src);

        if (GetValue(mUI.chkJson))
        {
            const QFileInfo info(image_file);
            nlohmann::json json;
            base::JsonWrite(json, "name",   app::ToUtf8(info.fileName()));
            base::JsonWrite(json, "width",  width);
            base::JsonWrite(json, "height", height);
            base::JsonWrite(json, "xpos",   xpos);
            base::JsonWrite(json, "ypos",   ypos);
            base::JsonWrite(json, "index", (unsigned)index);
            if (!glyph_key.isEmpty())
                base::JsonWrite(json, "char",  app::ToUtf8(glyph_key));
            mJson["images"].push_back(std::move(json));
        }
    }

    mPackedImage = buffer;

    SetImage(mUI.lblPackagedImage, QPixmap::fromImage(mPackedImage));
    SetValue(mUI.grpPackagedImage, app::toString("Packed image %1x%2", dst_img_width, dst_img_width));
    SetEnabled(mUI.grpPackagedImage, true);
    SetEnabled(mUI.btnSaveAs, true);
}

} // namespace

