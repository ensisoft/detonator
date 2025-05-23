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
#include "graphics/debug_drawable.h"
#include "graphics/simple_shape.h"
#include "graphics/linebatch.h"
#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/material_instance.h"
#include "graphics/transform.h"
#include "graphics/drawing.h"
#include "graphics/types.h"
#include "graphics/shader_source.h"
#include "graphics/shader_program.h"
#include "graphics/shader_programs.h"
#include "graphics/texture_file_source.h"
#include "graphics/texture_bitmap_generator_source.h"
#include "graphics/texture_text_buffer_source.h"
#include "editor/app/eventlog.h"
#include "editor/app/resource.h"
#include "editor/app/utility.h"
#include "editor/app/workspace.h"
#include "editor/app/resource-uri.h"
#include "editor/gui/main.h"
#include "editor/gui/settings.h"
#include "editor/gui/utility.h"
#include "editor/gui/drawing.h"
#include "editor/gui/dlgtext.h"
#include "editor/gui/dlgbitmap.h"
#include "editor/gui/uniform.h"
#include "editor/gui/sampler.h"
#include "editor/gui/dlgtexturerect.h"
#include "editor/gui/dlgtextedit.h"
#include "editor/gui/dlgimgview.h"
#include "editor/gui/materialwidget.h"
#include "editor/gui/imgpack.h"
#include "editor/gui/translation.h"

namespace {
    enum class PreviewScene {
        FlatColor, BasicShading
    };
    std::string TranslateEnum(PreviewScene scene)
    {
        if (scene == PreviewScene::FlatColor)
            return "Flat Color";
        else if (scene == PreviewScene::BasicShading)
            return "Basic Shading";
        BUG("Bug on preview env translation");
        return "???";
    }

} // namespace

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
    mUI.widget->onPaintScene   = std::bind(&MaterialWidget::PaintScene, this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onZoomIn       = std::bind(&MaterialWidget::ZoomIn, this);
    mUI.widget->onZoomOut      = std::bind(&MaterialWidget::ZoomOut, this);
    mUI.widget->onMouseMove    = std::bind(&MaterialWidget::MouseMove, this, std::placeholders::_1);
    mUI.widget->onMousePress   = std::bind(&MaterialWidget::MousePress, this, std::placeholders::_1);
    mUI.widget->onMouseRelease = std::bind(&MaterialWidget::MouseRelease, this, std::placeholders::_1);
    mUI.widget->onKeyPress     = std::bind(&MaterialWidget::KeyPress, this, std::placeholders::_1);
    mUI.actionPause->setEnabled(false);
    mUI.actionPlay->setEnabled(true);
    mUI.actionStop->setEnabled(false);

    {
        QMenu* menu = new QMenu(this);
        QAction* add_texture_from_file = menu->addAction(QIcon("icons:folder.png"), "From File");
        QAction* add_texture_from_text = menu->addAction(QIcon("icons:text.png"), "From Text");
        QAction* add_texture_from_bitmap = menu->addAction(QIcon("icons:bitmap.png"), "From Bitmap");
        connect(add_texture_from_file, &QAction::triggered, this, &MaterialWidget::AddNewTextureMapFromFile);
        connect(add_texture_from_text, &QAction::triggered, this, &MaterialWidget::AddNewTextureMapFromText);
        connect(add_texture_from_bitmap, &QAction::triggered, this, &MaterialWidget::AddNewTextureMapFromBitmap);
        mUI.btnAddTextureMap->setMenu(menu);
    }

    {
        QMenu* menu = new QMenu(this);
        menu->addAction(mUI.actionCreateShader);
        menu->addAction(mUI.actionSelectShader);
        menu->addAction(mUI.actionEditShader);
        menu->addAction(mUI.actionCustomizeShader);
        menu->addAction(mUI.actionShowShader);
        mUI.btnAddShader->setMenu(menu);
    }

    connect(&mFileWatcher, &QFileSystemWatcher::fileChanged, this, &MaterialWidget::ShaderFileChanged);
    connect(mWorkspace, &app::Workspace::ResourceUpdated, this, &MaterialWidget::ResourceUpdated);

    PopulateFromEnum<gfx::MaterialClass::MinTextureFilter>(mUI.textureMinFilter);
    PopulateFromEnum<gfx::MaterialClass::MagTextureFilter>(mUI.textureMagFilter);
    PopulateFromEnum<gfx::MaterialClass::TextureWrapping>(mUI.textureWrapX);
    PopulateFromEnum<gfx::MaterialClass::TextureWrapping>(mUI.textureWrapY);
    PopulateFromEnum<gfx::MaterialClass::SurfaceType>(mUI.surfaceType);
    PopulateFromEnum<gfx::MaterialClass::Type>(mUI.materialType);
    PopulateFromEnum<gfx::MaterialClass::ParticleEffect>(mUI.particleAction);
    PopulateFromEnum<gfx::TextureMap::Type>(mUI.textureMapType);
    PopulateFromEnum<gfx::TextureFileSource::ColorSpace>(mUI.cmbColorSpace);
    PopulateFromEnum<gfx::MaterialClass::ParticleRotation>(mUI.particleRotationMode);
    PopulateFromEnum<gfx::MaterialClass::GradientType>(mUI.cmbGradientType);
    PopulateFromEnum<PreviewScene>(mUI.cmbScene);


    // leave this out for now. particle UI can take care
    // PopulateShaderList(mUI.shaderFile);

    SetList(mUI.cmbModel, workspace->ListPrimitiveDrawables());
    SetValue(mUI.cmbModel, ListItemId("_rect"));
    SetValue(mUI.materialID, mMaterial->GetId());
    SetValue(mUI.materialName, mMaterial->GetName());
    SetValue(mUI.cmbScene, PreviewScene::FlatColor);
    setWindowTitle(GetValue(mUI.materialName));

    ShowMaterialProperties();
    ShowTextureMapProperties();
    ShowTextureProperties();

    SetValue(mUI.zoom, 1.0f);

    // hide this for now.
    SetVisible(mUI.textureRect, false);

    mModelRotationTotal.x = glm::radians(-45.0f);
    mModelRotationTotal.y = glm::radians(15.0f);

    mUI.sprite->SetMaterial(mMaterial);
    mUI.sprite->RenderTimeBar(true);
}

MaterialWidget::MaterialWidget(app::Workspace* workspace, const app::Resource& resource) : MaterialWidget(workspace)
{
    DEBUG("Editing material: '%1'", resource.GetName());
    mMaterial = resource.GetContent<gfx::MaterialClass>()->Copy();
    mOriginalHash = mMaterial->GetHash();
    SetValue(mUI.materialID, resource.GetId());
    SetValue(mUI.materialName, resource.GetName());
    GetUserProperty(resource, "model", mUI.cmbModel);
    GetUserProperty(resource, "scene", mUI.cmbScene);
    GetUserProperty(resource, "zoom", mUI.zoom);
    GetUserProperty(resource, "widget", mUI.widget);
    GetUserProperty(resource, "time", mUI.kTime);
    GetUserProperty(resource, "main_splitter", mUI.mainSplitter);
    GetUserProperty(resource, "right_splitter", mUI.rightSplitter);
    GetUserProperty(resource, "model_rotation", &mModelRotationTotal);
    GetUserProperty(resource, "light_position", &mLightPositionTotal);

    // Because of the Qt bugs related to having any effin sanity
    // when it comes to being able to have a splitter division
    // sized reasonably we're setting off a timer in InitializeSettings.
    // However, if we actually were able to recover the splitter
    // geometry then that timer should not do anything.
    if (!GetUserProperty(resource, "sprite_splitter", mUI.spriteSplitter))
    {
        QTimer::singleShot(10, this, [this]() {
            if (mMaterial->GetType() == gfx::MaterialClass::Type::Sprite)
                mUI.spriteSplitter->setSizes({80, 20});
            else mUI.spriteSplitter->setSizes({100, 0});
        });
    }

    ApplyShaderDescription();
    ShowMaterialProperties();
    ShowTextureMapProperties();
    ShowTextureProperties();

    mUI.sprite->SetMaterial(mMaterial);
}

MaterialWidget::~MaterialWidget()
{
    DEBUG("Destroy MaterialWidget");
}

QString MaterialWidget::GetId() const
{
    return GetValue(mUI.materialID);
}

QImage MaterialWidget::TakeScreenshot() const
{
    return mUI.widget->TakeSreenshot();
}

void MaterialWidget::InitializeSettings(const UISettings& settings)
{
    SetValue(mUI.zoom, settings.zoom);

    QTimer::singleShot(10, this, [this]() {
        mUI.spriteSplitter->setSizes({100, 0});
    });
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
    SetVisible(mUI.textureMap,        false);
    SetVisible(mUI.scrollArea,        false);

    mUI.mainSplitter->setSizes({0, 100, 0});

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
    bar.addAction(mUI.actionCreateShader);
    bar.addAction(mUI.actionSelectShader);
    bar.addAction(mUI.actionEditShader);
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
    menu.addSeparator();
    menu.addAction(mUI.actionCreateShader);
    menu.addAction(mUI.actionSelectShader);
    menu.addAction(mUI.actionEditShader);

    // we don't add these to the menu since they're
    // already added to the *Edit* menu globally for
    // every type of main widget. So adding these again
    // for the material menu would only create confusion.
    //menu.addAction(mUI.actionReloadShaders);
    //menu.addAction(mUI.actionReloadTextures);
}

bool MaterialWidget::LoadState(const Settings& settings)
{
    data::JsonObject json;
    settings.GetValue("Material", "content", &json);
    settings.GetValue("Material", "hash", &mOriginalHash);
    settings.GetValue("Material", "model_rotation", &mModelRotationTotal);
    settings.GetValue("Material", "light_position", &mLightPositionTotal);
    settings.LoadWidget("Material", mUI.materialName);
    settings.LoadWidget("Material", mUI.zoom);
    settings.LoadWidget("Material", mUI.cmbModel);
    settings.LoadWidget("Material", mUI.cmbScene);
    settings.LoadWidget("Material", mUI.widget);
    settings.LoadWidget("Material", mUI.kTime);
    settings.LoadWidget("Material", mUI.kTileIndex);
    settings.LoadWidget("Material", mUI.mainSplitter);
    settings.LoadWidget("Material", mUI.rightSplitter);
    settings.LoadWidget("Material", mUI.spriteSplitter);

    mMaterial = gfx::MaterialClass::ClassFromJson(json);
    if (!mMaterial)
    {
        WARN("Failed to restore material state.");
        mMaterial = std::make_shared<gfx::ColorClass>(gfx::MaterialClass::Type::Color, mMaterial->GetId());
    }

    ApplyShaderDescription();
    ShowMaterialProperties();

    QString selected_item;
    if (settings.GetValue("Material", "selected_item", &selected_item))
        SelectItem(mUI.textures, ListItemId(selected_item));

    ShowTextureMapProperties();
    ShowTextureProperties();

    mUI.sprite->SetMaterial(mMaterial);
    return true;
}

bool MaterialWidget::SaveState(Settings& settings) const
{
    data::JsonObject json;
    mMaterial->IntoJson(json);
    settings.SetValue("Material", "content", json);
    settings.SetValue("Material", "hash", mOriginalHash);
    settings.SetValue("Material", "model_rotation", mModelRotationTotal);
    settings.SetValue("Material", "light_position", mLightPositionTotal);
    settings.SaveWidget("Material", mUI.materialName);
    settings.SaveWidget("Material", mUI.zoom);
    settings.SaveWidget("Material", mUI.cmbModel);
    settings.SaveWidget("Material", mUI.cmbScene);
    settings.SaveWidget("Material", mUI.widget);
    settings.SaveWidget("Material", mUI.kTime);
    settings.SaveWidget("Material", mUI.kTileIndex);
    settings.SaveWidget("Material", mUI.mainSplitter);
    settings.SaveWidget("Material", mUI.rightSplitter);
    settings.SaveWidget("Material", mUI.spriteSplitter);
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
        case Actions::CanScreenshot:
            return false;
        case Actions::CanZoomIn:
            return CanZoomIn(mUI.zoom);
        case Actions::CanZoomOut:
            return CanZoomOut(mUI.zoom);
        case Actions::CanReloadTextures:
        case Actions::CanReloadShaders:
            return true;
    }
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

    // reset material instance so that any one time error logging will take place.
    mMaterialInst.reset();

    mUI.widget->GetPainter()->ClearErrors();

    NOTE("Reloaded shaders.");
}
void MaterialWidget::ReloadTextures()
{
    mUI.widget->reloadTextures();

    // reset material instance so that any one time error logging will take place.
    mMaterialInst.reset();

    ShowTextureProperties();

    NOTE("Reloaded textures.");
}

