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
#  include <QTextStream>
#include "warnpop.h"

#include "base/assert.h"
#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/transform.h"
#include "graphics/drawing.h"
#include "graphics/types.h"
#include "editor/app/eventlog.h"
#include "editor/app/resource.h"
#include "editor/app/utility.h"
#include "editor/app/workspace.h"
#include "editor/gui/settings.h"
#include "editor/gui/utility.h"
#include "materialwidget.h"

namespace gui
{

MaterialWidget::MaterialWidget(app::Workspace* workspace)
{
    mWorkspace = workspace;

    DEBUG("Create MaterialWidget");

    mUI.setupUi(this);
    mUI.widget->setFramerate(60);
    mUI.widget->onPaintScene = std::bind(&MaterialWidget::paintScene,
        this, std::placeholders::_1, std::placeholders::_2);
    mUI.actionPause->setEnabled(false);
    mUI.actionPlay->setEnabled(true);
    mUI.actionStop->setEnabled(false);
    mUI.materialName->setText("My Material");
    setWindowTitle("My Material");

    PopulateFromEnum<gfx::Material::MinTextureFilter>(mUI.minFilter);
    PopulateFromEnum<gfx::Material::MagTextureFilter>(mUI.magFilter);
    PopulateFromEnum<gfx::Material::TextureWrapping>(mUI.wrapX);
    PopulateFromEnum<gfx::Material::TextureWrapping>(mUI.wrapY);
    PopulateFromEnum<gfx::Material::SurfaceType>(mUI.surfaceType);
    PopulateFromEnum<gfx::Material::Type>(mUI.materialType);
}

MaterialWidget::MaterialWidget(app::Workspace* workspace, const app::Resource& resource) : MaterialWidget(workspace)
{
    DEBUG("Editing material: '%1'", resource.GetName());

    const gfx::Material* material;
    ASSERT(resource.GetContent(&material));

    SetValue(mUI.materialType, material->GetType());
    SetValue(mUI.surfaceType,  material->GetSurfaceType());
    SetValue(mUI.minFilter,    material->GetMinTextureFilter());
    SetValue(mUI.magFilter,    material->GetMagTextureFilter());
    SetValue(mUI.wrapX,        material->GetTextureWrapX());
    SetValue(mUI.wrapY,        material->GetTextureWrapY());
    SetValue(mUI.baseColor,    material->GetBaseColor());
    SetValue(mUI.scaleX,       material->GetTextureScaleX());
    SetValue(mUI.scaleY,       material->GetTextureScaleY());
    SetValue(mUI.fps,          material->GetFps());
    SetValue(mUI.blend,        material->GetBlendFrames());
    SetValue(mUI.gamma,        material->GetGamma());

    QStringList texture_files;
    QStringList texture_rects;

    resource.GetProperty("texture_files", &texture_files);
    resource.GetProperty("texture_rects", &texture_rects);
    ASSERT(texture_files.count() == texture_rects.count());

    for (int i=0; i<texture_files.count(); ++i)
    {
        const auto& rect = texture_rects[i];
        const auto& file = texture_files[i];
        const QFileInfo info(file);
        if (!info.exists())
        {
            WARN("Texture '%1' could no longer be found.", file);
            continue;
        }
        QString stupid(rect);
        QTextStream ss(&stupid);
        float x, y, w, h;
        ss >> x >> y >> w >> h;

        TextureData data;
        data.file = file;
        data.rectx = x;
        data.recty = y;
        data.rectw = w;
        data.recth = h;

        const auto& key = app::RandomString();
        QListWidgetItem* item = new QListWidgetItem(mUI.textures);
        item->setText(info.fileName());
        item->setData(Qt::UserRole, key);
        mUI.textures->addItem(item);

        // store meta data.
        mTextures[key] = data;
    }


    mUI.textures->setCurrentRow(0);
    mUI.materialName->setText(resource.GetName());
    setWindowTitle(resource.GetName());

    // enable/disable UI elements based on the material type
    on_materialType_currentIndexChanged("");
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
    settings.loadWidget("Material", mUI.materialName);
    settings.loadWidget("Material", mUI.materialType);
    settings.loadWidget("Material", mUI.surfaceType);
    settings.loadWidget("Material", mUI.baseColor);
    settings.loadWidget("Material", mUI.gamma);
    settings.loadWidget("Material", mUI.scaleX);
    settings.loadWidget("Material", mUI.scaleY);
    settings.loadWidget("Material", mUI.minFilter);
    settings.loadWidget("Material", mUI.magFilter);
    settings.loadWidget("Material", mUI.wrapX);
    settings.loadWidget("Material", mUI.wrapY);
    settings.loadWidget("Material", mUI.fps);
    settings.loadWidget("Material", mUI.blend);
    settings.loadWidget("Material", mUI.shader);
    settings.loadWidget("Material", mUI.zoom);

    const auto& texture_files = settings.getValue("Material", "texture_files", QStringList());
    const auto& texture_rects = settings.getValue("Material", "texture_rects", QStringList());
    ASSERT(texture_files.size() == texture_rects.size());

    for (int i=0; i<texture_files.size(); ++i)
    {
        const auto& rect = texture_rects[i];
        const auto& file = texture_files[i];
        const QFileInfo info(file);
        if (!info.exists())
        {
            WARN("Texture '%1' could no longer be found.", file);
            continue;
        }
        QString stupid(rect);
        QTextStream ss(&stupid);
        float x, y, w, h;
        ss >> x >> y >> w >> h;

        TextureData data;
        data.file = file;
        data.rectx = x;
        data.recty = y;
        data.rectw = w;
        data.recth = h;

        const auto& key = app::RandomString();
        auto* item = new QListWidgetItem(mUI.textures);
        item->setText(info.fileName());
        item->setData(Qt::UserRole, key);
        mUI.textures->addItem(item);

        // store meta data.
        mTextures[key] = data;
    }
    // select first texture if any
    mUI.textures->setCurrentRow(0);

    // enable/disable UI elements based on the material type
    on_materialType_currentIndexChanged("");

    setWindowTitle(mUI.materialName->text());
    return true;
}

bool MaterialWidget::saveState(Settings& settings) const
{
    // serialize texture files and rectangles.
    QStringList texture_files;
    QStringList texture_rects;
    for (int i=0; i<mUI.textures->count(); ++i)
    {
        const auto* item = mUI.textures->item(i);
        const auto& key  = item->data(Qt::UserRole).toString();
        const auto& data = mTextures[key];
        texture_files << data.file;
        texture_rects << QString("%1 %2 %3 %4")
            .arg(data.rectx).arg(data.recty)
            .arg(data.rectw).arg(data.recth);
    }
    settings.setValue("Material", "texture_files", texture_files);
    settings.setValue("Material", "texture_rects", texture_rects);
    settings.saveWidget("Material", mUI.materialName);
    settings.saveWidget("Material", mUI.materialType);
    settings.saveWidget("Material", mUI.surfaceType);
    settings.saveWidget("Material", mUI.baseColor);
    settings.saveWidget("Material", mUI.gamma);
    settings.saveWidget("Material", mUI.scaleX);
    settings.saveWidget("Material", mUI.scaleY);
    settings.saveWidget("Material", mUI.minFilter);
    settings.saveWidget("Material", mUI.magFilter);
    settings.saveWidget("Material", mUI.wrapX);
    settings.saveWidget("Material", mUI.wrapY);
    settings.saveWidget("Material", mUI.fps);
    settings.saveWidget("Material", mUI.blend);
    settings.saveWidget("Material", mUI.shader);
    settings.saveWidget("Material", mUI.zoom);
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

    // careful here!
    // the order in which we add our textures is important,
    // mTextures map is mapping an arbitary key to a texture data
    // object but it doesn't have the exact order of textures
    // we need to use the UI object for this.
    QStringList texture_files;
    QStringList texture_rects;
    for (int i=0; i<mUI.textures->count(); ++i)
    {
        const auto* item = mUI.textures->item(i);
        const auto& key  = item->data(Qt::UserRole).toString();
        const auto& data = mTextures[key];
        texture_files << data.file;
        texture_rects << QString("%1 %2 %3 %4")
            .arg(data.rectx).arg(data.recty)
            .arg(data.rectw).arg(data.rectw);
    }

    gfx::Material material((gfx::Material::Type)GetValue(mUI.materialType));
    fillMaterial(material);

    app::MaterialResource resource(std::move(material), name);
    // set the properties, note that these are also shared by the particle widget
    resource.SetProperty("texture_files", texture_files);
    resource.SetProperty("texture_rects", texture_rects);
    resource.SetProperty("color", mUI.baseColor->color());
    resource.SetProperty("surface", mUI.surfaceType->currentText());
    mWorkspace->SaveResource(resource);

    setWindowTitle(name);

    NOTE("Saved material '%1'", name);
}

void MaterialWidget::on_textureAdd_clicked()
{
    const auto& list = QFileDialog::getOpenFileNames(this,
        tr("Select Image File(s)"), "", tr("Images (*.png *.jpg *.jpg)"));
    if (list.isEmpty())
        return;
    for (const auto& file : list)
    {
        const QFileInfo info(file);

        // generate random key for the file that maps
        // the list widget item to texture meta data.
        const auto& key = app::RandomString();

        auto* item = new QListWidgetItem(mUI.textures);
        item->setText(info.fileName());
        item->setData(Qt::UserRole, key);
        mUI.textures->addItem(item);

        // store the texture details in a map by the unique key
        TextureData data;
        data.file = file;
        data.rectx = 0.0f;
        data.recty = 0.0f;
        data.rectw = 1.0f;
        data.recth = 1.0f;
        mTextures[key] = data;
    }
    const auto index = mUI.textures->currentRow();
    mUI.textures->setCurrentRow(index == -1 ? 0 : index);
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

    mUI.textureDel->setEnabled(false);
    mUI.textureRect->setEnabled(false);
    mUI.textureProp->setEnabled(false);
}

void MaterialWidget::on_textures_currentRowChanged(int index)
{
    mUI.textureRect->setEnabled(false);
    mUI.textureProp->setEnabled(false);
    mUI.textureWidth->setText("");
    mUI.textureHeight->setText("");
    mUI.textureDepth->setText("");
    mUI.textureFile->setText("");
    mUI.textureDel->setEnabled(false);

    const auto& key = getCurrentTextureKey();
    if (key.isEmpty())
        return;

    const auto& data = mTextures[key];
    const auto& img  = QPixmap(data.file);
    mUI.textureFile->setText(data.file);
    mUI.textureFile->setCursorPosition(0);
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
    mUI.rectX->setValue(data.rectx);
    mUI.rectY->setValue(data.recty);
    mUI.rectW->setValue(data.rectw);
    mUI.rectH->setValue(data.recth);
    mUI.textureProp->setEnabled(true);
    mUI.textureRect->setEnabled(true);
    mUI.textureDel->setEnabled(true);
}

void MaterialWidget::on_materialType_currentIndexChanged(const QString& text)
{
    mUI.texturing->setEnabled(false);
    mUI.animation->setEnabled(false);
    mUI.customMaterial->setEnabled(false);
    mUI.textureMaps->setEnabled(false);
    mUI.textureProp->setEnabled(false);
    mUI.textureRect->setEnabled(false);

    // enable/disable UI properties based on the
    // material type.
    const gfx::Material::Type type = GetValue(mUI.materialType);

    if (type == gfx::Material::Type::Color)
    {
        mUI.baseProperties->setEnabled(true);
    }
    else if (type == gfx::Material::Type::Texture)
    {
        mUI.texturing->setEnabled(true);
        mUI.textureMaps->setEnabled(true);
        mUI.textureProp->setEnabled(true);
        mUI.textureRect->setEnabled(true);
    }
    else if (type == gfx::Material::Type::Sprite)
    {
        mUI.texturing->setEnabled(true);
        mUI.textureMaps->setEnabled(true);
        mUI.textureProp->setEnabled(true);
        mUI.textureRect->setEnabled(true);
        mUI.animation->setEnabled(true);
    }
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
    // todo: allow for a custom shader to be used.
    material.SetBaseColor(GetValue(mUI.baseColor));
    material.SetGamma(GetValue(mUI.gamma));
    material.SetSurfaceType(GetValue(mUI.surfaceType));
    material.SetRuntime(mTime);
    material.SetFps(GetValue(mUI.fps));
    material.SetBlendFrames(GetValue(mUI.blend));
    material.SetTextureMinFilter(GetValue(mUI.minFilter));
    material.SetTextureMagFilter(GetValue(mUI.magFilter));
    material.SetTextureScaleX(GetValue(mUI.scaleX));
    material.SetTextureScaleY(GetValue(mUI.scaleY));
    material.SetTextureWrapX(GetValue(mUI.wrapX));
    material.SetTextureWrapY(GetValue(mUI.wrapY));

    // add textures that we have
    const int num_textures_add = mUI.textures->count();
    for (int i=0; i<num_textures_add; ++i)
    {
        const auto* item = mUI.textures->item(i);
        const auto& key  = item->data(Qt::UserRole).toString();
        const auto& data = mTextures[key];
        material.AddTexture(app::ToUtf8(data.file));
        material.SetTextureRect(i, gfx::FRect(data.rectx, data.recty, data.rectw, data.recth));
    }
}

void MaterialWidget::paintScene(gfx::Painter& painter, double secs)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    gfx::Material material((gfx::Material::Type)GetValue(mUI.materialType));
    fillMaterial(material);

    const auto zoom = mUI.zoom->value();
    const auto content_width  = width * zoom;
    const auto content_height = width * zoom;
    const auto xpos = (width - content_width) * 0.5f;
    const auto ypos = (height - content_height) * 0.5f;

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
