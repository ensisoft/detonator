// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

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

#include "editor/app/eventlog.h"
#include "graphics/image_packing.h"
#include "base/assert.h"
#include "dlgimgpack.h"

// todo: checking for dupes ?
// todo: output image format / bit depth ?
// todo: POT/NPOT flag ?
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
        tr("Select Image File(s)"), "", tr("Images (*.png *.jpg *.jpeg)"));
    for (int i=0; i<list.size(); ++i)
    {
        const QPixmap pix(list[i]);
        if (pix.isNull())
        {
            QMessageBox msg(this);
            msg.setStandardButtons(QMessageBox::Ok);
            msg.setIcon(QMessageBox::Critical);
            msg.setText(tr("There was a problem reading the image.\n"
                           "'%1'\n"
                           "Perhaps the image is not a valid image?").arg(list[i]));
            msg.exec();
            return;
        }

        QListWidgetItem* item = new QListWidgetItem(mUI.listWidget);
        item->setText(list[i]);
        mUI.listWidget->addItem(item);
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

    const QImage& image = mPackedImage.toImage();
    QImageWriter writer;
    writer.setFormat(mUI.cmbFormat->currentText().toLatin1());
    writer.setQuality(mUI.quality->value());
    writer.setFileName(filename);
    if (!writer.write(mPackedImage.toImage()))
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("Failed to write the image.\n"
                       "%1").arg(writer.errorString()));
        msg.exec();
    }

    DEBUG("Wrote packaged image in '%1'", filename);

    if (mJson.empty())
        return;

    const QFileInfo info(filename);
    mJson["image_file"] = app::ToUtf8(info.fileName());
    mJson["json_version"]  = 1;
    mJson["made_with_app"] = APP_TITLE;
    mJson["made_with_ver"] = APP_VERSION;

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
    }

    const auto& json = mJson.dump(2);
    file.write(&json[0], json.size());
    file.close();
}

void DlgImgPack::on_btnClose_clicked()
{
    close();
}

void DlgImgPack::on_listWidget_currentRowChanged(int index)
{
    if (index == -1)
    {
        mUI.grpImageProperties->setEnabled(false);
        mUI.lblImagePreview->setPixmap(QPixmap(":texture.png"));
        mUI.lblImageWidth->setText("");
        mUI.lblImageHeight->setText("");
        mUI.btnDeleteImage->setEnabled(false);
        return;
    }

    const QListWidgetItem* item = mUI.listWidget->item(index);
    const QString& file = item->text();

    QPixmap pix(file);
    if (pix.isNull())
        return;

    const auto& preview_height = mUI.lblImagePreview->height();

    mUI.btnDeleteImage->setEnabled(true);
    mUI.grpImageProperties->setEnabled(true);
    mUI.lblImageWidth->setText(QString::number(pix.width()));
    mUI.lblImageHeight->setText(QString::number(pix.height()));
    mUI.lblImageDepth->setText(QString::number(pix.depth()));
    mUI.lblImagePreview->setPixmap(pix.scaledToHeight(preview_height));
}

void DlgImgPack::on_colorChanged()
{
    repack();
}

void DlgImgPack::repack()
{
    std::vector<gfx::pack::NamedImage> images;

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
            msg.setText(tr("There was a problem reading the image.\n"
                            "'%1'\n"
                            "Perhaps the image is not a valid image?").arg(file));
            msg.exec();
            continue;
        }
        gfx::pack::NamedImage img;
        img.width  = pix.width();
        img.height = pix.height();
        img.name   = i; // index to the source list.
        images.push_back(img);
    }

    // nothing to pack ?
    if (images.empty())
    {
        mUI.grpPackagedImage->setEnabled(false);
        mUI.grpPackagedImage->setTitle(tr("Packed image"));
        mUI.lblPackagedImage->setPixmap(QPixmap());
        return;
    }

    const auto ret = gfx::pack::PackImages(images);
    DEBUG("Packaged image size %1x%2 pixels", ret.width, ret.height);
    if (ret.width == 0 || ret.height == 0)
        return;

    // after the pack images algorithm has completed the list of images
    // has been modifed to include a x,y coordinate for each input image
    // now we need to render the input images in the final image.
    //QPixmap buffer(ret.width, ret.height);
    mPackedImage = QPixmap(ret.width, ret.height);
    // fill with transparency
    //buffer.fill(QColor(0xff, 0x00, 0x00, 0x00));
    mPackedImage.fill(mUI.bgColor->color());

    QPainter painter(&mPackedImage);
    painter.setCompositionMode(QPainter::CompositionMode_Source); // copy src pixel as is

    mJson.clear();

    // keep in mind that order of the images in the vector is no
    // longer the same as the input order (obviously).
    for (size_t i=0; i<images.size(); ++i)
    {
        const auto& img     = images[i];
        const auto index    = img.name;
        const auto* item    = mUI.listWidget->item(index);
        const QString& file = item->text();
        const QRectF dst(img.xpos, img.ypos, img.width, img.height);
        const QRectF src(0, 0, img.width, img.height);
        const QPixmap pix(file);
        painter.drawPixmap(dst, pix, src);

        if (mUI.chkJson->isChecked())
        {
            const QFileInfo info(file);
            const nlohmann::json image_obj = {
                {"name",   app::ToUtf8(info.fileName()) },
                {"width",  img.width},
                {"height", img.height},
                {"xpos",   img.xpos},
                {"ypos",   img.ypos},
                {"index",  index}
            };
            mJson["images"].push_back(image_obj);
        }
    }

    const auto height = mUI.lblPackagedImage->height();
    mUI.lblPackagedImage->setPixmap(mPackedImage.scaledToHeight(height));
    mUI.grpPackagedImage->setTitle(tr("Packed image %1x%2").arg(ret.width).arg(ret.height));
    mUI.grpPackagedImage->setEnabled(true);
    mUI.btnSaveAs->setEnabled(true);
}

} // namespace