void MaterialWidget::Shutdown()
{
    mUI.widget->dispose();

    if (mShaderEditor)
    {
        mShaderEditor->closeFU();
        delete mShaderEditor;
        mShaderEditor = nullptr;
    }
}

void MaterialWidget::Update(double secs)
{
    if (mState == PlayState::Playing)
    {
        mTime += secs;
        if (mMaterial->GetType() == gfx::MaterialClass::Type::Sprite)
        {
            const auto& texture_map_id = mMaterial->GetActiveTextureMap();
            const auto* texture_map = mMaterial->FindTextureMapById(texture_map_id);
            if (!texture_map)
                return;
            if (!texture_map->IsSpriteLooping())
            {
                if (mTime >= texture_map->GetSpriteCycleDuration())
                    on_actionStop_triggered();
            }
        }
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
    mUI.sprite->Render();
}

void MaterialWidget::on_widgetColor_colorChanged(QColor color)
{
    mUI.widget->SetClearColor(color);
    mUI.sprite->SetClearColor(color);
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
    SetUserProperty(resource, "scene", mUI.cmbScene);
    SetUserProperty(resource, "widget", mUI.widget);    
    SetUserProperty(resource, "zoom", mUI.zoom);
    SetUserProperty(resource, "time", mUI.kTime);
    SetUserProperty(resource, "main_splitter", mUI.mainSplitter);
    SetUserProperty(resource, "right_splitter", mUI.rightSplitter);
    SetUserProperty(resource, "sprite_splitter", mUI.spriteSplitter);
    SetUserProperty(resource, "model_rotation", mModelRotationTotal);
    SetUserProperty(resource, "light_position", mLightPositionTotal);

    // WARNING WARNING WARNING WARNING unsafe code!
    // We have a stupid ass recursion happening when we call
    // SaveResource since that will invoke callbacks which
    // will then end up calling back here in ResourceUpdate.
    // The reason why we have ResourceUpdated implementation
    // is to realize changes done to this material in the
    // particle editor.
    //
    // We differentiate between the two cases with the hash
    // value by writing it here first before saving.
    mOriginalHash = mMaterial->GetHash();

    // callback hell!
    mWorkspace->SaveResource(resource);
}

void MaterialWidget::on_actionNewMap_triggered()
{
    const auto type = mMaterial->GetType();
    const auto maps = mMaterial->GetNumTextureMaps();

    auto map = std::make_unique<gfx::TextureMap>();
    if (type == gfx::MaterialClass::Type::Sprite)
    {
        map->SetType(gfx::TextureMap::Type::Sprite);
        map->SetName(base::FormatString("Sprite %1", maps));
    }
    else if (type == gfx::MaterialClass::Type::Texture)
    {
        map->SetType(gfx::TextureMap::Type::Texture2D);
        map->SetName(base::FormatString("Texture %1", maps));
    }
    else if (type == gfx::MaterialClass::Type::Tilemap)
    {
        map->SetType(gfx::TextureMap::Type::Texture2D);
        map->SetName(base::FormatString("Tilemap %1", maps));
    }
    else if (type == gfx::MaterialClass::Type::Particle2D)
    {
        map->SetType(gfx::TextureMap::Type::Texture2D);
        map->SetName(base::FormatString("Particle Alpha Mask %1", maps));
    }
    else return;

    mMaterial->SetNumTextureMaps(maps + 1);
    mMaterial->SetTextureMap(maps, std::move(map));
    ShowMaterialProperties();
    SelectLastItem(mUI.textures);
    ShowTextureMapProperties();
    ShowTextureProperties();
}

void MaterialWidget::on_actionDelMap_triggered()
{
    if (const auto* map = GetSelectedTextureMap())
    {
        const auto index = mMaterial->FindTextureMapIndexById(map->GetId());
        if (index < mMaterial->GetNumTextureMaps())
            mMaterial->DeleteTextureMap(index);

        ShowMaterialProperties();
        SelectLastItem(mUI.textures);
        ShowTextureMapProperties();
        ShowTextureProperties();
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

    ShowMaterialProperties();
    SelectLastItem(mUI.textures);
    ShowTextureProperties();
    ShowTextureMapProperties();
}

void MaterialWidget::on_actionReloadShaders_triggered()
{
    if (Editor::DevEditor())
    {
        app::Workspace::ClearAppGraphicsCache();
    }
    else
    {
        const auto& uri = mMaterial->GetShaderUri();
        if (base::StartsWith(uri, "app://"))
            WARN("Editor's shaders will not reload without --editor-dev option.");
    }

    QString selected_item;
    if (const auto* item = GetSelectedItem(mUI.textures))
        selected_item = GetItemId(item);

    ReloadShaders();
    ApplyShaderDescription();
    ShowMaterialProperties();
    SelectItem(mUI.textures, ListItemId(selected_item));
    ShowTextureMapProperties();
    ShowTextureProperties();

}
void MaterialWidget::on_actionReloadTextures_triggered()
{
    if (Editor::DevEditor())
    {
        app::Workspace::ClearAppGraphicsCache();
    }
    else
    {
        for (unsigned i=0; i<mMaterial->GetNumTextureMaps(); ++i)
        {
            const auto* map = mMaterial->GetTextureMap(i);
            for (unsigned j=0; j<map->GetNumTextures(); ++j)
            {
                const auto* texture = map->GetTextureSource(j);
                if (const auto* file = dynamic_cast<const gfx::TextureFileSource*>(texture))
                {
                    const auto& uri = file->GetFilename();
                    if (base::StartsWith(uri, "app://"))
                    {
                        WARN("Editor's textures will not reload without --editor-dev option.");
                        break;
                    }
                }
            }
        }
    }

    this->ReloadTextures();
}

void MaterialWidget::on_actionSelectShader_triggered()
{
    const auto type = mMaterial->GetType();
    if (type != gfx::MaterialClass::Type::Custom)
        return;

    const auto& shader = QFileDialog::getOpenFileName(this,
        tr("Select Shader File"), "",
        tr("Shaders (*.glsl)"));
    if (shader.isEmpty())
        return;

    mMaterial->SetShaderUri(mWorkspace->MapFileToWorkspace(shader));
    ApplyShaderDescription();
    ReloadShaders();
    ShowMaterialProperties();
}

void MaterialWidget::on_actionCreateShader_triggered()
{
    const auto type = mMaterial->GetType();
    if (type != gfx::MaterialClass::Type::Custom)
        return;

    CreateCustomShaderStub();

    ApplyShaderDescription();
    ReloadShaders();
    ShowMaterialProperties();

    on_actionEditShader_triggered();
}

void MaterialWidget::on_actionEditShader_triggered()
{
    const auto& uri = mMaterial->GetShaderUri();
    if (uri.empty())
        return;
    const auto& glsl = mWorkspace->MapFileToFilesystem(uri);
    emit OpenExternalShader(glsl);
}

void MaterialWidget::on_actionShowShader_triggered()
{
    auto* device = mUI.widget->GetDevice();

    gfx::ShaderSource source;
    gfx::Material::Environment environment;
    environment.editing_mode  = false; // we want to see the shader as it will be, so using false here

    const auto scene = (PreviewScene)GetValue(mUI.cmbScene);

    if (scene == PreviewScene::BasicShading)
    {
        gfx::BasicLightProgram program;
        source = program.GetShader(gfx::MaterialInstance(mMaterial), environment, *device);
    }
    else
    {
        gfx::FlatShadedColorProgram program;
        source = program.GetShader(gfx::MaterialInstance(mMaterial), environment, *device);
    }

    DlgTextEdit dlg(this);
    dlg.SetText(source.GetSource(), "GLSL");
    dlg.SetReadOnly(true);
    dlg.SetTitle("Shader Source");
    dlg.LoadGeometry(mWorkspace, "shader-source-dialog-geometry");
    dlg.execFU();
    dlg.SaveGeometry(mWorkspace, "shader-source-dialog-geometry");

}

void MaterialWidget::on_actionCustomizeShader_triggered()
{
    if (mMaterial->GetType() == gfx::MaterialClass::Type::Custom)
        return;

    if (mShaderEditor)
    {
        mShaderEditor->activateWindow();
        return;
    }

    mCustomizedSource = mMaterial->GetShaderSrc();

    // todo: improve the stub somehow. extract it from the shader source?
    // todo: add the material interface somewhere, i.e. the varyings and
    // and the uniforms.

    if (!mMaterial->HasShaderSrc())
    {
        auto* device = mUI.widget->GetDevice();

        gfx::ShaderSource source;
        gfx::Material::Environment environment;
        environment.editing_mode = true;

        gfx::FlatShadedColorProgram program;
        source = program.GetShader(gfx::MaterialInstance(mMaterial), environment, *device);

        const auto& src = app::FromUtf8(source.GetSource(gfx::ShaderSource::SourceVariant::Development));
        const auto& lines = src.split("\n", Qt::KeepEmptyParts);

        unsigned start = 0;
        unsigned end = 0;
        for (unsigned i=0; i<lines.size(); ++i)
        {
            const auto& line = lines[i];
            if (line.contains("void FragmentShaderMain() {"))
                start = i;
            else if (start > 0 && line == "}" && end == 0)
                end = i;

            if (start && end)
                break;
        }

        QString starter;
        starter.append(R"(
// this is your custom fragment (material) shader main.
// this will replace the built-in function but uses the
// same uniform interface.
)");

        if (start && end && start < lines.size() && end <lines.size())
        {
            for (; start <= end; ++start)
            {
                starter.append(lines[start]);
                starter.append("\n");
            }

            starter = starter.replace("FragmentShaderMain", "CustomFragmentShaderMain");
        }
        else
        {
            starter.append(R"(
void CustomFragmentShaderMain() {
  vec4 color = vec4(0.3);

  #ifdef GEOMETRY_IS_PARTICLES
    color.a *= vParticleAlpha;
  #endif

  fs_out.color = color;
  fs_out.flags =  kMaterialFlags;
}

)");
        }
        mMaterial->SetShaderSrc(app::ToUtf8(starter));
    }

    mShaderEditor = new DlgTextEdit(this);
    mShaderEditor->LoadGeometry(mWorkspace, "shader-editor-geometry");
    mShaderEditor->SetText(mMaterial->GetShaderSrc(), "GLSL");
    mShaderEditor->SetTitle("Shader Source");
    mShaderEditor->EnableApply(true);
    mShaderEditor->showFU();
    mShaderEditor->finished = [this](int ret) {
        if (ret == QDialog::Rejected)
            mMaterial->SetShaderSrc(mCustomizedSource);
        else if (ret == QDialog::Accepted)
            mMaterial->SetShaderSrc(mShaderEditor->GetText());
        mShaderEditor->SaveGeometry(mWorkspace, "shader-editor-geometry");
        mShaderEditor->deleteLater();
        mShaderEditor = nullptr;
        ShowMaterialProperties();
    };
    mShaderEditor->apply = [this]() {
        mMaterial->SetShaderSrc(mShaderEditor->GetText());
        on_actionReloadShaders_triggered();
    };
}

void MaterialWidget::on_btnResetShader_clicked()
{
    if (mMaterial->HasShaderUri())
    {
        mMaterial->ClearShaderUri();
        SetEnabled(mUI.actionEditShader, false);
        SetEnabled(mUI.btnResetShader, false);
        SetValue(mUI.shaderFile, QString(""));
        ClearCustomUniforms();
        ShowMaterialProperties();
    }
    else if (mMaterial->HasShaderSrc())
    {
        mMaterial->ClearShaderSrc();
        ShowMaterialProperties();
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

        ShowMaterialProperties();
        SelectItem(mUI.textures, ListItemId(map->GetId()));
        ShowTextureMapProperties();
        ShowTextureProperties();
    }
}

void MaterialWidget::on_btnEditTexture_clicked()
{
    if (auto* source = GetSelectedTextureSrc())
    {
        if (const auto* ptr = dynamic_cast<const gfx::TextureFileSource*>(source))
        {
            emit OpenExternalImage(app::FromUtf8(ptr->GetFilename()));
        }
        else if (auto* ptr = dynamic_cast<gfx::TextureTextBufferSource*>(source))
        {
            // make a copy for editing.
            gfx::TextBuffer text(ptr->GetTextBuffer());
            DlgText dlg(this, mWorkspace, text);
            if (dlg.exec() == QDialog::Rejected)
                return;

            auto* map = GetSelectedTextureMap();

            bool replace_with_export = false;
            if (dlg.DidExport() && map->GetNumTextures() == 1)
            {
                QString file = dlg.GetSaveFile();

                QMessageBox msg(this);
                msg.setWindowTitle(tr("Replace Text With Image?"));
                msg.setText(tr("Do you want to replace the text with the static PNG image you just saved?\n\n%1").arg(file));
                msg.setIcon(QMessageBox::Question);
                msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                if (msg.exec() == QMessageBox::Yes)
                    replace_with_export = true;
            }
            if (replace_with_export)
            {
                QString png = dlg.GetSaveFile();

                const QFileInfo info(png);
                const auto& name = info.baseName();
                const auto& file = mWorkspace->MapFileToWorkspace(info.absoluteFilePath());

                auto source = std::make_unique<gfx::TextureFileSource>(file);
                source->SetName(app::ToUtf8(name));
                source->SetFileName(file);
                source->SetColorSpace(gfx::TextureFileSource::ColorSpace::sRGB);

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
                ShowMaterialProperties();
                SelectLastItem(mUI.textures);
                ShowTextureMapProperties();
                ShowTextureProperties();
            }
            else
            {
                // map the font files.
                auto& style_and_text = text.GetText();
                style_and_text.font = mWorkspace->MapFileToWorkspace(style_and_text.font);

                // Update the texture source's TextBuffer
                ptr->SetTextBuffer(std::move(text));

                // update the preview.
                ShowTextureProperties();
            }
        }
        else if (auto* ptr = dynamic_cast<gfx::TextureBitmapGeneratorSource*>(source))
        {
            // make a copy for editing.
            auto copy = ptr->GetGenerator().Clone();
            DlgBitmap dlg(this, std::move(copy));
            if (dlg.exec() == QDialog::Rejected)
                return;

            // override the generator with changes.
            ptr->SetGenerator(dlg.GetResult());

            // update the preview.
            ShowTextureProperties();
        }
    }
}

void MaterialWidget::on_btnResetTextureRect_clicked()
{
    if (auto* src = GetSelectedTextureSrc())
    {
        mMaterial->SetTextureRect(src->GetId(), gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));

        ShowTextureProperties();
    }
}

void MaterialWidget::on_btnSelectTextureRect_clicked()
{
    if (auto* src = GetSelectedTextureSrc())
    {
        auto rect = mMaterial->FindTextureRect(src->GetId());
        DlgTextureRect dlg(this, rect, src->Clone());
        dlg.LoadGeometry(mWorkspace, "texture-rect-dialog-geometry");
        dlg.LoadState(mWorkspace, "texture-rect-dialog", src->GetId());

        const auto ret = dlg.execFU();
        dlg.SaveGeometry(mWorkspace, "texture-rect-dialog-geometry");
        dlg.SaveState(mWorkspace, "texture-rect-dialog", src->GetId());
        if (ret == QDialog::Rejected)
            return;

        mMaterial->SetTextureRect(src->GetId(), dlg.GetRect());

        ShowTextureProperties();
    }
}

void MaterialWidget::on_textures_itemSelectionChanged()
{
    ShowTextureProperties();
    ShowTextureMapProperties();
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

    QMenu add;
    add.setTitle("Add Texture...");
    add.addAction(mUI.actionAddFile);
    add.addAction(mUI.actionAddText);
    add.addAction(mUI.actionAddBitmap);
    add.setIcon(QIcon("icons:add.png"));
    add.setEnabled(tag == "map");

    const auto type = mMaterial->GetType();
    if (type == gfx::MaterialClass::Type::Color ||
        type == gfx::MaterialClass::Type::Gradient ||
        type == gfx::MaterialClass::Type::BasicLight ||
        type == gfx::MaterialClass::Type::Custom)
        SetEnabled(mUI.actionNewMap, false);

    QMenu menu(this);
    menu.addAction(mUI.actionNewMap);
    menu.addSeparator();
    menu.addMenu(&add);
    menu.addAction(mUI.actionEditTexture);
    menu.addSeparator();
    menu.addAction(mUI.actionRemoveTexture);
    menu.addSeparator();
    menu.addAction(mUI.actionDelMap);
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
    other.SetName(mMaterial->GetName());

    if (type == gfx::MaterialClass::Type::Texture)
    {
        auto map = std::make_unique<gfx::TextureMap>();
        map->SetType(gfx::TextureMap::Type::Texture2D);
        map->SetName("Texture");
        other.SetActiveTextureMap(map->GetId());
        other.SetNumTextureMaps(1);
        other.SetTextureMap(0, std::move(map));
    }
    else if (type == gfx::MaterialClass::Type::Tilemap)
    {
        auto map = std::make_unique<gfx::TextureMap>();
        map->SetType(gfx::TextureMap::Type::Texture2D);
        map->SetName("Tilemap");
        other.SetActiveTextureMap(map->GetId());
        other.SetNumTextureMaps(1);
        other.SetTextureMap(0, std::move(map));

        SetValue(mUI.kTileIndex, 0);
    }
    else if (type == gfx::MaterialClass::Type::Sprite)
    {
        auto map = std::make_unique<gfx::TextureMap>();
        map->SetType(gfx::TextureMap::Type::Sprite);
        map->SetName("Sprite");
        map->SetSpriteFrameRate(10.0f);
        other.SetActiveTextureMap(map->GetId());
        other.SetNumTextureMaps(1);
        other.SetTextureMap(0, std::move(map));
    }
    else if (type == gfx::MaterialClass::Type::Particle2D)
    {
        auto map = std::make_unique<gfx::TextureMap>();
        map->SetType(gfx::TextureMap::Type::Texture2D);
        map->SetName("Particle Alpha Mask");
        map->SetSamplerName("kMask");
        other.SetActiveTextureMap(map->GetId());
        other.SetNumTextureMaps(1);
        other.SetTextureMap(0, std::move(map));
    }
    else if (type == gfx::MaterialClass::Type::BasicLight)
    {
        auto diffuse = std::make_unique<gfx::TextureMap>();
        diffuse->SetType(gfx::TextureMap::Type::Texture2D);
        diffuse->SetName("Diffuse Map");
        diffuse->SetSamplerName("kDiffuseMap");
        diffuse->SetRectUniformName("kDiffuseMapRect");

        auto specular = std::make_unique<gfx::TextureMap>();
        specular->SetType(gfx::TextureMap::Type::Texture2D);
        specular->SetName("Specular Map");
        specular->SetSamplerName("kSpecularMap");
        specular->SetRectUniformName("kSpecularMapRect");

        auto normal = std::make_unique<gfx::TextureMap>();
        normal->SetType(gfx::TextureMap::Type::Texture2D);
        normal->SetName("Normal Map");
        normal->SetSamplerName("kNormalMap");
        normal->SetRectUniformName("kNormalMapRect");

        other.SetNumTextureMaps(3);
        other.SetTextureMap(0, std::move(diffuse));
        other.SetTextureMap(1, std::move(specular));
        other.SetTextureMap(2, std::move(normal));
    }
    *mMaterial = other;

    ClearCustomUniforms();
    ShowMaterialProperties();
    ShowTextureMapProperties();
    ShowTextureProperties();
}
void MaterialWidget::on_surfaceType_currentIndexChanged(int)
{
    SetMaterialProperties();
    ShowTextureMapProperties();
    ShowTextureProperties();
}

void MaterialWidget::on_diffuseColor_colorChanged(QColor color)
{
    SetMaterialProperties();
}
void MaterialWidget::on_ambientColor_colorChanged(QColor color)
{
    SetMaterialProperties();
}
void MaterialWidget::on_specularColor_colorChanged(QColor color)
{
    SetMaterialProperties();
}
void MaterialWidget::on_specularExponent_valueChanged(double)
{
    SetMaterialProperties();
}

void MaterialWidget::on_particleStartColor_colorChanged(QColor color)
{
    SetMaterialProperties();
}
void MaterialWidget::on_particleMidColor_colorChanged(QColor color)
{
    SetMaterialProperties();
}
void MaterialWidget::on_particleEndColor_colorChanged(QColor color)
{
    SetMaterialProperties();
}
void MaterialWidget::on_particleBaseRotation_valueChanged(double)
{
    SetMaterialProperties();
}
void MaterialWidget::on_particleRotationMode_currentIndexChanged(int)
{
    SetMaterialProperties();
}

void MaterialWidget::on_tileWidth_valueChanged(int)
{
    SetMaterialProperties();
}
void MaterialWidget::on_tileHeight_valueChanged(int)
{
    SetMaterialProperties();
}

void MaterialWidget::on_tileLeftOffset_valueChanged(int)
{
    SetMaterialProperties();
}
void MaterialWidget::on_tileTopOffset_valueChanged(int)
{
    SetMaterialProperties();
}
void MaterialWidget::on_tileLeftPadding_valueChanged(int)
{
    SetMaterialProperties();
}
void MaterialWidget::on_tileTopPadding_valueChanged(int)
{
    SetMaterialProperties();
}
void MaterialWidget::on_activeMap_currentIndexChanged(int)
{
    SetMaterialProperties();
}
void MaterialWidget::on_alphaCutoff_valueChanged(bool, double value)
{
    SetMaterialProperties();
}
void MaterialWidget::on_baseColor_colorChanged(QColor color)
{
    SetMaterialProperties();
}
void MaterialWidget::on_particleAction_currentIndexChanged(int)
{
    SetMaterialProperties();
}
void MaterialWidget::on_cmbGradientType_currentIndexChanged(int)
{
    SetMaterialProperties();
}

void MaterialWidget::on_spriteFps_valueChanged(double)
{
    SetMaterialProperties();
}
void MaterialWidget::on_chkBlendPreMulAlpha_stateChanged(int)
{
    SetMaterialProperties();
}
void MaterialWidget::on_chkStaticInstance_stateChanged(int)
{
    SetMaterialProperties();
}
void MaterialWidget::on_chkEnableBloom_stateChanged(int)
{
    SetMaterialProperties();
}
void MaterialWidget::on_chkBlendFrames_stateChanged(int)
{
    SetMaterialProperties();
}
void MaterialWidget::on_chkLooping_stateChanged(int)
{
    SetMaterialProperties();
}
void MaterialWidget::on_spriteCols_valueChanged(int)
{
    SetMaterialProperties();
}
void MaterialWidget::on_spriteRows_valueChanged(int)
{
    SetMaterialProperties();
}
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
void MaterialWidget::on_spriteDuration_valueChanged(double)
{
    if (auto* map = GetSelectedTextureMap())
    {
        map->SetSpriteFrameRateFromDuration(GetValue(mUI.spriteDuration));
        SetValue(mUI.spriteFps, map->GetSpriteFrameRate());
    }
}
void MaterialWidget::on_colorMap0_colorChanged(QColor)
{
    SetMaterialProperties();
}
void MaterialWidget::on_colorMap1_colorChanged(QColor)
{
    SetMaterialProperties();
}
void MaterialWidget::on_colorMap2_colorChanged(QColor)
{
    SetMaterialProperties();
}
void MaterialWidget::on_colorMap3_colorChanged(QColor)
{
    SetMaterialProperties();
}
void MaterialWidget::on_gradientOffsetX_valueChanged(int value)
{
    SetMaterialProperties();
}
void MaterialWidget::on_gradientOffsetY_valueChanged(int value)
{
    SetMaterialProperties();
}
void MaterialWidget::on_gradientGamma_valueChanged(double)
{
    SetMaterialProperties();
}

void MaterialWidget::on_textureScaleX_valueChanged(double)
{
    SetMaterialProperties();
}
void MaterialWidget::on_textureScaleY_valueChanged(double)
{
    SetMaterialProperties();
}
void MaterialWidget::on_textureRotation_valueChanged(double)
{
    SetMaterialProperties();
}
void MaterialWidget::on_textureVelocityX_valueChanged(double)
{
    SetMaterialProperties();
}
void MaterialWidget::on_textureVelocityY_valueChanged(double)
{
    SetMaterialProperties();
}
void MaterialWidget::on_textureVelocityZ_valueChanged(double)
{
    SetMaterialProperties();
}
void MaterialWidget::on_textureMinFilter_currentIndexChanged(int)
{
    SetMaterialProperties();
}
void MaterialWidget::on_textureMagFilter_currentIndexChanged(int)
{
    SetMaterialProperties();
}
void MaterialWidget::on_textureWrapX_currentIndexChanged(int)
{
    SetMaterialProperties();
}
void MaterialWidget::on_textureWrapY_currentIndexChanged(int)
{
    SetMaterialProperties();
}
void MaterialWidget::on_rectX_valueChanged(double value)
{
    SetTextureRect();
}
void MaterialWidget::on_rectY_valueChanged(double value)
{
    SetTextureRect();
}
void MaterialWidget::on_rectW_valueChanged(double value)
{
    SetTextureRect();
}
void MaterialWidget::on_rectH_valueChanged(double value)
{
    SetTextureRect();
}
void MaterialWidget::on_chkAllowPacking_stateChanged(int)
{
    SetTextureFlags();
}
void MaterialWidget::on_chkAllowResizing_stateChanged(int)
{
    SetTextureFlags();
}
void MaterialWidget::on_chkPreMulAlpha_stateChanged(int)
{
    SetTextureFlags();
}
void MaterialWidget::on_chkBlurTexture_stateChanged(int)
{
    SetTextureFlags();
}
void MaterialWidget::on_chkDetectEdges_stateChanged(int)
{
    SetTextureFlags();
}
void MaterialWidget::on_cmbColorSpace_currentIndexChanged(int)
{
    if (auto* source = GetSelectedTextureSrc())
    {
        if (auto* ptr = dynamic_cast<gfx::TextureFileSource*>(source))
        {
            ptr->SetColorSpace(GetValue(mUI.cmbColorSpace));
        }
    }
}

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
            ShowTextureMapProperties();
            ShowTextureProperties();
            mUI.textures->scrollToItem(item);
            return;
        }
    }
}

