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
#include "graphics/transform.h"
#include "graphics/drawing.h"
#include "graphics/types.h"
#include "editor/app/eventlog.h"
#include "editor/app/resource.h"
#include "editor/app/utility.h"
#include "editor/app/workspace.h"
#include "editor/gui/utility.h"
#include "editor/gui/settings.h"
#include "editor/gui/drawing.h"
#include "editor/gui/utility.h"
#include "editor/gui/particlewidget.h"
#include "editor/gui/dlgmaterial.h"
#include "editor/gui/tool.h"

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
    virtual void MouseMove(QMouseEvent* mickey, gfx::Transform& view) override
    {
        const float viz_width  = mState.visualization_width;
        const float viz_height = mState.visualization_height;
        const float viz_rot    = mState.visualization_rotation;
        view.Push();
        view.Scale(viz_width, viz_height);
        view.Translate(-viz_width*0.5, -viz_height*0.5);
        view.Rotate(viz_rot);

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
    virtual void MousePress(QMouseEvent* mickey, gfx::Transform& view) override
    {
        const float viz_width  = mState.visualization_width;
        const float viz_height = mState.visualization_height;
        const float viz_rot    = mState.visualization_rotation;
        view.Push();
        view.Scale(viz_width, viz_height);
        view.Translate(-viz_width*0.5, -viz_height*0.5);
        view.Rotate(viz_rot);

        const auto& local_to_view  = view.GetAsMatrix();
        const auto& view_to_local  = glm::inverse(local_to_view);
        const auto& coord_in_local = view_to_local * ToVec4(mickey->pos());
        mMousePos = coord_in_local;
        view.Pop();
    }
    virtual bool MouseRelease(QMouseEvent* mickey, gfx::Transform& view) override
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
    virtual void MouseMove(QMouseEvent* mickey, gfx::Transform& view) override
    {
        const float viz_width  = mState.visualization_width;
        const float viz_height = mState.visualization_height;
        const float viz_rot    = mState.visualization_rotation;
        view.Push();
        view.Scale(viz_width, viz_height);
        view.Translate(-viz_width*0.5, -viz_height*0.5);
        view.Rotate(viz_rot);

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
    virtual void MousePress(QMouseEvent* mickey, gfx::Transform& view) override
    {
        const float viz_width  = mState.visualization_width;
        const float viz_height = mState.visualization_height;
        const float viz_rot    = mState.visualization_rotation;
        view.Push();
        view.Scale(viz_width, viz_height);
        view.Translate(-viz_width*0.5, -viz_height*0.5);
        view.Rotate(viz_rot);

        const auto& local_to_view  = view.GetAsMatrix();
        const auto& view_to_local  = glm::inverse(local_to_view);
        const auto& coord_in_local = view_to_local * ToVec4(mickey->pos());
        mMousePos = coord_in_local;
        view.Pop();
    }
    virtual bool MouseRelease(QMouseEvent* mickey, gfx::Transform& view) override
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
    virtual void MouseMove(QMouseEvent* mickey, gfx::Transform& view) override
    {
        const auto& local_to_view  = view.GetAsMatrix();
        const auto& view_to_local  = glm::inverse(local_to_view);
        const auto& coord_in_local = view_to_local * ToVec4(mickey->pos());
        const auto& mouse_delta    = coord_in_local - mMousePos;
        mState.visualization_xpos += mouse_delta.x;
        mState.visualization_ypos += mouse_delta.y;
        mMousePos = coord_in_local;
    }
    virtual void MousePress(QMouseEvent* mickey, gfx::Transform& view) override
    {
        const auto& local_to_view  = view.GetAsMatrix();
        const auto& view_to_local  = glm::inverse(local_to_view);
        const auto& coord_in_local = view_to_local * ToVec4(mickey->pos());
        mMousePos = coord_in_local;
    }
    virtual bool MouseRelease(QMouseEvent* mickey, gfx::Transform& view) override
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
    virtual void MouseMove(QMouseEvent* mickey, gfx::Transform& view) override
    {
        const auto& local_to_view  = view.GetAsMatrix();
        const auto& view_to_local  = glm::inverse(local_to_view);
        const auto& coord_in_local = view_to_local * ToVec4(mickey->pos());
        const auto& mouse_delta    = coord_in_local - mMousePos;
        mState.visualization_width  += (mouse_delta.x * 2.0f);
        mState.visualization_height += (mouse_delta.y * 2.0f);
        mMousePos = coord_in_local;
    }
    virtual void MousePress(QMouseEvent* mickey, gfx::Transform& view) override
    {
        const auto& local_to_view  = view.GetAsMatrix();
        const auto& view_to_local  = glm::inverse(local_to_view);
        const auto& coord_in_local = view_to_local * ToVec4(mickey->pos());
        mMousePos = coord_in_local;
    }
    virtual bool MouseRelease(QMouseEvent* mickey, gfx::Transform& view) override
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
    mClass = std::make_shared<gfx::KinematicsParticleEngineClass>();
    DEBUG("Create ParticleEditorWidget");

    mUI.setupUi(this);
    mUI.widget->onPaintScene = std::bind(&ParticleEditorWidget::PaintScene,
        this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onMouseMove    = std::bind(&ParticleEditorWidget::MouseMove, this, std::placeholders::_1);
    mUI.widget->onMouseRelease = std::bind(&ParticleEditorWidget::MouseRelease, this, std::placeholders::_1);
    mUI.widget->onMousePress   = std::bind(&ParticleEditorWidget::MousePress, this, std::placeholders::_1);
    mUI.widget->onZoomIn  = std::bind(&ParticleEditorWidget::ZoomIn, this);
    mUI.widget->onZoomOut = std::bind(&ParticleEditorWidget::ZoomOut, this);

    // if you change this change the UI widget values min/max values too!
    mUI.velocity->SetScale(1000.0f);
    mUI.velocity->SetExponent(2.2f);
    mUI.lifetime->SetScale(100.0f);
    mUI.lifetime->SetExponent(1.0f);
    mUI.pointsize->SetScale(2048);
    mUI.pointsize->SetExponent(2.2f);
    mUI.alpha->SetScale(1.0f);
    mUI.alpha->SetExponent(1.0f);

    PopulateFromEnum<gfx::KinematicsParticleEngineClass::CoordinateSpace>(mUI.space);
    PopulateFromEnum<gfx::KinematicsParticleEngineClass::Motion>(mUI.motion);
    PopulateFromEnum<gfx::KinematicsParticleEngineClass::BoundaryPolicy>(mUI.boundary);
    PopulateFromEnum<gfx::KinematicsParticleEngineClass::SpawnPolicy>(mUI.when);
    PopulateFromEnum<gfx::KinematicsParticleEngineClass::EmitterShape>(mUI.shape);
    PopulateFromEnum<gfx::KinematicsParticleEngineClass::Placement>(mUI.placement);
    PopulateFromEnum<gfx::KinematicsParticleEngineClass::Direction>(mUI.direction);
    PopulateFromEnum<GridDensity>(mUI.cmbGrid);
    SetList(mUI.materials, workspace->ListAllMaterials());
    SetValue(mUI.name, QString("My Particle System"));
    SetValue(mUI.ID, mClass->GetId());
    SetValue(mUI.scaleX, 500.0f);
    SetValue(mUI.scaleY, 500.0f);
    SetValue(mUI.rotation, 0.0f);
    SetValue(mUI.materials, ListItemId(QString("_White")));
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

    // connect workspace signals for resource management
    connect(mWorkspace, &app::Workspace::NewResourceAvailable,this, &ParticleEditorWidget::NewResourceAvailable);
    connect(mWorkspace, &app::Workspace::ResourceToBeDeleted, this, &ParticleEditorWidget::ResourceToBeDeleted);
    connect(mWorkspace, &app::Workspace::ResourceUpdated,     this, &ParticleEditorWidget::ResourceUpdated);

    connect(mUI.velocity,  &gui::RangeWidget::RangeChanged, this, &ParticleEditorWidget::VelocityChanged);
    connect(mUI.lifetime,  &gui::RangeWidget::RangeChanged, this, &ParticleEditorWidget::LifetimeChanged);
    connect(mUI.pointsize, &gui::RangeWidget::RangeChanged, this, &ParticleEditorWidget::PointsizeChanged);
    connect(mUI.alpha,     &gui::RangeWidget::RangeChanged, this, &ParticleEditorWidget::AlphaChanged);
    setWindowTitle("My Particle System");
    mOriginalHash = mClass->GetHash();

}

ParticleEditorWidget::ParticleEditorWidget(app::Workspace* workspace, const app::Resource& resource)
  : ParticleEditorWidget(workspace)
{
    const auto& name   = resource.GetName();
    const auto* klass  = resource.GetContent<gfx::KinematicsParticleEngineClass>();
    const auto& params = klass->GetParams();
    mClass = std::make_shared<gfx::KinematicsParticleEngineClass>(*klass);
    mOriginalHash = mClass->GetHash();
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
    GetProperty(resource, "use_lifetime", mUI.canExpire);
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
    if (mWorkspace->IsValidMaterial(material))
    {
        SetValue(mUI.materials, ListItemId(material));
    }
    else
    {
        WARN("Material '%1' is no longer available.", material);
        SetValue(mUI.materials, QString("White"));
    }
    MinMax();
    ShowParams();
    on_motion_currentIndexChanged(0);
    on_space_currentIndexChanged(0);
    on_direction_currentIndexChanged(0);
    on_canExpire_stateChanged(0);
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
}

void ParticleEditorWidget::AddActions(QMenu& menu)
{
    menu.addAction(mUI.actionPlay);
    menu.addAction(mUI.actionPause);
    menu.addSeparator();
    menu.addAction(mUI.actionStop);
    menu.addSeparator();
    menu.addAction(mUI.actionSave);
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
    settings.SaveWidget("Particle", mUI.canExpire);
    settings.SaveWidget("Particle", mUI.chkShowGrid);
    settings.SaveWidget("Particle", mUI.chkShowBounds);
    settings.SaveWidget("Particle", mUI.chkShowEmitter);
    settings.SaveWidget("Particle", mUI.cmbGrid);
    settings.SaveWidget("Particle", mUI.zoom);
    settings.SaveWidget("Particle", mUI.widget);
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
    settings.LoadWidget("Particle", mUI.canExpire);
    settings.LoadWidget("Particle", mUI.chkShowGrid);
    settings.LoadWidget("Particle", mUI.chkShowBounds);
    settings.LoadWidget("Particle", mUI.chkShowEmitter);
    settings.LoadWidget("Particle", mUI.cmbGrid);
    settings.LoadWidget("Particle", mUI.zoom);
    settings.LoadWidget("Particle", mUI.widget);

    mClass = std::make_shared<gfx::KinematicsParticleEngineClass>();
    if (!mClass->FromJson(json))
        WARN("Failed to restore particle engine state.");

    SetValue(mUI.ID, mClass->GetId());
    SetValue(mUI.materials, ListItemId(material));

    ShowParams();
    MinMax();
    on_motion_currentIndexChanged(0);
    on_space_currentIndexChanged(0);
    on_direction_currentIndexChanged(0);
    on_canExpire_stateChanged(0);
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
    BUG("Unhandled action query.");
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
        model.Rotate(viz_rot);
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
    if (mOriginalHash != mClass->GetHash())
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

    app::ParticleSystemResource particle_resource(*mClass, GetValue(mUI.name));
    SetProperty(particle_resource, "material", (QString)GetItemId(mUI.materials));
    SetProperty(particle_resource, "transform_xpos", mUI.translateX);
    SetProperty(particle_resource, "transform_ypos", mUI.translateY);
    SetProperty(particle_resource, "transform_width", mUI.scaleX);
    SetProperty(particle_resource, "transform_height", mUI.scaleY);
    SetProperty(particle_resource, "transform_rotation", mUI.rotation);
    SetProperty(particle_resource, "use_lifetime", mUI.canExpire);
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

    mWorkspace->SaveResource(particle_resource);
    mOriginalHash = mClass->GetHash();
}

void ParticleEditorWidget::SetParams()
{
    gfx::KinematicsParticleEngineClass::Params params;
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
    if (GetValue(mUI.canExpire))
    {
        params.min_lifetime   = GetValue(mUI.minLifetime);
        params.max_lifetime   = GetValue(mUI.maxLifetime);
    }
    else
    {
        params.min_lifetime   = std::numeric_limits<float>::max();
        params.max_lifetime   = std::numeric_limits<float>::max();
    }
    params.min_time = GetValue(mUI.minTime);
    params.max_time = GetValue(mUI.maxTime);
    if (params.max_time == 0.0f)
        params.max_time = std::numeric_limits<float>::max();

    params.delay = GetValue(mUI.delay);

    const gfx::KinematicsParticleEngineClass::CoordinateSpace space = GetValue(mUI.space);
    if (space == gfx::KinematicsParticleEngineClass::CoordinateSpace::Local)
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
    mClass->SetParams(params);
}

void ParticleEditorWidget::ShowParams()
{
    const auto& params = mClass->GetParams();
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

    if (params.coordinate_space == gfx::KinematicsParticleEngineClass::CoordinateSpace::Local)
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
    model.Rotate(viz_rot);
    model.Translate(viz_xpos, viz_ypos);
    const auto& model_matrix = model.GetAsMatrix();

    gfx::Drawable::Environment env;
    env.model_matrix = &model_matrix;
    env.editing_mode = true;

    mEngine.reset(new gfx::KinematicsParticleEngine(mClass));
    mEngine->Restart(env);
    mTime   = 0.0f;
    mPaused = false;
    SetEnabled(mUI.actionPause, true);
    SetEnabled(mUI.actionStop, true);
    DEBUG("Created new particle engine");
}

void ParticleEditorWidget::on_actionStop_triggered()
{
    mEngine.reset();
    mMaterial.reset();
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
}

void ParticleEditorWidget::on_space_currentIndexChanged(int)
{
    const gfx::KinematicsParticleEngineClass::CoordinateSpace space = GetValue(mUI.space);
    if (space == gfx::KinematicsParticleEngineClass::CoordinateSpace::Local)
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
    const gfx::KinematicsParticleEngineClass::Motion motion = GetValue(mUI.motion);
    if (motion == gfx::KinematicsParticleEngineClass::Motion::Projectile)
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
    const gfx::KinematicsParticleEngineClass::Direction dir = GetValue(mUI.direction);
    if (dir == gfx::KinematicsParticleEngineClass::Direction::Sector)
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
    if (GetValue(mUI.canExpire))
    {
        SetEnabled(mUI.minLifetime, true);
        SetEnabled(mUI.maxLifetime, true);
    }
    else
    {
        SetEnabled(mUI.minLifetime, false);
        SetEnabled(mUI.maxLifetime, false);
    }
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
    model.Rotate(viz_rot);
    model.Translate(viz_xpos, viz_ypos);

    if (GetValue(mUI.chkShowBounds))
    {
        painter.Draw(gfx::Rectangle(gfx::Drawable::Style::Outline), model,
                     gfx::CreateMaterialFromColor(gfx::Color::HotPink));
        const auto scalex = zoom * viz_width;
        const auto scaley = zoom * viz_height;
        model.Push();
            model.Scale(10.0f/scalex, 10.0f/scaley);
            model.Translate(1.0f-(10.0f/scalex), 1.0f-(10.0f/scaley));
            painter.Draw(gfx::Rectangle(gfx::Drawable::Style::Outline, 2.0f), model,
                         gfx::CreateMaterialFromColor(gfx::Color::HotPink));
        model.Pop();
    }

    if (mEngine)
    {
        const auto& id = (std::string)GetItemId(mUI.materials);
        if (!mMaterial || (mMaterial->GetClassId() != id))
        {
            auto klass = mWorkspace->GetMaterialClassById(GetItemId(mUI.materials));
            mMaterial = gfx::CreateMaterialInstance(klass);
        }
        painter.Draw(*mEngine, model, *mMaterial);
    }

    const gfx::KinematicsParticleEngineClass::CoordinateSpace space = GetValue(mUI.space);

    if (GetValue(mUI.chkShowEmitter) && space == gfx::KinematicsParticleEngineClass::CoordinateSpace::Local)
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
            painter.Draw(gfx::Rectangle(gfx::Drawable::Style::Outline, 2.0f), model,
                         gfx::CreateMaterialFromColor(gfx::Color::Green));

            const auto scalex = zoom * viz_width * emitter_width;
            const auto scaley = zoom * viz_height * emitter_height;
            model.Push();
                model.Scale(10.0f/scalex, 10.0f/scaley);
                model.Translate(1.0f-(10.0f/scalex), 1.0f-(10.0f/scaley));
                painter.Draw(gfx::Rectangle(gfx::Drawable::Style::Outline, 2.0f), model,
                             gfx::CreateMaterialFromColor(gfx::Color::Green));
            model.Pop();
        model.Pop();
    }

    painter.ResetViewMatrix();

    // Draw the visualization for the particle direction sector
    // we draw this in the widget/window coordinates in the top right
    {
        const float dir_angle_start = GetValue(mUI.dirStartAngle);
        const float dir_angle_size  = GetValue(mUI.dirSizeAngle);

        gfx::Transform transform;
        transform.Translate(widget_width - 70.0f, 70.0f);
        transform.Push();
            transform.Scale(100.0f, 100.0f);
            transform.Translate(-50.0f, -50.0f);
            painter.Draw(gfx::Rectangle(), transform,
                         gfx::CreateMaterialFromColor(gfx::Color4f(gfx::Color::Black, 0.6f)));
        transform.Pop();
        transform.Push();
            transform.Rotate(qDegreesToRadians(dir_angle_start));
            transform.Push();
                transform.Scale(50.0f, 5.0f);
                transform.Translate(0.0f, -2.5f);
                transform.Rotate(qDegreesToRadians(dir_angle_size));
                painter.Draw(gfx::Rectangle(), transform,
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

    if (mEngine)
    {
        ShowMessage(base::FormatString("Particles %1", mEngine->GetNumParticlesAlive()), painter);
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
            view.Rotate(viz_rot);
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

        const gfx::KinematicsParticleEngineClass::CoordinateSpace space = GetValue(mUI.space);
        if ((space == gfx::KinematicsParticleEngineClass::CoordinateSpace::Local) &&
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

void ParticleEditorWidget::NewResourceAvailable(const app::Resource* resource)
{
    // this is simple, just add new resources in the appropriate UI objects.
    if (resource->GetType() == app::Resource::Type::Material)
    {
        SetList(mUI.materials, mWorkspace->ListAllMaterials());
    }
}

void ParticleEditorWidget::ResourceUpdated(const app::Resource* resource)
{
    if (resource->GetType() == app::Resource::Type::Material)
    {
        if (mMaterial && mMaterial->GetClassId() == resource->GetIdUtf8())
            mMaterial.reset();
    }
}

void ParticleEditorWidget::ResourceToBeDeleted(const app::Resource* resource)
{
    if (resource->GetType() == app::Resource::Type::Material)
    {
        SetList(mUI.materials, mWorkspace->ListAllMaterials());
        if (mMaterial && mMaterial->GetClassId() == resource->GetIdUtf8())
        {
            mMaterial.reset();
            SetValue(mUI.materials, ListItemId(QString("_White")));
        }
    }
}

} // namespace
