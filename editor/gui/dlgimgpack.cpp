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

namespace {
    QString GetFileName(const QString& file)
    {
        const QFileInfo info(file);
        return info.baseName();
    }
}

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
    base::SafeErase(mSources, static_cast<size_t>(row));

    repack();
}
void DlgImgPack::on_btnBrowseImage_clicked()
{
    const auto& list = QFileDialog::getOpenFileNames(this,
        tr("Select Image File(s)"), "",
        tr("Images (*.png *.jpg *.jpeg);;Json (*.json);;Source list (*.list.txt)"));

    std::vector<SourceImage> sources;

    for (int i=0; i<list.size(); ++i)
    {
        const auto& filename = list[i];
        bool success = true;
        if (filename.endsWith(".list.txt", Qt::CaseInsensitive))
            success = ReadList(filename, &sources);
        else if (filename.endsWith(".json", Qt::CaseInsensitive))
            success = ReadJson(filename, &sources);
        else success = ReadImage(filename, &sources);

        if (!success)
        {
            QMessageBox msg(this);
            msg.setStandardButtons(QMessageBox::Ok);
            msg.setIcon(QMessageBox::Critical);
            msg.setText(tr("There was a problem reading the source file.\n'%1'\n"
                           "Please see the application log for details").arg(filename));
            msg.exec();
        }
    }

    ClearList(mUI.listWidget);
    for (const auto& source : sources)
        AddItem(mUI.listWidget, source.file);

    mSources = std::move(sources);

    SetCurrentRow(mUI.listWidget, 0);
    on_listWidget_currentRowChanged(0);

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

    QString filename = mLastSaveFile;
    if (filename.isEmpty())
        filename = QString("untitled") + "." + mUI.cmbFormat->currentText().toLower();
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
        return;
    }
    INFO("Wrote packaged image to '%1'.", filename);

    {
        // dump the source files into a simple .txt list file so that
        // it's possible to quickly recover the same list of source files.
        // this is useful when repacking (for example adding new images)
        // to the same packed image.
        QFile file;
        file.setFileName(filename + ".list.txt");
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            QTextStream stream(&file);
            stream.setCodec("UTF-8");
            for (const auto& src : mSources)
            {
                if (src.sub_image)
                    continue;
                stream << src.file;
                stream << "\n";
            }
            // don't write these for now since this is only a list of source images
            // stream << "premultiply_alpha=" << (bool)GetValue(mUI.chkPremulAlpha);
            // stream << "power_of_two=" << (bool)GetValue(mUI.chkPot);
            // stream << "padding=" << (int)GetValue(mUI.padding);
        }
        file.flush();
        file.close();
    }

    mLastSaveFile = filename;

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

    QString err_str;
    QFile::FileError err_val = QFile::FileError::NoError;
    if (!app::WriteTextFile(filename + ".json", mJson.dump(2), &err_val, &err_str))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr("Failed to write the JSON description file.\n"
                       "File error '%1'").arg(err_str));
        msg.exec();
        return;
    }
    INFO("Wrote packaged image json file to '%1'.", filename + ".json");
}

void DlgImgPack::on_btnClose_clicked()
{
    mClosed = true;
    hide();
}

void DlgImgPack::on_listWidget_currentRowChanged(int index)
{
    if (index == -1 || mSources.empty())
    {
        SetEnabled(mUI.grpImageProperties, false);
        SetEnabled(mUI.btnDeleteImage, false);
        SetValue(mUI.lblImageOffset, QString(""));
        SetValue(mUI.lblImageWidth, QString(""));
        SetValue(mUI.lblImageHeight, QString(""));
        SetValue(mUI.lblImageDepth, QString(""));
        SetImage(mUI.lblImagePreview, QPixmap(":texture.png"));
        SetValue(mUI.glyphIndex, QString(""));
        SetValue(mUI.imgName, QString(""));
        return;
    }

    const auto& source = mSources[index];
    SetValue(mUI.imgName, source.name);
    SetValue(mUI.glyphIndex, source.glyph);
    SetValue(mUI.lblImageOffset, app::toString("%1,%2", source.xpos, source.ypos));
    SetEnabled(mUI.btnDeleteImage, true);
    SetEnabled(mUI.grpImageProperties, true);

    const QPixmap pix(source.file);
    if (pix.isNull())
        return;

    if (source.sub_image)
    {
        SetValue(mUI.lblImageWidth, source.width);
        SetValue(mUI.lblImageHeight, source.height);
        SetValue(mUI.lblImageDepth, pix.depth());
        SetImage(mUI.lblImagePreview, pix.copy(source.xpos, source.ypos, source.width, source.height));
    }
    else
    {
        SetValue(mUI.lblImageWidth, pix.width());
        SetValue(mUI.lblImageHeight, pix.height());
        SetValue(mUI.lblImageDepth, pix.depth());
        SetImage(mUI.lblImagePreview, pix);
    }
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
    const auto row = GetCurrentRow(mUI.listWidget);
    if (row == -1)
        return;

    base::SafeIndex(mSources, static_cast<size_t>(row)).glyph = text;
}

void DlgImgPack::on_imgName_textChanged(const QString& text)
{
    const auto row = GetCurrentRow(mUI.listWidget);
    if (row == -1)
        return;

    base::SafeIndex(mSources, static_cast<size_t>(row)).name = text;
}

