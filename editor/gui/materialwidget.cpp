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
#  include <boost/algorithm/string/erase.hpp>
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
#include "editor/gui/drawing.h"
#include "editor/gui/dlgtext.h"
#include "editor/gui/dlgbitmap.h"
#include "editor/gui/uniform.h"
#include "editor/gui/sampler.h"
#include "editor/gui/dlgtexturerect.h"
#include "editor/gui/materialwidget.h"

namespace gui
{

MaterialWidget::MaterialWidget(app::Workspace* workspace)
{
    DEBUG("Create MaterialWidget");
    mWorkspace = workspace;
    mMaterial  = std::make_shared<gfx::ColorClass>(gfx::MaterialClass::Type::Color, base::RandomString(10));
    mMaterial->SetName("My Material");
    mOriginalHash = mMaterial->GetHash();

    mUI.setupUi(this);
    mUI.widget->onPaintScene = std::bind(&MaterialWidget::PaintScene, this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onZoomIn     = std::bind(&MaterialWidget::ZoomIn, this);
    mUI.widget->onZoomOut    = std::bind(&MaterialWidget::ZoomOut, this);
    mUI.actionPause->setEnabled(false);
    mUI.actionPlay->setEnabled(true);
    mUI.actionStop->setEnabled(false);

    QMenu* menu  = new QMenu(this);
    QAction* add_texture_from_file = menu->addAction(QIcon("icons:folder.png"), "File");
    QAction* add_texture_from_text = menu->addAction(QIcon("icons:text.png"), "Text");
    QAction* add_texture_from_bitmap = menu->addAction(QIcon("icons:bitmap.png"), "Bitmap");
    connect(add_texture_from_file, &QAction::triggered, this, &MaterialWidget::AddNewTextureMapFromFile);
    connect(add_texture_from_text, &QAction::triggered, this, &MaterialWidget::AddNewTextureMapFromText);
    connect(add_texture_from_bitmap, &QAction::triggered, this, &MaterialWidget::AddNewTextureMapFromBitmap);

    mUI.btnAddTextureMap->setMenu(menu);

    PopulateFromEnum<gfx::MaterialClass::MinTextureFilter>(mUI.minFilter);
    PopulateFromEnum<gfx::MaterialClass::MagTextureFilter>(mUI.magFilter);
    PopulateFromEnum<gfx::MaterialClass::TextureWrapping>(mUI.wrapX);
    PopulateFromEnum<gfx::MaterialClass::TextureWrapping>(mUI.wrapY);
    PopulateFromEnum<gfx::MaterialClass::SurfaceType>(mUI.surfaceType);
    PopulateFromEnum<gfx::MaterialClass::Type>(mUI.materialType);
    PopulateFromEnum<gfx::MaterialClass::ParticleAction>(mUI.particleAction);
    PopulateFromEnum<gfx::TextureMap::Type>(mUI.textureMapType);

    SetList(mUI.cmbModel, workspace->ListPrimitiveDrawables());
    SetValue(mUI.cmbModel, "Rectangle");
    SetValue(mUI.materialID, mMaterial->GetId());
    SetValue(mUI.materialName, mMaterial->GetName());
    setWindowTitle(GetValue(mUI.materialName));

    GetMaterialProperties();
    GetTextureMapProperties();
    GetTextureProperties();

    SetValue(mUI.zoom, 1.0f);

    mUI.scrollAreaWidgetContents->setMinimumSize(mUI.scrollAreaWidgetContents->sizeHint());
}

MaterialWidget::MaterialWidget(app::Workspace* workspace, const app::Resource& resource) : MaterialWidget(workspace)
{
    DEBUG("Editing material: '%1'", resource.GetName());
    mMaterial = resource.GetContent<gfx::MaterialClass>()->Copy();
    mOriginalHash = mMaterial->GetHash();
    SetValue(mUI.materialID, resource.GetId());
    SetValue(mUI.materialName, resource.GetName());
    GetUserProperty(resource, "model", mUI.cmbModel);
    GetUserProperty(resource, "zoom", mUI.zoom);
    GetUserProperty(resource, "widget", mUI.widget);
    GetUserProperty(resource, "time", mUI.kTime);

    ApplyShaderDescription();
    GetMaterialProperties();
    GetTextureMapProperties();
    GetTextureProperties();
}

MaterialWidget::~MaterialWidget()
{
    DEBUG("Destroy MaterialWidget");
}

QString MaterialWidget::GetId() const
{
    return GetValue(mUI.materialID);
}

void MaterialWidget::InitializeSettings(const UISettings& settings)
{
    SetValue(mUI.zoom, settings.zoom);
}

void MaterialWidget::SetViewerMode()
{
    SetVisible(mUI.baseProperties,    false);
    SetVisible(mUI.builtInProperties, false);
    SetVisible(mUI.gradientMap,       false);
    SetVisible(mUI.textureCoords,     false);
    SetVisible(mUI.customUniforms,    false);
    SetVisible(mUI.textureFilters,    false);
    SetVisible(mUI.textureMaps,       false);
    SetVisible(mUI.textureProp,       false);
    SetVisible(mUI.textureRect,       false);
    on_actionPlay_triggered();
}

void MaterialWidget::AddActions(QToolBar& bar)
{
    bar.addAction(mUI.actionPlay);
    bar.addAction(mUI.actionPause);
    bar.addSeparator();
    bar.addAction(mUI.actionStop);
    bar.addSeparator();
    bar.addAction(mUI.actionSave);
    bar.addSeparator();
    bar.addAction(mUI.actionReloadShaders);
    bar.addAction(mUI.actionReloadTextures);
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
    data::JsonObject json;
    settings.GetValue("Material", "content", &json);
    settings.GetValue("Material", "hash", &mOriginalHash);
    settings.LoadWidget("Material", mUI.materialName);
    settings.LoadWidget("Material", mUI.zoom);
    settings.LoadWidget("Material", mUI.cmbModel);
    settings.LoadWidget("Material", mUI.widget);
    settings.LoadWidget("Material", mUI.kTime);

    mMaterial = gfx::MaterialClass::ClassFromJson(json);
    if (!mMaterial)
    {
        WARN("Failed to restore material state.");
        mMaterial = std::make_shared<gfx::ColorClass>(gfx::MaterialClass::Type::Color, mMaterial->GetId());
    }

    ApplyShaderDescription();
    GetMaterialProperties();

    QString selected_item;
    if (settings.GetValue("Material", "selected_item", &selected_item))
        SelectItem(mUI.textures, ListItemId(selected_item));

    GetTextureMapProperties();
    GetTextureProperties();
    return true;
}

bool MaterialWidget::SaveState(Settings& settings) const
{
    data::JsonObject json;
    mMaterial->IntoJson(json);
    settings.SetValue("Material", "content", json);
    settings.SetValue("Material", "hash", mOriginalHash);
    settings.SaveWidget("Material", mUI.materialName);
    settings.SaveWidget("Material", mUI.zoom);
    settings.SaveWidget("Material", mUI.cmbModel);
    settings.SaveWidget("Material", mUI.widget);
    settings.SaveWidget("Material", mUI.kTime);
    if (auto* item = GetSelectedItem(mUI.textures))
        settings.SetValue("Material", "selected_item", (QString)GetItemId(item));
    return true;
}

bool MaterialWidget::CanTakeAction(Actions action, const Clipboard* clipboard) const
{
    switch (action)
    {
        case Actions::CanCut:
        case Actions::CanCopy:
        case Actions::CanPaste:
        case Actions::CanUndo:
            return false;
        case Actions::CanZoomIn:
            return CanZoomIn(mUI.zoom);
        case Actions::CanZoomOut:
            return CanZoomOut(mUI.zoom);
        case Actions::CanReloadTextures:
        case Actions::CanReloadShaders:
            return true;
    }
    BUG("Unhandled action query.");
    return false;
}

void MaterialWidget::ZoomIn()
{
    const float value = GetValue(mUI.zoom);
    SetValue(mUI.zoom, value + 0.1f);
}

void MaterialWidget::ZoomOut()
{
    const float value = GetValue(mUI.zoom);
    SetValue(mUI.zoom, value - 0.1f);
}

void MaterialWidget::ReloadShaders()
{
    mUI.widget->reloadShaders();
}
void MaterialWidget::ReloadTextures()
{
    mUI.widget->reloadTextures();

    GetTextureProperties();
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
    if (mOriginalHash != mMaterial->GetHash())
        return true;
    return false;
}

bool MaterialWidget::GetStats(Stats* stats) const
{
    stats->time  = mTime;
    stats->graphics.valid = true;
    stats->graphics.fps   = mUI.widget->getCurrentFPS();
    stats->graphics.vsync = mUI.widget->haveVSYNC();
    const auto& dev_stats = mUI.widget->getDeviceResourceStats();
    stats->device.static_vbo_mem_alloc    = dev_stats.static_vbo_mem_alloc;
    stats->device.static_vbo_mem_use      = dev_stats.static_vbo_mem_use;
    stats->device.dynamic_vbo_mem_alloc   = dev_stats.dynamic_vbo_mem_alloc;
    stats->device.dynamic_vbo_mem_use     = dev_stats.dynamic_vbo_mem_use;
    stats->device.streaming_vbo_mem_use   = dev_stats.streaming_vbo_mem_use;
    stats->device.streaming_vbo_mem_alloc = dev_stats.streaming_vbo_mem_alloc;
    return true;
}

void MaterialWidget::Render()
{
    mUI.widget->triggerPaint();
}

void MaterialWidget::on_widgetColor_colorChanged(QColor color)
{
    mUI.widget->SetClearColor(ToGfx(color));
}

void MaterialWidget::on_actionPlay_triggered()
{
    mState = PlayState::Playing;
    SetEnabled(mUI.actionPlay, false);
    SetEnabled(mUI.actionPause, true);
    SetEnabled(mUI.actionStop, true);
    SetEnabled(mUI.kTime, false);
}

void MaterialWidget::on_actionPause_triggered()
{
    mState = PlayState::Paused;
    SetEnabled(mUI.actionPlay, true);
    SetEnabled(mUI.actionPause, false);
    SetEnabled(mUI.actionStop, true);
}

void MaterialWidget::on_actionStop_triggered()
{
    mState = PlayState::Stopped;
    SetEnabled(mUI.actionPlay, true);
    SetEnabled(mUI.actionPause, false);
    SetEnabled(mUI.actionStop, false);
    SetEnabled(mUI.kTime, true);
    mTime = 0.0f;
}

void MaterialWidget::on_actionSave_triggered()
{
    if (!MustHaveInput(mUI.materialName))
        return;

    app::MaterialResource resource(mMaterial, GetValue(mUI.materialName));
    SetUserProperty(resource, "model", mUI.cmbModel);
    SetUserProperty(resource, "widget", mUI.widget);    
    SetUserProperty(resource, "zoom", mUI.zoom);
    SetUserProperty(resource, "time", mUI.kTime);

    mWorkspace->SaveResource(resource);
    mOriginalHash = mMaterial->GetHash();
}

void MaterialWidget::on_actionNewMap_triggered()
{
    auto map = std::make_unique<gfx::TextureMap>();
    if (mMaterial->GetType() == gfx::MaterialClass::Type::Sprite) {
        map->SetType(gfx::TextureMap::Type::Sprite);
        map->SetName("Sprite");
    } else if (mMaterial->GetType() == gfx::MaterialClass::Type::Texture) {
        map->SetType(gfx::TextureMap::Type::Texture2D);
        map->SetName("Texture");
    } else return;

    const auto maps = mMaterial->GetNumTextureMaps();
    mMaterial->SetNumTextureMaps(maps + 1);
    mMaterial->SetTextureMap(maps, std::move(map));
    GetMaterialProperties();
    SelectLastItem(mUI.textures);
    GetTextureMapProperties();
    GetTextureProperties();
}

void MaterialWidget::on_actionDelMap_triggered()
{
    if (const auto* map = GetSelectedTextureMap())
    {
        const auto index = mMaterial->FindTextureMapIndexById(map->GetId());
        if (index < mMaterial->GetNumTextureMaps())
            mMaterial->DeleteTextureMap(index);

        GetMaterialProperties();
        SelectLastItem(mUI.textures);
        GetTextureMapProperties();
        GetTextureProperties();
    }
}

void MaterialWidget::on_actionAddFile_triggered()
{
    AddNewTextureMapFromFile();
}
void MaterialWidget::on_actionAddText_triggered()
{
    AddNewTextureMapFromText();
}
void MaterialWidget::on_actionAddBitmap_triggered()
{
    AddNewTextureMapFromBitmap();
}
void MaterialWidget::on_actionEditTexture_triggered()
{
    on_btnEditTexture_clicked();
}

void MaterialWidget::on_actionRemoveTexture_triggered()
{
    // make sure currentRowChanged is blocked while we're deleting shit.
    QSignalBlocker block(mUI.textures);
    QList<QListWidgetItem*> items = mUI.textures->selectedItems();

    for (int i=0; i<items.size(); ++i)
    {
        mMaterial->DeleteTextureSrc(GetItemId(items[i]));
    }

    qDeleteAll(items);

    GetMaterialProperties();
    SelectLastItem(mUI.textures);
    GetTextureProperties();
    GetTextureMapProperties();
}

void MaterialWidget::on_actionReloadShaders_triggered()
{
    QString selected_item;
    if (const auto* item = GetSelectedItem(mUI.textures))
        selected_item = GetItemId(item);

    ReloadShaders();
    ApplyShaderDescription();
    GetMaterialProperties();
    SelectItem(mUI.textures, ListItemId(selected_item));
    GetTextureMapProperties();
    GetTextureProperties();

}
void MaterialWidget::on_actionReloadTextures_triggered()
{
    this->ReloadTextures();
}

void MaterialWidget::on_btnSelectShader_clicked()
{
    if (mMaterial->GetType() == gfx::MaterialClass::Type::Custom)
    {
        const auto& shader = QFileDialog::getOpenFileName(this,
            tr("Select Shader File"), "",
            tr("Shaders (*.glsl)"));
        if (shader.isEmpty())
            return;

        mMaterial->SetShaderUri(mWorkspace->MapFileToWorkspace(shader));
        ApplyShaderDescription();
        ReloadShaders();
        GetMaterialProperties();
    }
}

void MaterialWidget::on_btnCreateShader_clicked()
{
    if (mMaterial->GetType() == gfx::MaterialClass::Type::Custom)
    {
        QString name = GetValue(mUI.materialName);
        name.replace(' ', '_');
        const auto& glsl_uri = QString("ws://shaders/es2/%1.glsl").arg(name);
        const auto& json_uri = QString("ws://shaders/es2/%1.json").arg(name);
        const auto& glsl_file = mWorkspace->MapFileToFilesystem(glsl_uri);
        const auto& json_file = mWorkspace->MapFileToFilesystem(json_uri);

        const QString files[2] = {
            glsl_file, json_file
        };
        for (int i=0; i<2; ++i)
        {
            const auto& file = files[i];
            if (!FileExists(file))
                continue;

            QMessageBox msg(this);
            msg.setIcon(QMessageBox::Question);
            msg.setWindowTitle("File Exists");
            msg.setText(tr("A file by the same name already exists in the project folder.\n%1\nOverwrite file ?").arg(files[i]));
            msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            if (msg.exec() == QMessageBox::No)
                return;
        }
        const auto& path = mWorkspace->MapFileToFilesystem(QString("ws://shaders/es2"));
        if (!app::MakePath(path))
        {
            ERROR("Failed to create path. [path='%1']", path);
            QMessageBox msg(this);
            msg.setIcon(QMessageBox::Critical);
            msg.setWindowTitle("Filesystem Error");
            msg.setText(tr("Failed to create file system path.\n%1").arg(path));
            msg.exec();
            return;
        }
constexpr auto glsl = R"(
#version 100

precision highp float;

// build-in uniforms
// material time in seconds.
uniform float kTime;

// when rendering particles with points the material
// shader must sample gl_PointCoord for texture coordinates
// instead of the texcoord varying from the vertex shader.
// the value kRenderPoints will be set to 1.0 so for portability
// the material shader can do:
//   vec2 coords = mix(vTexCoord, gl_PointCoord, kRenderPoints);
uniform float kRenderPoints;

// custom uniforms that need to match the
// json description
uniform float kGamma;

uniform sampler2D kNoise;
uniform vec2 kNoiseRect;

// varyings from vertex stage.
varying vec2 vTexCoord;

// per particle data.
// these are only written when the drawable is a particle engine
varying float vParticleAlpha;
// particle random value.
varying float vParticleRandomValue;
// normalized particle lifetime.
varying float vParticleTime;

void main() {
    vec2 coords = mix(vTexCoord, gl_PointCoord, kRenderPoints);
    float a = texture2D(kNoise, coords).a;
    float r = coords.x + a + kTime;
    float g = coords.y + a;
    float b = kTime;
    vec3 col = vec3(0.5) + 0.5*cos(vec3(r, g, b));
    gl_FragColor = vec4(pow(col, vec3(kGamma)), 1.0);
}
)";
constexpr auto json = R"(
{
  "uniforms": [
     {
        "type": "Float",
        "name": "kGamma",
        "desc": "Gamma",
        "value": 2.2
     }
  ],
  "maps": [
     {
        "type": "Texture2D",
        "name": "kNoise",
        "rect": "kNoiseRect",
        "desc": "Noise"
     }
  ]
}
)";
        const char* content[2] = { glsl, json };

        for (int i=0; i<2; ++i)
        {
            QString error_str;
            QFile::FileError err_val = QFile::FileError::NoError;
            if (!app::WriteTextFile(files[i], content[i], &err_val, &error_str))
            {
                ERROR("Failed to write shader glsl file. [file='%1', error=%2]", files[i], error_str);
                QMessageBox msg(this);
                msg.setIcon(QMessageBox::Critical);
                msg.setWindowTitle("Filesystem Error");
                msg.setText(tr("Failed to write file.\n%1\n%2").arg(files[i]).arg(error_str));
                msg.exec();
                return;
            }
        }

        gfx::NoiseBitmapGenerator noise;
        noise.SetWidth(100);
        noise.SetHeight(100);
        // todo: fix this
        // the min/max prime indices need to be kept in sync with DlgBitmap!
        noise.Randomize(1, 1000, 3);

        auto tex = gfx::GenerateNoiseTexture(std::move(noise));
        tex->SetName("Noise");

        gfx::TextureMap2D map;
        map.SetType(gfx::TextureMap::Type::Texture2D);
        map.SetName("kNoise");
        map.SetNumTextures(1);
        map.SetTextureSource(0, std::move(tex));
        map.SetRectUniformName("kNoiseRect");
        map.SetSamplerName("kNoise");
        mMaterial->SetShaderUri(app::ToUtf8(glsl_uri));
        mMaterial->SetNumTextureMaps(1);
        mMaterial->SetTextureMap(0, std::move(map));

        ApplyShaderDescription();
        ReloadShaders();
        GetMaterialProperties();
    } // if
}

