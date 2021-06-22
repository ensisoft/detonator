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
#  include <QMessageBox>
#  include <QFileDialog>
#  include <QFileInfo>
#  include <QPixmap>
#  include <QTextStream>
#  include <base64/base64.h>
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <algorithm>

#include "base/assert.h"
#include "base/utility.h"
#include "base/json.h"
#include "data/json.h"
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
#include "editor/gui/dlgtext.h"
#include "editor/gui/dlgbitmap.h"
#include "editor/gui/dlgtexturerect.h"
#include "editor/gui/materialwidget.h"

namespace {

struct TexturePackImage {
    QString name;
    unsigned width  = 0;
    unsigned height = 0;
    unsigned xpos   = 0;
    unsigned ypos   = 0;
    unsigned index  = 0;
};

bool ReadTexturePack(const QString& json_file, std::vector<TexturePackImage>* out)
{
    QFile file(json_file);
    if (!file.open(QIODevice::ReadOnly))
    {
        ERROR("Failed to open '%1' for reading. (%2)", json_file, file.error());
        return false;
    }
    const auto& buff = file.readAll();
    if (buff.isEmpty())
    {
        ERROR("JSON file '%1' contains no content.", json_file);
        return false;
    }
    const auto* beg  = buff.data();
    const auto* end  = buff.data() + buff.size();
    const auto& json = nlohmann::json::parse(beg, end, nullptr, false);
    if (json.is_discarded())
    {
        ERROR("JSON file '%1' could not be parsed.", json_file);
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
        TexturePackImage tpi;
        tpi.name   = app::FromUtf8(name);
        tpi.width  = w;
        tpi.height = h;
        tpi.xpos   = x;
        tpi.ypos   = y;
        tpi.index  = index;
        out->push_back(std::move(tpi));
    }

    // finally sort based on the image index.
    std::sort(std::begin(*out), std::end(*out), [&](const auto& a, const auto& b) {
        return a.index < b.index;
    });

    INFO("Succesfully parsed '%1'. %2 images found.", json_file, out->size());
    return true;
}

} // namespace