void MaterialWidget::on_cmbModel_currentIndexChanged(int)
{
    mDrawable.reset();
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

    for (const auto& image: images)
    {
        QString image_name;
        QString image_file;
        gfx::FRect image_rect(0.0f, 0.0f, 1.0f, 1.0f);

        // If it's a sprite and we only have a single file it's
        // probably a spritesheet that contains the sprite animation
        // frames in some particular col,row arrangement. this is
        // handled with the single sheet sprite settings.
        if (map->GetType() == gfx::TextureMap::Type::Texture2D)
        {
            QString json_file;
            json_file = app::FindImageJsonFile(image);
            if (json_file.isEmpty())
            {
                const QFileInfo info(image);
                image_file = info.absoluteFilePath();
                image_name = info.baseName();
            }
            else
            {
                if (mMaterial->GetType() == gfx::MaterialClass::Type::Texture)
                {
                    DlgImgView dlg(this);
                    dlg.SetDialogMode();
                    dlg.show();
                    dlg.LoadImage(image);
                    dlg.LoadJson(json_file);
                    dlg.ResetTransform();
                    if (dlg.exec() == QDialog::Rejected)
                        return;
                    image_file = dlg.GetImageFileName();
                    image_name = dlg.GetImageName();
                    image_rect = ToGfx(dlg.GetImageRectF());
                }
                else if (mMaterial->GetType() == gfx::MaterialClass::Type::Tilemap)
                {
                    ImagePack pack;
                    if (ReadImagePack(json_file, &pack))
                    {
                        ImagePack::Tilemap map;
                        mMaterial->SetTileSize(glm::vec2(pack.tilemap.value_or(map).tile_width,
                                                         pack.tilemap.value_or(map).tile_height));
                        mMaterial->SetTileOffset(glm::vec2(pack.tilemap.value_or(map).xoffset,
                                                           pack.tilemap.value_or(map).yoffset));
                        mMaterial->SetTilePadding(glm::vec2(pack.padding, pack.padding));
                    }
                    const QFileInfo info(image);
                    image_file = info.absoluteFilePath();
                    image_name = info.baseName();
                }
            }
        }
        else
        {
            const QFileInfo info(image);
            image_file = info.absoluteFilePath();
            image_name = info.baseName();
        }

        const auto& uri = mWorkspace->MapFileToWorkspace(image_file);

        auto source = std::make_unique<gfx::TextureFileSource>(uri);
        source->SetName(app::ToUtf8(image_name));
        source->SetFileName(uri);
        source->SetColorSpace(gfx::TextureFileSource::ColorSpace::sRGB);

        if (map->GetType() == gfx::TextureMap::Type::Texture2D)
        {
            map->SetNumTextures(1);
            map->SetTextureSource(0, std::move(source));
            map->SetTextureRect(0, image_rect);
            if (map->GetSamplerName(0) == "kNormalMap")
                map->GetTextureSource(0)->SetColorSpace(gfx::TextureFileSource::ColorSpace::Linear);
        }
        else if (map->GetType() == gfx::TextureMap::Type::Sprite)
        {
            const auto textures = map->GetNumTextures();
            map->SetNumTextures(textures + 1);
            map->SetTextureSource(textures, std::move(source));
            map->SetTextureRect(textures, image_rect);
        }
    }

    ShowMaterialProperties();
    SelectLastItem(mUI.textures);
    ShowTextureMapProperties();
    ShowTextureProperties();
}