void MaterialWidget::on_btnEditShader_clicked()
{
    if (mMaterial->GetType() == gfx::MaterialClass::Type::Custom)
    {
        const auto& uri = mMaterial->GetShaderUri();
        if (uri.empty())
            return;
        const auto& glsl = mWorkspace->MapFileToFilesystem(uri);
        emit OpenExternalShader(glsl);
    }
}

void MaterialWidget::on_btnAddTextureMap_clicked()
{
    mUI.btnAddTextureMap->showMenu();
}

void MaterialWidget::on_btnResetTextureMap_clicked()
{
    if (auto* map = GetSelectedTextureMap())
    {
        map->SetNumTextures(0);

        GetMaterialProperties();
        SelectItem(mUI.textures, ListItemId(map->GetId()));
        GetTextureMapProperties();
        GetTextureProperties();
    }
}

void MaterialWidget::on_btnEditTexture_clicked()
{
    if (auto* source = GetSelectedTextureSrc())
    {
        if (const auto* ptr = dynamic_cast<const gfx::detail::TextureFileSource*>(source))
        {
            emit OpenExternalImage(app::FromUtf8(ptr->GetFilename()));
        }
        else if (auto* ptr = dynamic_cast<gfx::detail::TextureTextBufferSource*>(source))
        {
            // make a copy for editing.
            gfx::TextBuffer text(ptr->GetTextBuffer());
            DlgText dlg(this, text);
            if (dlg.exec() == QDialog::Rejected)
                return;

            // map the font files.
            auto& style_and_text = text.GetText();
            style_and_text.font = mWorkspace->MapFileToWorkspace(style_and_text.font);

            // Update the texture source's TextBuffer
            ptr->SetTextBuffer(std::move(text));

            // update the preview.
            GetTextureProperties();
        }
        else if (auto* ptr = dynamic_cast<gfx::detail::TextureBitmapGeneratorSource*>(source))
        {
            // make a copy for editing.
            auto copy = ptr->GetGenerator().Clone();
            DlgBitmap dlg(this, std::move(copy));
            if (dlg.exec() == QDialog::Rejected)
                return;

            // override the generator with changes.
            ptr->SetGenerator(dlg.GetResult());

            // update the preview.
            GetTextureProperties();
        }
    }
}

