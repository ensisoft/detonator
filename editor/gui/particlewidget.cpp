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

#define LOGTAG "particle"

#include "config.h"

#include "warnpush.h"
#  include <QtMath>
#  include <QFileDialog>
#  include <QMessageBox>
#  include <QTextStream>
#  include <base64/base64.h>
#include "warnpop.h"

#include "base/math.h"
#include "data/json.h"
#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/material_class.h"
#include "graphics/material_instance.h"
#include "graphics/transform.h"
#include "graphics/drawing.h"
#include "graphics/types.h"
#include "graphics/simple_shape.h"
#include "graphics/particle_engine.h"
#include "graphics/texture_file_source.h"
#include "editor/app/eventlog.h"
#include "editor/app/resource.h"
#include "editor/app/utility.h"
#include "editor/app/workspace.h"
#include "editor/gui/main.h"
#include "editor/gui/utility.h"
#include "editor/gui/settings.h"
#include "editor/gui/drawing.h"
#include "editor/gui/utility.h"
#include "editor/gui/particlewidget.h"
#include "editor/gui/dlgmaterial.h"
#include "editor/gui/dlgparticle.h"
#include "editor/gui/tool.h"
#include "editor/gui/translation.h"

namespace gui
{
struct ParticleEditorWidget::UIState {
    float emitter_xpos = 0.0f;
    float emitter_ypos = 0.0f;
    float emitter_width  = 0.0f;
    float emitter_height = 0.0f;
    float visualization_xpos   = 0.0f;
    float visualization_ypos   = 0.0f;
    float visualization_width  = 0.0f;
    float visualization_height = 0.0f;
    float visualization_rotation = 0.0f;
};

class ParticleEditorWidget::MoveEmitterTool : public MouseTool
{
public:
    MoveEmitterTool(UIState& state) : mState(state)
    {}
    virtual void MouseMove(const MouseEvent& mickey, gfx::Transform& view) override
    {
        const float viz_width  = mState.visualization_width;
        const float viz_height = mState.visualization_height;
        const float viz_rot    = mState.visualization_rotation;
        view.Push();
        view.Scale(viz_width, viz_height);
        view.Translate(-viz_width*0.5, -viz_height*0.5);
        view.RotateAroundZ(viz_rot);

        const auto& local_to_view  = view.GetAsMatrix();
        const auto& view_to_local  = glm::inverse(local_to_view);
        const auto& coord_in_local = view_to_local * ToVec4(mickey->pos());
        const auto& mouse_delta    = coord_in_local - mMousePos;

        const auto max_xpos = 1.0f - mState.emitter_width;
        const auto max_ypos = 1.0f - mState.emitter_height;
        mState.emitter_xpos = math::clamp(0.0f, max_xpos, mState.emitter_xpos + mouse_delta.x);
        mState.emitter_ypos = math::clamp(0.0f, max_ypos, mState.emitter_ypos + mouse_delta.y);
        mMousePos = coord_in_local;
        view.Pop();
    }
    virtual void MousePress(const MouseEvent& mickey, gfx::Transform& view) override
    {
        const float viz_width  = mState.visualization_width;
        const float viz_height = mState.visualization_height;
        const float viz_rot    = mState.visualization_rotation;
        view.Push();
        view.Scale(viz_width, viz_height);
        view.Translate(-viz_width*0.5, -viz_height*0.5);
        view.RotateAroundZ(viz_rot);

        const auto& local_to_view  = view.GetAsMatrix();
        const auto& view_to_local  = glm::inverse(local_to_view);
        const auto& coord_in_local = view_to_local * ToVec4(mickey->pos());
        mMousePos = coord_in_local;
        view.Pop();
    }
    virtual bool MouseRelease(const MouseEvent& mickey, gfx::Transform& view) override
    {
        return true;
    }
private:
    UIState& mState;
    glm::vec4 mMousePos;
};
class ParticleEditorWidget::SizeEmitterTool : public MouseTool
{
public:
    SizeEmitterTool(UIState& state) : mState(state)
    {}
    virtual void MouseMove(const MouseEvent& mickey, gfx::Transform& view) override
    {
        const float viz_width  = mState.visualization_width;
        const float viz_height = mState.visualization_height;
        const float viz_rot    = mState.visualization_rotation;
        view.Push();
        view.Scale(viz_width, viz_height);
        view.Translate(-viz_width*0.5, -viz_height*0.5);
        view.RotateAroundZ(viz_rot);

        const auto& local_to_view  = view.GetAsMatrix();
        const auto& view_to_local  = glm::inverse(local_to_view);
        const auto& coord_in_local = view_to_local * ToVec4(mickey->pos());
        const auto& mouse_delta    = coord_in_local - mMousePos;

        const auto max_width  = 1.0f - mState.emitter_xpos;
        const auto max_height = 1.0f - mState.emitter_ypos;

        mState.emitter_width  = math::clamp(0.0f, max_width, mState.emitter_width + mouse_delta.x);
        mState.emitter_height = math::clamp(0.0f, max_height, mState.emitter_height + mouse_delta.y);
        mMousePos = coord_in_local;
        view.Pop();
    }
    virtual void MousePress(const MouseEvent& mickey, gfx::Transform& view) override
    {
        const float viz_width  = mState.visualization_width;
        const float viz_height = mState.visualization_height;
        const float viz_rot    = mState.visualization_rotation;
        view.Push();
        view.Scale(viz_width, viz_height);
        view.Translate(-viz_width*0.5, -viz_height*0.5);
        view.RotateAroundZ(viz_rot);

        const auto& local_to_view  = view.GetAsMatrix();
        const auto& view_to_local  = glm::inverse(local_to_view);
        const auto& coord_in_local = view_to_local * ToVec4(mickey->pos());
        mMousePos = coord_in_local;
        view.Pop();
    }
    virtual bool MouseRelease(const MouseEvent& mickey, gfx::Transform& view) override
    {
        return true;
    }
private:
    UIState& mState;
    glm::vec4 mMousePos;
};

class ParticleEditorWidget::MoveVizTool : public MouseTool
{
public:
    MoveVizTool(UIState& state) : mState(state)
    {}
    virtual void MouseMove(const MouseEvent& mickey, gfx::Transform& view) override
    {
        const auto& local_to_view  = view.GetAsMatrix();
        const auto& view_to_local  = glm::inverse(local_to_view);
        const auto& coord_in_local = view_to_local * ToVec4(mickey->pos());
        const auto& mouse_delta    = coord_in_local - mMousePos;
        mState.visualization_xpos += mouse_delta.x;
        mState.visualization_ypos += mouse_delta.y;
        mMousePos = coord_in_local;
    }
    virtual void MousePress(const MouseEvent& mickey, gfx::Transform& view) override
    {
        const auto& local_to_view  = view.GetAsMatrix();
        const auto& view_to_local  = glm::inverse(local_to_view);
        const auto& coord_in_local = view_to_local * ToVec4(mickey->pos());
        mMousePos = coord_in_local;
    }
    virtual bool MouseRelease(const MouseEvent& mickey, gfx::Transform& view) override
    {
        return true;
    }
private:
    UIState& mState;
    glm::vec4 mMousePos;
};

class ParticleEditorWidget::SizeVizTool : public MouseTool
{
public:
    SizeVizTool(UIState& state) : mState(state)
    {}
    virtual void MouseMove(const MouseEvent& mickey, gfx::Transform& view) override
    {
        const auto& local_to_view  = view.GetAsMatrix();
        const auto& view_to_local  = glm::inverse(local_to_view);
        const auto& coord_in_local = view_to_local * ToVec4(mickey->pos());
        const auto& mouse_delta    = coord_in_local - mMousePos;
        mState.visualization_width  += (mouse_delta.x * 2.0f);
        mState.visualization_height += (mouse_delta.y * 2.0f);
        mMousePos = coord_in_local;
    }
    virtual void MousePress(const MouseEvent& mickey, gfx::Transform& view) override
    {
        const auto& local_to_view  = view.GetAsMatrix();
        const auto& view_to_local  = glm::inverse(local_to_view);
        const auto& coord_in_local = view_to_local * ToVec4(mickey->pos());
        mMousePos = coord_in_local;
    }
    virtual bool MouseRelease(const MouseEvent& mickey, gfx::Transform& view) override
    {
        return true;
    }
private:
    UIState& mState;
    glm::vec4 mMousePos;
};


ParticleEditorWidget::ParticleEditorWidget(app::Workspace* workspace)
{
    mWorkspace = workspace;
    mClass = std::make_shared<gfx::ParticleEngineClass>();
    DEBUG("Create ParticleEditorWidget");

    mUI.setupUi(this);
    mUI.widget->onPaintScene   = std::bind(&ParticleEditorWidget::PaintScene, this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onMouseMove    = std::bind(&ParticleEditorWidget::MouseMove, this, std::placeholders::_1);
    mUI.widget->onMouseRelease = std::bind(&ParticleEditorWidget::MouseRelease, this, std::placeholders::_1);
    mUI.widget->onMousePress   = std::bind(&ParticleEditorWidget::MousePress, this, std::placeholders::_1);
    mUI.widget->onZoomIn       = std::bind(&ParticleEditorWidget::ZoomIn, this);
    mUI.widget->onZoomOut      = std::bind(&ParticleEditorWidget::ZoomOut, this);

    // if you change this change the UI widget values min/max values too!
    mUI.velocity->SetScale(1000.0f);
    mUI.velocity->SetExponent(2.2f);
    mUI.lifetime->SetScale(100.0f);
    mUI.lifetime->SetExponent(1.0f);
    mUI.pointsize->SetScale(2048);
    mUI.pointsize->SetExponent(2.2f);
    mUI.alpha->SetScale(1.0f);
    mUI.alpha->SetExponent(1.0f);

    PopulateFromEnum<gfx::ParticleEngineClass::DrawPrimitive>(mUI.primitive);
    PopulateFromEnum<gfx::MaterialClass::SurfaceType>(mUI.cmbSurface);
    PopulateFromEnum<gfx::ParticleEngineClass::CoordinateSpace>(mUI.space);
    PopulateFromEnum<gfx::ParticleEngineClass::Motion>(mUI.motion);
    PopulateFromEnum<gfx::ParticleEngineClass::BoundaryPolicy>(mUI.boundary);
    PopulateFromEnum<gfx::ParticleEngineClass::SpawnPolicy>(mUI.when);
    PopulateFromEnum<gfx::ParticleEngineClass::EmitterShape>(mUI.shape);
    PopulateFromEnum<gfx::ParticleEngineClass::Placement>(mUI.placement);
    PopulateFromEnum<gfx::ParticleEngineClass::Direction>(mUI.direction);
    PopulateFromEnum<GridDensity>(mUI.cmbGrid);
    PopulateParticleList(mUI.cmbParticle);

    SetList(mUI.materials, workspace->ListAllMaterials());
    SetValue(mUI.name, QString("My Particle System"));
    SetValue(mUI.ID, mClass->GetId());
    SetValue(mUI.scaleX, 500.0f);
    SetValue(mUI.scaleY, 500.0f);
    SetValue(mUI.rotation, 0.0f);
    SetValue(mUI.materials, ListItemId("_White"));
    SetValue(mUI.cmbGrid, GridDensity::Grid50x50);
    SetValue(mUI.zoom, 1.0f);
    SetEnabled(mUI.actionPause, false);
    SetEnabled(mUI.actionStop, false);
    SetParams(); // apply the defaults from the UI to the params
    MinMax();
    ShowParams();
    on_motion_currentIndexChanged(0);
    on_space_currentIndexChanged(0);
    on_direction_currentIndexChanged(0);
    on_canExpire_stateChanged(0);
    on_when_currentIndexChanged(0);

    connect(mUI.velocity,  &gui::RangeWidget::RangeChanged, this, &ParticleEditorWidget::VelocityChanged);
    connect(mUI.lifetime,  &gui::RangeWidget::RangeChanged, this, &ParticleEditorWidget::LifetimeChanged);
    connect(mUI.pointsize, &gui::RangeWidget::RangeChanged, this, &ParticleEditorWidget::PointsizeChanged);
    connect(mUI.alpha,     &gui::RangeWidget::RangeChanged, this, &ParticleEditorWidget::AlphaChanged);
    setWindowTitle("My Particle System");

    mOriginalHash = GetHash();

    // hah, of course this is broken when loading the widget
    // effin Qt bugs again...
    QTimer::singleShot(0, this, [this]() {
        SetImage(mUI.preview, QPixmap(":texture.png"));
    });
}

ParticleEditorWidget::ParticleEditorWidget(app::Workspace* workspace, const app::Resource& resource)
  : ParticleEditorWidget(workspace)
{
    const auto& name   = resource.GetName();
    const auto* klass  = resource.GetContent<gfx::ParticleEngineClass>();
    const auto& params = klass->GetParams();
    mClass = std::make_shared<gfx::ParticleEngineClass>(*klass);

    DEBUG("Editing particle system: '%1'", name);

    SetValue(mUI.name, name);
    SetValue(mUI.ID, mClass->GetId());

    QString material;
    GetProperty(resource, "material", &material);
    GetProperty(resource, "transform_xpos", mUI.translateX);
    GetProperty(resource, "transform_ypos", mUI.translateY);
    GetProperty(resource, "transform_width", mUI.scaleX);
    GetProperty(resource, "transform_height", mUI.scaleY);
    GetProperty(resource, "transform_rotation", mUI.rotation);
    GetProperty(resource, "local_emitter_x", mUI.initX);
    GetProperty(resource, "local_emitter_y", mUI.initY);
    GetProperty(resource, "local_emitter_w", mUI.initWidth);
    GetProperty(resource, "local_emitter_h", mUI.initHeight);
    GetUserProperty(resource, "grid", mUI.cmbGrid);
    GetUserProperty(resource, "zoom", mUI.zoom);
    GetUserProperty(resource, "show_grid", mUI.chkShowGrid);
    GetUserProperty(resource, "show_bounds", mUI.chkShowBounds);
    GetUserProperty(resource, "show_emitter", mUI.chkShowEmitter);
    GetUserProperty(resource, "widget", mUI.widget);
    GetUserProperty(resource, "main_splitter", mUI.mainSplitter);
    GetUserProperty(resource, "material_group", mUI.materialGroup);
    GetUserProperty(resource, "emission_group", mUI.particleEmissionGroup);
    GetUserProperty(resource, "simulation_space_group", mUI.simulationSpaceGroup);
    GetUserProperty(resource, "local_emitter_group", mUI.localEmitterGroup);
    GetUserProperty(resource, "viz_group", mUI.vizGroup);

    if (FindMaterialClass(""))
    {
        SetValue(mUI.materials, ListItemId(mMaterialClass->GetId()));
    }
    else
    {
        if (mWorkspace->IsValidMaterial(material))
        {
            SetValue(mUI.materials, ListItemId(material));
        }
        else
        {
            WARN("Material '%1' is no longer available.", material);
            SetValue(mUI.materials, ListItemId("_White"));
        }
    }
    MinMax();
    ShowParams();
    on_motion_currentIndexChanged(0);
    on_space_currentIndexChanged(0);
    on_direction_currentIndexChanged(0);
    on_canExpire_stateChanged(0);
    on_when_currentIndexChanged(0);

    mOriginalHash = GetHash();
}

ParticleEditorWidget::~ParticleEditorWidget()
{
    DEBUG("Destroy ParticleEditorWidget");
}

QString ParticleEditorWidget::GetId() const
{
    return GetValue(mUI.ID);
}

void ParticleEditorWidget::InitializeSettings(const UISettings& settings)
{
    SetValue(mUI.zoom,        settings.zoom);
    SetValue(mUI.cmbGrid,     settings.grid);
    SetValue(mUI.chkShowGrid, settings.show_grid);

    // try to make the default splitter partitions sane.
    // looks like this garbage needs to be done *after* the
    // widget has been shown (of course) so using a timer
    // hack for a hack
    QTimer::singleShot(10, this, [this]() {
        QList<int> sizes;
        sizes << mUI.leftSide->sizeHint().width() + 80;
        sizes << mUI.center->sizeHint().width();
        sizes << mUI.rightSide->sizeHint().width() + 40;
        mUI.mainSplitter->setSizes(sizes);
    });
}

void ParticleEditorWidget::SetViewerMode()
{
    SetVisible(mUI.baseProperties,     false);
    SetVisible(mUI.dirSector,          false);
    SetVisible(mUI.localSpace,         false);
    SetVisible(mUI.localEmitter,       false);
    SetVisible(mUI.transform,          false);
    SetVisible(mUI.particleProperties, false);
    SetVisible(mUI.sizeDerivatives,    false);
    SetVisible(mUI.alphaDerivatives,   false);
    SetVisible(mUI.materials,          false);
    SetVisible(mUI.btnSelectMaterial,  false);
    SetVisible(mUI.chkShowEmitter,     false);
    SetVisible(mUI.chkShowBounds,      false);
    SetValue(mUI.chkShowEmitter,       false);
    SetValue(mUI.chkShowBounds,        false);
    mViewMode = true;
    on_actionPlay_triggered();
}

void ParticleEditorWidget::AddActions(QToolBar& bar)
{
    bar.addAction(mUI.actionPlay);
    bar.addAction(mUI.actionPause);
    bar.addSeparator();
    bar.addAction(mUI.actionStop);
    bar.addSeparator();
    bar.addAction(mUI.actionSave);
    bar.addSeparator();
    bar.addAction(mUI.actionEmit);
    bar.addSeparator();
    bar.addAction(mUI.actionLoadPreset);
}

void ParticleEditorWidget::AddActions(QMenu& menu)
{
    menu.addAction(mUI.actionPlay);
    menu.addAction(mUI.actionPause);
    menu.addSeparator();
    menu.addAction(mUI.actionStop);
    menu.addSeparator();
    menu.addAction(mUI.actionSave);
    menu.addSeparator();
    menu.addAction(mUI.actionEmit);
    menu.addSeparator();
    menu.addAction(mUI.actionLoadPreset);

    if (Editor::DevEditor())
    {
        menu.addSeparator();
        menu.addAction(mUI.actionSavePreset);
    }
}

bool ParticleEditorWidget::SaveState(Settings& settings) const
{
    data::JsonObject json;
    mClass->IntoJson(json);
    settings.SetValue("Particle", "content", json);
    settings.SetValue("Particle", "hash", mOriginalHash);
    settings.SetValue("Particle", "material", (QString)GetItemId(mUI.materials));
    settings.SaveWidget("Particle", mUI.initX);
    settings.SaveWidget("Particle", mUI.initY);
    settings.SaveWidget("Particle", mUI.initWidth);
    settings.SaveWidget("Particle", mUI.initHeight);
    settings.SaveWidget("Particle", mUI.name);
    settings.SaveWidget("Particle", mUI.translateX);
    settings.SaveWidget("Particle", mUI.translateY);
    settings.SaveWidget("Particle", mUI.scaleX);
    settings.SaveWidget("Particle", mUI.scaleY);
    settings.SaveWidget("Particle", mUI.rotation);
    settings.SaveWidget("Particle", mUI.chkShowGrid);
    settings.SaveWidget("Particle", mUI.chkShowBounds);
    settings.SaveWidget("Particle", mUI.chkShowEmitter);
    settings.SaveWidget("Particle", mUI.cmbGrid);
    settings.SaveWidget("Particle", mUI.zoom);
    settings.SaveWidget("Particle", mUI.widget);
    settings.SaveWidget("Particle", mUI.mainSplitter);
    settings.SaveWidget("Particle", mUI.materialGroup);
    settings.SaveWidget("Particle", mUI.particleEmissionGroup);
    settings.SaveWidget("Particle", mUI.simulationSpaceGroup);
    settings.SaveWidget("Particle", mUI.localEmitterGroup);
    settings.SaveWidget("Particle", mUI.vizGroup);

    if (mMaterialClass)
    {
        data::JsonObject json;
        mMaterialClass->IntoJson(json);
        settings.SetValue("Particle", "material-class", json);
    }
    return true;
}

bool ParticleEditorWidget::LoadState(const Settings& settings)
{
    QString material;
    data::JsonObject json;
    settings.GetValue("Particle", "content", &json);
    settings.GetValue("Particle", "hash", &mOriginalHash);
    settings.GetValue("Particle", "material", &material);
    settings.LoadWidget("Particle", mUI.initX);
    settings.LoadWidget("Particle", mUI.initY);
    settings.LoadWidget("Particle", mUI.initWidth);
    settings.LoadWidget("Particle", mUI.initHeight);
    settings.LoadWidget("Particle", mUI.name);
    settings.LoadWidget("Particle", mUI.translateX);
    settings.LoadWidget("Particle", mUI.translateY);
    settings.LoadWidget("Particle", mUI.scaleX);
    settings.LoadWidget("Particle", mUI.scaleY);
    settings.LoadWidget("Particle", mUI.rotation);
    settings.LoadWidget("Particle", mUI.chkShowGrid);
    settings.LoadWidget("Particle", mUI.chkShowBounds);
    settings.LoadWidget("Particle", mUI.chkShowEmitter);
    settings.LoadWidget("Particle", mUI.cmbGrid);
    settings.LoadWidget("Particle", mUI.zoom);
    settings.LoadWidget("Particle", mUI.widget);
    settings.LoadWidget("Particle", mUI.mainSplitter);
    settings.LoadWidget("Particle", mUI.materialGroup);
    settings.LoadWidget("Particle", mUI.particleEmissionGroup);
    settings.LoadWidget("Particle", mUI.simulationSpaceGroup);
    settings.LoadWidget("Particle", mUI.localEmitterGroup);
    settings.LoadWidget("Particle", mUI.vizGroup);

    mClass = std::make_shared<gfx::ParticleEngineClass>();
    if (!mClass->FromJson(json))
        WARN("Failed to restore particle engine state.");

    SetValue(mUI.ID, mClass->GetId());
    SetValue(mUI.materials, ListItemId(material));

    auto RestoreMaterial = [this, &settings]() {
        data::JsonObject json;
        if (!settings.GetValue("Particle", "material-class", &json))
            return;

        mMaterialClass = gfx::MaterialClass::ClassFromJson(json);
        if (!mMaterialClass)
        {
            WARN("Failed to restore material class state.");
            SetValue(mUI.materials, ListItemId("_White"));
        }

        if (mWorkspace->IsValidMaterial(mMaterialClass->GetId()))
        {
            SetValue(mUI.materials, ListItemId(mMaterialClass->GetId()));
        }
        else
        {
            SetValue(mUI.materials, mMaterialClass->GetName());
        }
        DEBUG("Restored particle engine material state.");
    };

    RestoreMaterial();

    MinMax();
    ShowParams();
    on_motion_currentIndexChanged(0);
    on_space_currentIndexChanged(0);
    on_direction_currentIndexChanged(0);
    on_canExpire_stateChanged(0);
    on_when_currentIndexChanged(0);

    return true;
}

bool ParticleEditorWidget::CanTakeAction(Actions action, const Clipboard* clipboard) const
{
    switch (action)
    {
        case Actions::CanCut:
        case Actions::CanCopy:
        case Actions::CanPaste:
        case Actions::CanUndo:
            return false;
        case Actions::CanReloadTextures:
        case Actions::CanReloadShaders:
            return true;
        case Actions::CanZoomIn:
            return CanZoomIn(mUI.zoom);
        case Actions::CanZoomOut:
            return CanZoomOut(mUI.zoom);
    }
    return false;
}

void ParticleEditorWidget::ZoomIn()
{
    const float value = GetValue(mUI.zoom);
    SetValue(mUI.zoom, value + 0.1f);
}
void ParticleEditorWidget::ZoomOut()
{
    const float value = GetValue(mUI.zoom);
    SetValue(mUI.zoom, value - 0.1f);
}

void ParticleEditorWidget::ReloadShaders()
{
    mUI.widget->reloadShaders();
}

void ParticleEditorWidget::ReloadTextures()
{
    mUI.widget->reloadTextures();
}

void ParticleEditorWidget::Shutdown()
{
    mUI.widget->dispose();
}

void ParticleEditorWidget::Update(double secs)
{
    if (!mEngine)
        return;

    if (!mPaused)
    {
        const float viz_xpos   = GetValue(mUI.translateX);
        const float viz_ypos   = GetValue(mUI.translateY);
        const float viz_width  = GetValue(mUI.scaleX);
        const float viz_height = GetValue(mUI.scaleY);
        const float viz_rot    = qDegreesToRadians((float)GetValue(mUI.rotation));

        gfx::Transform model;
        model.Scale(viz_width, viz_height);
        model.Translate(-viz_width*0.5, -viz_height*0.5);
        model.RotateAroundZ(viz_rot);
        model.Translate(viz_xpos, viz_ypos);
        const auto& model_matrix = model.GetAsMatrix();

        gfx::Drawable::Environment env;
        env.model_matrix = &model_matrix;
        env.editing_mode = true;

        mEngine->Update(env, secs);
        mTime += secs;
        if (mMaterial)
            mMaterial->SetRuntime(mTime);
    }

    if (!mEngine->IsAlive())
    {
        DEBUG("Particle simulation finished");
        SetEnabled(mUI.actionStop, false);
        SetEnabled(mUI.actionPause, false);
        SetEnabled(mUI.actionPlay, true);
        mEngine.reset();
        mMaterial.reset();
        if (mViewMode)
        {
            on_actionPlay_triggered();
        }
    }
}

void ParticleEditorWidget::Render()
{
    mUI.widget->triggerPaint();
}

void ParticleEditorWidget::Save()
{
    on_actionSave_triggered();
}

bool ParticleEditorWidget::HasUnsavedChanges() const
{
    if (mOriginalHash != GetHash())
        return true;
    return false;
}

bool ParticleEditorWidget::GetStats(Stats* stats) const
{
    stats->time  = mTime;
    stats->graphics.valid = true;
    stats->graphics.vsync = mUI.widget->haveVSYNC();
    stats->graphics.fps   = mUI.widget->getCurrentFPS();
    const auto& dev_stats = mUI.widget->getDeviceResourceStats();
    stats->device.static_vbo_mem_alloc    = dev_stats.static_vbo_mem_alloc;
    stats->device.static_vbo_mem_use      = dev_stats.static_vbo_mem_use;
    stats->device.dynamic_vbo_mem_alloc   = dev_stats.dynamic_vbo_mem_alloc;
    stats->device.dynamic_vbo_mem_use     = dev_stats.dynamic_vbo_mem_use;
    stats->device.streaming_vbo_mem_use   = dev_stats.streaming_vbo_mem_use;
    stats->device.streaming_vbo_mem_alloc = dev_stats.streaming_vbo_mem_alloc;
    return true;
}

void ParticleEditorWidget::on_actionPause_triggered()
{
    mPaused = true;
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
}

void ParticleEditorWidget::on_actionSave_triggered()
{
    if (!MustHaveInput(mUI.name))
        return;

    if (mMaterialClass)
    {
        app::MaterialResource resource(mMaterialClass, (QString)GetValue(mUI.name) + " Particle");
        resource.SetProperty("particle-engine-class-id", (QString)GetValue(mUI.ID));
        mWorkspace->SaveResource(resource);

        SetValue(mUI.materials, ListItemId(mMaterialClass->GetId()));
    }

    app::ParticleSystemResource particle_resource(*mClass, GetValue(mUI.name));
    SetProperty(particle_resource, "material", (QString)GetItemId(mUI.materials));
    SetProperty(particle_resource, "transform_xpos", mUI.translateX);
    SetProperty(particle_resource, "transform_ypos", mUI.translateY);
    SetProperty(particle_resource, "transform_width", mUI.scaleX);
    SetProperty(particle_resource, "transform_height", mUI.scaleY);
    SetProperty(particle_resource, "transform_rotation", mUI.rotation);
    SetProperty(particle_resource, "local_emitter_x", mUI.initX);
    SetProperty(particle_resource, "local_emitter_y", mUI.initY);
    SetProperty(particle_resource, "local_emitter_w", mUI.initWidth);
    SetProperty(particle_resource, "local_emitter_h", mUI.initHeight);
    SetUserProperty(particle_resource, "grid", mUI.cmbGrid);
    SetUserProperty(particle_resource, "zoom", mUI.zoom);
    SetUserProperty(particle_resource, "show_grid", mUI.chkShowGrid);
    SetUserProperty(particle_resource, "show_bounds", mUI.chkShowBounds);
    SetUserProperty(particle_resource, "show_emitter", mUI.chkShowEmitter);
    SetUserProperty(particle_resource, "widget", mUI.widget);
    SetUserProperty(particle_resource, "main_splitter", mUI.mainSplitter);
    SetUserProperty(particle_resource, "material_group", mUI.materialGroup);
    SetUserProperty(particle_resource, "emission_group", mUI.particleEmissionGroup);
    SetUserProperty(particle_resource, "simulation_space_group", mUI.simulationSpaceGroup);
    SetUserProperty(particle_resource, "local_emitter_group", mUI.localEmitterGroup);
    SetUserProperty(particle_resource, "viz_group", mUI.vizGroup);
    mWorkspace->SaveResource(particle_resource);

    mOriginalHash = GetHash();
}

void ParticleEditorWidget::on_actionEmit_triggered()
{
    const auto& p = mClass->GetParams();
    if (p.mode != gfx::ParticleEngineClass::SpawnPolicy::Command)
        return;

    if (!mEngine)
        on_actionPlay_triggered();

    const float viz_xpos   = GetValue(mUI.translateX);
    const float viz_ypos   = GetValue(mUI.translateY);
    const float viz_width  = GetValue(mUI.scaleX);
    const float viz_height = GetValue(mUI.scaleY);
    const float viz_rot    = qDegreesToRadians((float)GetValue(mUI.rotation));

    gfx::Transform model;
    model.Scale(viz_width, viz_height);
    model.Translate(-viz_width*0.5, -viz_height*0.5);
    model.RotateAroundZ(viz_rot);
    model.Translate(viz_xpos, viz_ypos);
    const auto& model_matrix = model.GetAsMatrix();

    gfx::Drawable::Environment env;
    env.model_matrix = &model_matrix;
    env.editing_mode = true;

    gfx::Drawable::Command cmd;
    cmd.name = "EmitParticles";
    // no count here.
    mEngine->Execute(env, cmd);
}

void ParticleEditorWidget::on_actionLoadPreset_triggered()
{
    DlgParticle dlg(this, mWorkspace);
    if (dlg.exec() == QDialog::Rejected)
        return;

    const auto& particle = dlg.GetParticleClass();
    const auto& material = dlg.GetMaterialClass();

    if (HasUnsavedChanges() && mWorkspace->IsValidDrawable(mClass->GetId()))
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::StandardButton::Yes | QMessageBox::Cancel);
        msg.setIcon(QMessageBox::Icon::Question);
        msg.setWindowTitle(tr("Load Preset?"));
        msg.setText(tr("The changes made in this particle engine will be lost. Do you want to proceed?"));
        if (msg.exec() == QMessageBox::Rejected)
            return;
    }
    mClass->SetParams(particle->GetParams());

