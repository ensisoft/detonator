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

#define LOGTAG "material"

#include "config.h"

#include "warnpush.h"
#  include <QFileDialog>
#  include <QFileInfo>
#  include <QPixmap>
#include "warnpop.h"

#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/transform.h"
#include "graphics/drawing.h"
#include "graphics/types.h"

#include "editor/app/eventlog.h"
#include "editor/app/utility.h"
#include "editor/gui/settings.h"
#include "editor/gui/utility.h"
#include "materialwidget.h"

namespace gui
{

MaterialWidget::MaterialWidget()
{
    DEBUG("Create MaterialWidget");

    mUI.setupUi(this);
    mUI.widget->setFramerate(60);
    mUI.widget->onPaintScene = std::bind(&MaterialWidget::paintScene,
        this, std::placeholders::_1, std::placeholders::_2);
    mUI.actionPause->setEnabled(false);
    mUI.actionPlay->setEnabled(true);
    mUI.actionStop->setEnabled(false);

    PopulateFromEnum<gfx::Material::MinTextureFilter>(mUI.minFilter);
    PopulateFromEnum<gfx::Material::MagTextureFilter>(mUI.magFilter);
    PopulateFromEnum<gfx::Material::TextureWrapping>(mUI.wrapX);
    PopulateFromEnum<gfx::Material::TextureWrapping>(mUI.wrapY);
    PopulateFromEnum<gfx::Material::SurfaceType>(mUI.surfaceType);
}

MaterialWidget::~MaterialWidget()
{
    DEBUG("Destroy MaterialWidget");

    mUI.widget->dispose();
}

void MaterialWidget::addActions(QToolBar& bar)
{
    bar.addAction(mUI.actionPlay);
    bar.addAction(mUI.actionPause);
    bar.addSeparator();
    bar.addAction(mUI.actionStop);
}
void MaterialWidget::addActions(QMenu& menu)
{
    menu.addAction(mUI.actionPlay);
    menu.addAction(mUI.actionPause);
    menu.addSeparator();
    menu.addAction(mUI.actionStop);
}

bool MaterialWidget::loadState(const Settings& settings)
{
    return true;
}

bool MaterialWidget::saveState(Settings& settings) const
{
    return true;
}

void MaterialWidget::zoomIn()
{
    const auto value = mUI.zoom->value();
    mUI.zoom->setValue(value + 0.1);
}

void MaterialWidget::zoomOut()
{
    const auto value = mUI.zoom->value();
    if (value > 0.1)
        mUI.zoom->setValue(value - 0.1);
}

void MaterialWidget::reloadShaders()
{
    mUI.widget->reloadShaders();
}

void MaterialWidget::on_actionPlay_triggered()
{
    mState = PlayState::Playing;
    mUI.actionPlay->setEnabled(false);
    mUI.actionPause->setEnabled(true);
    mUI.actionStop->setEnabled(true);
}

void MaterialWidget::on_actionPause_triggered()
{
    mState = PlayState::Paused;
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(true);
}

void MaterialWidget::on_actionStop_triggered()
{
    mState = PlayState::Stopped;
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(false);
    mTime = 0.0f;
}

void MaterialWidget::on_textureAdd_clicked()
{
    const auto& list = QFileDialog::getOpenFileNames(this,
        tr("Select Image File(s)"), "", tr("Images (*.png *.jpg *.jpg)"));
    if (list.isEmpty())
        return;
    for (const auto& file : list)
    {
        QFileInfo info(file);
        QListWidgetItem* item = new QListWidgetItem(mUI.textures);
        // generate random key for the file.
        const QString& key = app::randomString();

        item->setText(info.fileName());
        item->setData(Qt::UserRole, key);
        mUI.textures->addItem(item);

        // store the texture details in a map by the unique key
        mTextures[key].file = file;
        mTextures[key].rectw = 1.0f;
        mTextures[key].recth = 1.0f;
    }
    const auto count = mUI.textures->count();
    const auto index = mUI.textures->currentRow();
    mUI.texProps->setEnabled(true);
    mUI.textureDel->setEnabled(true);
    if (index == -1)
        mUI.textures->setCurrentRow(0);
}

void MaterialWidget::on_textureDel_clicked()
{
    QList<QListWidgetItem*> items = mUI.textures->selectedItems();

    for (int i=0; i<items.count(); ++i)
    {
        const QString& key = items[i]->data(Qt::UserRole).toString();
        mTextures.remove(key);
    }

    qDeleteAll(items);

    if (mUI.textures->count() == 0)
        mUI.textureDel->setEnabled(false);

    mUI.imgRect->setEnabled(false);
    mUI.rectX->setValue(0.0f);
    mUI.rectY->setValue(0.0f);
    mUI.rectW->setValue(1.0f);
    mUI.rectH->setValue(1.0f);
}

void MaterialWidget::on_textures_currentRowChanged(int index)
{
    mUI.imgRect->setEnabled(false);
    mUI.rectX->setValue(0.0f);
    mUI.rectY->setValue(0.0f);
    mUI.rectW->setValue(1.0f);
    mUI.rectH->setValue(1.0f);

    mUI.texProps->setEnabled(false);
    mUI.textureWidth->setText("");
    mUI.textureHeight->setText("");
    mUI.textureDepth->setText("");
    mUI.textureFile->setText("");

    const auto& key = getCurrentTextureKey();
    if (key.isEmpty())
        return;

    const auto& data = mTextures[key];

    mUI.textureFile->setText(data.file);
    mUI.textureFile->setCursorPosition(0);

    const QPixmap img(data.file);
    if (img.isNull())
    {
        ERROR("Could not load image '%1'", data.file);
        mUI.textureWidth->setText("Error");
        mUI.textureHeight->setText("Error");
        mUI.textureDepth->setText("Error");
        mUI.texturePreview->setEnabled(false);
        return;
    }

    const auto w = img.width();
    const auto h = img.height();
    const auto d = img.depth();
    mUI.textureWidth->setText(QString::number(w));
    mUI.textureHeight->setText(QString::number(h));
    mUI.textureDepth->setText(QString::number(d));
    mUI.texturePreview->setEnabled(true);
    mUI.texturePreview->setPixmap(img.scaledToHeight(128));

    mUI.imgRect->setEnabled(data.rect_enabled);
    mUI.rectX->setValue(data.rectx);
    mUI.rectY->setValue(data.recty);
    mUI.rectW->setValue(data.rectw);
    mUI.rectH->setValue(data.recth);

    mUI.texProps->setEnabled(true);
    mUI.imgRect->setEnabled(true);

}

void MaterialWidget::on_imgRect_clicked(bool checked)
{
    mTextures[getCurrentTextureKey()].rect_enabled = checked;
}

void MaterialWidget::on_rectX_valueChanged(double value)
{
    mTextures[getCurrentTextureKey()].rectx = value;
}
void MaterialWidget::on_rectY_valueChanged(double value)
{
    mTextures[getCurrentTextureKey()].recty = value;
}
void MaterialWidget::on_rectW_valueChanged(double value)
{
    mTextures[getCurrentTextureKey()].rectw = value;
}
void MaterialWidget::on_rectH_valueChanged(double value)
{
    mTextures[getCurrentTextureKey()].recth = value;
}

void MaterialWidget::paintScene(gfx::Painter& painter, double secs)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();