void MaterialWidget::on_btnResetTextureRect_clicked()
{
    if (auto* src = GetSelectedTextureSrc())
    {
        mMaterial->SetTextureRect(src->GetId(), gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));

        GetTextureProperties();
    }
}

void MaterialWidget::on_btnSelectTextureRect_clicked()
{
    if (auto* src = GetSelectedTextureSrc())
    {
        auto rect = mMaterial->FindTextureRect(src->GetId());
        DlgTextureRect dlg(this, mWorkspace, rect, src->Clone());
        if (dlg.exec() == QDialog::Rejected)
            return;

        mMaterial->SetTextureRect(src->GetId(), dlg.GetRect());

        GetTextureProperties();
    }
}

void MaterialWidget::on_textures_itemSelectionChanged()
{
    GetTextureProperties();
    GetTextureMapProperties();
}

void MaterialWidget::on_textures_customContextMenuRequested(const QPoint&)
{
    const auto* item = GetSelectedItem(mUI.textures);
    const auto& tag  = (QString)GetItemTag(item);
    SetEnabled(mUI.actionDelMap,    tag == "map");
    SetEnabled(mUI.actionAddFile,   tag == "map");
    SetEnabled(mUI.actionAddText,   tag == "map");
    SetEnabled(mUI.actionAddBitmap, tag == "map");
    SetEnabled(mUI.actionRemoveTexture, tag == "texture");
    SetEnabled(mUI.actionEditTexture,   tag == "texture");

    QMenu menu(this);
    menu.addAction(mUI.actionNewMap);
    menu.addAction(mUI.actionDelMap);
    menu.addSeparator();
    menu.addAction(mUI.actionAddFile);
    menu.addAction(mUI.actionAddText);
    menu.addAction(mUI.actionAddBitmap);
    menu.addSeparator();
    menu.addAction(mUI.actionEditTexture);
    menu.addAction(mUI.actionRemoveTexture);
    menu.exec(QCursor::pos());
}

void MaterialWidget::on_materialName_textChanged(const QString& text)
{
    mMaterial->SetName(GetValue(mUI.materialName));
}