    mMaterial.reset();
    mMaterialClass = material->Clone();
    mMaterialClass->SetName((std::string)GetValue(mUI.name) + std::string(" Particle"));

    SetValue(mUI.materials, -1);
    SetValue(mUI.materials, mMaterialClass->GetName());

    ShowParams();

    on_motion_currentIndexChanged(0);
    on_space_currentIndexChanged(0);
    on_direction_currentIndexChanged(0);
    on_canExpire_stateChanged(0);
    on_when_currentIndexChanged(0);
}

void ParticleEditorWidget::on_actionSavePreset_triggered()
{
    if (!MustHaveInput(mUI.name))
        return;

    QString filename = mWorkspace->MapFileToFilesystem("app://presets/particles/preset.json");
    filename = QFileDialog::getSaveFileName(this,
        tr("Export Preset"),
        filename,
        tr("JSON (*.json)"));
    if (filename.isEmpty())
        return;

    data::JsonObject data;
    auto particle = data.NewWriteChunk();

    mClass->SetName(GetValue(mUI.name));
    mClass->IntoJson(*particle);
    data.Write("particle", std::move(particle));
    data.Write("viz_width", (float)GetValue(mUI.scaleX));
    data.Write("viz_height", (float)GetValue(mUI.scaleY));

    if (mMaterialClass)
    {
        auto material = data.NewWriteChunk();
        mMaterialClass->IntoJson(*material);
        data.Write("material", std::move(material));
    }
    else
    {
        const QString materialId = GetItemId(mUI.materials);
        const auto& resource = mWorkspace->GetResourceById(materialId);
        if (resource.IsPrimitive())
        {
            data.Write("material-id", resource.GetIdUtf8());
        }
        else
        {
            const auto* klass = resource.GetContent<gfx::MaterialClass>();
            auto material = data.NewWriteChunk();
            klass->IntoJson(*material);
            data.Write("material", std::move(material));
        }
    }

    data::JsonFile file;
    file.SetRootObject(data);
    const auto [success, error] = file.Save(app::ToUtf8(filename));
    if (!success)
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Icon::Critical);
        msg.setText("Failed to save the preset file.");
        msg.setStandardButtons(QMessageBox::StandardButton::Ok);
        msg.exec();
        return;
    }
}