    painter.SetViewport(0, 0, width, height);

    const auto& type = mUI.materialType->currentText();
    const auto& surf = mUI.surfaceType->currentText();
    const auto& min  = mUI.minFilter->currentText();
    const auto& mag  = mUI.magFilter->currentText();
    const auto& wrapx = mUI.wrapX->currentText();
    const auto& wrapy = mUI.wrapY->currentText();
    const auto& color = mUI.baseColor->color();
    const auto textures = mUI.textures->count();
    const auto fps = mUI.fps->value();
    const auto blend = mUI.blend->isChecked();

    // todo: fix this
    std::string shader;
    if (type == "Color")
        shader = "solid_color.glsl";
    else if (type == "Texture")
        shader = "texture_map.glsl";

    const auto surface    = EnumFromCombo<gfx::Material::SurfaceType>(mUI.surfaceType);
    const auto min_filter = EnumFromCombo<gfx::Material::MinTextureFilter>(mUI.minFilter);
    const auto mag_filter = EnumFromCombo<gfx::Material::MagTextureFilter>(mUI.magFilter);
    const auto tex_wrap_x = EnumFromCombo<gfx::Material::TextureWrapping>(mUI.wrapX);
    const auto tex_wrap_y = EnumFromCombo<gfx::Material::TextureWrapping>(mUI.wrapY);

    gfx::Material material(shader);
    material.SetBaseColor(app::toGfx(color));
    material.SetGamma(mUI.gamma->value());
    material.SetSurfaceType(surface);
    material.SetRuntime(mTime);
    material.SetFps(fps);
    material.SetTextureBlendWeight(blend ? 1.0f : 0.0f);
    material.SetTextureMinFilter(min_filter);
    material.SetTextureMagFilter(mag_filter);
    material.SetTextureScaleX(mUI.scaleX->value());
    material.SetTextureScaleY(mUI.scaleY->value());
    material.SetTextureWrapX(tex_wrap_x);
    material.SetTextureWrapY(tex_wrap_y);

    const bool stopped = mState == PlayState::Stopped;
    const auto texture_start = stopped ? mUI.textures->currentRow() : 0;
    const auto texture_count = stopped ? 1 : textures;
    if (texture_start != -1)
    {
        for (int i=0; i<texture_count; ++i)
        {
            const auto* item = mUI.textures->item(texture_start + i);
            const auto& key  = item->data(Qt::UserRole).toString();
            const auto& data = mTextures[key];
            material.AddTexture(app::strToEngine(data.file));
            material.SetTextureRect(i, gfx::FRect(0, 0, 1.0f, 1.0f));

            if (!data.rect_enabled)
                continue;
            const gfx::FRect rect(data.rectx,
                data.recty,
                data.rectw,
                data.recth);
            material.SetTextureRect(i, rect);

        }
    }

    const auto zoom = mUI.zoom->value();
    const auto content_width  = width * zoom;
    const auto content_height = width * zoom;
    const auto xpos = (width - content_width) * 0.5;
    const auto ypos = (height - content_height) * 0.5;

    gfx::FillRect(painter,
        gfx::FRect(xpos, ypos, content_width, content_height),
        material);

    if (mState == PlayState::Playing)
    {
        const unsigned wtf = secs * 1000;
        mTime += (wtf / 1000.0f);
    }
    mUI.time->setText(QString::number(mTime));
}

QString MaterialWidget::getCurrentTextureKey() const
{
    const auto row = mUI.textures->currentRow();
    if (row == -1)
        return "";
    const QListWidgetItem* item = mUI.textures->item(row);
    return item->data(Qt::UserRole).toString();
}

} // namespace