void MaterialWidget::on_materialType_currentIndexChanged(int)
{
    const gfx::MaterialClass::Type type = GetValue(mUI.materialType);
    if (type == mMaterial->GetType())
        return;

    gfx::MaterialClass other(type, mMaterial->GetId());
    if (type == gfx::MaterialClass::Type::Texture)
    {
        auto map = std::make_unique<gfx::TextureMap>();
        map->SetType(gfx::TextureMap::Type::Texture2D);
        map->SetName("Texture");
        other.SetActiveTextureMap(map->GetId());
        other.SetNumTextureMaps(1);
        other.SetTextureMap(0, std::move(map));
    }
    else if (type == gfx::MaterialClass::Type::Sprite)
    {
        auto map = std::make_unique<gfx::TextureMap>();
        map->SetType(gfx::TextureMap::Type::Sprite);
        map->SetName("Sprite");
        map->SetFps(10.0f);
        other.SetActiveTextureMap(map->GetId());
        other.SetNumTextureMaps(1);
        other.SetTextureMap(0, std::move(map));
    }
    *mMaterial = other;

    ClearCustomUniforms();
    GetMaterialProperties();
    GetTextureMapProperties();
    GetTextureProperties();
}
void MaterialWidget::on_surfaceType_currentIndexChanged(int)
{ SetMaterialProperties(); }
void MaterialWidget::on_activeMap_currentIndexChanged(int)
{ SetMaterialProperties(); }
void MaterialWidget::on_gamma_valueChanged(double)
{ SetMaterialProperties(); }
void MaterialWidget::on_baseColor_colorChanged(QColor color)
{ SetMaterialProperties(); }
void MaterialWidget::on_particleAction_currentIndexChanged(int)
{ SetMaterialProperties(); }
void MaterialWidget::on_textureMapFps_valueChanged(double)
{ SetMaterialProperties(); }
void MaterialWidget::on_chkBlendPreMulAlpha_stateChanged(int)
{ SetMaterialProperties(); }
void MaterialWidget::on_chkStaticInstance_stateChanged(int)
{ SetMaterialProperties(); }
void MaterialWidget::on_chkBlendFrames_stateChanged(int)
{ SetMaterialProperties(); }
void MaterialWidget::on_chkLooping_stateChanged(int)
{ SetMaterialProperties(); }
void MaterialWidget::on_spriteCols_valueChanged(int)
{ SetMaterialProperties(); }
void MaterialWidget::on_spriteRows_valueChanged(int)
{ SetMaterialProperties(); }
void MaterialWidget::on_spriteSheet_toggled(bool)
{
    SetMaterialProperties();
    if (GetValue(mUI.spriteSheet))
    {
        SetEnabled(mUI.spriteRows, true);
        SetEnabled(mUI.spriteCols, true);
    }
    else
    {
        SetEnabled(mUI.spriteRows, false);
        SetEnabled(mUI.spriteCols, false);
    }
}
void MaterialWidget::on_colorMap0_colorChanged(QColor)
{ SetMaterialProperties(); }
void MaterialWidget::on_colorMap1_colorChanged(QColor)
{ SetMaterialProperties(); }
void MaterialWidget::on_colorMap2_colorChanged(QColor)
{ SetMaterialProperties(); }
void MaterialWidget::on_colorMap3_colorChanged(QColor)
{ SetMaterialProperties(); }
void MaterialWidget::on_gradientOffsetX_valueChanged(int value)
{ SetMaterialProperties(); }
void MaterialWidget::on_gradientOffsetY_valueChanged(int value)
{ SetMaterialProperties(); }
void MaterialWidget::on_scaleX_valueChanged(double)
{ SetMaterialProperties(); }
void MaterialWidget::on_scaleY_valueChanged(double)
{ SetMaterialProperties(); }
void MaterialWidget::on_rotation_valueChanged(double)
{ SetMaterialProperties(); }
void MaterialWidget::on_velocityX_valueChanged(double)
{ SetMaterialProperties(); }
void MaterialWidget::on_velocityY_valueChanged(double)
{ SetMaterialProperties(); }
void MaterialWidget::on_velocityZ_valueChanged(double)
{ SetMaterialProperties(); }
void MaterialWidget::on_minFilter_currentIndexChanged(int)
{ SetMaterialProperties(); }
void MaterialWidget::on_magFilter_currentIndexChanged(int)
{ SetMaterialProperties(); }
void MaterialWidget::on_wrapX_currentIndexChanged(int)
{ SetMaterialProperties(); }
void MaterialWidget::on_wrapY_currentIndexChanged(int)
{ SetMaterialProperties(); }
void MaterialWidget::on_rectX_valueChanged(double value)
{ SetTextureRect(); }
void MaterialWidget::on_rectY_valueChanged(double value)
{ SetTextureRect(); }
void MaterialWidget::on_rectW_valueChanged(double value)
{ SetTextureRect(); }
void MaterialWidget::on_rectH_valueChanged(double value)
{ SetTextureRect(); }
void MaterialWidget::on_chkAllowPacking_stateChanged(int)
{ SetTextureFlags(); }
void MaterialWidget::on_chkAllowResizing_stateChanged(int)
{ SetTextureFlags(); }
void MaterialWidget::on_chkPreMulAlpha_stateChanged(int)
{ SetTextureFlags(); }
void MaterialWidget::on_chkBlurTexture_stateChanged(int)
{ SetTextureFlags(); }

void MaterialWidget::on_textureMapName_textChanged(const QString& text)
{
    if (auto* item = GetSelectedItem(mUI.textures))
    {
        if (auto* map = mMaterial->FindTextureMapById(GetItemId(item)))
        {
            map->SetName(GetValue(mUI.textureMapName));
            item->setText(GetValue(mUI.textureMapName));
            SetText(mUI.activeMap, ListItemId((QString)GetItemId(item)), GetValue(mUI.textureMapName));
        }
    }
}

void MaterialWidget::on_textureSourceName_textChanged(const QString& text)
{
    if (auto* item = GetSelectedItem(mUI.textures))
    {
        if (auto* source = mMaterial->FindTextureSource(GetItemId(item)))
        {
            source->SetName(GetValue(mUI.textureSourceName));
            item->setText("    " + GetValue(mUI.textureSourceName));
        }
    }
}

void MaterialWidget::on_findMap_textChanged(const QString& needle)
{
    const auto current = GetCurrentRow(mUI.textures);
    const auto count   = GetCount(mUI.textures);
    for (unsigned i=0; i<count; ++i)
    {
        const auto index = (current + i) % count;
        const auto* item = mUI.textures->item(index);
        const auto& haystack = (QString) GetItemText(item);
        if (haystack.contains(needle, Qt::CaseInsensitive))
        {
            SelectItem(mUI.textures, index);
            GetTextureMapProperties();
            GetTextureProperties();
            mUI.textures->scrollToItem(item);
            return;
        }
    }
}

