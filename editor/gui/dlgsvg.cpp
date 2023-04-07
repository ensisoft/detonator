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

#include "config.h"

#include "warnpush.h"
#  include <QFileDialog>
#  include <QMessageBox>
#  include <QImage>
#  include <QImageWriter>
#  include <QPainter>
#  include <QSvgRenderer>
#  include <QDomDocument>
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <algorithm>

#include "base/json.h"
#include "editor/app/format.h"
#include "editor/app/utility.h"
#include "editor/gui/dlgsvg.h"
#include "editor/gui/utility.h"

namespace gui
{

DlgSvgView::DlgSvgView(QWidget* parent) : QDialog(parent)
{
    mUI.setupUi(this);
    SetVisible(mUI.chkRasterAspect, false);
}

void DlgSvgView::on_btnClose_clicked()
{
    mClosed = true;
    close();
}

void DlgSvgView::on_btnSelectImage_clicked()
{
    const auto& file = QFileDialog::getOpenFileName(this,
        tr("Select Svg File"), "",
        tr("SVG (*.svg)"));
    if (file.isEmpty())
        return;

    if (!mUI.view->openFile(file))
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Critical);
        msg.setText("Failed to open the SVG file.");
        msg.exec();
        return;
    }
    QStringList items;
    items << "";

    QFile::FileError err_val;
    QString err_str;
    QString svg_xml = app::ReadTextFile(file, &err_val, &err_str);
    if (svg_xml.isEmpty())
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Critical);
        msg.setText("Failed to open the SVG file.");
        msg.exec();
        return;
    }
    QDomDocument document("svg");
    document.setContent(svg_xml);
    const QDomElement& root = document.firstChildElement();
    const QDomNodeList& ids = root.elementsByTagName("g");
    for (int i=0; i<ids.count(); ++i) {
        const QDomNode& node = ids.at(i);
        const QDomElement& elem = node.toElement();
        if (elem.isNull())
            continue;
        const QString& attr = elem.attribute("id");
        if (attr.isNull())
            continue;
        items << attr;
    }

    const auto& view_box = mUI.view->viewBox();
    const auto& svg_size = mUI.view->svgSize();
    SetValue(mUI.imageFile, file);
    SetValue(mUI.viewX, view_box.x());
    SetValue(mUI.viewY, view_box.y());
    SetValue(mUI.viewW, view_box.width());
    SetValue(mUI.viewH, view_box.height());
    SetValue(mUI.rasterWidth, svg_size.width());
    SetValue(mUI.rasterHeight, svg_size.height());
    SetEnabled(mUI.btnSaveAs, true);
    SetList(mUI.cmbElement, items);

    mElements = items;
    mViewAspect = (float)view_box.width() / (float)view_box.height();
}

void DlgSvgView::on_chkShowBackground_stateChanged(int)
{
    mUI.view->setViewBackground(GetValue(mUI.chkShowBackground));
}
void DlgSvgView::on_chkShowOutline_stateChanged(int)
{
    mUI.view->setViewOutline(GetValue(mUI.chkShowOutline));
}

void DlgSvgView::on_viewX_valueChanged(int)
{
    SetViewBox();
}
void DlgSvgView::on_viewY_valueChanged(int)
{
    SetViewBox();
}
void DlgSvgView::on_viewW_valueChanged(int)
{
    const int new_width = GetValue(mUI.viewW);

    if (GetValue(mUI.chkViewAspect))
    {
        SetValue(mUI.viewH, qRound(qreal(new_width) / mViewAspect));
    }
    SetViewBox();
}

void DlgSvgView::on_viewH_valueChanged(int)
{
    const int new_height = GetValue(mUI.viewH);

    if (GetValue(mUI.chkViewAspect))
    {
        SetValue(mUI.viewW, qRound(qreal(new_height) * mViewAspect));

    }
    SetViewBox();
}

void DlgSvgView::on_rasterWidth_valueChanged(int)
{
    const int new_width = GetValue(mUI.rasterWidth);

    if (GetValue(mUI.chkRasterAspect))
    {
        //SetValue(mUI.rasterHeight, qRound(qreal(new_width) / mRasterAspect));
    }
}
void DlgSvgView::on_rasterHeight_valueChanged(int)
{
    const int new_height = GetValue(mUI.rasterHeight);

    if (GetValue(mUI.chkRasterAspect))
    {
        //SetValue(mUI.rasterWidth, qRound(qreal(new_height) * mRasterAspect));
    }
}

void DlgSvgView::on_btnDoubleSize_clicked()
{
    const int width  = GetValue(mUI.viewW);
    const int height = GetValue(mUI.viewH);
    SetValue(mUI.viewW, width*2);
    SetValue(mUI.viewH, height*2);
    SetViewBox();
}

void DlgSvgView::on_btnHalveSize_clicked()
{
    const int width  = GetValue(mUI.viewW);
    const int height = GetValue(mUI.viewH);
    SetValue(mUI.viewW, width/2);
    SetValue(mUI.viewH, height/2);
    SetViewBox();
}

