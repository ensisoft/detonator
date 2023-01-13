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
#include "editor/app/eventlog.h"
#include "editor/gui/dlgtextureatlas.h"
#include "editor/gui/utility.h"

namespace {
    bool ReadTexturePack(const QString& file, std::vector<gui::DlgTextureAtlas::Image>* out)
    {
        QFile io(file);
        if (!io.open(QIODevice::ReadOnly))
        {
            ERROR("Failed to open '%1' for reading. (%2)", file, io.error());
            return false;
        }
        const auto& buff = io.readAll();
        if (buff.isEmpty())
        {
            ERROR("JSON file '%1' contains no content.", file);
            return false;
        }
        const auto* beg  = buff.data();
        const auto* end  = buff.data() + buff.size();
        const auto& json = nlohmann::json::parse(beg, end, nullptr, false);
        if (json.is_discarded())
        {
            ERROR("JSON file '%1' could not be parsed.", file);
            return false;
        }
        if (!json.contains("images") || !json["images"].is_array())
        {
            ERROR("JSON file '%1' doesn't contain images array.");
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
                ERROR("Failed to read JSON image box data.");
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
}

void DlgTextureAtlas::on_btnSelectImage_clicked()
{
    const auto& file_name = QFileDialog::getOpenFileName(this,
        tr("Select Image File"), "",
        tr("Images (*.png *.jpg *.jpeg)"));
    if (file_name.isEmpty())
        return;

    const QPixmap pixmap(file_name);
    if (pixmap.isNull())
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("There was a problem reading the image.\n'%1'\n"
                       "Perhaps the image is not a valid image?").arg(file_name));
        msg.exec();
        return;
    }
    SetImage(mUI.lblImage, pixmap);
    SetValue(mUI.imageFile, file_name);
    mImage = pixmap;
}

void DlgTextureAtlas::on_btnSelectJson_clicked()
{
    const auto& file_name = QFileDialog::getOpenFileName(this,
        tr("Select Json File"), "",
        tr("Json (*.json)"));
    if (file_name.isEmpty())
        return;

    std::vector<Image> image_list;
    if (!ReadTexturePack(file_name, &image_list))
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("There was a problem reading the file.\n'%1'\n"
                       "Perhaps the image is not a valid JSON file?").arg(file_name));
        msg.exec();
        return;
    }
    ClearList(mUI.listWidget);
    for (const auto& img : image_list)
    {
        QListWidgetItem* item = new QListWidgetItem(mUI.listWidget);
        item->setText(img.name);
    }
    mList = std::move(image_list);
    SetValue(mUI.jsonFile, file_name);
}

void DlgTextureAtlas::on_btnClose_clicked()
{
    close();
}

void DlgTextureAtlas::on_listWidget_currentRowChanged(int index)
{
    if (index == -1)
    {
        SetValue(mUI.imgSize, QString(""));
        SetValue(mUI.imgPos, QString(""));
        mUI.lblImagePreview->setPixmap(QPixmap(":texture.png"));
        return;
    }

    const auto& item = mList[index];
    SetValue(mUI.imgSize, app::toString("w=%1, h=%2", item.width, item.height));
    SetValue(mUI.imgPos, app::toString("x=%1, y=%2", item.xpos, item.ypos));

    if (mImage.isNull())
        return;

    const auto& img = mImage.copy(item.xpos,
                                  item.ypos,
                                  item.width,
                                  item.height);
    SetImage(mUI.lblImagePreview, img);
}

} // namespace
