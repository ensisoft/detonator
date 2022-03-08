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

namespace gui
{

ParticleEditorWidget::ParticleEditorWidget(app::Workspace* workspace)
{
    mWorkspace = workspace;

    DEBUG("Create ParticleEditorWidget");
    mUI.setupUi(this);
    mUI.widget->onPaintScene = std::bind(&ParticleEditorWidget::PaintScene,
        this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onZoomIn  = std::bind(&ParticleEditorWidget::ZoomIn, this);
    mUI.widget->onZoomOut = std::bind(&ParticleEditorWidget::ZoomOut, this);

    // get the current list of materials from the workspace
    SetList(mUI.materials, workspace->ListAllMaterials());
    SetValue(mUI.materials, QString("White"));

    // Set default transform state here. if there's a previous user setting
    // they'll get loaded in LoadState and will override these values.
    mUI.transform->setChecked(false);
    mUI.scaleX->setValue(500);
    mUI.scaleY->setValue(500);
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(false);
    SetValue(mUI.name, QString("My Particle System"));
    SetValue(mUI.ID, mClass.GetId());

    PopulateFromEnum<gfx::KinematicsParticleEngineClass::Motion>(mUI.motion);
    PopulateFromEnum<gfx::KinematicsParticleEngineClass::BoundaryPolicy>(mUI.boundary);
    PopulateFromEnum<gfx::KinematicsParticleEngineClass::SpawnPolicy>(mUI.when);
    PopulateFromEnum<GridDensity>(mUI.cmbGrid);
    SetValue(mUI.cmbGrid, GridDensity::Grid50x50);
    on_motion_currentIndexChanged(0);

    // connect workspace signals for resource management
    connect(mWorkspace, &app::Workspace::NewResourceAvailable,this, &ParticleEditorWidget::NewResourceAvailable);
    connect(mWorkspace, &app::Workspace::ResourceToBeDeleted, this, &ParticleEditorWidget::ResourceToBeDeleted);
    connect(mWorkspace, &app::Workspace::ResourceUpdated,     this, &ParticleEditorWidget::ResourceUpdated);
    setWindowTitle("My Particle System");
}

ParticleEditorWidget::ParticleEditorWidget(app::Workspace* workspace, const app::Resource& resource) : ParticleEditorWidget(workspace)
{
    const auto& name   = resource.GetName();
    const auto* engine = resource.GetContent<gfx::KinematicsParticleEngineClass>();
    const auto& params = engine->GetParams();
    DEBUG("Editing particle system: '%1'", name);

    QString material;
    GetProperty(resource, "material", &material);
    GetProperty(resource, "transform_width", mUI.scaleX);
    GetProperty(resource, "transform_height", mUI.scaleY);
    GetProperty(resource, "transform_rotation", mUI.rotation);
    GetProperty(resource, "use_init_rect", mUI.initRect);
    GetProperty(resource, "use_direction_sector", mUI.dirSector);
    GetProperty(resource, "use_size_derivatives", mUI.sizeDerivatives);
    GetProperty(resource, "use_alpha_derivatives", mUI.alphaDerivatives);
    GetProperty(resource, "use_lifetime", mUI.canExpire);
    GetUserProperty(resource, "grid", mUI.cmbGrid);
    GetUserProperty(resource, "zoom", mUI.zoom);
    GetUserProperty(resource, "show_grid", mUI.chkShowGrid);
    GetUserProperty(resource, "show_bounds", mUI.chkShowBounds);

    if (mWorkspace->IsValidMaterial(material))
    {
        SetValue(mUI.materials, ListItemId(material));
    }
    else
    {
        WARN("Material '%1' is no longer available.", material);
        SetValue(mUI.materials, QString("White"));
    }

    SetValue(mUI.name, name);
    SetValue(mUI.ID, engine->GetId());
    SetValue(mUI.motion, params.motion);
    SetValue(mUI.when,   params.mode);
    SetValue(mUI.boundary, params.boundary);
    SetValue(mUI.numParticles, params.num_particles);
    SetValue(mUI.simWidth, params.max_xpos);
    SetValue(mUI.simHeight, params.max_ypos);
    SetValue(mUI.gravityX, params.gravity.x);
    SetValue(mUI.gravityY, params.gravity.y);
    SetValue(mUI.minLifetime, params.min_lifetime);
    SetValue(mUI.maxLifetime, params.max_lifetime);
    SetValue(mUI.minPointsize, params.min_point_size);
    SetValue(mUI.maxPointsize, params.max_point_size);
    SetValue(mUI.minAlpha, params.min_alpha);
    SetValue(mUI.maxAlpha, params.max_alpha);
    SetValue(mUI.minVelocity, params.min_velocity);
    SetValue(mUI.maxVelocity, params.max_velocity);
    SetValue(mUI.initX, params.init_rect_xpos);
    SetValue(mUI.initY, params.init_rect_ypos);
    SetValue(mUI.initWidth, params.init_rect_width);
    SetValue(mUI.initHeight, params.init_rect_height);
    SetValue(mUI.dirStartAngle, qRadiansToDegrees(params.direction_sector_start_angle));
    SetValue(mUI.dirSizeAngle, qRadiansToDegrees(params.direction_sector_size));
    SetValue(mUI.timeSizeDerivative, params.rate_of_change_in_size_wrt_time);
    SetValue(mUI.distSizeDerivative, params.rate_of_change_in_size_wrt_dist);
    SetValue(mUI.timeAlphaDerivative, params.rate_of_change_in_alpha_wrt_time);
    SetValue(mUI.distAlphaDerivative, params.rate_of_change_in_alpha_wrt_dist);
    on_motion_currentIndexChanged(0);
    mOriginalHash = engine->GetHash();
    mClass = *engine;

    setWindowTitle(name);
}

ParticleEditorWidget::~ParticleEditorWidget()
{
    DEBUG("Destroy ParticleEdtiorWidget");
}

void ParticleEditorWidget::Initialize(const UISettings& settings)
{
    SetValue(mUI.zoom,        settings.zoom);
    SetValue(mUI.cmbGrid,     settings.grid);
    SetValue(mUI.chkShowGrid, settings.show_grid);
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
    mClass.IntoJson(json);
    settings.SetValue("Particle", "content", base64::Encode(json.ToString()));
    settings.SetValue("Particle", "material", (QString) GetItemId(mUI.materials));
    settings.SaveWidget("Particle", mUI.name);
    settings.SaveWidget("Particle", mUI.ID);
    settings.SaveWidget("Particle", mUI.scaleX);
    settings.SaveWidget("Particle", mUI.scaleY);
    settings.SaveWidget("Particle", mUI.rotation);
    settings.SaveWidget("Particle", mUI.simWidth);
    settings.SaveWidget("Particle", mUI.simHeight);
    settings.SaveWidget("Particle", mUI.motion);
    settings.SaveWidget("Particle", mUI.boundary);
    settings.SaveWidget("Particle", mUI.gravityX);
    settings.SaveWidget("Particle", mUI.gravityY);
    settings.SaveWidget("Particle", mUI.when);
    settings.SaveWidget("Particle", mUI.numParticles);
    settings.SaveWidget("Particle", mUI.initRect);
    settings.SaveWidget("Particle", mUI.initX);
    settings.SaveWidget("Particle", mUI.initY);
    settings.SaveWidget("Particle", mUI.initWidth);
    settings.SaveWidget("Particle", mUI.initHeight);
    settings.SaveWidget("Particle", mUI.dirSector);
    settings.SaveWidget("Particle", mUI.dirStartAngle);
    settings.SaveWidget("Particle", mUI.dirSizeAngle);
    settings.SaveWidget("Particle", mUI.minVelocity);
    settings.SaveWidget("Particle", mUI.maxVelocity);
    settings.SaveWidget("Particle", mUI.minLifetime);
    settings.SaveWidget("Particle", mUI.maxLifetime);
    settings.SaveWidget("Particle", mUI.minPointsize);
    settings.SaveWidget("Particle", mUI.maxPointsize);
    settings.SaveWidget("Particle", mUI.minAlpha);
    settings.SaveWidget("Particle", mUI.maxAlpha);
    settings.SaveWidget("Particle", mUI.canExpire);
    settings.SaveWidget("Particle", mUI.sizeDerivatives);
    settings.SaveWidget("Particle", mUI.timeSizeDerivative);
    settings.SaveWidget("Particle", mUI.distSizeDerivative);
    settings.SaveWidget("Particle", mUI.alphaDerivatives);
    settings.SaveWidget("Particle", mUI.timeAlphaDerivative);
    settings.SaveWidget("Particle", mUI.distAlphaDerivative);
    settings.SaveWidget("Particle", mUI.chkShowGrid);
    settings.SaveWidget("Particle", mUI.chkShowBounds);
    settings.SaveWidget("Particle", mUI.cmbGrid);
    settings.SaveWidget("Particle", mUI.zoom);
    return true;
}

bool ParticleEditorWidget::LoadState(const Settings& settings)
{
    std::string base64;
    settings.GetValue("Particle", "content", &base64);

    data::JsonObject json;
    auto [ok, error] = json.ParseString(base64::Decode(base64));
    if (!ok)
    {
        ERROR("Failed to parse content JSON. '%1'", error);
        return false;
    }
    auto ret = gfx::KinematicsParticleEngineClass::FromJson(json);
    if (!ret.has_value())
    {
        ERROR("Failed to restore class from JSON.");
        return false;
    }
    mClass = std::move(ret.value());
    mOriginalHash = mClass.GetHash();

    QString material;
    settings.GetValue("Particle", "material", &material);
    if (mWorkspace->IsValidMaterial(material))
    {
        SetValue(mUI.materials, ListItemId(material));
    }
    else
    {
        WARN("Material '%1' is no longer available.", material);
        SetValue(mUI.materials, QString("White"));
    }

    settings.LoadWidget("Particle", mUI.name);
    settings.LoadWidget("Particle", mUI.ID);
    settings.LoadWidget("Particle", mUI.scaleX);
    settings.LoadWidget("Particle", mUI.scaleY);
    settings.LoadWidget("Particle", mUI.rotation);
    settings.LoadWidget("Particle", mUI.simWidth);
    settings.LoadWidget("Particle", mUI.simHeight);
    settings.LoadWidget("Particle", mUI.motion);
    settings.LoadWidget("Particle", mUI.boundary);
    settings.LoadWidget("Particle", mUI.when);
    settings.LoadWidget("Particle", mUI.gravityX);
    settings.LoadWidget("Particle", mUI.gravityY);
    settings.LoadWidget("Particle", mUI.numParticles);
    settings.LoadWidget("Particle", mUI.initRect);
    settings.LoadWidget("Particle", mUI.initX);
    settings.LoadWidget("Particle", mUI.initY);
    settings.LoadWidget("Particle", mUI.initWidth);
    settings.LoadWidget("Particle", mUI.initHeight);
    settings.LoadWidget("Particle", mUI.dirSector);
    settings.LoadWidget("Particle", mUI.dirStartAngle);
    settings.LoadWidget("Particle", mUI.dirSizeAngle);
    settings.LoadWidget("Particle", mUI.minVelocity);
    settings.LoadWidget("Particle", mUI.maxVelocity);
    settings.LoadWidget("Particle", mUI.minLifetime);
    settings.LoadWidget("Particle", mUI.maxLifetime);
    settings.LoadWidget("Particle", mUI.minPointsize);
    settings.LoadWidget("Particle", mUI.maxPointsize);
    settings.LoadWidget("Particle", mUI.minAlpha);
    settings.LoadWidget("Particle", mUI.maxAlpha);
    settings.LoadWidget("Particle", mUI.canExpire);
    settings.LoadWidget("Particle", mUI.sizeDerivatives);
    settings.LoadWidget("Particle", mUI.timeSizeDerivative);
    settings.LoadWidget("Particle", mUI.distSizeDerivative);
    settings.LoadWidget("Particle", mUI.alphaDerivatives);
    settings.LoadWidget("Particle", mUI.timeAlphaDerivative);
    settings.LoadWidget("Particle", mUI.distAlphaDerivative);
    settings.LoadWidget("Particle", mUI.chkShowGrid);
    settings.LoadWidget("Particle", mUI.chkShowBounds);
    settings.LoadWidget("Particle", mUI.cmbGrid);
    settings.LoadWidget("Particle", mUI.zoom);
    on_motion_currentIndexChanged(0);
    return true;
}

bool ParticleEditorWidget::CanTakeAction(Actions action, const Clipboard* clipboard) const
{
    switch (action)
    {
        case Actions::CanCut:
        case Actions::CanCopy:
        case Actions::CanPaste:
            return false;
        case Actions::CanReloadTextures:
        case Actions::CanReloadShaders:
            return true;
        case Actions::CanZoomIn: {
            const auto max = mUI.zoom->maximum();
            const auto val = mUI.zoom->value();
            return val < max;
        } break;
        case Actions::CanZoomOut: {
            const auto min = mUI.zoom->minimum();
            const auto val = mUI.zoom->value();
            return val > min;
        } break;
        case Actions::CanUndo:
            return false;
    }
    BUG("Unhandled action query.");
    return false;
}

void ParticleEditorWidget::ZoomIn()
{
    const auto value = mUI.zoom->value();
    mUI.zoom->setValue(value + 0.1);
}
void ParticleEditorWidget::ZoomOut()
{
    const auto value = mUI.zoom->value();
    mUI.zoom->setValue(value - 0.1);
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
        mEngine->Update(secs);

        mTime += secs;

        // created in paint function.
        if (mMaterial)
            mMaterial->SetRuntime(mTime);
    }

    if (!mEngine->IsAlive())
    {
        DEBUG("Particle simulation finished");
        mUI.actionStop->setEnabled(false);
        mUI.actionPause->setEnabled(false);
        mUI.actionPlay->setEnabled(true);
        mEngine.reset();
        mMaterial.reset();
        return;
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
    if (!mOriginalHash)
        return false;

    gfx::KinematicsParticleEngineClass::Params params;
    gfx::KinematicsParticleEngineClass klass;
    FillParams(params);
    klass.SetParams(params);
    const auto hash = klass.GetHash();
    return hash != mOriginalHash;
}

bool ParticleEditorWidget::ConfirmClose()
{
    gfx::KinematicsParticleEngineClass::Params params;
    FillParams(params);
    mClass.SetParams(params);

    const auto hash = mClass.GetHash();
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

    const QString& name = GetValue(mUI.name);

    gfx::KinematicsParticleEngineClass::Params params;
    FillParams(params);
    mClass.SetParams(params);

    // setup the resource with the current auxiliary params
    app::ParticleSystemResource particle_resource(mClass, name);
    SetProperty(particle_resource, "material", (QString)GetItemId(mUI.materials));
    SetProperty(particle_resource, "transform_width", mUI.scaleX);
    SetProperty(particle_resource, "transform_height", mUI.scaleY);
    SetProperty(particle_resource, "transform_rotation", mUI.rotation);
    SetProperty(particle_resource, "use_init_rect", mUI.initRect);
    SetProperty(particle_resource, "use_direction_sector", mUI.dirSector);
    SetProperty(particle_resource, "use_size_derivatives", mUI.sizeDerivatives);
    SetProperty(particle_resource, "use_alpha_derivatives", mUI.alphaDerivatives);
    SetProperty(particle_resource, "use_lifetime", mUI.canExpire);
    SetUserProperty(particle_resource, "grid", mUI.cmbGrid);
    SetUserProperty(particle_resource, "zoom", mUI.zoom);
    SetUserProperty(particle_resource, "show_grid", mUI.chkShowGrid);
    SetUserProperty(particle_resource, "show_bounds", mUI.chkShowBounds);
    mWorkspace->SaveResource(particle_resource);
    mOriginalHash = mClass.GetHash();

    INFO("Saved particle system '%1'", name);
    NOTE("Saved particle system '%1'", name);
    setWindowTitle(name);
}

void ParticleEditorWidget::FillParams(gfx::KinematicsParticleEngineClass::Params& params) const
{
    params.motion         = GetValue(mUI.motion);
    params.mode           = GetValue(mUI.when);
    params.boundary       = GetValue(mUI.boundary);
    params.num_particles  = GetValue(mUI.numParticles);
    params.max_xpos       = GetValue(mUI.simWidth);
    params.max_ypos       = GetValue(mUI.simHeight);
    params.gravity.x      = GetValue(mUI.gravityX);
    params.gravity.y      = GetValue(mUI.gravityY);
    params.min_point_size = GetValue(mUI.minPointsize);
    params.max_point_size = GetValue(mUI.maxPointsize);
    params.min_velocity   = GetValue(mUI.minVelocity);
    params.max_velocity   = GetValue(mUI.maxVelocity);
    params.min_alpha      = GetValue(mUI.minAlpha);
    params.max_alpha      = GetValue(mUI.maxAlpha);

    if (mUI.initRect->isChecked())
    {
        params.init_rect_xpos   = GetValue(mUI.initX);
        params.init_rect_ypos   = GetValue(mUI.initY);
        params.init_rect_width  = GetValue(mUI.initWidth);
        params.init_rect_height = GetValue(mUI.initHeight);
    }
    else
    {
        params.init_rect_xpos = 0.0f;
        params.init_rect_ypos = 0.0f;
        params.init_rect_width = params.max_xpos;
        params.init_rect_height = params.max_ypos;
    }
    if (mUI.dirSector->isChecked())
    {
        params.direction_sector_start_angle = qDegreesToRadians(mUI.dirStartAngle->value());
        params.direction_sector_size        = qDegreesToRadians(mUI.dirSizeAngle->value());
    }
    else
    {
        params.direction_sector_start_angle = 0.0f;
        params.direction_sector_size        = qDegreesToRadians(360.0f);
    }
    if (mUI.canExpire->isChecked())
    {
        params.min_lifetime   = GetValue(mUI.minLifetime);
        params.max_lifetime   = GetValue(mUI.maxLifetime);
    }
    else
    {
        params.min_lifetime   = std::numeric_limits<float>::max();
        params.max_lifetime   = std::numeric_limits<float>::max();
    }

    if (mUI.sizeDerivatives->isChecked())
    {
        params.rate_of_change_in_size_wrt_time = GetValue(mUI.timeSizeDerivative);
        params.rate_of_change_in_size_wrt_dist = GetValue(mUI.distSizeDerivative);
    }
    else
    {
        params.rate_of_change_in_size_wrt_time = 0.0f;
        params.rate_of_change_in_size_wrt_dist = 0.0f;
    }

    if (mUI.alphaDerivatives->isChecked())
    {
        params.rate_of_change_in_alpha_wrt_time = GetValue(mUI.timeAlphaDerivative);
        params.rate_of_change_in_alpha_wrt_dist = GetValue(mUI.distAlphaDerivative);
    }
    else
    {
        params.rate_of_change_in_alpha_wrt_time = 0.0f;
        params.rate_of_change_in_alpha_wrt_dist = 0.0f;
    }
}

void ParticleEditorWidget::on_actionPlay_triggered()
{
    if (mPaused)
    {
        mPaused = false;
        mUI.actionPause->setEnabled(true);
        return;
    }

    gfx::KinematicsParticleEngineClass::Params p;
    FillParams(p);
    mEngine.reset(new gfx::KinematicsParticleEngine(p));

    mUI.actionPause->setEnabled(true);
    mUI.actionStop->setEnabled(true);
    mTime   = 0.0f;
    mPaused = false;

    DEBUG("Created new particle engine");
}

void ParticleEditorWidget::on_actionStop_triggered()
{
    mEngine.reset();
    mUI.actionStop->setEnabled(false);
    mUI.actionPause->setEnabled(false);
    mUI.actionPlay->setEnabled(true);
    mPaused = false;
}

void ParticleEditorWidget::on_resetTransform_clicked()
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    mUI.scaleX->setValue(width);
    mUI.scaleY->setValue(height);
    mUI.rotation->setValue(0);
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

void ParticleEditorWidget::on_motion_currentIndexChanged(int)
{
    const gfx::KinematicsParticleEngineClass::Motion motion = GetValue(mUI.motion);
    if (motion == gfx::KinematicsParticleEngineClass::Motion::Projectile)
    {
        mUI.gravityY->setEnabled(true);
        mUI.gravityX->setEnabled(true);
    }
    else
    {
        mUI.gravityY->setEnabled(false);
        mUI.gravityX->setEnabled(false);
    }
}

void ParticleEditorWidget::PaintScene(gfx::Painter& painter, double secs)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto zoom   = (float)GetValue(mUI.zoom);
    painter.SetViewport(0, 0, width, height);
    painter.SetPixelRatio(glm::vec2(zoom, zoom));

    gfx::Transform view;
    view.Scale(zoom, zoom);
    view.Translate(width*0.5, height*0.5);

    // get the material based on the selection in the materials combobox
    const std::string& id = GetItemId(mUI.materials);
    if (!mMaterial || mMaterial->GetClassId() != id)
    {
        auto klass = mWorkspace->GetMaterialClassById(GetItemId(mUI.materials));
        mMaterial  = gfx::CreateMaterialInstance(klass);
    }

    if (GetValue(mUI.chkShowGrid))
    {
        const float xs = 1.0f;
        const float ys = 1.0f;
        const GridDensity grid = GetValue(mUI.cmbGrid);
        DrawCoordinateGrid(painter, view, grid, zoom, xs, ys, width, height);
    }

    const float simulation_width  = GetValue(mUI.scaleX);
    const float simulation_height = GetValue(mUI.scaleY);
    view.Push();
    view.Scale(simulation_width, simulation_height);
    view.Translate(-simulation_width*0.5, -simulation_height*0.5);
    view.Rotate(qDegreesToRadians((float)GetValue(mUI.rotation)));

    if (GetValue(mUI.chkShowBounds))
    {
        painter.Draw(gfx::Rectangle(gfx::Drawable::Style::Outline), view,
                     gfx::CreateMaterialFromColor(gfx::Color::Green));
    }

    if (mEngine)
    {
        painter.Draw(*mEngine, view, *mMaterial);
        ShowMessage(base::FormatString("Particles %1", mEngine->GetNumParticlesAlive()),
                    painter, width, height);
    }
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
            SetValue(mUI.materials, QString("White"));
        }
    }
}

} // namespace