// static
bool DlgImgPack::ReadJson(const QString& file, std::vector<SourceImage>* sources)
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

    /*
    std::string image_file;
    if (!base::JsonReadSafe(json, "image_file", &image_file))
    {
        ERROR("Missing image_source property. [file='%1']", file);
        return false;
    }
     */
    // the json should have something like "image.png"
    // and we're going to assume that the image is then in the
    // same folder as the JSON file.
    const QString& image_source_file = app::FindJsonImageFile(file);
    if (image_source_file.isEmpty())
    {
        ERROR("Failed to find image file for JSON. [file='%1']", file);
        return false;
    }


    if (json.contains("images") && json["images"].is_array())
    {
        for (const auto& img_json: json["images"].items())
        {
            const auto& obj = img_json.value();

            DlgImgPack::SourceImage img;
            img.sub_image = true;
            img.file = image_source_file;
            if (!base::JsonReadSafe(obj, "width", &img.width))
                WARN("Image is missing 'width' attribute. [file='%1']", file);
            if (!base::JsonReadSafe(obj, "height", &img.height))
                WARN("Image is missing 'height' attribute. [file='%1']", file);
            if (!base::JsonReadSafe(obj, "xpos", &img.xpos))
                WARN("Image is missing 'xpos' attribute. [file='%1']", file);
            if (!base::JsonReadSafe(obj, "ypos", &img.ypos))
                WARN("Image is missing 'ypos' attribute. [file='%1']", file);

            // optional
            std::string name;
            if (base::JsonReadSafe(obj, "name", &name))
                img.name = app::FromUtf8(name);

            std::string character;
            if (base::JsonReadSafe(obj, "char", &character))
                img.glyph = app::FromUtf8(character);

            unsigned index = 0;
            if (base::JsonReadSafe(obj, "index", &index))
                img.index = index;

            sources->push_back(std::move(img));
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

        // must have this data in order to split the source image into tiles.
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

                SourceImage img;
                img.sub_image = true;
                img.width  = tile_width;
                img.height = tile_height;
                img.xpos   = tile_xpos;
                img.ypos   = tile_ypos;
                sources->push_back(std::move(img));
            }
        }
    }

    // finally, sort based on the image index.
    std::sort(std::begin(*sources), std::end(*sources), [&](const auto& a, const auto& b) {
        return a.index < b.index;
    });

    INFO("Successfully parsed '%1'. %2 images found.", file, sources->size());
    return true;
}

// static
bool DlgImgPack::ReadList(const QString& filename, std::vector<SourceImage>* sources)
{
    QFile file;
    file.setFileName(filename);
    if (!file.open(QIODevice::ReadOnly))
    {
        ERROR("Failed to open file. [file='%1', error='%2']", filename, file.errorString());
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    while (!stream.atEnd())
    {
        QString line = stream.readLine();
        if (line.isEmpty())
            continue;
        QPixmap pix(line);
        if (pix.isNull())
        {
            WARN("Could not open image file. [file='%1']", line);
            continue;
        }

        SourceImage source;
        source.file = line;
        source.name = GetFileName(line);
        sources->push_back(source);
    }
    return true;
}

// static
bool DlgImgPack::ReadImage(const QString& filename, std::vector<SourceImage>* sources)
{
    const QPixmap pix(filename);
    if (pix.isNull())
    {
        WARN("Could not open image file. [file='%1']", filename);
        return false;
    }
    SourceImage source;
    source.file = filename;
    source.name = GetFileName(filename);
    sources->push_back(source);
    return true;
}

void DlgImgPack::repack()
{
    std::vector<app::PackingRectangle> images;

    const unsigned padding  = GetValue(mUI.padding);
    const bool power_of_two = GetValue(mUI.chkPot);

    QPixmap pixmap;
    QString pixmap_file;

    // take the files and build a list of "named images" for the algorithm
    // to work on.
    for (size_t index=0; index<mSources.size(); ++index)
    {
        const auto& src = mSources[index];
        // only load new QPixmap if the source filepath has changed.
        if (src.file != pixmap_file)
        {
            QPixmap pix(src.file);
            if (pix.isNull())
            {
                QMessageBox msg(this);
                msg.setStandardButtons(QMessageBox::Ok);
                msg.setIcon(QMessageBox::Critical);
                msg.setText(tr("There was a problem reading the image.\n'%1'\n"
                               "Perhaps the image is not a valid image?").arg(src.file));
                msg.exec();
                continue;
            }
            pixmap = pix;
            pixmap_file = src.file;
        }
        const auto width  = src.sub_image ? src.width  : pixmap.width();
        const auto height = src.sub_image ? src.height : pixmap.height();

        app::PackingRectangle img;
        img.width  = width + 2 * padding;
        img.height = height + 2 * padding;
        img.index  = index; // index to the source list.
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
        const auto& img  = images[i];
        const auto& src  = mSources[img.index];

        // this is where the source image ends up in the destination
        const auto xpos   = img.xpos+padding;
        const auto ypos   = img.ypos+padding;
        const auto width  = img.width-2*padding;
        const auto height = img.height-2*padding;

        const QRectF dst_rect(xpos, ypos, width, height);
        const QRectF src_rect(src.xpos, src.ypos, src.width, src.height);
        // only load the pixmap if the pixmap source file has changed.
        if (pixmap_file != src.file)
        {
            pixmap = QPixmap(src.file);
            pixmap_file = src.file;
        }
        painter.drawPixmap(dst_rect, pixmap, src_rect);

        if (GetValue(mUI.chkJson))
        {
            nlohmann::json json;
            base::JsonWrite(json, "width",  width);
            base::JsonWrite(json, "height", height);
            base::JsonWrite(json, "xpos",   xpos);
            base::JsonWrite(json, "ypos",   ypos);
            base::JsonWrite(json, "index",  (unsigned)img.index);
            if (!src.glyph.isEmpty())
                base::JsonWrite(json, "char",  app::ToUtf8(src.glyph));
            if (!src.name.isEmpty())
                base::JsonWrite(json, "name", app::ToUtf8(src.name));

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