void ParticleEditorWidget::SetParams()
{
    gfx::ParticleEngineClass::Params params;
    params.primitive                        = GetValue(mUI.primitive);
    params.coordinate_space                 = GetValue(mUI.space);
    params.motion                           = GetValue(mUI.motion);
    params.shape                            = GetValue(mUI.shape);
    params.placement                        = GetValue(mUI.placement);
    params.direction                        = GetValue(mUI.direction);
    params.mode                             = GetValue(mUI.when);
    params.boundary                         = GetValue(mUI.boundary);
    params.num_particles                    = GetValue(mUI.numParticles);
    params.max_xpos                         = GetValue(mUI.simWidth);
    params.max_ypos                         = GetValue(mUI.simHeight);
    params.gravity.x                        = GetValue(mUI.gravityX);
    params.gravity.y                        = GetValue(mUI.gravityY);
    params.min_point_size                   = GetValue(mUI.minPointsize);
    params.max_point_size                   = GetValue(mUI.maxPointsize);
    params.min_velocity                     = GetValue(mUI.minVelocity);
    params.max_velocity                     = GetValue(mUI.maxVelocity);
    params.min_alpha                        = GetValue(mUI.minAlpha);
    params.max_alpha                        = GetValue(mUI.maxAlpha);
    params.rate_of_change_in_size_wrt_time  = GetValue(mUI.timeSizeDerivative);
    params.rate_of_change_in_size_wrt_dist  = GetValue(mUI.distSizeDerivative);
    params.rate_of_change_in_alpha_wrt_time = GetValue(mUI.timeAlphaDerivative);
    params.rate_of_change_in_alpha_wrt_dist = GetValue(mUI.distAlphaDerivative);
    params.direction_sector_start_angle     = qDegreesToRadians((float)mUI.dirStartAngle->value());
    params.direction_sector_size            = qDegreesToRadians((float)mUI.dirSizeAngle->value());
    params.min_lifetime                     = GetValue(mUI.minLifetime);
    params.max_lifetime                     = GetValue(mUI.maxLifetime);

    params.min_time = GetValue(mUI.minTime);
    params.max_time = GetValue(mUI.maxTime);
    if (params.max_time == 0.0f)
        params.max_time = std::numeric_limits<float>::max();

    params.delay = GetValue(mUI.delay);

    const gfx::ParticleEngineClass::CoordinateSpace space = GetValue(mUI.space);
    if (space == gfx::ParticleEngineClass::CoordinateSpace::Local)
    {
        params.init_rect_xpos   = GetValue(mUI.initX);
        params.init_rect_ypos   = GetValue(mUI.initY);
        params.init_rect_width  = GetValue(mUI.initWidth);
        params.init_rect_height = GetValue(mUI.initHeight);
    }
    else
    {
        params.init_rect_xpos   = 0.0f;
        params.init_rect_ypos   = 0.0f;
        params.init_rect_width  = 1.0f;
        params.init_rect_height = 1.0f;
    }
    params.flags.set(gfx::ParticleEngineClass::Flags::ParticlesCanExpire, GetValue(mUI.canExpire));

    mClass->SetParams(params);
}