namespace gui
{

MaterialWidget::MaterialWidget(app::Workspace* workspace)
{
    mWorkspace = workspace;

    DEBUG("Create MaterialWidget");

    mUI.setupUi(this);
    mUI.widget->onPaintScene = std::bind(&MaterialWidget::PaintScene,
        this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onZoomIn = std::bind(&MaterialWidget::ZoomIn, this);
    mUI.widget->onZoomOut = std::bind(&MaterialWidget::ZoomOut, this);
    mUI.actionPause->setEnabled(false);
    mUI.actionPlay->setEnabled(true);
    mUI.actionStop->setEnabled(false);
    mUI.materialName->setText("My Material");

    QMenu* menu  = new QMenu(this);
    QAction* add_texture_from_file = menu->addAction("File");
    QAction* add_texture_from_text = menu->addAction("Text");
    QAction* add_texture_from_bitmap = menu->addAction(("Bitmap"));
    connect(add_texture_from_file, &QAction::triggered, this, &MaterialWidget::AddNewTextureMapFromFile);
    connect(add_texture_from_text, &QAction::triggered, this, &MaterialWidget::AddNewTextureMapFromText);
    connect(add_texture_from_bitmap, &QAction::triggered, this, &MaterialWidget::AddNewTextureMapFromBitmap);
    mUI.btnAddTextureMap->setMenu(menu);

    setWindowTitle("My Material");
    SetValue(mUI.materialID, mMaterial.GetId());

    PopulateFromEnum<gfx::MaterialClass::MinTextureFilter>(mUI.minFilter);
    PopulateFromEnum<gfx::MaterialClass::MagTextureFilter>(mUI.magFilter);
    PopulateFromEnum<gfx::MaterialClass::TextureWrapping>(mUI.wrapX);
    PopulateFromEnum<gfx::MaterialClass::TextureWrapping>(mUI.wrapY);
    PopulateFromEnum<gfx::MaterialClass::SurfaceType>(mUI.surfaceType);
    PopulateFromEnum<gfx::MaterialClass::Shader>(mUI.materialType);
    PopulateFromEnum<gfx::MaterialClass::ParticleAction>(mUI.particleAction);
    SetList(mUI.cmbModel, workspace->ListPrimitiveDrawables());
    SetValue(mUI.cmbModel, "Rectangle");
}

MaterialWidget::MaterialWidget(app::Workspace* workspace, const app::Resource& resource) : MaterialWidget(workspace)
{
    DEBUG("Editing material: '%1'", resource.GetName());

    mMaterial = *resource.GetContent<gfx::MaterialClass>();
    SetValue(mUI.materialType, mMaterial.GetType());
    SetValue(mUI.surfaceType,  mMaterial.GetSurfaceType());
    SetValue(mUI.minFilter,    mMaterial.GetMinTextureFilter());
    SetValue(mUI.magFilter,    mMaterial.GetMagTextureFilter());
    SetValue(mUI.wrapX,        mMaterial.GetTextureWrapX());
    SetValue(mUI.wrapY,        mMaterial.GetTextureWrapY());
    SetValue(mUI.baseColor,    mMaterial.GetBaseColor());
    SetValue(mUI.scaleX,       mMaterial.GetTextureScaleX());
    SetValue(mUI.scaleY,       mMaterial.GetTextureScaleY());
    SetValue(mUI.velocityX,    mMaterial.GetTextureVelocityX());
    SetValue(mUI.velocityY,    mMaterial.GetTextureVelocityY());
    SetValue(mUI.velocityZ,    mMaterial.GetTextureVelocityZ());
    SetValue(mUI.particleAction, mMaterial.GetParticleAction());
    SetValue(mUI.fps,          mMaterial.GetFps());
    SetValue(mUI.blend,        mMaterial.GetBlendFrames());
    SetValue(mUI.gamma,        mMaterial.GetGamma());
    SetValue(mUI.staticInstance, mMaterial.IsStatic());
    SetValue(mUI.colorMap0,    mMaterial.GetColorMapColor(gfx::MaterialClass::ColorIndex::TopLeft));
    SetValue(mUI.colorMap1,    mMaterial.GetColorMapColor(gfx::MaterialClass::ColorIndex::TopRight));
    SetValue(mUI.colorMap2,    mMaterial.GetColorMapColor(gfx::MaterialClass::ColorIndex::BottomLeft));
    SetValue(mUI.colorMap3,    mMaterial.GetColorMapColor(gfx::MaterialClass::ColorIndex::BottomRight));
    GetProperty(resource, "shader_file", mUI.shaderFile);
    GetProperty(resource, "use_shader_file", mUI.customShader);
    GetUserProperty(resource, "model", mUI.cmbModel);
    GetUserProperty(resource, "zoom", mUI.zoom);
    GetUserProperty(resource, "widget", mUI.widget);

    // add the textures to the UI
    size_t num_textures = mMaterial.GetNumTextures();
    for (size_t i=0; i<num_textures; ++i)
    {
        const auto& source = mMaterial.GetTextureSource(i);
        QListWidgetItem* item = new QListWidgetItem(mUI.textures);
        item->setText(app::FromUtf8(source.GetName()));
        item->setData(Qt::UserRole, app::FromUtf8(source.GetId()));
        mUI.textures->addItem(item);
    }

    mUI.textures->setCurrentRow(0);
    mUI.materialName->setText(resource.GetName());
    SetValue(mUI.materialName, resource.GetName());
    SetValue(mUI.materialID, mMaterial.GetId());
    setWindowTitle(resource.GetName());

    mOriginalHash = mMaterial.GetHash();

    // enable/disable UI elements based on the material type
    on_materialType_currentIndexChanged("");
}

MaterialWidget::~MaterialWidget()
{
    DEBUG("Destroy MaterialWidget");
}

void MaterialWidget::AddActions(QToolBar& bar)
{
    bar.addAction(mUI.actionPlay);
    bar.addAction(mUI.actionPause);
    bar.addSeparator();
    bar.addAction(mUI.actionStop);
    bar.addSeparator();
    bar.addAction(mUI.actionSave);
}
void MaterialWidget::AddActions(QMenu& menu)
{
    menu.addAction(mUI.actionPlay);
    menu.addAction(mUI.actionPause);
    menu.addSeparator();
    menu.addAction(mUI.actionStop);
    menu.addSeparator();
    menu.addAction(mUI.actionSave);
}

bool MaterialWidget::LoadState(const Settings& settings)
{
    std::string base64;
    settings.getValue("Material", "content", &base64);

    data::JsonObject json;
    auto [ok, error] = json.ParseString(base64::Decode(base64));
    if (!ok)
    {
        ERROR("Failed to parse content JSON. '%1'", error);
        return false;
    }
    auto ret = gfx::MaterialClass::FromJson(json);
    if (!ret.has_value())
    {
        WARN("Failed to load material widget state.");
        return false;
    }
    mMaterial = std::move(ret.value());

    // restore UI from the material values.
    SetValue(mUI.materialID, mMaterial.GetId());
    SetValue(mUI.materialType, mMaterial.GetType());
    SetValue(mUI.surfaceType,  mMaterial.GetSurfaceType());
    SetValue(mUI.minFilter,    mMaterial.GetMinTextureFilter());
    SetValue(mUI.magFilter,    mMaterial.GetMagTextureFilter());
    SetValue(mUI.wrapX,        mMaterial.GetTextureWrapX());
    SetValue(mUI.wrapY,        mMaterial.GetTextureWrapY());
    SetValue(mUI.baseColor,    mMaterial.GetBaseColor());
    SetValue(mUI.scaleX,       mMaterial.GetTextureScaleX());
    SetValue(mUI.scaleY,       mMaterial.GetTextureScaleY());
    SetValue(mUI.velocityX,    mMaterial.GetTextureVelocityX());
    SetValue(mUI.velocityY,    mMaterial.GetTextureVelocityY());
    SetValue(mUI.velocityZ,    mMaterial.GetTextureVelocityZ());
    SetValue(mUI.particleAction, mMaterial.GetParticleAction());
    SetValue(mUI.fps,          mMaterial.GetFps());
    SetValue(mUI.blend,        mMaterial.GetBlendFrames());
    SetValue(mUI.gamma,        mMaterial.GetGamma());
    SetValue(mUI.staticInstance, mMaterial.IsStatic());
    SetValue(mUI.colorMap0,    mMaterial.GetColorMapColor(gfx::MaterialClass::ColorIndex::TopLeft));
    SetValue(mUI.colorMap1,    mMaterial.GetColorMapColor(gfx::MaterialClass::ColorIndex::TopRight));
    SetValue(mUI.colorMap2,    mMaterial.GetColorMapColor(gfx::MaterialClass::ColorIndex::BottomLeft));
    SetValue(mUI.colorMap3,    mMaterial.GetColorMapColor(gfx::MaterialClass::ColorIndex::BottomRight));
    settings.loadWidget("Material", mUI.materialName);
    settings.loadWidget("Material", mUI.shaderFile);
    settings.loadWidget("Material", mUI.customShader);
    settings.loadWidget("Material", mUI.zoom);
    settings.loadWidget("Material", mUI.cmbModel);
    settings.loadWidget("Material", mUI.widget);

    // restore textures list
    size_t num_textures = mMaterial.GetNumTextures();
    for (size_t i=0; i<num_textures; ++i)
    {
        const auto& source = mMaterial.GetTextureSource(i);
        QListWidgetItem* item = new QListWidgetItem(mUI.textures);
        item->setText(app::FromUtf8(source.GetName()));
        item->setData(Qt::UserRole, app::FromUtf8(source.GetId()));
        mUI.textures->addItem(item);
    }

    // select first texture if any
    mUI.textures->setCurrentRow(0);
    // enable/disable UI elements based on the material type
    on_materialType_currentIndexChanged("");

    setWindowTitle(mUI.materialName->text());

    mOriginalHash = mMaterial.GetHash();
    return true;
}

bool MaterialWidget::SaveState(Settings& settings) const
{
    settings.saveWidget("Material", mUI.materialName);
    settings.saveWidget("Material", mUI.shaderFile);
    settings.saveWidget("Material", mUI.customShader);
    settings.saveWidget("Material", mUI.zoom);
    settings.saveWidget("Material", mUI.cmbModel);
    settings.saveWidget("Material", mUI.widget);
    SetMaterialProperties();

    // the material can already serialize into JSON.
    // so let's use the material's JSON serialization here as well.
    data::JsonObject json;
    mMaterial.IntoJson(json);
    settings.setValue("Material", "content", base64::Encode(json.ToString()));
    return true;
}

bool MaterialWidget::CanTakeAction(Actions action, const Clipboard* clipboard) const
{
    switch (action)
    {
        case Actions::CanCut:
        case Actions::CanCopy:
        case Actions::CanPaste:
            return false;
        case Actions::CanUndo:
            return false;
        case Actions::CanReloadTextures:
        case Actions::CanReloadShaders:
            return true;
        case Actions::CanZoomIn: {
            const auto max = mUI.zoom->maximum();
            const auto val = mUI.zoom->value();
            return val < max;
        }
        break;
        case Actions::CanZoomOut: {
            const auto min = mUI.zoom->minimum();
            const auto val = mUI.zoom->value();
            return val > min;
        }
        break;
    }
    BUG("Unhandled action query.");
    return false;
}

void MaterialWidget::ZoomIn()
{
    const auto value = mUI.zoom->value();
    mUI.zoom->setValue(value + 0.1);
}

void MaterialWidget::ZoomOut()
{
    const auto value = mUI.zoom->value();
    if (value > 0.1)
        mUI.zoom->setValue(value - 0.1);
}

void MaterialWidget::ReloadShaders()
{
    mUI.widget->reloadShaders();
}
void MaterialWidget::ReloadTextures()
{
    mUI.widget->reloadTextures();
}

void MaterialWidget::Shutdown()
{
    mUI.widget->dispose();
}

void MaterialWidget::Update(double secs)
{
    if (mState == PlayState::Playing)
    {
        mTime += secs;
    }
}

void MaterialWidget::Save()
{
    on_actionSave_triggered();
}

bool MaterialWidget::HasUnsavedChanges() const
{
    if (!mOriginalHash)
        return false;
    const auto hash = mMaterial.GetHash();
    return hash != mOriginalHash;
}

bool MaterialWidget::ConfirmClose()
{
    // any unsaved changes ?
    const auto hash = mMaterial.GetHash();
    if (hash == mOriginalHash)
        return true;

    QMessageBox msg(this);
    msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    msg.setIcon(QMessageBox::Question);
    msg.setText(tr("Looks like you have unsaved changes. Would you like to save them?"));
    const auto ret = msg.exec();
    if (ret == QMessageBox::Cancel)
        return false;
    else if (ret == QMessageBox::No)
        return true;

    on_actionSave_triggered();
    return true;
}
bool MaterialWidget::GetStats(Stats* stats) const
{
    stats->time  = mTime;
    stats->fps   = mUI.widget->getCurrentFPS();
    stats->vsync = mUI.widget->haveVSYNC();
    return true;
}

void MaterialWidget::Render()
{
    mUI.widget->triggerPaint();
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
    if (!MustHaveInput(mUI.materialName))
        return;

    SetMaterialProperties();

    const QString& name = GetValue(mUI.materialName);

    app::MaterialResource resource(mMaterial, name);
    SetProperty(resource,"shader_file", mUI.shaderFile);
    SetProperty(resource,"use_shader_file", mUI.customShader);
    SetUserProperty(resource, "model", mUI.cmbModel);
    SetUserProperty(resource, "widget", mUI.widget);    
    SetUserProperty(resource, "zoom", mUI.zoom);
    mWorkspace->SaveResource(resource);
    mOriginalHash = mMaterial.GetHash();

    NOTE("Saved material '%1'", name);
    INFO("Saved material '%1'", name);
    setWindowTitle(name);
}

void MaterialWidget::on_btnAddTextureMap_clicked()
{
    mUI.btnAddTextureMap->showMenu();
}

void MaterialWidget::on_btnDelTextureMap_clicked()
{
    // make sure currentRowChanged is blocked while we're deleting shit.
    QSignalBlocker cockblocker(mUI.textures);

    QList<QListWidgetItem*> items = mUI.textures->selectedItems();

    // a bit ugly but ok.
    for (int i=0; i<items.count(); ++i)
    {
        const QListWidgetItem* item = items[i];
        const auto& data = item->data(Qt::UserRole).toString();
        const auto& id   = app::ToUtf8(data);
        const auto num_textures = mMaterial.GetNumTextures();
        bool deleted = false;
        for (size_t i=0; i<num_textures; ++i)
        {
            if (mMaterial.GetTextureSource(i).GetId() != id)
                continue;

            mMaterial.DeleteTexture(i);
            deleted = true;
            break;
        }
        ASSERT(deleted);
    }

    qDeleteAll(items);

    const auto current = mUI.textures->currentRow();

    on_textures_currentRowChanged(current);
}

void MaterialWidget::on_btnEditTextureMap_clicked()
{
    const auto row = mUI.textures->currentRow();
    if (row == -1)
        return;

    auto& source = mMaterial.GetTextureSource(row);
    if (const auto* ptr = dynamic_cast<const gfx::detail::TextureFileSource*>(&source))
    {
        emit OpenExternalImage(app::FromUtf8(ptr->GetFilename()));
    }
    else if (auto* ptr = dynamic_cast<gfx::detail::TextureTextBufferSource*>(&source))
    {
        // make a copy for editing.
        gfx::TextBuffer text(ptr->GetTextBuffer());
        DlgText dlg(this, text);
        if (dlg.exec() == QDialog::Rejected)
            return;

        // map the font files.
        for (size_t i=0; i<text.GetNumTexts(); ++i)
        {
            auto& style_and_text = text.GetText(i);
            style_and_text.font  = mWorkspace->MapFileToWorkspace(style_and_text.font);
        }

        // Update the texture source's TextBuffer
        ptr->SetTextBuffer(std::move(text));

        // update the preview.
        on_textures_currentRowChanged(row);
    }
    else if (auto* ptr = dynamic_cast<gfx::detail::TextureBitmapGeneratorSource*>(&source))
    {
        // make a copy for editing.
        auto copy = ptr->GetGenerator().Clone();
        DlgBitmap dlg(this, std::move(copy));
        if (dlg.exec() == QDialog::Rejected)
            return;

        // override the generator with changes.
        ptr->SetGenerator(dlg.GetResult());

        // update the preview.
        on_textures_currentRowChanged(row);
    }
}

void MaterialWidget::on_btnEditShader_clicked()
{
    emit OpenExternalShader(mUI.shaderFile->text());
}

void MaterialWidget::on_btnResetTextureRect_clicked()
{
    const auto row = mUI.textures->currentRow();
    if (row == -1)
        return;

    const gfx::FRect rect(0.0f, 0.0f, 1.0f, 1.0f);
    mMaterial.SetTextureRect(row, rect);

    on_textures_currentRowChanged(row);
}

void MaterialWidget::on_btnSelectTextureRect_clicked()
{
    const auto row = mUI.textures->currentRow();
    if (row == -1)
        return;
    const auto& source = mMaterial.GetTextureSource(row);
    const auto& rect   = mMaterial.GetTextureRect(row);
    DlgTextureRect dlg(this, rect, source.Clone());
    if (dlg.exec() == QDialog::Rejected)
        return;

    mMaterial.SetTextureRect(row, dlg.GetRect());

    on_textures_currentRowChanged(row);

}

void MaterialWidget::on_browseShader_clicked()
{
    const auto& list = QFileDialog::getOpenFileNames(this,
        tr("Select Shader File"), "", tr("GLSL files (*.glsl)"));
    if (list.isEmpty())
        return;
    const auto& file = mWorkspace->MapFileToWorkspace(list[0]);
    mUI.shaderFile->setText(file);
    mUI.shaderFile->setCursorPosition(0);
}

void MaterialWidget::on_textures_currentRowChanged(int index)
{
    if (index == -1)
    {
        mUI.textureRect->setEnabled(false);
        mUI.textureProp->setEnabled(false);
        mUI.textureWidth->clear();
        mUI.textureHeight->clear();
        mUI.textureDepth->clear();
        mUI.btnDelTextureMap->setEnabled(false);
        mUI.texturePreview->setPixmap(QPixmap(":texture.png"));
        mUI.textureFile->clear();
        mUI.textureID->clear();
        return;
    }

    const auto& source = mMaterial.GetTextureSource(index);
    const auto& rect   = mMaterial.GetTextureRect(index);
    const auto& bitmap = source.GetData(); // returns nullptr if fails to source the data.
    if (bitmap != nullptr)
    {
        const auto width  = bitmap->GetWidth();
        const auto height = bitmap->GetHeight();
        const auto depth  = bitmap->GetDepthBits();
        QImage img;
        if (depth == 8)
            img = QImage((const uchar*)bitmap->GetDataPtr(), width, height, width, QImage::Format_Grayscale8);
        else if (depth == 24)
            img = QImage((const uchar*)bitmap->GetDataPtr(), width, height, width * 3, QImage::Format_RGB888);
        else if (depth == 32)
            img = QImage((const uchar*)bitmap->GetDataPtr(), width, height, width * 4, QImage::Format_RGBA8888);
        else ERROR("Unexpected image bit depth: %1", depth);

        QPixmap pix;
        pix.convertFromImage(img);
        mUI.textureWidth->setText(QString::number(width));
        mUI.textureHeight->setText(QString::number(height));
        mUI.textureDepth->setText(QString::number(depth));
        mUI.texturePreview->setPixmap(pix.scaledToHeight(128));
    }
    else
    {
        mUI.texturePreview->setPixmap(QPixmap("texture.png"));
    }
    SetValue(mUI.rectX, rect.GetX());
    SetValue(mUI.rectY, rect.GetY());
    SetValue(mUI.rectW, rect.GetWidth());
    SetValue(mUI.rectH, rect.GetHeight());
    SetValue(mUI.textureID, source.GetId());
    SetValue(mUI.textureFile, QString("N/A"));
    if (const auto* ptr = dynamic_cast<const gfx::detail::TextureFileSource*>(&source))
    {
        SetValue(mUI.textureFile, ptr->GetFilename());
    }

    mUI.textureProp->setEnabled(true);
    mUI.textureRect->setEnabled(true);
    mUI.btnDelTextureMap->setEnabled(true);
}

void MaterialWidget::on_materialType_currentIndexChanged(const QString& text)
{
    mUI.texturing->setEnabled(false);
    mUI.animation->setEnabled(false);
    mUI.textureMaps->setEnabled(false);
    mUI.textureProp->setEnabled(false);
    mUI.textureRect->setEnabled(false);
    //mUI.colorMap->setEnabled(false);

    // enable/disable UI properties based on the
    // material type.
    const gfx::MaterialClass::Shader type = GetValue(mUI.materialType);

    if (type == gfx::MaterialClass::Shader::Color)
    {
        mUI.baseProperties->setEnabled(true);
    }
    else if (type == gfx::MaterialClass::Shader::Gradient)
    {
        //mUI.colorMap->setEnabled(true);
    }
    else if (type == gfx::MaterialClass::Shader::Texture)
    {
        mUI.texturing->setEnabled(true);
        mUI.textureMaps->setEnabled(true);
        mUI.textureProp->setEnabled(true);
        mUI.textureRect->setEnabled(true);
    }
    else if (type == gfx::MaterialClass::Shader::Sprite)
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
    const auto row = mUI.textures->currentRow();
    if (row == -1)
        return;

    gfx::FRect rect = mMaterial.GetTextureRect(row);
    rect.SetX(value);
    mMaterial.SetTextureRect(row, rect);
}
void MaterialWidget::on_rectY_valueChanged(double value)
{
    const auto row = mUI.textures->currentRow();
    if (row == -1)
        return;

    gfx::FRect rect = mMaterial.GetTextureRect(row);
    rect.SetY(value);
    mMaterial.SetTextureRect(row, rect);
}
void MaterialWidget::on_rectW_valueChanged(double value)
{
    const auto row = mUI.textures->currentRow();
    if (row == -1)
        return;

    gfx::FRect rect = mMaterial.GetTextureRect(row);
    rect.SetWidth(value);
    mMaterial.SetTextureRect(row, rect);
}
void MaterialWidget::on_rectH_valueChanged(double value)
{
    const auto row = mUI.textures->currentRow();
    if (row == -1)
        return;

    gfx::FRect rect = mMaterial.GetTextureRect(row);
    rect.SetHeight(value);
    mMaterial.SetTextureRect(row, rect);
}

void MaterialWidget::AddNewTextureMapFromFile()
{
    const auto& list = QFileDialog::getOpenFileNames(this,
    tr("Select Image File(s)"), "", tr("Images (*.png *.jpg *.jpeg)"));
    if (list.isEmpty())
        return;

    for (const auto& item : list)
    {
        const QFileInfo info(item);
        const auto& name = info.baseName();
        const auto& file = mWorkspace->MapFileToWorkspace(info.absoluteFilePath());

        std::vector<TexturePackImage> images;

        if (FileExists(file + ".json"))
        {
            QMessageBox msg(this);
            msg.setIcon(QMessageBox::Question);
            msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msg.setText(tr("Looks like this file '%1' has an associated JSON file."
                           "Would you like me to try to read it?").arg(name));
            if (msg.exec() == QMessageBox::Yes)
            {
                ReadTexturePack(file + ".json", &images);
            }
        }
        const QPixmap pix(file);
        if (pix.isNull())
        {
            ERROR("Failed to read image '%1'", file);
            continue;
        }

        // if no JSON file was read (or failed to read) then we create one
        // texture pack image which covers the whole image.
        if (images.empty())
        {
            TexturePackImage whole_image;
            whole_image.width  = pix.width();
            whole_image.height = pix.height();
            whole_image.xpos   = 0;
            whole_image.ypos   = 0;
            whole_image.name   = name;
            images.push_back(std::move(whole_image));
        }

        const float texture_width  = pix.width();
        const float texture_height = pix.height();
        // add all the images in the material and in the UI's list widget
        for (const auto& img : images)
        {
            auto source = std::make_unique<gfx::detail::TextureFileSource>(app::ToUtf8(file));
            source->SetName(app::ToUtf8(img.name));
            QListWidgetItem* item = new QListWidgetItem(mUI.textures);
            item->setText(img.name);
            item->setData(Qt::UserRole, app::FromUtf8(source->GetId()));
            mUI.textures->addItem(item);

            // normalize the texture coords.
            gfx::FRect texture_box;
            texture_box.SetX(img.xpos / texture_width);
            texture_box.SetY(img.ypos / texture_height);
            texture_box.SetWidth(img.width / texture_width);
            texture_box.SetHeight(img.height / texture_height);
            // add texture source with texture source box.
            mMaterial.AddTexture(std::move(source), texture_box);
        }
    }
    const auto index = mUI.textures->currentRow();
    mUI.textures->setCurrentRow(index == -1 ? 0 : index);
}

void MaterialWidget::AddNewTextureMapFromText()
{
    // anything set in this text buffer will be default
    // when the dialog is opened.
    gfx::TextBuffer text(100, 100);

    DlgText dlg(this, text);

    if (dlg.exec() == QDialog::Rejected)
        return;

    // map the font files.
    for (size_t i=0; i<text.GetNumTexts(); ++i)
    {
        auto& style_and_text = text.GetText(i);
        style_and_text.font  = mWorkspace->MapFileToWorkspace(style_and_text.font);
    }

    auto source = std::make_unique<gfx::detail::TextureTextBufferSource>(std::move(text));
    source->SetName("TextBuffer");

    QListWidgetItem* item = new QListWidgetItem(mUI.textures);
    item->setText("TextBuffer");
    item->setData(Qt::UserRole, app::FromUtf8(source->GetId()));
    mUI.textures->addItem(item);

    mMaterial.AddTexture(std::move(source));

    const auto index = mUI.textures->currentRow();
    mUI.textures->setCurrentRow(index == -1 ? 0 : index);

}
void MaterialWidget::AddNewTextureMapFromBitmap()
{
    auto generator = std::make_unique<gfx::NoiseBitmapGenerator>();
    generator->SetWidth(100);
    generator->SetHeight(100);

    DlgBitmap dlg(this, std::move(generator));

    if (dlg.exec() == QDialog::Rejected)
        return;

    auto result = dlg.GetResult();
    auto source = std::make_unique<gfx::detail::TextureBitmapGeneratorSource>(std::move(result));
    source->SetName("Noise");
    QListWidgetItem* item = new QListWidgetItem(mUI.textures);
    item->setText("Noise");
    item->setData(Qt::UserRole, app::FromUtf8(source->GetId()));
    mUI.textures->addItem(item);

    mMaterial.AddTexture(std::move(source));
}

void MaterialWidget::SetMaterialProperties() const
{
    if (mUI.customShader->isChecked())
    {
        const QString& file = mUI.shaderFile->text();
        mMaterial.SetShader(app::ToUtf8(file));
    }
    else
    {
        mMaterial.SetShader("");
    }

    //mMaterial.SetShader(GetValue(mUI.materialType));
    mMaterial.SetBaseColor(GetValue(mUI.baseColor));
    mMaterial.SetColorMapColor(GetValue(mUI.colorMap0), gfx::MaterialClass::ColorIndex::TopLeft);
    mMaterial.SetColorMapColor(GetValue(mUI.colorMap1), gfx::MaterialClass::ColorIndex::TopRight);
    mMaterial.SetColorMapColor(GetValue(mUI.colorMap2), gfx::MaterialClass::ColorIndex::BottomLeft);
    mMaterial.SetColorMapColor(GetValue(mUI.colorMap3), gfx::MaterialClass::ColorIndex::BottomRight);
    mMaterial.SetGamma(GetValue(mUI.gamma));
    mMaterial.SetStatic(GetValue(mUI.staticInstance));
    mMaterial.SetSurfaceType(GetValue(mUI.surfaceType));
    mMaterial.SetParticleAction(GetValue(mUI.particleAction));
    mMaterial.SetFps(GetValue(mUI.fps));
    mMaterial.SetBlendFrames(GetValue(mUI.blend));
    mMaterial.SetTextureMinFilter(GetValue(mUI.minFilter));
    mMaterial.SetTextureMagFilter(GetValue(mUI.magFilter));
    mMaterial.SetTextureScaleX(GetValue(mUI.scaleX));
    mMaterial.SetTextureScaleY(GetValue(mUI.scaleY));
    mMaterial.SetTextureWrapX(GetValue(mUI.wrapX));
    mMaterial.SetTextureWrapY(GetValue(mUI.wrapY));
    mMaterial.SetTextureVelocityX(GetValue(mUI.velocityX));
    mMaterial.SetTextureVelocityY(GetValue(mUI.velocityY));
    mMaterial.SetTextureVelocityZ(GetValue(mUI.velocityZ));
}

void MaterialWidget::PaintScene(gfx::Painter& painter, double secs)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    SetMaterialProperties();

    gfx::Material material(mMaterial);
    material.SetRuntime(mTime);

    const auto zoom = mUI.zoom->value();
    const auto content_width  = width * zoom;
    const auto content_height = width * zoom;
    const auto xpos = (width - content_width) * 0.5f;
    const auto ypos = (height - content_height) * 0.5f;

    // use the material to fill a model shape in the middle of the screen.
    gfx::Transform transform;
    transform.MoveTo(xpos, ypos);
    transform.Resize(content_width, content_height);

    auto drawable = mWorkspace->MakeDrawableByName(GetValue(mUI.cmbModel));
    painter.Draw(*drawable, transform, material);
}

} // namespace