void MaterialWidget::AddNewTextureMapFromFile()
{
    const auto& images = QFileDialog::getOpenFileNames(this,
            tr("Select Image File"), "",
            tr("Images (*.png *.jpg *.jpeg)"));
    if (images.isEmpty())
        return;

    auto* map = GetSelectedTextureMap();
    if (map == nullptr)
        return;

    for (const auto& image : images)
    {
        const QFileInfo info(image);
        const auto& name = info.baseName();
        const auto& file = mWorkspace->MapFileToWorkspace(info.absoluteFilePath());

        auto source = std::make_unique<gfx::detail::TextureFileSource>(file);
        source->SetName(app::ToUtf8(name));
        source->SetFileName(file);
        source->SetColorSpace(gfx::detail::TextureFileSource::ColorSpace::Linear);

        if (map->GetType() == gfx::TextureMap::Type::Texture2D)
        {
            map->SetNumTextures(1);
            map->SetTextureSource(0, std::move(source));
            map->SetTextureRect(0, gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
        }
        else if (map->GetType() == gfx::TextureMap::Type::Sprite)
        {
            const auto textures = map->GetNumTextures();
            map->SetNumTextures(textures + 1);
            map->SetTextureSource(textures, std::move(source));
            map->SetTextureRect(textures, gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
        }
    }

    GetMaterialProperties();
    SelectLastItem(mUI.textures);
    GetTextureMapProperties();
    GetTextureProperties();
}

void MaterialWidget::AddNewTextureMapFromText()
{
    auto* map = GetSelectedTextureMap();
    if (map == nullptr)
        return;

    // anything set in this text buffer will be default
    // when the dialog is opened.
    gfx::TextBuffer text(100, 100);
    DlgText dlg(this, text);
    if (dlg.exec() == QDialog::Rejected)
        return;

    // map the selected font files to the workspace.
    auto& style_and_text = text.GetText();
    style_and_text.font = mWorkspace->MapFileToWorkspace(style_and_text.font);

    auto source = std::make_unique<gfx::detail::TextureTextBufferSource>(std::move(text));
    source->SetName("TextBuffer");

    if (map->GetType() == gfx::TextureMap::Type::Texture2D)
    {
        map->SetNumTextures(1);
        map->SetTextureSource(0, std::move(source));
        map->SetTextureRect(0, gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
    }
    else if (map->GetType() == gfx::TextureMap::Type::Sprite)
    {
        auto textures = map->GetNumTextures();
        map->SetNumTextures(textures + 1);
        map->SetTextureSource(textures, std::move(source));
        map->SetTextureRect(0, gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
    }

    GetMaterialProperties();
    SelectLastItem(mUI.textures);
    GetTextureMapProperties();
    GetTextureProperties();
}

void MaterialWidget::AddNewTextureMapFromBitmap()
{
    auto* map = GetSelectedTextureMap();
    if (map == nullptr)
        return;

    auto generator = std::make_unique<gfx::NoiseBitmapGenerator>();
    generator->SetWidth(100);
    generator->SetHeight(100);
    DlgBitmap dlg(this, std::move(generator));
    if (dlg.exec() == QDialog::Rejected)
        return;

    auto result = dlg.GetResult();
    auto source = std::make_unique<gfx::detail::TextureBitmapGeneratorSource>(std::move(result));
    source->SetName("Noise");

    if (map->GetType() == gfx::TextureMap::Type::Texture2D)
    {
        map->SetNumTextures(1);
        map->SetTextureSource(0, std::move(source));
        map->SetTextureRect(0, gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
    }
    else if (map->GetType() == gfx::TextureMap::Type::Sprite)
    {
        auto textures = map->GetNumTextures();
        map->SetNumTextures(textures + 1);
        map->SetTextureSource(textures, std::move(source));
        map->SetTextureRect(0, gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
    }

    GetMaterialProperties();
    SelectLastItem(mUI.textures);
    GetTextureMapProperties();
    GetTextureProperties();
}
void MaterialWidget::UniformValueChanged(const Uniform* uniform)
{
    //DEBUG("Uniform '%1' was modified.", uniform->GetName());
    SetMaterialProperties();
}

void MaterialWidget::ClearCustomUniforms()
{
    // sometimes Qt just f*n hates you. There's no simple/easy way to
    // just recreate a layout with a bunch of widgets! You cannot set a
    // layout when a layout already exists on a widget but deleting it
    // doesn't work as expected either!!
    // https://stackoverflow.com/questions/4272196/qt-remove-all-widgets-from-layout
    if (auto* layout = mUI.customUniforms->layout())
    {
        while (auto* item = layout->takeAt(0))
        {
            delete item->widget();
            delete item;
        }
    }
    mUniforms.clear();
}


void MaterialWidget::ApplyShaderDescription()
{
    ClearCustomUniforms();

    if (mMaterial->GetType() != gfx::MaterialClass::Type::Custom)
        return;

    // try to load the .json file that should contain the meta information
    // about the shader input parameters.
    auto uri = mMaterial->GetShaderUri();
    if (uri.empty())
    {
        ERROR("Empty material shader uri.");
        return;
    }

    boost::replace_all(uri, ".glsl", ".json");
    const auto [parse_success, json, error] = base::JsonParseFile(mWorkspace->MapFileToFilesystem(uri));
    if (!parse_success)
    {
        ERROR("Failed to parse the shader description file '%1' %2", uri, error);
        return;
    }

    if (json.contains("uniforms"))
    {
        if (!mUI.customUniforms->layout())
            mUI.customUniforms->setLayout(new QGridLayout);
        auto* layout = qobject_cast<QGridLayout*>(mUI.customUniforms->layout());

        auto uniforms = mMaterial->GetUniforms();
        auto widget_row = 0;
        auto widget_col = 0;
        for (const auto& json : json["uniforms"].items())
        {
            Uniform::Type type = Uniform::Type::Float;
            std::string name = "kUniform";
            std::string desc = "Uniform";
            if (!base::JsonReadSafe(json.value(), "desc", &desc))
                WARN("Uniform is missing 'desc' parameter.");
            if (!base::JsonReadSafe(json.value(), "name", &name))
                WARN("Uniform is missing 'name' parameter.");
            if (!base::JsonReadSafe(json.value(), "type", &type))
                WARN("Uniform is missing 'type' parameter or the type is not understood.");

            auto* label = new QLabel(this);
            SetValue(label, desc);
            layout->addWidget(label, widget_row, 0);

            auto* widget = new Uniform(this);
            connect(widget, &Uniform::ValueChanged, this, &MaterialWidget::UniformValueChanged);
            widget->SetType(type);
            widget->SetName(app::FromUtf8(name));
            layout->addWidget(widget, widget_row, 1);
            mUniforms.push_back(widget);

            widget_row++;
            DEBUG("Read uniform description '%1'", name);
            uniforms.erase(name);

            // if the uniform already exists *and* has the matching type then
            // don't reset anything.
            if (type == Uniform::Type::Float && mMaterial->HasUniform<float>(name) ||
                type == Uniform::Type::Vec2  && mMaterial->HasUniform<glm::vec2>(name) ||
                type == Uniform::Type::Vec3  && mMaterial->HasUniform<glm::vec3>(name) ||
                type == Uniform::Type::Vec4  && mMaterial->HasUniform<glm::vec4>(name) ||
                type == Uniform::Type::Color && mMaterial->HasUniform<gfx::Color4f>(name) ||
                type == Uniform::Type::Int   && mMaterial->HasUniform<int>(name))
                continue;

            // set default uniform value if it doesn't exist already.
            if (type == Uniform::Type::Float)
            {
                float value = 0.0f;
                base::JsonReadSafe(json.value(), "value", &value);
                mMaterial->SetUniform(name, value);
            }
            else if (type == Uniform::Type::Vec2)
            {
                glm::vec2 value = {0.0f, 0.0f};
                base::JsonReadSafe(json.value(), "value", &value);
                mMaterial->SetUniform(name, value);
            }
            else if (type == Uniform::Type::Vec3)
            {
                glm::vec3 value = {0.0f, 0.0f, 0.0f};
                base::JsonReadSafe(json.value(), "value", &value);
                mMaterial->SetUniform(name, value);
            }
            else if (type == Uniform::Type::Vec4)
            {
                glm::vec4 value = {0.0f, 0.0f, 0.0f, 0.0f};
                base::JsonReadSafe(json.value(), "value", &value);
                mMaterial->SetUniform(name, value);
            }
            else if (type == Uniform::Type::Color)
            {
                gfx::Color4f value = gfx::Color::White;
                base::JsonReadSafe(name, "value", &value);
                mMaterial->SetUniform(name, value);
            }
            else if (type == Uniform::Type::Int)
            {
                int value = 0;
                base::JsonReadSafe(name, "value", &value);
                mMaterial->SetUniform(name, value);
            }
            else BUG("Unhandled uniform type.");
        }
        // delete the material uniforms that were no longer in the description
        for (auto pair : uniforms)
        {
            mMaterial->DeleteUniform(pair.first);
        }
    }
    else
    {
        mMaterial->DeleteUniforms();
    }

    if (json.contains("maps"))
    {
        std::set<std::string> texture_map_names;
        for (size_t i=0; i<mMaterial->GetNumTextureMaps(); ++i)
        {
            const auto* map = mMaterial->GetTextureMap(i);
            texture_map_names.insert(map->GetName());
        }

        auto widget_row = 0;
        auto widget_col = 0;
        for (const auto& json : json["maps"].items())
        {
            std::string desc = "Texture";
            std::string name = "kTexture";
            gfx::TextureMap::Type type = gfx::TextureMap::Type::Texture2D;
            if (!base::JsonReadSafe(json.value(), "desc", &desc))
                WARN("Texture map is missing 'desc' parameter.");
            if (!base::JsonReadSafe(json.value(), "name", &name))
                WARN("Texture map is missing 'name' parameter.");
            if (!base::JsonReadSafe(json.value(), "type", &type))
                WARN("Texture map is missing 'type' parameter or the type is not understood.");

            DEBUG("Read texture map description '%1'", name);
            texture_map_names.erase(name);

            // create new texture map and add to the material
            if (type == gfx::TextureMap::Type::Texture2D)
            {
                std::string sampler_name = name; //"kTexture";
                std::string texture_rect_uniform_name = name + "Rect"; //"kTextureRect";
                if (!base::JsonReadSafe(json.value(), "sampler", &sampler_name))
                    WARN("Texture map '%1' has no name for texture sampler. Using '%2'", name, sampler_name);
                if (!base::JsonReadSafe(json.value(), "rect", &texture_rect_uniform_name))
                    WARN("Texture map '%1' has no name for texture rectangle uniform. Using '%2'.", name, texture_rect_uniform_name);

                gfx::TextureMap* map = nullptr;
                const auto map_index = mMaterial->FindTextureMapIndexByName(name);
                if (map_index == mMaterial->GetNumTextureMaps())
                {
                    auto new_map = std::make_unique<gfx::TextureMap2D>();
                    mMaterial->SetNumTextureMaps(map_index + 1);
                    mMaterial->SetTextureMap(map_index, std::move(new_map));
                    map = mMaterial->GetTextureMap(map_index);
                    DEBUG("Added material texture map. [type=%1, name='%2']", type, name);
                }
                else map = mMaterial->GetTextureMap(map_index);

                map->SetType(gfx::TextureMap::Type::Texture2D);
                map->SetName(name);
                map->SetRectUniformName(std::move(texture_rect_uniform_name));
                map->SetSamplerName(std::move(sampler_name));
            }
            else if (type == gfx::TextureMap::Type::Sprite)
            {
                std::string sampler_name0 = name + "0"; //kTexture0";
                std::string sampler_name1 = name + "1"; //"kTexture1";
                std::string texture_rect_uniform_name0 = name + "Rect0";
                std::string texture_rect_uniform_name1 = name + "Rect1";
                if (!base::JsonReadSafe(json.value(), "sampler0", &sampler_name0))
                    WARN("Texture map '%1' has no name for texture sampler 0. Using '%2'.", name, sampler_name0);
                if (!base::JsonReadSafe(json.value(), "sampler1", &sampler_name1))
                    WARN("Texture map '%1' has no name for texture sampler 1. Using '%2'.", name, sampler_name1);
                if (!base::JsonReadSafe(json.value(), "rect0", &texture_rect_uniform_name0))
                    WARN("Texture map '%1' has no name for texture 0 rectangle uniform. Using '%2'.", name, texture_rect_uniform_name0);
                if (!base::JsonReadSafe(json.value(), "rect1", &texture_rect_uniform_name1))
                    WARN("Texture map '%1' has no name for texture 1 rectangle uniform. Using '%2'.", name, texture_rect_uniform_name1);

                gfx::TextureMap* map = nullptr;
                const auto map_index = mMaterial->FindTextureMapIndexByName(name);
                if (map_index == mMaterial->GetNumTextureMaps())
                {
                    auto new_map = std::make_unique<gfx::TextureMap2D>();
                    mMaterial->SetNumTextureMaps(map_index + 1);
                    mMaterial->SetTextureMap(map_index, std::move(new_map));
                    map = mMaterial->GetTextureMap(map_index);
                    DEBUG("Added material texture map. [type=%1, name='%2']", type, name);
                }
                else map = mMaterial->GetTextureMap(map_index);

                map->SetType(gfx::TextureMap::Type::Sprite);
                map->SetSamplerName(sampler_name0, 0);
                map->SetSamplerName(sampler_name1, 1);
                map->SetRectUniformName(texture_rect_uniform_name0, 0);
                map->SetRectUniformName(texture_rect_uniform_name1, 1);
            }
            else BUG("Unhandled texture map type.");
        }
        // delete texture maps that were no longer in the description from the material
        for (const auto& carcass_texture_map_name : texture_map_names)
        {
            const auto index = mMaterial->FindTextureMapIndexByName(carcass_texture_map_name);
            if (index != mMaterial->GetNumTextureMaps())
                mMaterial->DeleteTextureMap(index);
        }
    }
    else
    {
        mMaterial->SetNumTextureMaps(0);
    }
    INFO("Loaded shader description '%1'", uri);
}

void MaterialWidget::SetTextureRect()
{
    if (auto* src = GetSelectedTextureSrc())
    {
        const gfx::FRect rect(GetValue(mUI.rectX),
                              GetValue(mUI.rectY),
                              GetValue(mUI.rectW),
                              GetValue(mUI.rectH));
        mMaterial->SetTextureRect(src->GetId(), rect);
    }
}

void MaterialWidget::SetTextureFlags()
{
    if (auto* source = GetSelectedTextureSrc())
    {
        if (auto* ptr = dynamic_cast<gfx::detail::TextureFileSource*>(source))
        {
            ptr->SetFlag(gfx::detail::TextureFileSource::Flags::AllowPacking, GetValue(mUI.chkAllowPacking));
            ptr->SetFlag(gfx::detail::TextureFileSource::Flags::AllowResizing, GetValue(mUI.chkAllowResizing));
            ptr->SetFlag(gfx::detail::TextureFileSource::Flags::PremulAlpha, GetValue(mUI.chkPreMulAlpha));
            ptr->SetEffect(gfx::TextureSource::Effect::Blur, GetValue(mUI.chkBlurTexture));
        }
        else if (auto* ptr = dynamic_cast<gfx::detail::TextureTextBufferSource*>(source))
        {
            ptr->SetEffect(gfx::TextureSource::Effect::Blur, GetValue(mUI.chkBlurTexture));
        }
    }
}

void MaterialWidget::SetMaterialProperties()
{
    mMaterial->SetFlag(gfx::MaterialClass::Flags::PremultipliedAlpha, GetValue(mUI.chkBlendPreMulAlpha));
    mMaterial->SetStatic(GetValue(mUI.chkStaticInstance));
    mMaterial->SetSurfaceType(GetValue(mUI.surfaceType));
    mMaterial->SetParticleAction(GetValue(mUI.particleAction));
    mMaterial->SetGamma(GetValue(mUI.gamma));
    mMaterial->SetTextureMinFilter(GetValue(mUI.minFilter));
    mMaterial->SetTextureMagFilter(GetValue(mUI.magFilter));
    mMaterial->SetTextureWrapX(GetValue(mUI.wrapX));
    mMaterial->SetTextureWrapY(GetValue(mUI.wrapY));
    mMaterial->SetTextureScaleX(GetValue(mUI.scaleX));
    mMaterial->SetTextureScaleY(GetValue(mUI.scaleY));
    mMaterial->SetTextureRotation(GetAngle(mUI.rotation));
    mMaterial->SetTextureVelocityX(GetValue(mUI.velocityX));
    mMaterial->SetTextureVelocityY(GetValue(mUI.velocityY));
    mMaterial->SetTextureVelocityZ(GetValue(mUI.velocityZ));
    mMaterial->SetBlendFrames(GetValue(mUI.chkBlendFrames));
    mMaterial->SetActiveTextureMap(GetItemId(mUI.activeMap));

    if (mMaterial->GetType() == gfx::MaterialClass::Type::Gradient)
    {
        mMaterial->SetColor(GetValue(mUI.colorMap0), gfx::GradientClass::ColorIndex::TopLeft);
        mMaterial->SetColor(GetValue(mUI.colorMap1), gfx::GradientClass::ColorIndex::TopRight);
        mMaterial->SetColor(GetValue(mUI.colorMap2), gfx::GradientClass::ColorIndex::BottomLeft);
        mMaterial->SetColor(GetValue(mUI.colorMap3), gfx::GradientClass::ColorIndex::BottomRight);
    }
    else mMaterial->SetColor(GetValue(mUI.baseColor), gfx::MaterialClass::ColorIndex::BaseColor);

    glm::vec2 offset;
    offset.x = GetNormalizedValue(mUI.gradientOffsetX);
    offset.y = GetNormalizedValue(mUI.gradientOffsetY);
    mMaterial->SetColorWeight(offset);

    for (auto* widget : mUniforms)
    {
        const auto& name = app::ToUtf8(widget->GetName());
        if (auto* val = mMaterial->GetUniformValue<float>(name))
            *val = widget->GetAsFloat();
        else if (auto* val = mMaterial->GetUniformValue<glm::vec2>(name))
            *val = widget->GetAsVec2();
        else if (auto* val = mMaterial->GetUniformValue<glm::vec3>(name))
            *val = widget->GetAsVec3();
        else if (auto* val = mMaterial->GetUniformValue<glm::vec4>(name))
            *val = widget->GetAsVec4();
        else if (auto* val = mMaterial->GetUniformValue<gfx::Color4f>(name))
            *val = ToGfx(widget->GetAsColor());
        else if (auto* val = mMaterial->GetUniformValue<int>(name))
            *val = widget->GetAsInt();
        else BUG("No such uniform in material. UI and material are out of sync.");
    }

    if (auto* map = GetSelectedTextureMap())
    {
        map->SetFps(GetValue(mUI.textureMapFps));
        map->SetLooping(GetValue(mUI.chkLooping));
        if (GetValue(mUI.spriteSheet))
        {
            gfx::TextureMap::SpriteSheet sheet;
            sheet.rows = GetValue(mUI.spriteRows);
            sheet.cols = GetValue(mUI.spriteCols);
            map->SetSpriteSheet(sheet);
        } else map->ResetSpriteSheet();
    }
}

void MaterialWidget::GetMaterialProperties()
{
    SetEnabled(mUI.shaderFile,         false);
    SetEnabled(mUI.btnSelectShader,    false);
    SetEnabled(mUI.btnCreateShader,    false);
    SetEnabled(mUI.btnEditShader,      false);
    SetEnabled(mUI.baseColor,          false);
    SetEnabled(mUI.particleAction,     false);
    SetEnabled(mUI.activeMap,          false);
    SetEnabled(mUI.textureMaps,        false);
    SetEnabled(mUI.textureMap,         false);
    SetEnabled(mUI.textureProp,        false);
    SetEnabled(mUI.textureRect,        false);

    SetVisible(mUI.builtInProperties,  false);
    SetVisible(mUI.gradientMap,        false);
    SetVisible(mUI.textureCoords,      false);
    SetVisible(mUI.textureFilters,     false);
    SetVisible(mUI.customUniforms,     false);
    SetVisible(mUI.chkBlendPreMulAlpha,false);
    SetVisible(mUI.chkStaticInstance,  false);
    SetVisible(mUI.chkBlendFrames,     false);

    SetValue(mUI.materialID,          mMaterial->GetId());
    SetValue(mUI.materialType,        mMaterial->GetType());
    SetValue(mUI.surfaceType,         mMaterial->GetSurfaceType());
    SetValue(mUI.shaderFile,          mMaterial->GetShaderUri());
    SetValue(mUI.chkStaticInstance,   mMaterial->IsStatic());
    SetValue(mUI.chkBlendPreMulAlpha, mMaterial->PremultipliedAlpha());
    SetValue(mUI.chkBlendFrames,      mMaterial->BlendFrames());
    SetValue(mUI.gamma,               mMaterial->GetGamma());
    SetValue(mUI.baseColor,           mMaterial->GetBaseColor());
    SetValue(mUI.particleAction,      mMaterial->GetParticleAction());
    SetValue(mUI.scaleX,              mMaterial->GetTextureScaleX());
    SetValue(mUI.scaleY,              mMaterial->GetTextureScaleY());
    SetValue(mUI.rotation,            mMaterial->GetTextureRotation());
    SetValue(mUI.velocityX,           mMaterial->GetTextureVelocityX());
    SetValue(mUI.velocityY,           mMaterial->GetTextureVelocityY());
    SetValue(mUI.velocityZ,           mMaterial->GetTextureVelocityZ());
    SetValue(mUI.minFilter,           mMaterial->GetTextureMinFilter());
    SetValue(mUI.magFilter,           mMaterial->GetTextureMagFilter());
    SetValue(mUI.wrapX,               mMaterial->GetTextureWrapX());
    SetValue(mUI.wrapY,               mMaterial->GetTextureWrapY());
    ClearList(mUI.activeMap);

    if (mMaterial->GetType() == gfx::MaterialClass::Type::Custom)
    {
        SetEnabled(mUI.shaderFile,      true);
        SetEnabled(mUI.btnSelectShader, true);
        SetEnabled(mUI.btnCreateShader, true);
        SetEnabled(mUI.btnEditShader,   true);
    }
    else
    {
        SetVisible(mUI.builtInProperties,   true);
        SetEnabled(mUI.chkStaticInstance,   true);
        SetVisible(mUI.chkStaticInstance,   true);

        if (mMaterial->GetType() == gfx::MaterialClass::Type::Gradient)
        {
            const auto& offset = mMaterial->GetColorWeight();
            SetValue(mUI.colorMap0, mMaterial->GetColor(gfx::MaterialClass::ColorIndex::TopLeft));
            SetValue(mUI.colorMap1, mMaterial->GetColor(gfx::MaterialClass::ColorIndex::TopRight));
            SetValue(mUI.colorMap2, mMaterial->GetColor(gfx::MaterialClass::ColorIndex::BottomLeft));
            SetValue(mUI.colorMap3, mMaterial->GetColor(gfx::MaterialClass::ColorIndex::BottomRight));
            SetValue(mUI.gradientOffsetY, NormalizedFloat(offset.y));
            SetValue(mUI.gradientOffsetX, NormalizedFloat(offset.x));
            SetVisible(mUI.gradientMap, true);
        }
        else
        {
            SetEnabled(mUI.baseColor, true);
            if (mMaterial->GetType() == gfx::MaterialClass::Type::Texture ||
                mMaterial->GetType() == gfx::MaterialClass::Type::Sprite)
            {
                SetEnabled(mUI.particleAction, true);
                SetEnabled(mUI.activeMap,      true);
            }
        }
    }

    if (mMaterial->GetSurfaceType() == gfx::MaterialClass::SurfaceType::Transparent)
    {
        SetVisible(mUI.chkBlendPreMulAlpha, true);
        SetValue(mUI.chkBlendPreMulAlpha, mMaterial->PremultipliedAlpha());
    }

    if (!mUniforms.empty())
    {
        SetVisible(mUI.customUniforms, true);
        for (auto* widget : mUniforms)
        {
            const auto& name = widget->GetName();
            if (const auto* val = mMaterial->GetUniformValue<float>(name))
                widget->SetValue(*val);
            else if (const auto* val = mMaterial->GetUniformValue<glm::vec2>(name))
                widget->SetValue(*val);
            else if (const auto* val = mMaterial->GetUniformValue<glm::vec3>(name))
                widget->SetValue(*val);
            else if (const auto* val = mMaterial->GetUniformValue<glm::vec4>(name))
                widget->SetValue(*val);
            else if (const auto* val = mMaterial->GetUniformValue<gfx::Color4f>(name))
                widget->SetValue(FromGfx(*val));
            else if (const auto* val = mMaterial->GetUniformValue<int>(name))
                widget->SetValue(*val);
            else BUG("No such uniform in material. UI and material are out of sync.");
        }
    }

    if (mMaterial->GetNumTextureMaps())
    {
        SetVisible(mUI.textureCoords,  true);
        SetVisible(mUI.textureFilters, true);
        SetEnabled(mUI.textureMaps,    true);

        std::vector<ResourceListItem> maps;
        std::vector<ResourceListItem> items;
        for (unsigned i=0; i<mMaterial->GetNumTextureMaps(); ++i)
        {
            const auto* map = mMaterial->GetTextureMap(i);
            if (map->GetType() == gfx::TextureMap::Type::Sprite)
                SetVisible(mUI.chkBlendFrames, true);

            ResourceListItem item;
            item.id   = map->GetId();
            item.name = map->GetName();
            item.tag  = "map";
            maps.push_back(item);
            items.push_back(item);

            for (unsigned j=0; j<map->GetNumTextures(); ++j)
            {
                const auto* source = map->GetTextureSource(j);
                ResourceListItem item;
                item.id   = source->GetId();
                item.name = "    " + source->GetName();
                item.tag  = "texture";
                items.push_back(item);
            }
        }
        SetList(mUI.textures,  items);
        SetList(mUI.activeMap, maps);
        SetValue(mUI.activeMap, ListItemId(mMaterial->GetActiveTextureMap()));
    }
}

void MaterialWidget::GetTextureProperties()
{
    SetEnabled(mUI.textureProp, false);
    SetValue(mUI.textureSourceFile,   QString(""));
    SetValue(mUI.textureSourceID,     QString(""));
    SetValue(mUI.textureSourceName,   QString(""));
    SetValue(mUI.textureWidth,        QString(""));
    SetValue(mUI.textureHeight,       QString(""));
    SetValue(mUI.textureDepth,        QString(""));
    SetImage(mUI.texturePreview,      QPixmap(":texture.png"));
    SetEnabled(mUI.chkAllowPacking,   false);
    SetEnabled(mUI.chkAllowResizing,  false);
    SetEnabled(mUI.chkPreMulAlpha,    false);
    SetEnabled(mUI.chkBlurTexture,    false);

    SetEnabled(mUI.textureRect, false);
    SetValue(mUI.rectX, 0.0f);
    SetValue(mUI.rectY, 0.0f);
    SetValue(mUI.rectW, 1.0f);
    SetValue(mUI.rectH, 1.0f);

    const auto* source = GetSelectedTextureSrc();
    if (source == nullptr)
        return;

    SetEnabled(mUI.textureProp,    true);
    SetEnabled(mUI.textureRect,    true);

    if (const auto bitmap = source->GetData())
    {
        SetImage(mUI.texturePreview, *bitmap);
        SetValue(mUI.textureWidth,  bitmap->GetWidth());
        SetValue(mUI.textureHeight, bitmap->GetHeight());
        SetValue(mUI.textureDepth,  bitmap->GetDepthBits());
    } else WARN("Failed to load texture preview.");

    const auto& rect = mMaterial->FindTextureRect(source->GetId());
    SetValue(mUI.rectX,             rect.GetX());
    SetValue(mUI.rectY,             rect.GetY());
    SetValue(mUI.rectW,             rect.GetWidth());
    SetValue(mUI.rectH,             rect.GetHeight());
    SetValue(mUI.textureSourceID,   source->GetId());
    SetValue(mUI.textureSourceName, source->GetName());
    SetValue(mUI.textureSourceFile, QString("N/A"));
    if (const auto* ptr = dynamic_cast<const gfx::detail::TextureFileSource*>(source))
    {
        SetValue(mUI.textureSourceFile, ptr->GetFilename());
        SetValue(mUI.chkAllowPacking,   ptr->TestFlag(gfx::detail::TextureFileSource::Flags::AllowPacking));
        SetValue(mUI.chkAllowResizing,  ptr->TestFlag(gfx::detail::TextureFileSource::Flags::AllowResizing));
        SetValue(mUI.chkPreMulAlpha,    ptr->TestFlag(gfx::detail::TextureFileSource::Flags::PremulAlpha));
        SetValue(mUI.chkBlurTexture,    ptr->TestEffect(gfx::TextureSource::Effect::Blur));
        SetEnabled(mUI.chkAllowPacking,  true);
        SetEnabled(mUI.chkAllowResizing, true);
        SetEnabled(mUI.chkPreMulAlpha,   true);
        SetEnabled(mUI.chkBlurTexture,   true);
    }
    else if (const auto* ptr = dynamic_cast<const gfx::detail::TextureTextBufferSource*>(source))
    {
        SetEnabled(mUI.chkBlurTexture, true);
        SetValue(mUI.chkBlurTexture, ptr->TestEffect(gfx::TextureSource::Effect::Blur));
    }
}

void MaterialWidget::GetTextureMapProperties()
{
    SetEnabled(mUI.textureMap, false);
    SetValue(mUI.textureMapID,       QString(""));
    SetValue(mUI.textureMapName,     QString(""));
    SetValue(mUI.textureMapType,     gfx::TextureMap::Type::Texture2D);
    SetValue(mUI.textureMapTextures, QString(""));
    SetValue(mUI.textureMapFps,      0.0f);
    SetValue(mUI.spriteRows,         1);
    SetValue(mUI.spriteSheet,        1);
    SetValue(mUI.spriteSheet,  false);
    SetValue(mUI.chkLooping,   false);
    SetVisible(mUI.chkLooping, false);
    SetEnabled(mUI.textureMapFps, false);
    SetEnabled(mUI.spriteSheet,   false);
    SetVisible(mUI.spriteSheet,   false);
    SetVisible(mUI.lblFps,        false);
    SetVisible(mUI.textureMapFps, false);

    const auto* map = GetSelectedTextureMap();
    if (map == nullptr)
        return;

    SetEnabled(mUI.textureMap, true);
    SetValue(mUI.textureMapID,   map->GetId());
    SetValue(mUI.textureMapName, map->GetName());
    SetValue(mUI.textureMapType, map->GetType());
    SetValue(mUI.textureMapFps,  map->GetFps());
    SetValue(mUI.chkLooping,     map->IsLooping());

    if (map->GetType() == gfx::TextureMap::Type::Sprite)
    {
        SetEnabled(mUI.textureMapFps, true);
        SetEnabled(mUI.spriteSheet,   true);
        SetVisible(mUI.chkLooping,    true);
        SetVisible(mUI.lblFps,        true);
        SetVisible(mUI.textureMapFps, true);
        SetVisible(mUI.spriteSheet,   true);
    }

    if (const auto* sheet = map->GetSpriteSheet())
    {
        SetValue(mUI.spriteSheet,  true);
        SetEnabled(mUI.spriteCols, true);
        SetEnabled(mUI.spriteRows, true);
        SetValue(mUI.spriteRows, sheet->rows);
        SetValue(mUI.spriteCols, sheet->cols);
    }

    const auto count = map->GetNumTextures();
    if (count == 1)
    {
        const auto* src = map->GetTextureSource(0);
        if (const auto* ptr = dynamic_cast<const gfx::detail::TextureFileSource*>(src))
            SetValue(mUI.textureMapTextures, ptr->GetFilename());
        else SetValue(mUI.textureMapTextures, app::toString(src->GetSourceType()));
    }
    else if (count > 1)
    {
        SetValue(mUI.textureMapTextures, app::toString("%1 textures", count));
    }

}

void MaterialWidget::PaintScene(gfx::Painter& painter, double secs)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    SetValue(mUI.widgetColor, mUI.widget->GetCurrentClearColor());

    const float zoom = GetValue(mUI.zoom);
    const auto content_width  = width * zoom;
    const auto content_height = width * zoom;
    const auto xpos = (width - content_width) * 0.5f;
    const auto ypos = (height - content_height) * 0.5f;
    const auto drawable = mWorkspace->MakeDrawableByName(GetValue(mUI.cmbModel));

    // use the material to fill a model shape in the middle of the screen.
    gfx::Transform transform;
    transform.MoveTo(xpos, ypos);
    transform.Resize(content_width, content_height);

    std::string message;
    bool dummy   = false;
    bool success = true;

    for (unsigned i=0; i<mMaterial->GetNumTextureMaps(); ++i)
    {
        const auto& map = mMaterial->GetTextureMap(i);
        if (map->GetNumTextures() == 0)
        {
            dummy = true;
            success = false;
            message = base::FormatString("Missing texture map on '%1'", map->GetName());
            break;
        }
    }
    if (mMaterial->GetType() == gfx::MaterialClass::Type::Custom)
    {
        const auto& uri = mMaterial->GetShaderUri();
        if (uri.empty())
        {
            message = "No shader has been selected.";
            success = false;
        }
    }

    if (!success)
    {
        if (dummy)
        {
            static auto dummy = gfx::CreateMaterialClassFromImage("app://textures/Checkerboard.png");
            painter.Draw(*drawable, transform, gfx::MaterialClassInst(dummy));
        }
        ShowMessage(message, painter);
        return;
    }

    gfx::MaterialClassInst material(mMaterial);
    const auto time = mState == PlayState::Playing ? mTime : GetValue(mUI.kTime);
    material.SetRuntime(time);
    painter.Draw(*drawable, transform, material);
}

gfx::TextureMap* MaterialWidget::GetSelectedTextureMap()
{
    if (auto* item = GetSelectedItem(mUI.textures))
    {
        // if it's a map we're done.
        if (auto* map = mMaterial->FindTextureMapById(GetItemId(item)))
            return map;

        // if the selected item is a texture source then find the map that
        // owns the texture source.
        for (unsigned i=0; i<mMaterial->GetNumTextureMaps(); ++i)
        {
            auto* map = mMaterial->GetTextureMap(i);
            auto idx = map->FindTextureSourceIndexById(GetItemId(item));
            if (idx == map->GetNumTextures())
                continue;
            return map;
        }
    }
    return nullptr;
}

gfx::TextureSource* MaterialWidget::GetSelectedTextureSrc()
{
    if (auto* item = GetSelectedItem(mUI.textures))
    {
        return mMaterial->FindTextureSource(GetItemId(item));
    }
    return nullptr;
}

} // namespace