void ParticleEditorWidget::ShowParams()
{
    const auto& params = mClass->GetParams();
    const auto can_expire = params.flags.test(gfx::ParticleEngineClass::Flags::ParticlesCanExpire);

    SetValue(mUI.canExpire,           can_expire);
    SetValue(mUI.primitive,           params.primitive);
    SetValue(mUI.space,               params.coordinate_space);
    SetValue(mUI.motion,              params.motion);
    SetValue(mUI.shape,               params.shape);
    SetValue(mUI.placement,           params.placement);
    SetValue(mUI.direction,           params.direction);
    SetValue(mUI.when,                params.mode);
    SetValue(mUI.boundary,            params.boundary);
    SetValue(mUI.numParticles,        params.num_particles);
    SetValue(mUI.simWidth,            params.max_xpos);
    SetValue(mUI.simHeight,           params.max_ypos);
    SetValue(mUI.gravityX,            params.gravity.x);
    SetValue(mUI.gravityY,            params.gravity.y);
    SetValue(mUI.minLifetime,         params.min_lifetime);
    SetValue(mUI.maxLifetime,         params.max_lifetime);
    SetValue(mUI.minPointsize,        params.min_point_size);
    SetValue(mUI.maxPointsize,        params.max_point_size);
    SetValue(mUI.minAlpha,            params.min_alpha);
    SetValue(mUI.maxAlpha,            params.max_alpha);
    SetValue(mUI.minVelocity,         params.min_velocity);
    SetValue(mUI.maxVelocity,         params.max_velocity);
    SetValue(mUI.timeSizeDerivative,  params.rate_of_change_in_size_wrt_time);
    SetValue(mUI.distSizeDerivative,  params.rate_of_change_in_size_wrt_dist);
    SetValue(mUI.timeAlphaDerivative, params.rate_of_change_in_alpha_wrt_time);
    SetValue(mUI.distAlphaDerivative, params.rate_of_change_in_alpha_wrt_dist);
    SetValue(mUI.dirStartAngle,       qRadiansToDegrees(params.direction_sector_start_angle));
    SetValue(mUI.dirSizeAngle,        qRadiansToDegrees(params.direction_sector_size));

    SetValue(mUI.dsPerTime, params.rate_of_change_in_size_wrt_time);
    SetValue(mUI.dsPerDist, params.rate_of_change_in_size_wrt_dist);
    SetValue(mUI.daPerTime, params.rate_of_change_in_alpha_wrt_time * 100); // scale up the value
    SetValue(mUI.daPerDist, params.rate_of_change_in_alpha_wrt_dist * 100); // scale up the value

    SetValue(mUI.dirStartAngleSpin, qRadiansToDegrees(params.direction_sector_start_angle));
    SetValue(mUI.dirSizeAngleSpin, qRadiansToDegrees(params.direction_sector_size));

    mUI.velocity->SetLo(params.min_velocity);
    mUI.velocity->SetHi(params.max_velocity);
    mUI.lifetime->SetLo(params.min_lifetime);
    mUI.lifetime->SetHi(params.max_lifetime);
    mUI.pointsize->SetLo(params.min_point_size);
    mUI.pointsize->SetHi(params.max_point_size);
    mUI.alpha->SetLo(params.min_alpha);
    mUI.alpha->SetHi(params.max_alpha);

    if (params.coordinate_space == gfx::ParticleEngineClass::CoordinateSpace::Local)
    {
        SetValue(mUI.initX,      params.init_rect_xpos);
        SetValue(mUI.initY,      params.init_rect_ypos);
        SetValue(mUI.initWidth,  params.init_rect_width);
        SetValue(mUI.initHeight, params.init_rect_height);
    }

    SetValue(mUI.minTime, params.min_time);
    if (params.max_time == std::numeric_limits<float>::max())
        SetValue(mUI.maxTime, 0.0f);
    else SetValue(mUI.maxTime, params.max_time);

    SetValue(mUI.delay, params.delay);

    SetEnabled(mUI.cmbSurface, false);
    SetEnabled(mUI.cmbParticle, false);
    SetEnabled(mUI.startColor, false);
    SetEnabled(mUI.endColor, false);
    SetValue(mUI.cmbSurface, -1);
    SetValue(mUI.cmbParticle, -1);

    SetEnabled(mUI.minLifetime, can_expire);
    SetEnabled(mUI.maxLifetime, can_expire);
    SetEnabled(mUI.lifetime, can_expire);

    mUI.startColor->clearColor();
    mUI.endColor->clearColor();

    SetImage(mUI.preview, QPixmap(":texture.png"));

    if (auto* edit = mUI.materials->lineEdit())
        edit->setReadOnly(true);

    if (!mMaterialClass)
        return;

    // do a breakdown here, but only if the contents are as
    // we might expect, after all this is just a normal material
    // so the user can change this shit to whatever they want!
    if (mMaterialClass->GetNumTextureMaps() != 1)
        return;
    if (mMaterialClass->GetType() != gfx::MaterialClass::Type::Particle2D)
        return;

    const auto& texture_map = mMaterialClass->GetTextureMap(0);
    if (texture_map->GetNumTextures() != 1)
        return;

    const auto* texture_src = texture_map->GetTextureSource(0);
    if (texture_src->GetSourceType() != gfx::TextureSource::Source::Filesystem)
        return;
    const auto* file_texture_src = dynamic_cast<const gfx::TextureFileSource*>(texture_src);

    SetValue(mUI.cmbSurface, mMaterialClass->GetSurfaceType());
    SetValue(mUI.startColor, mMaterialClass->GetParticleStartColor());
    SetValue(mUI.endColor, mMaterialClass->GetParticleEndColor());
    SetValue(mUI.cmbParticle, ListItemId(file_texture_src->GetFilename()));
    if (const auto bitmap = texture_src->GetData())
    {
        // hah, of course this is broken when loading the widget
        // effin Qt bugs again...
        QTimer::singleShot(0, this, [this, bitmap]() {
            SetImage(mUI.preview, *bitmap);
        });
    }

    SetEnabled(mUI.cmbSurface, true);
    SetEnabled(mUI.cmbParticle, true);
    SetEnabled(mUI.startColor, true);
    SetEnabled(mUI.endColor, true);
}