void DlgSvgView::on_btnSaveAs_clicked()
{
    QString filter;
    if (mUI.cmbFormat->currentText() == "JPG")
        filter = "Images (*.jpg)";
    else if (mUI.cmbFormat->currentText() == "PNG")
        filter = "Images (*.png)";
    else if (mUI.cmbFormat->currentText() == "BMP")
        filter = "Images (*.bmp)";

    QString filename = mLastSaveFile;
    filename = QFileDialog::getSaveFileName(this,
        tr("Select Save File"), filename, filter);
    if (filename.isEmpty())
        return;

    // SVG Canvas size used to rasterize the contents.
    const auto& svg_size = mUI.view->svgSize();
    // This is the logical SVG viewport size.
    const auto& box_size = mUI.view->viewBox();

    QSize raster_size;
    raster_size.setWidth(GetValue(mUI.rasterWidth));
    raster_size.setHeight(GetValue(mUI.rasterHeight));
    QImage image(raster_size, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    const auto& target = app::CenterRectOnTarget(raster_size, svg_size);

    QPainter painter;
    painter.begin(&image);
    mUI.view->renderer()->render(&painter, target);
    painter.end();

    QImageWriter writer;
    writer.setFormat(mUI.cmbFormat->currentText().toLatin1());
    writer.setQuality(mUI.quality->value());
    writer.setFileName(filename);
    if (!writer.write(image))
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("Failed to write the image.\n%1").arg(writer.errorString()));
        msg.exec();
        return;
    }

    if (GetValue(mUI.chkJson))
    {
        const QFileInfo info(filename);

        nlohmann::json json;
        base::JsonWrite(json, "json_version", 1);
        base::JsonWrite(json, "made_with_app", APP_TITLE);
        base::JsonWrite(json, "made_with_ver", APP_VERSION);
        base::JsonWrite(json, "image_file", app::ToUtf8(info.fileName()));
        base::JsonWrite(json, "image_width", raster_size.width());
        base::JsonWrite(json, "image_height", raster_size.height());

        const auto current = mUI.view->element();

        const float box_width  = box_size.width();
        const float box_height = box_size.height();
        //const float svg_width  = svg_size.width();
        //const float svg_height = svg_size.height();
        const float raster_width  = raster_size.width();
        const float raster_height = raster_size.height();

        // First element is an empty string and refers to the whole SVG file.
        for (int i=1; i<mElements.size(); ++i)
        {
            const auto& element = mElements[i];
            // todo: should we worry about the element transform here?
            const auto& bounds  = mUI.view->elementBounds(element);
            const float xpos = bounds.x() / box_width * target.width() + target.x();
            const float ypos = bounds.y() / box_height * target.height() + target.y();
            const float width = bounds.width() / box_width * target.width();
            const float height = bounds.height() / box_height * target.height();
            nlohmann::json tile_json;
            base::JsonWrite(tile_json, "xpos", (unsigned)xpos);
            base::JsonWrite(tile_json, "ypos", (unsigned)ypos);
            base::JsonWrite(tile_json, "width",  (unsigned)width);
            base::JsonWrite(tile_json, "height", (unsigned)height);
            base::JsonWrite(tile_json, "name", app::ToUtf8(element));
            json["images"].push_back(std::move(tile_json));
        }
        mUI.view->setElement(current);
        QString err_str;
        QFile::FileError err_val = QFile::FileError::NoError;
        if (!app::WriteTextFile(filename + ".json", json.dump(2), &err_val, &err_str))
        {
            QMessageBox msg(this);
            msg.setIcon(QMessageBox::Critical);
            msg.setStandardButtons(QMessageBox::Ok);
            msg.setText(tr("Failed to write the JSON description file.\n"
                           "File error '%1'").arg(err_str));
            msg.exec();
        }

    }
    mLastSaveFile = filename;
}

void DlgSvgView::on_cmbElement_currentIndexChanged(const QString&)
{
    mUI.view->setElement(GetValue(mUI.cmbElement));
    const auto& view_box = mUI.view->viewBox();
    SetValue(mUI.viewX, view_box.x());
    SetValue(mUI.viewY, view_box.y());
    SetValue(mUI.viewW, view_box.width());
    SetValue(mUI.viewH, view_box.height());
    mViewAspect = (float)view_box.width() / (float)view_box.height();
}

void DlgSvgView::SetViewBox()
{
    QRect box;
    box.setX(GetValue(mUI.viewX));
    box.setY(GetValue(mUI.viewY));
    box.setWidth(GetValue(mUI.viewW));
    box.setHeight(GetValue(mUI.viewH));
    mUI.view->setViewBox(box);
}

void DlgSvgView::SetRasterSize(const QSize& size)
{
    SetValue(mUI.rasterWidth, size.width());
    SetValue(mUI.rasterHeight, size.height());

}

} // namespace
