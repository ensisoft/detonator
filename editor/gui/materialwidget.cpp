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
        item->setText(info.fileName());
        item->setData(Qt::UserRole, file);
        mUI.textures->addItem(item);
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
        const QString& file = items[i]->data(Qt::UserRole).toString();
        mTextureRects.remove(file);
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
    
    const auto& file = getCurrentTextureFilename();
    if (file.isEmpty())
        return;

    mUI.textureFile->setText(file);
    mUI.textureFile->setCursorPosition(0);

    const QPixmap img(file);
    if (img.isNull())
    {
        ERROR("Could not load image '%1'", file);
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

    if (mTextureRects.contains(file))
    {
        const auto& rect = mTextureRects.value(file);
        mUI.imgRect->setEnabled(rect.enabled);
        mUI.rectX->setValue(rect.x);
        mUI.rectY->setValue(rect.y);
        mUI.rectW->setValue(rect.w);
        mUI.rectH->setValue(rect.h);
    }
    mUI.texProps->setEnabled(true);
    mUI.imgRect->setEnabled(true);

}

void MaterialWidget::on_imgRect_clicked(bool checked)
{
    const auto& file = getCurrentTextureFilename();

    if (!mTextureRects.contains(file))
    {
        TextureRect rect;
        rect.enabled = checked;
        rect.x = 0.0f;
        rect.y = 0.0f;
        rect.w = 1.0f;
        rect.h = 1.0f;
        mTextureRects.insert(file, rect);
    }

    mTextureRects[file].enabled = checked;
}

void MaterialWidget::on_rectX_valueChanged(double value)
{
    mTextureRects[getCurrentTextureFilename()].x = value;
}
void MaterialWidget::on_rectY_valueChanged(double value)
{
    mTextureRects[getCurrentTextureFilename()].y = value;
}
void MaterialWidget::on_rectW_valueChanged(double value)
{
    mTextureRects[getCurrentTextureFilename()].w = value;
}
void MaterialWidget::on_rectH_valueChanged(double value)
{
    mTextureRects[getCurrentTextureFilename()].h = value;
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

    const auto& colorA = mUI.colorA->color();
    const auto& colorB = mUI.colorB->color();
    const auto textures = mUI.textures->count();
    const auto fps = mUI.fps->value();
    const auto blend = mUI.blend->isChecked();

    // todo: fix this
    std::string shader;
    if (type == "Color")
        shader = "solid_color.glsl";
    else if (type == "Texture")
        shader = "texture_map.glsl";

    gfx::Material::SurfaceType surface;
    if (surf == "Opaque")
        surface = gfx::Material::SurfaceType::Opaque;
    else if (surf == "Transparent")
        surface = gfx::Material::SurfaceType::Transparent;
    else if (surf == "Emissive")
        surface = gfx::Material::SurfaceType::Emissive;

    gfx::Material::MinTextureFilter min_filter;
    gfx::Material::MagTextureFilter mag_filter;
    if (min == "Nearest")
        min_filter = gfx::Material::MinTextureFilter::Nearest;
    else if (min == "Linear")
        min_filter = gfx::Material::MinTextureFilter::Linear;
    else if (min == "Mipmap")
        min_filter = gfx::Material::MinTextureFilter::Mipmap;
    else if (min == "Bilinear")
        min_filter = gfx::Material::MinTextureFilter::Bilinear;
    else if (min == "Trilinear")
        min_filter = gfx::Material::MinTextureFilter::Trilinear;

    if (mag == "Nearest")
        mag_filter = gfx::Material::MagTextureFilter::Nearest;
    else if (mag == "Linear")
        mag_filter = gfx::Material::MagTextureFilter::Linear;

    gfx::Material::TextureWrapping tex_wrap_x;
    gfx::Material::TextureWrapping tex_wrap_y;
    if (wrapx == "Clamp")
        tex_wrap_x = gfx::Material::TextureWrapping::Clamp;
    else if (wrapx == "Repeat")
        tex_wrap_x = gfx::Material::TextureWrapping::Repeat;
    if (wrapy == "Clamp")
        tex_wrap_y = gfx::Material::TextureWrapping::Clamp;
    else if (wrapy == "Repeat")
        tex_wrap_y = gfx::Material::TextureWrapping::Repeat;

    gfx::Material material(shader);
    material.SetColorA(app::toGfx(colorA));
    material.SetColorB(app::toGfx(colorB));
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
            const auto& file = item->data(Qt::UserRole).toString();
            material.AddTexture(app::strToEngine(file));
            if (!mTextureRects.contains(file))
                continue;
            
            const auto& rect = mTextureRects.value(file);
            if (!rect.enabled)
                continue;
            
            material.SetTextureRect(i, gfx::FRect(rect.x, rect.y, rect.w, rect.h));
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

QString MaterialWidget::getCurrentTextureFilename() const 
{
    const auto row = mUI.textures->currentRow();
    if (row == -1)
        return "";
    const QListWidgetItem* item = mUI.textures->item(row);
    return item->data(Qt::UserRole).toString();
}

} // namespace