void ParticleEditorWidget::MinMax()
{
    const auto& params = mClass->GetParams();
    SetRange(mUI.minVelocity, 0.0f, params.max_velocity);
    SetRange(mUI.maxVelocity, params.min_velocity, 1000.0f);
    SetRange(mUI.minPointsize, 1, params.max_point_size);
    SetRange(mUI.maxPointsize, params.min_point_size, 2048);
    SetRange(mUI.minLifetime, 0.0f, params.max_lifetime);
    SetRange(mUI.maxLifetime, params.min_lifetime, 100.0f);
    SetRange(mUI.minAlpha, 0.0f, params.max_alpha);
    SetRange(mUI.maxAlpha, params.min_alpha, 1.0f);
}

void ParticleEditorWidget::on_widgetColor_colorChanged(QColor color)
{
    mUI.widget->SetClearColor(ToGfx(color));
}
void ParticleEditorWidget::on_actionPlay_triggered()
{
    if (mPaused)
    {
        mPaused = false;
        SetEnabled(mUI.actionPause, true);
        return;
    }
    const float viz_xpos   = GetValue(mUI.translateX);
    const float viz_ypos   = GetValue(mUI.translateY);
    const float viz_width  = GetValue(mUI.scaleX);
    const float viz_height = GetValue(mUI.scaleY);
    const float viz_rot    = qDegreesToRadians((float)GetValue(mUI.rotation));

    gfx::Transform model;
    model.Scale(viz_width, viz_height);
    model.Translate(-viz_width*0.5, -viz_height*0.5);
    model.RotateAroundZ(viz_rot);
    model.Translate(viz_xpos, viz_ypos);
    const auto& model_matrix = model.GetAsMatrix();

    gfx::Drawable::Environment env;
    env.model_matrix = &model_matrix;
    env.editing_mode = true;

    mEngine.reset(new gfx::ParticleEngineInstance(mClass));
    mEngine->Restart(env);
    mTime   = 0.0f;
    mPaused = false;
    SetEnabled(mUI.actionPause, true);
    SetEnabled(mUI.actionStop, true);

    if (mClass->GetParams().mode == gfx::ParticleEngineClass::SpawnPolicy::Command)
        on_actionEmit_triggered();

    DEBUG("Created new particle engine");
}

void ParticleEditorWidget::on_actionStop_triggered()
{
    mEngine.reset();
    SetEnabled(mUI.actionStop, false);
    SetEnabled(mUI.actionPause, false);
    SetEnabled(mUI.actionPlay, true);
    mPaused = false;
}

void ParticleEditorWidget::on_resetEmitter_clicked()
{
    SetValue(mUI.initWidth, 1.0f);
    SetValue(mUI.initHeight, 1.0f);
    SetValue(mUI.initX, 0.0f);
    SetValue(mUI.initY, 0.0f);
    SetParams();
}

void ParticleEditorWidget::on_resetTransform_clicked()
{
    SetValue(mUI.translateX, 0.0f);
    SetValue(mUI.translateY, 0.0f);
    SetValue(mUI.scaleX, 500.0f);
    SetValue(mUI.scaleY, 500.0f);
    SetValue(mUI.rotation, 0.0f);
}

void ParticleEditorWidget::on_btnViewPlus90_clicked()
{
    const auto value = mUI.rotation->value();
    mUI.rotation->setValue(value + 90.0f);
}
void ParticleEditorWidget::on_btnViewMinus90_clicked()
{
    const auto value = mUI.rotation->value();
    mUI.rotation->setValue(value - 90.0f);
}

void ParticleEditorWidget::on_btnSelectMaterial_clicked()
{
    QString material = GetItemId(mUI.materials);
    DlgMaterial dlg(this, mWorkspace, material);
    if (dlg.exec() == QDialog::Rejected)
        return;

    SetValue(mUI.materials, ListItemId(dlg.GetSelectedMaterialId()));

    std::string id = GetItemId(mUI.materials);
    if (mMaterialClass && mMaterialClass->GetId() == id)
        return;

    mMaterialClass.reset();
    mMaterial.reset();

    FindMaterialClass(id);

    ShowParams();
}

