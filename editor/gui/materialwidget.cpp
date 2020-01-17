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
#  include <QMessageBox>
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
#include "editor/app/workspace.h"
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
    mUI.materialName->setText("My Material");

    PopulateFromEnum<gfx::Material::MinTextureFilter>(mUI.minFilter);
    PopulateFromEnum<gfx::Material::MagTextureFilter>(mUI.magFilter);
    PopulateFromEnum<gfx::Material::TextureWrapping>(mUI.wrapX);
    PopulateFromEnum<gfx::Material::TextureWrapping>(mUI.wrapY);
    PopulateFromEnum<gfx::Material::SurfaceType>(mUI.surfaceType);
    PopulateFromEnum<gfx::Material::Type>(mUI.materialType);
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
    bar.addSeparator();
    bar.addAction(mUI.actionSave);
}
void MaterialWidget::addActions(QMenu& menu)
{
    menu.addAction(mUI.actionPlay);
    menu.addAction(mUI.actionPause);
    menu.addSeparator();
    menu.addAction(mUI.actionStop);
    menu.addSeparator();
    menu.addAction(mUI.actionSave);
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

void MaterialWidget::setWorkspace(app::Workspace* workspace)
{
    mWorkspace = workspace;
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

void MaterialWidget::on_actionSave_triggered()
{
    const auto& name = mUI.materialName->text();
    if (name.isEmpty())
    {
        mUI.materialName->setFocus();
        return;
    }
    if (mWorkspace->HasMaterial(name))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Question);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setText(tr("Workspace already contains a material by this name. Overwrite ?"));
        if (msg.exec() == QMessageBox::No)
            return;
    }

    gfx::Material material("", "", gfx::Material::Type::Color);
    fillMaterial(material);

    mWorkspace->SaveMaterial(material);

    NOTE("Saved material %1", name);
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

    mUI.texRect->setEnabled(false);
    mUI.rectX->setValue(0.0f);
    mUI.rectY->setValue(0.0f);
    mUI.rectW->setValue(1.0f);
    mUI.rectH->setValue(1.0f);
}

void MaterialWidget::on_textures_currentRowChanged(int index)
{
    mUI.texRect->setEnabled(false);
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

    mUI.texRect->setEnabled(data.rect_enabled);
    mUI.rectX->setValue(data.rectx);
    mUI.rectY->setValue(data.recty);
    mUI.rectW->setValue(data.rectw);
    mUI.rectH->setValue(data.recth);

    mUI.texProps->setEnabled(true);
    mUI.texRect->setEnabled(true);

}

void MaterialWidget::on_materialType_currentIndexChanged(const QString& text)
{
    mUI.texturing->setEnabled(false);
    mUI.animation->setEnabled(false);
    mUI.customMaterial->setEnabled(false);
    mUI.texMaps->setEnabled(false);
    mUI.texProps->setEnabled(false);
    mUI.texRect->setEnabled(false);

    // enable/disable UI properties based on the
    // material type.
    const auto type = EnumFromCombo<gfx::Material::Type>(mUI.materialType);

    if (type == gfx::Material::Type::Color)
    {
        mUI.baseProperties->setEnabled(true);
    }
    else if (type == gfx::Material::Type::Texture)
    {
        mUI.texturing->setEnabled(true);
        mUI.texMaps->setEnabled(true);
        mUI.texProps->setEnabled(true);
        mUI.texRect->setEnabled(true);
    }
    else if (type == gfx::Material::Type::Sprite)
    {
        mUI.texturing->setEnabled(true);
        mUI.texMaps->setEnabled(true);
        mUI.texProps->setEnabled(true);
        mUI.texRect->setEnabled(true);
        mUI.animation->setEnabled(true);
    }
}

void MaterialWidget::on_texRect_clicked(bool checked)
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

void MaterialWidget::fillMaterial(gfx::Material& material) const
{
    const auto& name      = mUI.materialName->text();
    const auto& color     = mUI.baseColor->color();
    const auto blend      = mUI.blend->isChecked();
    const auto fps        = mUI.fps->value();
    const auto type       = EnumFromCombo<gfx::Material::Type>(mUI.materialType);
    const auto surface    = EnumFromCombo<gfx::Material::SurfaceType>(mUI.surfaceType);
    const auto min_filter = EnumFromCombo<gfx::Material::MinTextureFilter>(mUI.minFilter);
    const auto mag_filter = EnumFromCombo<gfx::Material::MagTextureFilter>(mUI.magFilter);
    const auto tex_wrap_x = EnumFromCombo<gfx::Material::TextureWrapping>(mUI.wrapX);
    const auto tex_wrap_y = EnumFromCombo<gfx::Material::TextureWrapping>(mUI.wrapY);

    // todo: allow for a custom shader to be used.
    std::string shader;
    if (type == gfx::Material::Type::Color)
        shader = "solid_color.glsl";
    else if (type == gfx::Material::Type::Texture)
        shader = "texture_map.glsl";
    else if (type == gfx::Material::Type::Sprite)
        shader = "texture_map.glsl";

    material.SetType(type);
    material.SetName(app::toUtf8(name));
    material.SetShader(shader);
    material.SetBaseColor(app::toGfx(color));
    material.SetGamma(mUI.gamma->value());
    material.SetSurfaceType(surface);
    material.SetRuntime(mTime);
    material.SetFps(fps);
    material.SetBlendFrames(blend);
    material.SetTextureMinFilter(min_filter);
    material.SetTextureMagFilter(mag_filter);
    material.SetTextureScaleX(mUI.scaleX->value());
    material.SetTextureScaleY(mUI.scaleY->value());
    material.SetTextureWrapX(tex_wrap_x);
    material.SetTextureWrapY(tex_wrap_y);


    // add textures that we have
    const int num_textures_add = mUI.textures->count();
    for (int i=0; i<num_textures_add; ++i)
    {
        const auto* item = mUI.textures->item(i);
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

void MaterialWidget::paintScene(gfx::Painter& painter, double secs)
{

    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    gfx::Material material("", "", gfx::Material::Type::Color);
    fillMaterial(material);


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