void MaterialWidget::AddNewTextureMapFromText()
{
    auto* map = GetSelectedTextureMap();
    if (map == nullptr)
        return;

    // anything set in this text buffer will be default
    // when the dialog is opened.
    gfx::TextBuffer text(100, 100);
    DlgText dlg(this, mWorkspace, text);
    if (dlg.exec() == QDialog::Rejected)
        return;

    bool replace_with_export = false;
    if (dlg.DidExport() && map->GetNumTextures() == 1)
    {
        QString file = dlg.GetSaveFile();
        QMessageBox msg(this);
        msg.setWindowTitle(tr("Replace Text With Image?"));
        msg.setText(tr("Do you want to replace the text with static PNG image you just saved?\n\n%1").arg(file));
        msg.setIcon(QMessageBox::Question);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        if (msg.exec() == QMessageBox::Yes)
            replace_with_export = true;
    }

    std::unique_ptr<gfx::TextureSource> texture_source;

    if (replace_with_export)
    {
        QString png = dlg.GetSaveFile();

        const QFileInfo info(png);
        const auto& name = info.baseName();
        const auto& file = mWorkspace->MapFileToWorkspace(info.absoluteFilePath());

        auto source = std::make_unique<gfx::TextureFileSource>(file);
        source->SetName(app::ToUtf8(name));
        source->SetFileName(file);
        source->SetColorSpace(gfx::TextureFileSource::ColorSpace::sRGB);
        texture_source = std::move(source);
    }
    else
    {
        // map the selected font files to the workspace.
        auto& style_and_text = text.GetText();
        style_and_text.font = mWorkspace->MapFileToWorkspace(style_and_text.font);

        auto source = std::make_unique<gfx::TextureTextBufferSource>(std::move(text));
        source->SetName("TextBuffer");

        texture_source = std::move(source);
    }

    if (map->GetType() == gfx::TextureMap::Type::Texture2D)
    {
        map->SetNumTextures(1);
        map->SetTextureSource(0, std::move(texture_source));
        map->SetTextureRect(0, gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
    }
    else if (map->GetType() == gfx::TextureMap::Type::Sprite)
    {
        auto textures = map->GetNumTextures();
        map->SetNumTextures(textures + 1);
        map->SetTextureSource(textures, std::move(texture_source));
        map->SetTextureRect(0, gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
    }

    ShowMaterialProperties();
    SelectLastItem(mUI.textures);
    ShowTextureMapProperties();
    ShowTextureProperties();
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
    auto source = std::make_unique<gfx::TextureBitmapGeneratorSource>(std::move(result));
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

    ShowMaterialProperties();
    SelectLastItem(mUI.textures);
    ShowTextureMapProperties();
    ShowTextureProperties();
}
void MaterialWidget::UniformValueChanged(const Uniform* uniform)
{
    //DEBUG("Uniform '%1' was modified.", uniform->GetName());
    SetMaterialProperties();
}

void MaterialWidget::ShaderFileChanged()
{
    const auto& uri = mMaterial->GetShaderUri();
    if (uri.empty())
        return;
    const auto& file = mWorkspace->MapFileToFilesystem(uri);

    DEBUG("Material shader was changed on file. Reloading.. [file='%1']", file);
    on_actionReloadShaders_triggered();
}

void MaterialWidget::ResourceUpdated(const app::Resource* resource)
{
    // we're interested here to realize an update that was done to
    // *this* material elsewhere and that elsewhere is the particle
    // editor.

    if (resource->GetIdUtf8() != mMaterial->GetId())
        return;

    // if we saved it, we already wrote the new hash value, so skip
    if (mOriginalHash == mMaterial->GetHash())
        return;

    mMaterial = resource->GetContent<gfx::MaterialClass>()->Copy();
    mMaterialInst.reset();

    ShowMaterialProperties();
    ShowTextureMapProperties();
    ShowTextureProperties();

    mOriginalHash = mMaterial->GetHash();
}

void MaterialWidget::CreateCustomShaderStub()
{
    QString name = GetValue(mUI.materialName);
    name.replace(' ', '_');
    name.replace('/', '_');
    name.replace('\\', '_');
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
// built-in uniforms

#version 300 es

// @uniforms

// bitset of material flags.
uniform uint kMaterialFlags;

// material time in seconds.
uniform float kTime;

// custom uniforms that need to match the json description
uniform vec4 kColor;
uniform sampler2D kNoise;
uniform vec2 kNoiseRect;

// @varyings

#ifdef DRAW_POINTS
  // when drawing points the gl_PointCoord must be used
  // for texture coordinates and we don't have any texture
  // coordinates coming from the vertex shader.
  #define vTexCoord gl_PointCoord
#else
  in vec2 vTexCoord;
#endif

// per particle data only exists when rendering particles
#ifdef GEOMETRY_IS_PARTICLES
  // per particle alpha value.
  in float vParticleAlpha;
  // particle random value.
  in float vParticleRandomValue;
  // normalized particle lifetime.
  in float vParticleTime;
  // Angle of particle's direction vector relative to X axis.
  in float vParticleAngle;
#else
   // we can support the editor and make the per particle data
   // dummies with macros
   #define vParticleAlpha 1.0
   #define vParticleRandomValue 0.0
   #define vParticleTime kTime
   #define vParticleAngle 0.0
#endif

// tile data only exists when rendering a tile batch
#ifdef GEOMETRY_IS_TILES
  in vec2 vTileData;
#endif

void FragmentShaderMain() {

    vec2 coords = vTexCoord;

    float a = texture(kNoise, coords).a;
    float r = coords.x + a + kTime;
    float g = coords.y + a;
    float b = kTime;
    vec3 col = vec3(0.5) + 0.5*cos(vec3(r, g, b));
    fs_out.color.rgb = col * kColor.rgb;
    fs_out.color.a   = 1.0;
}
)";
    constexpr auto json = R"(
{
  "uniforms": [
     {
        "type": "Color",
        "name": "kColor",
        "desc": "Color",
        "value": {"r":1.0, "g":1.0, "b":1.0, "a":1.0}
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
}

void MaterialWidget::CreateShaderStubFromSource(const char* source)
{
    QString name = GetValue(mUI.materialName);
    name.replace(' ', '_');
    name.replace('/', '_');
    name.replace('\\', '_');
    const auto& glsl_uri  = app::toString("ws://shaders/es2/%1.glsl", name);
    const auto& glsl_path = mWorkspace->MapFileToFilesystem("ws://shaders/es2");
    const auto& glsl_file = mWorkspace->MapFileToFilesystem(glsl_uri);

    if (FileExists(glsl_file))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Question);
        msg.setWindowTitle("File Exists");
        msg.setText(tr("A file by the same name already exists in the project folder.\n%1\nOverwrite file ?").arg(glsl_file));
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        if (msg.exec() == QMessageBox::No)
            return;
    }

    if (!app::MakePath(glsl_path))
    {
        ERROR("Failed to create path. [path='%1']", glsl_path);
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowTitle("Filesystem Error");
        msg.setText(tr("Failed to create file system path.\n%1").arg(glsl_path));
        msg.exec();
        return;
    }

    QString error_str;
    QFile::FileError err_val = QFile::FileError::NoError;
    if (!app::WriteTextFile(glsl_file, source, &err_val, &error_str))
    {
        ERROR("Failed to write shader glsl file. [file='%1', error=%2]", glsl_file, error_str);
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowTitle("Filesystem Error");
        msg.setText(tr("Failed to write file.\n%1\n%2").arg(glsl_file).arg(error_str));
        msg.exec();
        return;
    }
    mMaterial->SetShaderUri(app::ToUtf8(glsl_uri));
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
    const auto [parse_success, json_root, error] = base::JsonParseFile(mWorkspace->MapFileToFilesystem(uri));
    if (!parse_success)
    {
        ERROR("Failed to parse the shader description file '%1' %2", uri, error);
        return;
    }

    if (json_root.contains("uniforms"))
    {
        if (!mUI.customUniforms->layout())
            mUI.customUniforms->setLayout(new QGridLayout);
        auto* layout = qobject_cast<QGridLayout*>(mUI.customUniforms->layout());

        auto uniforms = mMaterial->GetUniforms();
        auto widget_row = 0;
        auto widget_col = 0;
        for (const auto& json : json_root["uniforms"].items())
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

            if (type == Uniform::Type::Int)
            {
                if (json_root.contains("meta"))
                {
                    const auto& meta = json_root["meta"];
                    if (meta.contains(name))
                    {
                        const auto& uniform_meta = meta[name];
                        std::string display;
                        base::JsonReadSafe(uniform_meta, "display", &display);
                        if (display == "combobox")
                        {
                            for (const auto& item : uniform_meta["values"].items())
                            {
                                std::string name;
                                int value = 0;
                                base::JsonReadSafe(item.value(), "name", &name);
                                base::JsonReadSafe(item.value(), "value", &value);
                                widget->AddComboValue(name, value);
                            }
                            widget->ShowIntAsCombo();
                        }
                    }
                }
            }

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
                base::JsonReadSafe(json.value(), "value", &value);
                mMaterial->SetUniform(name, value);
            }
            else if (type == Uniform::Type::Int)
            {
                int value = 0;
                base::JsonReadSafe(json.value(), "value", &value);
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

    if (json_root.contains("maps"))
    {
        std::set<std::string> texture_map_names;
        for (size_t i=0; i<mMaterial->GetNumTextureMaps(); ++i)
        {
            const auto* map = mMaterial->GetTextureMap(i);
            texture_map_names.insert(map->GetName());
        }

        auto widget_row = 0;
        auto widget_col = 0;
        for (const auto& json : json_root["maps"].items())
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
        if (auto* ptr = dynamic_cast<gfx::TextureFileSource*>(source))
        {
            ptr->SetFlag(gfx::TextureFileSource::Flags::AllowPacking, GetValue(mUI.chkAllowPacking));
            ptr->SetFlag(gfx::TextureFileSource::Flags::AllowResizing, GetValue(mUI.chkAllowResizing));
            ptr->SetFlag(gfx::TextureFileSource::Flags::PremulAlpha, GetValue(mUI.chkPreMulAlpha));
        }
        source->SetEffect(gfx::TextureSource::Effect::Blur, GetValue(mUI.chkBlurTexture));
        source->SetEffect(gfx::TextureSource::Effect::Edges, GetValue(mUI.chkDetectEdges));
    }
}

void MaterialWidget::SetMaterialProperties()
{
    mMaterial->SetFlag(gfx::MaterialClass::Flags::PremultipliedAlpha, GetValue(mUI.chkBlendPreMulAlpha));
    mMaterial->SetStatic(GetValue(mUI.chkStaticInstance));
    mMaterial->SetFlag(gfx::MaterialClass::Flags::EnableBloom, GetValue(mUI.chkEnableBloom));
    mMaterial->SetSurfaceType(GetValue(mUI.surfaceType));
    mMaterial->SetParticleEffect(GetValue(mUI.particleAction));
    mMaterial->SetTextureMinFilter(GetValue(mUI.textureMinFilter));
    mMaterial->SetTextureMagFilter(GetValue(mUI.textureMagFilter));
    mMaterial->SetTextureWrapX(GetValue(mUI.textureWrapX));
    mMaterial->SetTextureWrapY(GetValue(mUI.textureWrapY));
    mMaterial->SetBlendFrames(GetValue(mUI.chkBlendFrames));
    mMaterial->SetActiveTextureMap(GetItemId(mUI.activeMap));

    if (mMaterial->GetType() == gfx::MaterialClass::Type::Tilemap)
    {
        // the problem is that the normalized values suck for the UI
        // and they require a texture in order to normalize.

        // using absolute values (such as pixels) also sucks because
        // they require re-computation if the texture size changes
        // (for example when resampling/resizing when packing).
        //
        // It does seem that using absolute units is a bit simpler
        // since we can also then easily represent the values in
        // the user interface even without texture plus using them
        // is easier.

        glm::vec2 tile_size = {0.0f, 0.0f};
        tile_size.x = GetValue(mUI.tileWidth);
        tile_size.y = GetValue(mUI.tileHeight);

        glm::vec2 tile_offset = {0.0, 0.0f};
        tile_offset.x = GetValue(mUI.tileLeftOffset);
        tile_offset.y = GetValue(mUI.tileTopOffset);

        glm::vec2 tile_padding = {0.0f, 0.0f};
        tile_padding.x = GetValue(mUI.tileLeftPadding);
        tile_padding.y = GetValue(mUI.tileTopPadding);

        mMaterial->SetTileSize(tile_size);
        mMaterial->SetTileOffset(tile_offset);
        mMaterial->SetTilePadding(tile_padding);
    }
    else
    {
        mMaterial->DeleteUniform("kTileSize");
        mMaterial->DeleteUniform("kTileOffset");
        mMaterial->DeleteUniform("kTilePadding");
    }

    if (mMaterial->GetType() == gfx::MaterialClass::Type::Particle2D)
    {
        mMaterial->SetParticleStartColor(GetValue(mUI.particleStartColor));
        mMaterial->SetParticleMidColor(GetValue(mUI.particleMidColor));
        mMaterial->SetParticleEndColor(GetValue(mUI.particleEndColor));
        mMaterial->SetParticleBaseRotation(qDegreesToRadians((float)GetValue(mUI.particleBaseRotation)));
        mMaterial->SetParticleRotation(GetValue(mUI.particleRotationMode));
    }
    else
    {
        mMaterial->DeleteUniform("kParticleStartColor");
        mMaterial->DeleteUniform("kParticleMidColor");
        mMaterial->DeleteUniform("kParticleEndColor");
        mMaterial->DeleteUniform("kParticleBaseRotation");
        mMaterial->DeleteUniform("kParticleRotation");
    }

    if (mMaterial->GetType() == gfx::MaterialClass::Type::BasicLight)
    {
        mMaterial->SetDiffuseColor(GetValue(mUI.diffuseColor));
        mMaterial->SetAmbientColor(GetValue(mUI.ambientColor));
        mMaterial->SetSpecularColor(GetValue(mUI.specularColor));
        mMaterial->SetSpecularExponent(GetValue(mUI.specularExponent));
    }
    else
    {
        mMaterial->DeleteUniform("kDiffuseColor");
        mMaterial->DeleteUniform("kAmbientColor");
        mMaterial->DeleteUniform("kSpecularColor");
        mMaterial->DeleteUniform("kSpecularExponent");
    }

    if (mMaterial->GetType() == gfx::MaterialClass::Type::Gradient)
    {
        mMaterial->SetGradientType(GetValue(mUI.cmbGradientType));
        mMaterial->SetGradientGamma(GetValue(mUI.gradientGamma));

        const float gamma = GetValue(mUI.gradientGamma);
        if (math::equals(gamma, 1.0f))
            mMaterial->DeleteUniform("kGradientGamma");
    }

    // set of known uniforms if they differ from the defaults.
    // todo: this assumes implicit knowledge about the internals
    // of the material class. refactor these names away and the
    // default values away


    if (auto cutoff = mUI.alphaCutoff->GetValue())
        mMaterial->SetAlphaCutoff(cutoff.value());
    else mMaterial->DeleteUniform("kAlphaCutoff");

    glm::vec2 texture_scale;
    texture_scale.x = GetValue(mUI.textureScaleX);
    texture_scale.y = GetValue(mUI.textureScaleY);
    if (math::equals(texture_scale, glm::vec2(1.0f, 1.0f)))
        mMaterial->DeleteUniform("kTextureScale");
    else mMaterial->SetTextureScale(texture_scale);

    if (math::equals((float)GetValue(mUI.textureRotation), 0.0f))
        mMaterial->DeleteUniform("kTextureRotation");
    else mMaterial->SetTextureRotation(qDegreesToRadians((float)GetValue(mUI.textureRotation)));

    glm::vec2 linear_texture_velocity;
    linear_texture_velocity.x = GetValue(mUI.textureVelocityX);
    linear_texture_velocity.y = GetValue(mUI.textureVelocityY);
    const float angular_texture_velocity = qDegreesToRadians((float)GetValue(mUI.textureVelocityZ));
    if (math::equals(glm::vec3(linear_texture_velocity, angular_texture_velocity), glm::vec3(0.0f, 0.0f, 0.0f)))
        mMaterial->DeleteUniform("kTextureVelocity");
    else mMaterial->SetTextureVelocity(linear_texture_velocity, angular_texture_velocity);

    if (Equals(GetValue(mUI.colorMap0), gfx::Color::White))
        mMaterial->DeleteUniform(gfx::MaterialClass::GetColorUniformName(gfx::MaterialClass::ColorIndex::GradientColor0));
    else mMaterial->SetColor(GetValue(mUI.colorMap0), gfx::MaterialClass::ColorIndex::GradientColor0);

    if (Equals(GetValue(mUI.colorMap1), gfx::Color::White))
        mMaterial->DeleteUniform(gfx::MaterialClass::GetColorUniformName(gfx::MaterialClass::ColorIndex::GradientColor1));
    else mMaterial->SetColor(GetValue(mUI.colorMap1), gfx::MaterialClass::ColorIndex::GradientColor1);

    if (Equals(GetValue(mUI.colorMap2), gfx::Color::White))
        mMaterial->DeleteUniform(gfx::MaterialClass::GetColorUniformName(gfx::MaterialClass::ColorIndex::GradientColor2));
    else mMaterial->SetColor(GetValue(mUI.colorMap2), gfx::MaterialClass::ColorIndex::GradientColor2);

    if (Equals(GetValue(mUI.colorMap3), gfx::Color::White))
        mMaterial->DeleteUniform(gfx::MaterialClass::GetColorUniformName(gfx::MaterialClass::ColorIndex::GradientColor3));
    else mMaterial->SetColor(GetValue(mUI.colorMap3), gfx::MaterialClass::ColorIndex::GradientColor3);

    if (Equals(GetValue(mUI.baseColor), gfx::Color::White))
        mMaterial->DeleteUniform(gfx::MaterialClass::GetColorUniformName(gfx::MaterialClass::ColorIndex::BaseColor));
    else mMaterial->SetColor(GetValue(mUI.baseColor), gfx::MaterialClass::ColorIndex::BaseColor);

    glm::vec2 gradient_offset;
    gradient_offset.x = GetNormalizedValue(mUI.gradientOffsetX);
    gradient_offset.y = GetNormalizedValue(mUI.gradientOffsetY);
    if (math::equals(gradient_offset, glm::vec2(0.5f, 0.5f)))
        mMaterial->DeleteUniform("kGradientWeight");
    else mMaterial->SetGradientWeight(gradient_offset);

    for (auto* widget : mUniforms)
    {
        const auto& name = app::ToUtf8(widget->GetName());
        const auto type = widget->GetType();
        if (type == Uniform::Type::Float)
        {
            if (auto* val = mMaterial->FindUniformValue<float>(name))
                *val = widget->GetAsFloat();
            else mMaterial->SetUniform(name, widget->GetAsFloat());
        }
        else if (type == Uniform::Type::Vec2)
        {
            if (auto* val = mMaterial->FindUniformValue<glm::vec2>(name))
                *val = widget->GetAsVec2();
            else mMaterial->SetUniform(name, widget->GetAsVec2());
        }
        else if (type == Uniform::Type::Vec3)
        {
            if (auto* val = mMaterial->FindUniformValue<glm::vec3>(name))
                *val = widget->GetAsVec3();
            else mMaterial->SetUniform(name, widget->GetAsVec3());
        }
        else if (type == Uniform::Type::Vec4)
        {
            if (auto* val = mMaterial->FindUniformValue<glm::vec4>(name))
                *val = widget->GetAsVec4();
            else mMaterial->SetUniform(name, widget->GetAsVec4());
        }
        else if (type == Uniform::Type::Color)
        {
            if (auto* val = mMaterial->FindUniformValue<gfx::Color4f>(name))
                *val = ToGfx(widget->GetAsColor());
            else mMaterial->SetUniform(name, ToGfx(widget->GetAsColor()));
        }
        else if (type == Uniform::Type::Int)
        {
            if (auto* val = mMaterial->FindUniformValue<int>(name))
                *val = widget->GetAsInt();
            else mMaterial->SetUniform(name, widget->GetAsInt());
        } else BUG("Bug on uniform type.");
    }

    if (auto* map = GetSelectedTextureMap())
    {
        map->SetSpriteFrameRate(GetValue(mUI.spriteFps));
        map->SetSpriteLooping(GetValue(mUI.chkLooping));

        SetValue(mUI.spriteDuration, map->GetSpriteCycleDuration());

        if (GetValue(mUI.spriteSheet))
        {
            gfx::TextureMap::SpriteSheet sheet;
            sheet.rows = GetValue(mUI.spriteRows);
            sheet.cols = GetValue(mUI.spriteCols);
            map->SetSpriteSheet(sheet);
        } else map->ResetSpriteSheet();
    }
}

void MaterialWidget::ShowMaterialProperties()
{
    SetEnabled(mUI.shaderFile,         false);
    SetEnabled(mUI.actionSelectShader, false);
    SetEnabled(mUI.actionCreateShader, false);
    SetEnabled(mUI.actionEditShader,   false);
    SetEnabled(mUI.textureMaps,        false);
    SetEnabled(mUI.textureMap,         false);
    SetEnabled(mUI.textureProp,        false);
    SetEnabled(mUI.textureRect,        false);
    SetEnabled(mUI.btnAddShader,       false);
    SetEnabled(mUI.btnResetShader,     false);

    SetVisible(mUI.grpRenderFlags,     false);
    SetVisible(mUI.chkBlendPreMulAlpha,false);
    SetVisible(mUI.chkStaticInstance,  false);
    SetVisible(mUI.chkBlendFrames,     false);

    SetVisible(mUI.builtInProperties,  false);
    SetVisible(mUI.lblBaseColor,       false);
    SetVisible(mUI.baseColor,          false);
    SetVisible(mUI.alphaCutoff,        false);
    SetVisible(mUI.lblAlphaCutoff,     false);
    SetVisible(mUI.lblTileSize,        false);
    SetVisible(mUI.tileWidth,          false);
    SetVisible(mUI.tileHeight,         false);
    SetVisible(mUI.lblTileOffset,      false);
    SetVisible(mUI.tileLeftOffset,     false);
    SetVisible(mUI.tileTopOffset,      false);
    SetVisible(mUI.lblTilePadding,     false);
    SetVisible(mUI.tileLeftPadding,    false);
    SetVisible(mUI.tileTopPadding,     false);

    SetVisible(mUI.lblParticleStartColor,   false);
    SetVisible(mUI.lblParticleMidColor,     false);
    SetVisible(mUI.lblParticleEndColor,     false);
    SetVisible(mUI.lblParticleBaseRotation, false);
    SetVisible(mUI.lblParticleRotationMode, false);
    SetVisible(mUI.particleStartColor,      false);
    SetVisible(mUI.particleMidColor,        false);
    SetVisible(mUI.particleEndColor,        false);
    SetVisible(mUI.particleBaseRotation,    false);
    SetVisible(mUI.particleRotationMode,    false);

    SetVisible(mUI.lblParticleEffect,   false);
    SetVisible(mUI.particleAction,      false);
    SetVisible(mUI.lblActiveTextureMap, false);
    SetVisible(mUI.activeMap,           false);

    SetVisible(mUI.gradientMap,        false);
    SetVisible(mUI.textureCoords,      false);
    SetVisible(mUI.textureFilters,     false);
    SetVisible(mUI.customUniforms,     false);

    SetVisible(mUI.lblDiffuseColor,     false);
    SetVisible(mUI.lblAmbientColor,     false);
    SetVisible(mUI.lblSpecularColor,    false);
    SetVisible(mUI.lblSpecularExponent, false);
    SetVisible(mUI.ambientColor,        false);
    SetVisible(mUI.diffuseColor,        false);
    SetVisible(mUI.specularColor,       false);
    SetVisible(mUI.specularExponent,    false);

    SetVisible(mUI.gradientType,       false);
    SetVisible(mUI.cmbGradientType,    false);
    SetVisible(mUI.gradientGamma,      false);
    SetVisible(mUI.lblGradientGamma,   false);

    SetValue(mUI.materialName,         mMaterial->GetName());
    SetValue(mUI.materialID,           mMaterial->GetId());
    SetValue(mUI.materialType,         mMaterial->GetType());
    SetValue(mUI.surfaceType,          mMaterial->GetSurfaceType());
    SetValue(mUI.shaderFile,           mMaterial->GetShaderUri());
    SetValue(mUI.chkStaticInstance,    mMaterial->IsStatic());
    SetValue(mUI.chkEnableBloom, mMaterial->TestFlag(gfx::MaterialClass::Flags::EnableBloom));
    SetValue(mUI.chkBlendPreMulAlpha,  mMaterial->PremultipliedAlpha());
    SetValue(mUI.chkBlendFrames,       mMaterial->BlendFrames());

    // base
    SetValue(mUI.alphaCutoff,          mMaterial->GetAlphaCutoff());
    SetValue(mUI.baseColor,            mMaterial->GetBaseColor());

    // tilemap
    SetValue(mUI.tileWidth,            mMaterial->GetTileSize().x);
    SetValue(mUI.tileHeight,           mMaterial->GetTileSize().y);
    SetValue(mUI.tileLeftOffset,       mMaterial->GetTileOffset().x);
    SetValue(mUI.tileTopOffset,        mMaterial->GetTileOffset().y);
    SetValue(mUI.tileLeftPadding,      mMaterial->GetTilePadding().x);
    SetValue(mUI.tileTopPadding,       mMaterial->GetTilePadding().y);

    // particle
    SetValue(mUI.particleAction,       mMaterial->GetParticleEffect());
    SetValue(mUI.particleRotationMode, mMaterial->GetParticleRotation());
    SetValue(mUI.particleStartColor,   mMaterial->GetParticleStartColor());
    SetValue(mUI.particleMidColor,     mMaterial->GetParticleMidColor());
    SetValue(mUI.particleEndColor,     mMaterial->GetParticleEndColor());
    SetValue(mUI.particleBaseRotation, qRadiansToDegrees(mMaterial->GetParticleBaseRotation()));

    // gradient values.
    const auto& offset = mMaterial->GetGradientWeight();
    SetValue(mUI.colorMap0, mMaterial->GetColor(gfx::MaterialClass::ColorIndex::GradientColor0));
    SetValue(mUI.colorMap1, mMaterial->GetColor(gfx::MaterialClass::ColorIndex::GradientColor1));
    SetValue(mUI.colorMap2, mMaterial->GetColor(gfx::MaterialClass::ColorIndex::GradientColor2));
    SetValue(mUI.colorMap3, mMaterial->GetColor(gfx::MaterialClass::ColorIndex::GradientColor3));
    SetValue(mUI.gradientOffsetX, NormalizedFloat(offset.x));
    SetValue(mUI.gradientOffsetY, NormalizedFloat(offset.y));
    SetValue(mUI.cmbGradientType, mMaterial->GetGradientType());
    SetValue(mUI.gradientGamma, mMaterial->GetGradientGamma());

    /// basic light material
    SetValue(mUI.ambientColor, mMaterial->GetAmbientColor());
    SetValue(mUI.diffuseColor, mMaterial->GetDiffuseColor());
    SetValue(mUI.specularColor, mMaterial->GetSpecularColor());
    SetValue(mUI.specularExponent, mMaterial->GetSpecularExponent());

    SetValue(mUI.textureScaleX,        mMaterial->GetTextureScaleX());
    SetValue(mUI.textureScaleY,        mMaterial->GetTextureScaleY());
    SetValue(mUI.textureRotation,      qRadiansToDegrees(mMaterial->GetTextureRotation()));
    SetValue(mUI.textureVelocityX,     mMaterial->GetTextureVelocityX());
    SetValue(mUI.textureVelocityY,     mMaterial->GetTextureVelocityY());
    SetValue(mUI.textureVelocityZ,     qRadiansToDegrees(mMaterial->GetTextureVelocityZ()));
    SetValue(mUI.textureMinFilter,     mMaterial->GetTextureMinFilter());
    SetValue(mUI.textureMagFilter,     mMaterial->GetTextureMagFilter());
    SetValue(mUI.textureWrapX,         mMaterial->GetTextureWrapX());
    SetValue(mUI.textureWrapY,         mMaterial->GetTextureWrapY());

    ClearList(mUI.activeMap);

    mUI.alphaCutoff->ClearValue();
    if (mMaterial->HasUniform("kAlphaCutoff"))
        SetValue(mUI.alphaCutoff, mMaterial->GetAlphaCutoff());

    SetVisible(mUI.lblTileIndex, false);
    SetVisible(mUI.kTileIndex,   false);

    if (mMaterial->HasShaderUri())
    {
        const auto& uri = mMaterial->GetShaderUri();
        const auto& file = mWorkspace->MapFileToFilesystem(uri);
        // ignores duplicates
        mFileWatcher.addPath(file);
    }

    if (mMaterial->GetType() == gfx::MaterialClass::Type::Custom)
    {
        SetPlaceholderText(mUI.shaderFile, "None Selected");
        SetEnabled(mUI.btnAddShader,       true);
        SetEnabled(mUI.shaderFile,         true);
        SetEnabled(mUI.actionSelectShader, true);
        SetEnabled(mUI.actionCreateShader, true);
        SetEnabled(mUI.actionCustomizeShader, false);
        SetEnabled(mUI.actionEditShader,   mMaterial->HasShaderUri());
        SetEnabled(mUI.btnResetShader,     mMaterial->HasShaderUri());
        SetVisible(mUI.grpRenderFlags,     false);
    }
    else
    {
        ClearCustomUniforms();

        SetPlaceholderText(mUI.shaderFile, "Using The Built-in Shader");
        SetEnabled(mUI.btnAddShader,        true);
        SetVisible(mUI.grpRenderFlags,      true);
        SetVisible(mUI.chkStaticInstance,   true);
        SetEnabled(mUI.actionCustomizeShader, true);
        SetEnabled(mUI.btnResetShader, mMaterial->HasShaderSrc());

        if (mMaterial->GetType() == gfx::MaterialClass::Type::BasicLight)
        {
            SetVisible(mUI.builtInProperties,   true);
            SetVisible(mUI.lblDiffuseColor,     true);
            SetVisible(mUI.lblAmbientColor,     true);
            SetVisible(mUI.lblSpecularColor,    true);
            SetVisible(mUI.lblSpecularExponent, true);
            SetVisible(mUI.ambientColor,        true);
            SetVisible(mUI.diffuseColor,        true);
            SetVisible(mUI.specularColor,       true);
            SetVisible(mUI.specularExponent,    true);
        }
        else if (mMaterial->GetType() == gfx::MaterialClass::Type::Color)
        {
            SetVisible(mUI.builtInProperties,   true);
            SetVisible(mUI.baseColor,           true);
            SetVisible(mUI.lblBaseColor,        true);
        }
        else if (mMaterial->GetType() == gfx::MaterialClass::Type::Gradient)
        {
            SetVisible(mUI.builtInProperties, true);
            SetVisible(mUI.gradientMap,       true);
            SetVisible(mUI.gradientType,      true);
            SetVisible(mUI.cmbGradientType,   true);
            SetVisible(mUI.gradientGamma,     true);
            SetVisible(mUI.lblGradientGamma,  true);
        }
        else if (mMaterial->GetType() == gfx::MaterialClass::Type::Texture ||
                 mMaterial->GetType() == gfx::MaterialClass::Type::Sprite)
        {
            SetVisible(mUI.builtInProperties,   true);
            SetVisible(mUI.lblBaseColor,        true);
            SetVisible(mUI.baseColor,           true);
            SetVisible(mUI.lblAlphaCutoff,      true);
            SetVisible(mUI.alphaCutoff,         true);
            SetVisible(mUI.lblParticleEffect,   true);
            SetVisible(mUI.particleAction,      true);
            SetVisible(mUI.lblActiveTextureMap, true);
            SetVisible(mUI.activeMap,           true);
            SetVisible(mUI.textureCoords,       true);
            SetVisible(mUI.textureFilters,      true);
        }
        else if (mMaterial->GetType() == gfx::MaterialClass::Type::Tilemap)
        {
            SetVisible(mUI.builtInProperties,   true);
            SetVisible(mUI.lblBaseColor,        true);
            SetVisible(mUI.baseColor,           true);
            SetVisible(mUI.lblAlphaCutoff,      true);
            SetVisible(mUI.alphaCutoff,         true);
            SetVisible(mUI.lblTileSize,         true);
            SetVisible(mUI.tileWidth,           true);
            SetVisible(mUI.tileHeight,          true);
            SetVisible(mUI.lblTileOffset,       true);
            SetVisible(mUI.tileLeftOffset,      true);
            SetVisible(mUI.tileTopOffset,       true);
            SetVisible(mUI.lblTilePadding,      true);
            SetVisible(mUI.tileLeftPadding,     true);
            SetVisible(mUI.tileTopPadding,      true);
            SetVisible(mUI.lblActiveTextureMap, true);
            SetVisible(mUI.activeMap,           true);
            SetVisible(mUI.textureFilters,      true);
            SetVisible(mUI.lblTileIndex,        true);
            SetVisible(mUI.kTileIndex,          true);
        }
        else if (mMaterial->GetType() == gfx::MaterialClass::Type::Particle2D)
        {
            SetVisible(mUI.builtInProperties,       true);
            SetVisible(mUI.lblParticleStartColor,   true);
            SetVisible(mUI.lblParticleEndColor,     true);
            SetVisible(mUI.lblParticleMidColor,     true);
            SetVisible(mUI.lblParticleBaseRotation, true);
            SetVisible(mUI.lblParticleRotationMode, true);
            SetVisible(mUI.particleStartColor,      true);
            SetVisible(mUI.particleMidColor,        true);
            SetVisible(mUI.particleEndColor,        true);
            SetVisible(mUI.particleBaseRotation,    true);
            SetVisible(mUI.particleRotationMode,    true);
            SetVisible(mUI.textureFilters,          true);
            SetVisible(mUI.lblActiveTextureMap,     true);
            SetVisible(mUI.activeMap,               true);
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
            if (const auto* val = mMaterial->FindUniformValue<float>(name))
                widget->SetValue(*val);
            else if (const auto* val = mMaterial->FindUniformValue<glm::vec2>(name))
                widget->SetValue(*val);
            else if (const auto* val = mMaterial->FindUniformValue<glm::vec3>(name))
                widget->SetValue(*val);
            else if (const auto* val = mMaterial->FindUniformValue<glm::vec4>(name))
                widget->SetValue(*val);
            else if (const auto* val = mMaterial->FindUniformValue<gfx::Color4f>(name))
                widget->SetValue(FromGfx(*val));
            else if (const auto* val = mMaterial->FindUniformValue<int>(name))
                widget->SetValue(*val);
            else BUG("No such uniform in material. UI and material are out of sync.");
        }
    }

    if (mMaterial->GetNumTextureMaps())
    {
        SetVisible(mUI.textureFilters, true);
        SetEnabled(mUI.textureMaps,    true);

        std::vector<ResourceListItem> maps;
        std::vector<ResourceListItem> items;
        for (unsigned i=0; i<mMaterial->GetNumTextureMaps(); ++i)
        {
            const auto* map = mMaterial->GetTextureMap(i);
            if (map->GetType() == gfx::TextureMap::Type::Sprite)
                SetVisible(mUI.chkBlendFrames, true);

            auto name = map->GetName();
            if (name.empty())
                name = "--unnamed";

            ResourceListItem item;
            item.id   = map->GetId();
            item.name = name;
            item.tag  = "map";
            maps.push_back(item);
            items.push_back(item);

            for (unsigned j=0; j<map->GetNumTextures(); ++j)
            {
                const auto* source = map->GetTextureSource(j);

                std::string name = source->GetName();
                if (name.empty())
                    "--unnamed";

                ResourceListItem item;
                item.id   = source->GetId();
                item.name = "    " + name;
                item.tag  = "texture";
                items.push_back(item);
            }
        }
        SetList(mUI.textures,  items);
        SetList(mUI.activeMap, maps);
        SetValue(mUI.activeMap, ListItemId(mMaterial->GetActiveTextureMap()));
    }
    else
    {
        ClearList(mUI.textures);
    }
}

void MaterialWidget::ShowTextureProperties()
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
    SetEnabled(mUI.chkDetectEdges,    false);
    SetEnabled(mUI.cmbColorSpace,     false);

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
    if (const auto* ptr = dynamic_cast<const gfx::TextureFileSource*>(source))
    {
        SetValue(mUI.textureSourceFile, ptr->GetFilename());
        SetValue(mUI.cmbColorSpace,     ptr->GetColorSpace());
        SetValue(mUI.chkAllowPacking,   ptr->TestFlag(gfx::TextureFileSource::Flags::AllowPacking));
        SetValue(mUI.chkAllowResizing,  ptr->TestFlag(gfx::TextureFileSource::Flags::AllowResizing));
        SetValue(mUI.chkPreMulAlpha,    ptr->TestFlag(gfx::TextureFileSource::Flags::PremulAlpha));
        SetEnabled(mUI.chkAllowPacking,  true);
        SetEnabled(mUI.chkAllowResizing, true);
        SetEnabled(mUI.chkPreMulAlpha,   true);
        SetEnabled(mUI.cmbColorSpace,    true);
    }

    SetEnabled(mUI.chkBlurTexture, true);
    SetEnabled(mUI.chkDetectEdges, true);
    SetValue(mUI.chkBlurTexture, source->TestEffect(gfx::TextureSource::Effect::Blur));
    SetValue(mUI.chkDetectEdges, source->TestEffect(gfx::TextureSource::Effect::Edges));

}

void MaterialWidget::ShowTextureMapProperties()
{
    SetEnabled(mUI.textureMap, false);
    SetValue(mUI.textureMapID,       QString(""));
    SetValue(mUI.textureMapName,     QString(""));
    SetValue(mUI.textureMapType,     gfx::TextureMap::Type::Texture2D);
    SetValue(mUI.textureMapTextures, QString(""));
    SetValue(mUI.spriteFps,          0.0f);
    SetValue(mUI.spriteDuration,     0.0f);
    SetValue(mUI.spriteRows,         0);
    SetValue(mUI.spriteCols,         0);
    SetValue(mUI.spriteSheet,         false);
    SetValue(mUI.chkLooping,          false);
    SetVisible(mUI.chkLooping,        false);
    SetEnabled(mUI.spriteFps,         false);
    SetEnabled(mUI.spriteSheet,       false);
    SetEnabled(mUI.spriteDuration,    false);
    SetVisible(mUI.spriteSheet,       false);
    SetVisible(mUI.lblSpriteFps,      false);
    SetVisible(mUI.spriteFps,         false);
    SetVisible(mUI.lblSpriteDuration, false);
    SetVisible(mUI.spriteDuration,    false);
    SetVisible(mUI.textureMapFlags,   false);

    const auto* map = GetSelectedTextureMap();
    if (map == nullptr)
        return;

    SetEnabled(mUI.textureMap, true);
    SetValue(mUI.textureMapID,    map->GetId());
    SetValue(mUI.textureMapName,  map->GetName());
    SetValue(mUI.textureMapType,  map->GetType());
    SetValue(mUI.spriteDuration,  map->GetSpriteCycleDuration());
    SetValue(mUI.spriteFps,       map->GetSpriteFrameRate());
    SetValue(mUI.chkLooping,      map->IsSpriteLooping());

    if (map->GetType() == gfx::TextureMap::Type::Sprite)
    {
        SetEnabled(mUI.spriteDuration,    true);
        SetEnabled(mUI.spriteFps,         true);
        SetEnabled(mUI.spriteSheet,       true);
        SetVisible(mUI.chkLooping,        true);
        SetVisible(mUI.lblSpriteFps,      true);
        SetVisible(mUI.spriteFps,         true);
        SetVisible(mUI.spriteSheet,       true);
        SetVisible(mUI.spriteDuration,    true);
        SetVisible(mUI.lblSpriteDuration, true);
        SetVisible(mUI.textureMapFlags,   true);
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
        if (const auto* ptr = dynamic_cast<const gfx::TextureFileSource*>(src))
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

    const auto type =  mMaterial->GetType();
    const auto scene = (PreviewScene)GetValue(mUI.cmbScene);

    // check whether we have all the textures that are needed.
    // basic light material has optional texture maps.
    if (type != gfx::MaterialClass::Type::BasicLight)
    {
        for (unsigned i = 0; i < mMaterial->GetNumTextureMaps(); ++i)
        {
            const auto& map = mMaterial->GetTextureMap(i);
            if (map->GetNumTextures() > 0)
                continue;

            ShowMessage(base::FormatString("Missing texture map '%1' texture", map->GetName()), painter);
            return;
        }
    }

    // check we have shader
    if (mMaterial->GetType() == gfx::MaterialClass::Type::Custom)
    {
        const auto& uri = mMaterial->GetShaderUri();
        if (uri.empty())
        {
            ShowMessage("No shader has been selected.", painter);
            return;
        }
    }

    // try to figure out aspect ratio... dunno with multiple textures
    // we could theoretically have different sizes.. so?
    float texture_width_sum  = 0;
    float texture_height_sum = 0;
    unsigned texture_count = 0;
    const auto& active_texture_map_id = mMaterial->GetActiveTextureMap();
    if (const auto* active_texture_map = mMaterial->FindTextureMapById(active_texture_map_id))
    {
        unsigned sprite_rows = 0;
        unsigned sprite_cols = 0;
        if (const auto* sprite_sheet = active_texture_map->GetSpriteSheet())
        {
            sprite_rows = sprite_sheet->rows;
            sprite_cols = sprite_sheet->cols;
        }

        for (size_t i=0; i<active_texture_map->GetNumTextures(); ++i)
        {
            const auto& texture_source = active_texture_map->GetTextureSource(i);
            const auto& texture_rect = active_texture_map->GetTextureRect(i);
            const auto& texture_gpu_id = texture_source->GetGpuId();
            if (const auto* texture = painter.GetDevice()->FindTexture(texture_gpu_id))
            {
                const auto rect_width = texture_rect.GetWidth();
                const auto rect_height = texture_rect.GetHeight();
                const auto frame_width  = sprite_cols ? rect_width / (float)sprite_cols : rect_width;
                const auto frame_height = sprite_rows ? rect_height / (float)sprite_rows : rect_height;

                texture_width_sum  += (texture->GetWidthF() * frame_width);
                texture_height_sum += (texture->GetHeightF() * frame_height);
                texture_count++;
            }
        }
    }
    if (texture_width_sum == 0 || texture_height_sum == 0)
    {
        texture_width_sum  = width;
        texture_height_sum = height;
    }

    float aspect_ratio = 1.0f;
    if (texture_count)
    {
        const auto avg_width = texture_width_sum / float(texture_count);
        const auto avg_height = texture_height_sum / float(texture_count);
        if (avg_height > 0.0f && avg_width > 0.0f)
            aspect_ratio = avg_width / avg_height;
    }

    if (mMaterial->GetType() == gfx::MaterialClass::Type::Tilemap)
    {
        const auto tile_size = mMaterial->GetTileSize();
        const auto tile_width = tile_size.x > 0.0f ? (unsigned)tile_size.x : 0u;
        const auto tile_height = tile_size.y > 0.0f ? (unsigned)tile_size.y : 0u;
        if (tile_width && tile_height)
            aspect_ratio = float(tile_width) / float(tile_height);
    }

    const auto time = mState == PlayState::Playing || mState == PlayState::Paused
                      ? mTime : GetValue(mUI.kTime);
    const auto zoom = (float)GetValue(mUI.zoom);

    if (!mDrawable)
    {
        mDrawable = mWorkspace->MakeDrawableById(GetItemId(mUI.cmbModel));
    }

    if (!mMaterialInst)
        mMaterialInst = std::make_unique<gfx::MaterialInstance>(mMaterial);

    mMaterialInst->SetRuntime(time);
    mMaterialInst->SetUniform("kTileIndex", (float)GetValue(mUI.kTileIndex));

    mUI.sprite->SetTime(time);

    if (gfx::Is3DShape(*mDrawable))
    {
        const auto aspect = (float)width / (float)height;
        constexpr auto Fov = 45.0f;
        constexpr auto Far = 10000.0f;
        const auto half_width = width*0.5f;
        const auto half_height = height*0.5f;
        const auto ortho = glm::ortho(-half_width, half_width, -half_height, half_height, -1000.0f, 1000.0f);
        const auto near = half_height / std::tan(glm::radians(Fov*0.5f));
        const auto projection = ortho *
                glm::translate(glm::mat4(1.0f), glm::vec3 { 0.0f, 0.0f, -1000.0f} ) *
                glm::inverse(ortho) *
                glm::perspective(glm::radians(Fov), aspect, near, Far) *
                glm::translate(glm::mat4(1.0f), glm::vec3 { 0.0f, 0.0f, -near });

        const auto size = std::min(half_width, half_height);

        gfx::Transform transform;
        transform.Resize(size, size, size);
        transform.Scale(zoom, zoom, zoom);
        transform.RotateAroundY(mModelRotationTotal.x + mModelRotationDelta.x);
        transform.RotateAroundX(mModelRotationTotal.y + mModelRotationDelta.y);
        transform.Translate(0.0f, 0.0f, -size*0.5f);
        if (gfx::Is2DShape(*mDrawable))
        {
            transform.Push();
              transform.Translate(-0.5f, -0.5f);
              transform.RotateAroundX(gfx::FDegrees(90.0f));
        }

        gfx::Painter p(painter);
        p.ResetViewMatrix();
        p.SetProjectionMatrix(projection);

        gfx::Painter::DrawState state;
        state.depth_test = gfx::Painter::DepthTest::LessOrEQual;
        state.culling    = gfx::Painter::Culling::Back;
        state.line_width = 4.0f;

        if (scene == PreviewScene::BasicShading)
        {
            gfx::BasicLightProgram program;
            gfx::BasicLightProgram::Light light;
            light.type = gfx::BasicLightProgram::LightType::Directional;
            light.position = glm::vec3 { 0.0f, size, -size*0.5f };
            light.ambient_color = gfx::Color4f(gfx::Color::White) * 0.5f;
            light.diffuse_color = gfx::Color4f(gfx::Color::White) * 1.0f;
            light.specular_color = gfx::Color4f(gfx::Color::White) * 1.0f;
            light.direction = glm::vec3 { 0.0f, -1.0f, 0.0f };
            light.quadratic_attenuation = 0.00005;
            light.spot_half_angle = gfx::FDegrees(35.0f);
            program.AddLight(light);
            program.SetCameraCenter(0.0f, 0.0f, 0.0f);
            p.Draw(*mDrawable, transform, *mMaterialInst, state, program);

            if (Editor::DebugEditor())
            {
                gfx::FlatShadedColorProgram program;
                p.Draw(gfx::NormalMeshInstance(mDrawable), transform,
                       gfx::CreateMaterialFromColor(gfx::Color::HotPink), state, program);
            }

            if (light.type == gfx::BasicLightProgram::LightType::Point ||
                light.type == gfx::BasicLightProgram::LightType::Spot)
            {
                gfx::FlatShadedColorProgram program;
                gfx::Transform transform;
                transform.Resize(20.0f, 20.0f, 20.0f);
                transform.Scale(zoom, zoom, zoom);
                transform.Translate(0.0f, size, -size*0.5f);
                p.Draw(gfx::Cube(), transform, gfx::CreateMaterialFromColor(gfx::Color::White), state, program);
            }
        }
        else
        {
            gfx::FlatShadedColorProgram program;
            p.Draw(*mDrawable, transform, *mMaterialInst, state, program);
        }

        {
            gfx::FlatShadedColorProgram program;
            gfx::LineBatch3D lines;
            lines.AddLine({0.0f, 0.0f, 0.0f}, {0.75f, 0.0f, 0.0f});
            p.Draw(lines, transform, gfx::CreateMaterialFromColor(gfx::Color::DarkGreen), state, program);
        }
        {
            gfx::FlatShadedColorProgram program;
            gfx::LineBatch3D lines;
            lines.AddLine({0.0f, 0.0f, 0.0f}, {0.0f, 0.75f, 0.0f});
            p.Draw(lines, transform, gfx::CreateMaterialFromColor(gfx::Color::DarkRed), state, program);
        }

        {
            gfx::FlatShadedColorProgram program;
            gfx::LineBatch3D lines;
            lines.AddLine({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.75f});
            p.Draw(lines, transform, gfx::CreateMaterialFromColor(gfx::Color::DarkBlue), state, program);
        }


        if (Editor::DebugEditor())
        {
            state.winding = gfx::Painter::WindigOrder::ClockWise;

            gfx::FlatShadedColorProgram program;

            gfx::Transform transform;
            transform.Resize(size*zoom, size*zoom);
            transform.MoveTo(0.0f, 0.0f, 0.0f);
            transform.Push();
              transform.Translate(-0.5f, -0.5f);

            p.SetProjectionMatrix(ortho);
            p.Draw(gfx::Rectangle(gfx::SimpleShape::Style::Outline), transform,
                   gfx::CreateMaterialFromColor(gfx::Color::DarkGray), state, program);
        }

    }
    else
    {
        const auto content_width  = texture_width_sum * aspect_ratio;
        const auto content_height = texture_width_sum;
        const auto window_scaler = std::min((float)width/content_width, (float)height/content_height);
        const auto actual_width  = content_width * window_scaler * zoom;
        const auto actual_height = content_height * window_scaler * zoom;
        const auto xpos = (width - actual_width) * 0.5f;
        const auto ypos = (height - actual_height) * 0.5f;

        gfx::Transform transform;
        transform.MoveTo(xpos, ypos);
        transform.Resize(actual_width, actual_height);

        if (scene == PreviewScene::BasicShading)
        {
            glm::vec3 light_position = glm::vec3{width * 0.5, height*0.5, -100.0f};
            light_position += mLightPositionTotal;
            light_position += mLightPositionDelta;

            gfx::BasicLightProgram program;
            gfx::BasicLightProgram::Light light;
            light.type = gfx::BasicLightProgram::LightType::Point;
            light.position = light_position;
            light.ambient_color = gfx::Color4f(gfx::Color::White) * 0.5f;
            light.diffuse_color = gfx::Color4f(gfx::Color::White) * 1.0f;
            light.specular_color = gfx::Color4f(gfx::Color::White) * 1.0f;
            light.direction = glm::vec3 { 0.0f, 1.0f, 0.0f };
            light.quadratic_attenuation = 0.00005;
            light.spot_half_angle = gfx::FDegrees(35.0f);
            program.AddLight(light);
            program.SetCameraCenter(width*0.5f, height*0.5f, -10000.0f);

            gfx::Painter::DrawState state;
            painter.Draw(*mDrawable, transform, *mMaterialInst, state, program);

            if (light.type == gfx::BasicLightProgram::LightType::Point ||
                light.type == gfx::BasicLightProgram::LightType::Spot)
            {
                static std::shared_ptr<gfx::MaterialClass> light_material;
                if (!light_material)
                {
                    light_material = std::make_shared<gfx::MaterialClass>(gfx::CreateMaterialClassFromImage(res::LightIcon));
                    light_material->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
                }

                gfx::FlatShadedColorProgram program;
                gfx::Transform transform;
                transform.Resize(40.0f, 40.0f);
                transform.Translate(light_position.x, light_position.y);
                transform.Translate(-20.0f, -20.0f);
                painter.Draw(gfx::Rectangle(), transform, gfx::MaterialInstance(light_material), state, program);
            }
        }
        else
        {
            painter.Draw(*mDrawable, transform, *mMaterialInst);
        }
    }

    if (mMaterialInst->HasError())
    {
        ShowError("Error in material!", gfx::FPoint(10.0f, 10.0f), painter);
    }
    if (painter.GetErrorCount())
    {
        ShowMessage("Shader compile error:", gfx::FPoint(10.0f, 10.0f), painter);
        ShowMessage(painter.GetError(0), gfx::FPoint(10.0f, 30.0f), painter);
    }

    if (type == gfx::MaterialClass::Type::Sprite && mState == PlayState::Playing)
    {
        for (unsigned i=0; i<mMaterial->GetNumTextureMaps(); ++i)
        {
            const auto* map = mMaterial->GetTextureMap(i);
            if (!map->IsSpriteMap())
                continue;

            ShowMessage(base::FormatString("Press %1 for '%2'", i, map->GetName()), gfx::FPoint(20.0f, 20.0f  + i * 20.0f), painter);
        }
    }
}

void MaterialWidget::MouseMove(QMouseEvent* mickey)
{
    if (mMouseState == MouseState::RotateModel)
    {
        const auto mouse_movement = mickey->pos() - mMouseDownPoint;
        const auto mouse_dx = mouse_movement.x();
        const auto mouse_dy = mouse_movement.y();
        mModelRotationDelta.x = mouse_dx * 0.002f;
        mModelRotationDelta.y = mouse_dy * 0.002f;
    }
    else if (mMouseState == MouseState::MoveLight)
    {
        const auto mouse_movement = mickey->pos() - mMouseDownPoint;
        const auto mouse_dx = mouse_movement.x();
        const auto mouse_dy = mouse_movement.y();
        mLightPositionDelta.x = mouse_dx;
        mLightPositionDelta.y = mouse_dy;
    }
}

void MaterialWidget::MousePress(QMouseEvent* mickey)
{
    if (mickey->button() == Qt::RightButton)
    {
        mMouseDownPoint = mickey->pos();
        mMouseState = MouseState::RotateModel;
    }
    else if (mickey->button() == Qt::LeftButton)
    {
        const auto width = mUI.widget->width();
        const auto height = mUI.widget->height();
        const auto xpos = width*.5f;
        const auto ypos = height*0.5f;

        mMouseDownPoint = mickey->pos();
        mLightPositionTotal.x = mMouseDownPoint.x() - xpos;
        mLightPositionTotal.y = mMouseDownPoint.y() - ypos;
        mMouseState = MouseState::MoveLight;
    }
}
void MaterialWidget::MouseRelease(QMouseEvent* mickey)
{
    if (mMouseState == MouseState::RotateModel)
    {
        mModelRotationTotal += mModelRotationDelta;
        mModelRotationDelta = glm::vec3{0.0f, 0.0f, 0.0f};
    }
    else if (mMouseState == MouseState::MoveLight)
    {
        mLightPositionTotal += mLightPositionDelta;
        mLightPositionDelta = glm::vec3{0.0f, 0.0f, 0.0f};
    }
    mMouseState = MouseState::Nada;
}

bool MaterialWidget::KeyPress(QKeyEvent* key)
{
    if (mMaterial->GetType() != gfx::MaterialClass::Type::Sprite)
        return false;
    if (mState != PlayState::Playing)
        return false;

    static const std::unordered_map<int, unsigned> map {
        {Qt::Key_0, 0},
        {Qt::Key_1, 1},
        {Qt::Key_2, 2},
        {Qt::Key_3, 3},
        {Qt::Key_4, 4},
        {Qt::Key_5, 5},
        {Qt::Key_6, 6},
        {Qt::Key_7, 7},
        {Qt::Key_8, 8},
        {Qt::Key_9, 9}
    };
    auto it = map.find(key->key());
    if (it == map.end())
        return false;

    const auto index = it->second;
    if (index >= mMaterial->GetNumTextureMaps())
        return false;

    const auto* texture_map = mMaterial->GetTextureMap(index);
    if (!texture_map->IsSpriteMap())
        return false;

    gfx::Material::Environment env;
    env.editing_mode = true;
    env.draw_primitive = gfx::DrawPrimitive::Triangles;
    env.draw_category  = gfx::DrawCategory::Basic;
    env.renderpass     = gfx::RenderPass::ColorPass;

    gfx::Material::Command cmd;
    cmd.name = "RunSpriteCycle";
    cmd.args["id"]  = texture_map->GetId();
    cmd.args["delay"] = 0.0f;
    mMaterialInst->Execute(env, cmd);
    return true;
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