void ParticleEditorWidget::on_btnCreateMaterial_clicked()
{
    if (!MustHaveInput(mUI.name))
        return;

    const QString name = GetValue(mUI.name);
    const QString id   = GetValue(mUI.ID);

    std::string materialId = base::RandomString(10);

    // if we already have a previous material created for this
    // particle engine then find it and re-use the ID
    // so that when saved the previous material is overwritten.
    const auto& materials = mWorkspace->ListUserDefinedMaterials();
    for (const auto& material : materials)
    {
        std::string particleId;
        if (material.resource->GetProperty("particle-engine-class-id", &particleId))
        {
            materialId = material.resource->GetIdUtf8();
            break;
        }
    }

    SetValue(mUI.cmbSurface, gfx::MaterialClass::SurfaceType::Transparent);

    auto texture = std::make_unique<gfx::TextureFileSource>();
    texture->SetColorSpace(gfx::TextureSource::ColorSpace::sRGB);
    texture->SetFileName("app://textures/particles/circle_02.png");
    texture->SetName("Texture");

    auto map = std::make_unique<gfx::TextureMap>(base::RandomString(10));
    map->SetType(gfx::TextureMap::Type::Texture2D);
    map->SetName("Particle Alpha Mask");
    map->SetSamplerName("kMask");
    map->SetRectUniformName("kMaskRect");
    map->SetNumTextures(1);
    map->SetTextureSource(0, std::move(texture));

    mMaterialClass = std::make_shared<gfx::MaterialClass>(gfx::MaterialClass::Type::Particle2D, materialId);
    mMaterialClass->SetSurfaceType(GetValue(mUI.cmbSurface));
    mMaterialClass->SetNumTextureMaps(1);
    mMaterialClass->SetActiveTextureMap(map->GetId());
    mMaterialClass->SetTextureMap(0, std::move(map));
    mMaterialClass->SetName((std::string)GetValue(mUI.name) + std::string(" Particle"));
    mMaterialClass->SetParticleStartColor(GetValue(mUI.startColor));
    mMaterialClass->SetParticleEndColor(GetValue(mUI.endColor));
    mMaterialClass->SetParticleBaseRotation(0.0f);
    mMaterialClass->SetParticleRotation(gfx::MaterialClass::ParticleRotation::ParticleDirectionAndBase);

    mMaterial.reset();

    SetValue(mUI.materials, -1);
    SetValue(mUI.materials, mMaterialClass->GetName());

    ShowParams();
}

void ParticleEditorWidget::on_materials_currentIndexChanged(int)
{
    std::string id = GetItemId(mUI.materials);
    if (mMaterialClass && mMaterialClass->GetId() == id)
        return;

    mMaterialClass.reset();
    mMaterial.reset();

    FindMaterialClass(id);

    ShowParams();
}

void ParticleEditorWidget::on_cmbSurface_currentIndexChanged(int)
{
    if (!mMaterialClass)
        return;

    mMaterialClass->SetSurfaceType(GetValue(mUI.cmbSurface));
}
void ParticleEditorWidget::on_cmbParticle_currentIndexChanged(int)
{
    if (!mMaterialClass)
        return;

    auto* texture_map = mMaterialClass->GetTextureMap(0);
    if (texture_map->GetNumTextures() != 1)
        return;

    auto* texture_src = texture_map->GetTextureSource(0);
    if (texture_src->GetSourceType() != gfx::TextureSource::Source::Filesystem)
        return;
    auto* file_texture_src = dynamic_cast<gfx::TextureFileSource*>(texture_src);

    // this is an URI
    file_texture_src->SetFileName(GetItemId(mUI.cmbParticle));

    if (const auto bitmap = file_texture_src->GetData())
    {
        SetImage(mUI.preview, *bitmap);
    }
}

void ParticleEditorWidget::on_startColor_colorChanged(QColor)
{
    if (!mMaterialClass)
        return;

    mMaterialClass->SetParticleStartColor(GetValue(mUI.startColor));
}

void ParticleEditorWidget::on_endColor_colorChanged(QColor)
{
    if (!mMaterialClass)
        return;

    mMaterialClass->SetParticleEndColor(GetValue(mUI.endColor));
}

void ParticleEditorWidget::on_primitive_currentIndexChanged(int)
{
    SetParams();
}

void ParticleEditorWidget::on_space_currentIndexChanged(int)
{
    const gfx::ParticleEngineClass::CoordinateSpace space = GetValue(mUI.space);
    if (space == gfx::ParticleEngineClass::CoordinateSpace::Local)
    {
        SetEnabled(mUI.localEmitter, true);
        SetEnabled(mUI.chkShowEmitter, true);
        SetEnabled(mUI.boundary, true);
        SetEnabled(mUI.simWidth, true);
        SetEnabled(mUI.simHeight, true);
    }
    else
    {
        SetEnabled(mUI.localEmitter, false);
        SetEnabled(mUI.chkShowEmitter, false);
        SetEnabled(mUI.boundary, false);
        SetEnabled(mUI.simWidth, false);
        SetEnabled(mUI.simHeight, false);
    }
    SetParams();
}

void ParticleEditorWidget::on_motion_currentIndexChanged(int)
{
    const gfx::ParticleEngineClass::Motion motion = GetValue(mUI.motion);
    if (motion == gfx::ParticleEngineClass::Motion::Projectile)
    {
        SetEnabled(mUI.gravityY, true);
        SetEnabled(mUI.gravityX, true);
    }
    else
    {
        SetEnabled(mUI.gravityY, false);
        SetEnabled(mUI.gravityX, false);
    }
    SetParams();
}

void ParticleEditorWidget::on_boundary_currentIndexChanged(int)
{
    SetParams();
}

void ParticleEditorWidget::on_when_currentIndexChanged(int)
{
    const gfx::ParticleEngineClass::SpawnPolicy spawning = GetValue(mUI.when);
    if (spawning == gfx::ParticleEngineClass::SpawnPolicy::Command)
    {
        SetEnabled(mUI.delay, false);
        SetEnabled(mUI.actionEmit, true);
    }
    else
    {
        SetEnabled(mUI.delay, true);
        SetEnabled(mUI.actionEmit, false);
    }

    SetParams();
}
void ParticleEditorWidget::on_shape_currentIndexChanged(int)
{
    SetParams();
}
void ParticleEditorWidget::on_placement_currentIndexChanged(int)
{
    SetParams();
}
void ParticleEditorWidget::on_direction_currentIndexChanged(int)
{
    const gfx::ParticleEngineClass::Direction dir = GetValue(mUI.direction);
    if (dir == gfx::ParticleEngineClass::Direction::Sector)
    {
        SetEnabled(mUI.dirSizeAngle, true);
        SetEnabled(mUI.dirStartAngle, true);
        SetEnabled(mUI.dirStartAngleSpin, true);
        SetEnabled(mUI.dirSizeAngleSpin, true);
    }
    else
    {
        SetEnabled(mUI.dirSizeAngle, false);
        SetEnabled(mUI.dirStartAngle, false);
        SetEnabled(mUI.dirStartAngleSpin, false);
        SetEnabled(mUI.dirSizeAngleSpin, false);
    }
    SetParams();
}

void ParticleEditorWidget::on_simWidth_valueChanged(double)
{
    SetParams();
}
void ParticleEditorWidget::on_simHeight_valueChanged(double)
{
    SetParams();
}

void ParticleEditorWidget::on_gravityX_valueChanged(double)
{
    SetParams();
}
void ParticleEditorWidget::on_gravityY_valueChanged(double)
{
    SetParams();
}

void ParticleEditorWidget::on_minTime_valueChanged(double)
{
    SetParams();
}
void ParticleEditorWidget::on_maxTime_valueChanged(double)
{
    SetParams();
}

void ParticleEditorWidget::on_delay_valueChanged(double)
{
    SetParams();
}

void ParticleEditorWidget::on_btnResetMinTime_clicked()
{
    SetValue(mUI.minTime, 0.0f);
    SetParams();
}
void ParticleEditorWidget::on_btnResetMaxTime_clicked()
{
    SetValue(mUI.maxTime, 0.0f);
    SetParams();
}

void ParticleEditorWidget::on_btnResetDelay_clicked()
{
    SetValue(mUI.delay, 0.0f);
    SetParams();
}

void ParticleEditorWidget::on_numParticles_valueChanged(double)
{
    SetParams();
}
void ParticleEditorWidget::on_initX_valueChanged(double)
{
    SetParams();
}
void ParticleEditorWidget::on_initY_valueChanged(double)
{
    SetParams();
}
void ParticleEditorWidget::on_initWidth_valueChanged(double)
{
    SetParams();
}
void ParticleEditorWidget::on_initHeight_valueChanged(double)
{
    SetParams();
}
void ParticleEditorWidget::on_dirStartAngle_valueChanged()
{
    SetValue(mUI.dirStartAngleSpin, GetValue(mUI.dirStartAngle));
    SetParams();
}
void ParticleEditorWidget::on_dirSizeAngle_valueChanged()
{
    SetValue(mUI.dirSizeAngleSpin, GetValue(mUI.dirSizeAngle));
    SetParams();
}

void ParticleEditorWidget::on_dirStartAngleSpin_valueChanged(double value)
{
    SetValue(mUI.dirStartAngle, value);
    SetParams();
}
void ParticleEditorWidget::on_dirSizeAngleSpin_valueChanged(double value)
{
    SetValue(mUI.dirSizeAngle, value);
    SetParams();
}

void ParticleEditorWidget::on_minVelocity_valueChanged(double value)
{
    mUI.velocity->SetLo(value);
    SetParams();
    MinMax();
}
void ParticleEditorWidget::on_maxVelocity_valueChanged(double value)
{
    mUI.velocity->SetHi(value);
    SetParams();
    MinMax();
}
void ParticleEditorWidget::on_minLifetime_valueChanged(double value)
{
    mUI.lifetime->SetLo(value);
    SetParams();
    MinMax();
}
void ParticleEditorWidget::on_maxLifetime_valueChanged(double value)
{
    mUI.lifetime->SetHi(value);
    SetParams();
    MinMax();
}
void ParticleEditorWidget::on_minPointsize_valueChanged(int value)
{
    mUI.pointsize->SetLo(value);
    SetParams();
    MinMax();
}
void ParticleEditorWidget::on_maxPointsize_valueChanged(int value)
{
    mUI.pointsize->SetHi(value);
    SetParams();
    MinMax();
}
void ParticleEditorWidget::on_minAlpha_valueChanged(double value)
{
    mUI.alpha->SetLo(value);
    SetParams();
    MinMax();
}
void ParticleEditorWidget::on_maxAlpha_valueChanged(double value)
{
    mUI.alpha->SetHi(value);
    SetParams();
    MinMax();
}
void ParticleEditorWidget::on_timeSizeDerivative_valueChanged(double value)
{
    SetValue(mUI.dsPerTime, value);
    SetParams();
}
void ParticleEditorWidget::on_distSizeDerivative_valueChanged(double value)
{
    SetValue(mUI.dsPerDist, value);
    SetParams();
}
void ParticleEditorWidget::on_timeAlphaDerivative_valueChanged(double value)
{
    SetValue(mUI.daPerTime, value*100);
    SetParams();
}
void ParticleEditorWidget::on_distAlphaDerivative_valueChanged(double value)
{
    SetValue(mUI.daPerDist, value*100);
    SetParams();
}

void ParticleEditorWidget::on_dsPerTime_valueChanged()
{
    int val = GetValue(mUI.dsPerTime);
    SetValue(mUI.timeSizeDerivative, val);
    SetParams();
}
void ParticleEditorWidget::on_dsPerDist_valueChanged()
{
    int val = GetValue(mUI.dsPerDist);
    SetValue(mUI.distSizeDerivative, val);
    SetParams();
}
void ParticleEditorWidget::on_daPerTime_valueChanged()
{
    int val = GetValue(mUI.daPerTime);
    SetValue(mUI.timeAlphaDerivative, val / 100.0f); // scale down
    SetParams();
}
void ParticleEditorWidget::on_daPerDist_valueChanged()
{
    int val = GetValue(mUI.daPerDist);
    SetValue(mUI.distAlphaDerivative, val / 100.0f); // scale down
    SetParams();
}

void ParticleEditorWidget::on_canExpire_stateChanged(int)
{
    SetEnabled(mUI.minLifetime, GetValue(mUI.canExpire));
    SetEnabled(mUI.maxLifetime, GetValue(mUI.canExpire));
    SetEnabled(mUI.lifetime, GetValue(mUI.canExpire));
    SetParams();
}

void ParticleEditorWidget::on_btnResetSizeDerivativeDist_clicked()
{
    SetValue(mUI.distSizeDerivative, 0.0f);
    SetValue(mUI.dsPerDist, 0.0f);
    SetParams();
}
void ParticleEditorWidget::on_btnResetSizeDerivativeTime_clicked()
{
    SetValue(mUI.timeSizeDerivative, 0.0f);
    SetValue(mUI.dsPerTime, 0.0f);
    SetParams();
}
void ParticleEditorWidget::on_btnResetAlphaDerivativeDist_clicked()
{
    SetValue(mUI.distAlphaDerivative, 0.0f);
    SetValue(mUI.daPerDist, 0.0f);
    SetParams();
}
void ParticleEditorWidget::on_btnResetAlphaDerivativeTime_clicked()
{
    SetValue(mUI.timeAlphaDerivative, 0.0f);
    SetValue(mUI.daPerTime, 0.0f);
    SetParams();
}

void ParticleEditorWidget::PaintScene(gfx::Painter& painter, double secs)
{
    const auto widget_width  = mUI.widget->width();
    const auto widget_height = mUI.widget->height();
    const auto zoom   = (float)GetValue(mUI.zoom);
    SetValue(mUI.widgetColor, mUI.widget->GetCurrentClearColor());

    gfx::Transform view;
    view.Scale(zoom, zoom);
    view.Translate(widget_width*0.5, widget_height*0.5);

    painter.SetViewport(0, 0, widget_width, widget_height);
    painter.SetPixelRatio(glm::vec2(zoom, zoom));
    painter.ResetViewMatrix();

    if (GetValue(mUI.chkShowGrid))
    {
        const float xs = 1.0f;
        const float ys = 1.0f;
        const GridDensity grid = GetValue(mUI.cmbGrid);
        DrawCoordinateGrid(painter, view, grid, zoom, xs, ys, widget_width, widget_height);
    }

    painter.SetViewMatrix(view.GetAsMatrix());

    const float viz_xpos   = GetValue(mUI.translateX);
    const float viz_ypos   = GetValue(mUI.translateY);
    const float viz_width  = GetValue(mUI.scaleX);
    const float viz_height = GetValue(mUI.scaleY);
    const float viz_rot    = qDegreesToRadians((float)GetValue(mUI.rotation));

    gfx::Transform model;
    model.Scale(viz_width, viz_height);
    model.Translate(-viz_width*0.5, -viz_height*0.5);
    model.RotateAroundZ(viz_rot);
    model.Translate(viz_xpos, viz_ypos);

    if (GetValue(mUI.chkShowBounds))
    {
        painter.Draw(gfx::Rectangle(gfx::SimpleShapeStyle::Outline), model,
                     gfx::CreateMaterialFromColor(gfx::Color::HotPink));
        const auto scalex = zoom * viz_width;
        const auto scaley = zoom * viz_height;
        model.Push();
            model.Scale(10.0f/scalex, 10.0f/scaley);
            model.Translate(1.0f-(10.0f/scalex), 1.0f-(10.0f/scaley));
            painter.Draw(gfx::Rectangle(gfx::SimpleShapeStyle::Outline), model,
                         gfx::CreateMaterialFromColor(gfx::Color::HotPink), 2.0f);
        model.Pop();
    }

    if (mEngine)
    {
        CreateMaterial();

        painter.Draw(*mEngine, model, *mMaterial);
    }

    const gfx::ParticleEngineClass::CoordinateSpace space = GetValue(mUI.space);

    if (GetValue(mUI.chkShowEmitter) && space == gfx::ParticleEngineClass::CoordinateSpace::Local)
    {
        // visualize the emitter as a box inside the simulation space.
        // note that these are normalized coordinates that get scaled
        // already by scaling factor set above for the whole visualization
        const float emitter_width  = GetValue(mUI.initWidth);
        const float emitter_height = GetValue(mUI.initHeight);
        const float emitter_xpos   = GetValue(mUI.initX);
        const float emitter_ypos   = GetValue(mUI.initY);

        model.Push();
            model.Scale(emitter_width, emitter_height);
            model.Translate(emitter_xpos, emitter_ypos);
            painter.Draw(gfx::Rectangle(gfx::SimpleShapeStyle::Outline), model,
                         gfx::CreateMaterialFromColor(gfx::Color::Green), 2.0f);

            const auto scalex = zoom * viz_width * emitter_width;
            const auto scaley = zoom * viz_height * emitter_height;
            model.Push();
                model.Scale(10.0f/scalex, 10.0f/scaley);
                model.Translate(1.0f-(10.0f/scalex), 1.0f-(10.0f/scaley));
                painter.Draw(gfx::Rectangle(gfx::SimpleShapeStyle::Outline), model,
                             gfx::CreateMaterialFromColor(gfx::Color::Green), 2.0f);
            model.Pop();
        model.Pop();
    }

    painter.ResetViewMatrix();

    // Draw the visualization for the particle direction sector
    // we draw this in the widget/window coordinates in the top right
    const gfx::ParticleEngineClass::Direction particle_direction = GetValue(mUI.direction);
    if (particle_direction == gfx::ParticleEngineClass::Direction::Sector)
    {
        const float dir_angle_start = GetValue(mUI.dirStartAngle);
        const float dir_angle_size  = GetValue(mUI.dirSizeAngle);

        gfx::Transform transform;
        transform.Translate(widget_width - 70.0f, 70.0f);
        transform.Push();
            transform.Scale(100.0f, 100.0f);
            transform.Translate(-50.0f, -50.0f);
            painter.Draw(gfx::Rectangle(), transform,
                         gfx::CreateMaterialFromColor(gfx::Color4f(gfx::Color::Black, 0.2f)));
        transform.Pop();
        transform.Push();
        transform.RotateAroundZ(qDegreesToRadians(dir_angle_start));
            transform.Push();
                transform.Scale(50.0f, 5.0f);
                transform.Translate(0.0f, -2.5f);
                transform.RotateAroundZ(qDegreesToRadians(dir_angle_size));
                painter.Draw(gfx::Arrow(), transform,
                             gfx::CreateMaterialFromColor(gfx::Color::Yellow));
            transform.Pop();
            transform.Push();
                transform.Scale(50.0f, 5.0f);
                transform.Translate(0.0f, -2.5f);
                painter.Draw(gfx::Arrow(), transform,
                             gfx::CreateMaterialFromColor(gfx::Color::Green));
            transform.Pop();
        transform.Pop();
    }


    const auto& params = mClass->GetParams();
    const auto emission = params.mode;
    if (mEngine)
    {
        const auto count = mEngine->GetNumParticlesAlive();
        if (emission == gfx::ParticleEngineClass::SpawnPolicy::Command && count == 0)
        {
            const auto& shortcut = mUI.actionEmit->shortcut().toString();
            ShowMessage(app::toString("Hit %1 to emit some particles!", shortcut), painter);
        }
        else
        {
            ShowMessage(base::FormatString("Particles %1", mEngine->GetNumParticlesAlive()), painter);
        }
    }
}

void ParticleEditorWidget::MouseMove(QMouseEvent* mickey)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();

    // Apply tool action
    if (mMouseTool)
    {
        gfx::Transform view;
        view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
        view.Translate(width*0.5, height*0.5);
        mMouseTool->MouseMove(mickey, view);

        // update UI
        SetValue(mUI.initX, mState->emitter_xpos);
        SetValue(mUI.initY, mState->emitter_ypos);
        SetValue(mUI.initWidth, mState->emitter_width);
        SetValue(mUI.initHeight, mState->emitter_height);

        SetValue(mUI.scaleX, mState->visualization_width);
        SetValue(mUI.scaleY, mState->visualization_height);
        SetValue(mUI.translateX, mState->visualization_xpos);
        SetValue(mUI.translateY, mState->visualization_ypos);
        // apply changes from UI to the class object
        SetParams();
    }
}

void ParticleEditorWidget::MousePress(QMouseEvent* mickey)
{
    const auto widget_width  = mUI.widget->width();
    const auto widget_height = mUI.widget->height();
    const auto zoom   = (float)GetValue(mUI.zoom);

    gfx::Transform view;
    view.Scale(zoom, zoom);
    view.Translate(widget_width*0.5, widget_height*0.5);

    if (!mMouseTool)
    {
        const float viz_xpos   = GetValue(mUI.translateX);
        const float viz_ypos   = GetValue(mUI.translateY);
        const float viz_width  = GetValue(mUI.scaleX);
        const float viz_height = GetValue(mUI.scaleY);
        const float viz_rot    = qDegreesToRadians((float)GetValue(mUI.rotation));
        // note that these are normalized coordinates that get scaled
        // already by scaling factor set above for the whole visualization
        const float emitter_width  = GetValue(mUI.initWidth);
        const float emitter_height = GetValue(mUI.initHeight);
        const float emitter_left   = GetValue(mUI.initX);
        const float emitter_top    = GetValue(mUI.initY);
        const float emitter_right  = emitter_left + emitter_width;
        const float emitter_bottom = emitter_top + emitter_height;

        view.Push();
            view.Scale(viz_width, viz_height);
            view.Translate(-viz_width*0.5, -viz_height*0.5);
            view.RotateAroundZ(viz_rot);
            view.Translate(viz_xpos, viz_ypos);

        const auto& local_to_view  = view.GetAsMatrix();
        const auto& view_to_local  = glm::inverse(local_to_view);
        const auto& coord_in_local = view_to_local * ToVec4(mickey->pos());
        //DEBUG("click %1", coord_in_local);

        const auto local_x = coord_in_local.x;
        const auto local_y = coord_in_local.y;
        if ((local_x < 0.0f || local_x > 1.0f) || (local_y < 0.0f || local_y > 1.0f))
            return;

        auto state = std::make_unique<UIState>();
        state->visualization_xpos     = viz_xpos;
        state->visualization_ypos     = viz_ypos;
        state->visualization_width    = viz_width;
        state->visualization_height   = viz_height;
        state->visualization_rotation = viz_rot;
        state->emitter_xpos           = emitter_left;
        state->emitter_ypos           = emitter_top;
        state->emitter_width          = emitter_width;
        state->emitter_height         = emitter_height;

        const gfx::ParticleEngineClass::CoordinateSpace space = GetValue(mUI.space);
        if ((space == gfx::ParticleEngineClass::CoordinateSpace::Local) &&
            (local_x >= emitter_left && local_x <= emitter_right) &&
            (local_y >= emitter_top && local_y <= emitter_bottom))
        {
            const auto scalex = zoom * viz_width * emitter_width;
            const auto scaley = zoom * viz_height * emitter_height;
            view.Push();
                view.Scale(emitter_width, emitter_height);
                view.Translate(emitter_left, emitter_top);
                view.Push();
                    view.Scale(10.0f/scalex, 10.0f/scaley);
                    view.Translate(1.0f-(10.0f/scalex), 1.0f-(10.0f/scaley));
                    const auto& size_box_to_view = view.GetAsMatrix();
                    const auto& view_to_size_box = glm::inverse(size_box_to_view);
                    const auto& coord_in_size_box = view_to_size_box * ToVec4(mickey->pos());
                 view.Pop();
            view.Pop();
            //DEBUG("click %1", coord_in_size_box);
            const bool size_box_click = (coord_in_size_box.x >= 0.0f && coord_in_size_box.x <= 1.0f) &&
                                        (coord_in_size_box.y >= 0.0f && coord_in_size_box.y <= 1.0f);
            mState = std::move(state);
            mMouseTool.reset(size_box_click ? (MouseTool*)new SizeEmitterTool(*mState)
                                            : (MouseTool*)new MoveEmitterTool(*mState));

        }
        else if ((local_x >= 0.0f && local_x <= 1.0f) && (local_y >= 0.0f && local_y <= 1.0f))
        {
            const auto scalex = zoom * viz_width;
            const auto scaley = zoom * viz_height;
            view.Push();
                view.Scale(10.0f/scalex, 10.0f/scaley);
                view.Translate(1.0f-(10.0f/scalex), 1.0f-(10.0f/scaley));
                const auto& size_box_to_view = view.GetAsMatrix();
                const auto& view_to_size_box = glm::inverse(size_box_to_view);
                const auto& coord_in_size_box = view_to_size_box * ToVec4(mickey->pos());
            view.Pop();
            const bool size_box_click = (coord_in_size_box.x >= 0.0f && coord_in_size_box.x <= 1.0f) &&
                                        (coord_in_size_box.y >= 0.0f && coord_in_size_box.y <= 1.0f);
            DEBUG("click %1", coord_in_size_box);
            mState = std::move(state);
            mMouseTool.reset(size_box_click ? (MouseTool*)new SizeVizTool(*mState)
                                            : (MouseTool*)new MoveVizTool(*mState));
        }
        view.Pop();
    }

    if (mMouseTool)
    {
        mMouseTool->MousePress(mickey, view);
    }
}
void ParticleEditorWidget::MouseRelease(QMouseEvent* mickey)
{
    mMouseTool.reset();
}

void ParticleEditorWidget::VelocityChanged(float min, float max)
{
    SetValue(mUI.minVelocity, min);
    SetValue(mUI.maxVelocity, max);
    SetParams();
    MinMax();
}

void ParticleEditorWidget::LifetimeChanged(float min, float max)
{
    SetValue(mUI.minLifetime, min);
    SetValue(mUI.maxLifetime, max);
    SetParams();
    MinMax();
}

void ParticleEditorWidget::PointsizeChanged(float min, float max)
{
    SetValue(mUI.minPointsize, min);
    SetValue(mUI.maxPointsize, max);
    SetParams();
    MinMax();
}
void ParticleEditorWidget::AlphaChanged(float min, float max)
{
    SetValue(mUI.minAlpha, min);
    SetValue(mUI.maxAlpha, max);
    SetParams();
    MinMax();
}

void ParticleEditorWidget::OnAddResource(const app::Resource* resource)
{
    // this is simple, just add new resources in the appropriate UI objects.
    if (resource->GetType() == app::Resource::Type::Material)
    {
        SetList(mUI.materials, mWorkspace->ListAllMaterials());
    }
}

void ParticleEditorWidget::OnUpdateResource(const app::Resource* resource)
{
    if (resource->GetType() == app::Resource::Type::Material)
    {
        SetList(mUI.materials, mWorkspace->ListAllMaterials());

        if (mMaterialClass && mMaterialClass->GetId() == resource->GetIdUtf8())
        {
            mMaterialClass = resource->GetContent<gfx::MaterialClass>()->Copy();
            mMaterial.reset();
            DEBUG("Particle editor material was changed!");
            ShowParams();
        }
        else if (mMaterial && mMaterial->GetClassId() == resource->GetIdUtf8())
        {
            mMaterial.reset();
        }
    }
}

void ParticleEditorWidget::OnRemoveResource(const app::Resource* resource)
{
    if (resource->GetType() == app::Resource::Type::Material)
    {
        SetList(mUI.materials, mWorkspace->ListAllMaterials());

        if (mMaterialClass && mMaterialClass->GetId() == resource->GetIdUtf8())
        {
            // nothing to do here, if we have our own material class.
            // it was already saved, but then deleted, but ok, we still
            // keep our reference and if there's no save then it's gone
            // for ever.
            DEBUG("Particle engine material was deleted.");
            SetValue(mUI.materials, mMaterialClass->GetName());
        }
        else if (mMaterial && mMaterial->GetClassId() == resource->GetIdUtf8())
        {
            mMaterial.reset();
            SetValue(mUI.materials, ListItemId(QString("_White")));
        }
    }
}
void ParticleEditorWidget::CreateMaterial()
{
    if (mMaterial)
        return;

    if (mMaterialClass)
    {
        mMaterial = gfx::CreateMaterialInstance(mMaterialClass);
    }
    else
    {
        const auto& klass = mWorkspace->FindMaterialClassById(GetItemId(mUI.materials));
        mMaterial = gfx::CreateMaterialInstance(klass);
    }
}

bool ParticleEditorWidget::FindMaterialClass(const std::string& id)
{
    const auto& materials = mWorkspace->ListUserDefinedMaterials();
    for (const auto& material : materials)
    {
        if (!id.empty())
        {
            if (id != material.resource->GetIdUtf8())
                continue;
        }

        std::string particleId;
        if (!material.resource->GetProperty("particle-engine-class-id", &particleId))
            continue;

        if (particleId != mClass->GetId())
            continue;

        mMaterialClass = material.resource->GetContent<gfx::MaterialClass>()->Copy();
        DEBUG("Found previous material assignment for particle engine.");
        return true;
    }
    DEBUG("No material specific to this particle engine was found.");
    return false;
}

size_t ParticleEditorWidget::GetHash() const
{
    size_t hash = mClass->GetHash();
    if (mMaterialClass)
        hash = base::hash_combine(hash, mMaterialClass->GetHash());
    return hash;
}

} // namespace
