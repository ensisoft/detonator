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

#define LOGTAG "particle"

#include "config.h"

#include "warnpush.h"
#  include <QtMath>
#  include <QFileDialog>
#  include <QMessageBox>
#  include <QTextStream>
#include "warnpop.h"

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
#include "particlewidget.h"
#include "utility.h"

namespace gui
{

ParticleEditorWidget::ParticleEditorWidget(app::Workspace* workspace)
{
    mWorkspace = workspace;

    DEBUG("Create ParticleEditorWidget");
    mUI.setupUi(this);
    mUI.widget->setFramerate(60);
    mUI.widget->onPaintScene = std::bind(&ParticleEditorWidget::paintScene,
        this, std::placeholders::_1, std::placeholders::_2);

    // get the current list of materials from the workspace
    mUI.materials->addItems(workspace->ListMaterials());

    // Set default transform state here. if there's a previous user setting
    // they'll get loaded in loadState and will override these values.
    mUI.transform->setChecked(false);
    mUI.scaleX->setValue(500);
    mUI.scaleY->setValue(500);
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(false);
    mUI.name->setText("My Particle System");
    setWindowTitle("My Particle System");

    PopulateFromEnum<gfx::KinematicsParticleEngine::Motion>(mUI.motion);
    PopulateFromEnum<gfx::KinematicsParticleEngine::BoundaryPolicy>(mUI.boundary);
    PopulateFromEnum<gfx::KinematicsParticleEngine::SpawnPolicy>(mUI.when);

    // connect workspace signals for resource management
    connect(mWorkspace, &app::Workspace::NewResourceAvailable,
            this,       &ParticleEditorWidget::newResourceAvailable);
    connect(mWorkspace, &app::Workspace::ResourceToBeDeleted,
            this,       &ParticleEditorWidget::resourceToBeDeleted);
}

ParticleEditorWidget::ParticleEditorWidget(app::Workspace* workspace, const app::Resource& resource) : ParticleEditorWidget(workspace)
{
    const auto& name = resource.GetName();
    DEBUG("Editing particle system: '%1'", name);

    mUI.name->setText(name);

    GetProperty(resource, "material", mUI.materials);
    GetProperty(resource, "transform_enabled", mUI.transform);
    GetProperty(resource, "transform_xpos", mUI.translateX);
    GetProperty(resource, "transform_ypos", mUI.translateY);
    GetProperty(resource, "transform_width", mUI.scaleX);
    GetProperty(resource, "transform_height", mUI.scaleY);
    GetProperty(resource, "transform_rotation", mUI.rotation);
    GetProperty(resource, "use_init_rect", mUI.initRect);
    GetProperty(resource, "use_direction_sector", mUI.dirSector);
    GetProperty(resource, "use_growth", mUI.growth);
    GetProperty(resource, "use_lifetime", mUI.canExpire);

    const auto* engine = resource.GetContent<gfx::KinematicsParticleEngine>();
    const auto& params = engine->GetParams();
    SetValue(mUI.motion, params.motion);
    SetValue(mUI.when,   params.mode);
    SetValue(mUI.boundary, params.boundary);
    SetValue(mUI.numParticles, params.num_particles);
    SetValue(mUI.simWidth, params.max_xpos);
    SetValue(mUI.simHeight, params.max_ypos);
    SetValue(mUI.minLifetime, params.min_lifetime);
    SetValue(mUI.maxLifetime, params.max_lifetime);
    SetValue(mUI.minPointsize, params.min_point_size);
    SetValue(mUI.maxPointsize, params.max_point_size);
    SetValue(mUI.minVelocity, params.min_velocity);
    SetValue(mUI.maxVelocity, params.max_velocity);
    SetValue(mUI.initX, params.init_rect_xpos);
    SetValue(mUI.initY, params.init_rect_ypos);
    SetValue(mUI.initWidth, params.init_rect_width);
    SetValue(mUI.initHeight, params.init_rect_height);
    SetValue(mUI.dirStartAngle, qRadiansToDegrees(params.direction_sector_start_angle));
    SetValue(mUI.dirSizeAngle, qRadiansToDegrees(params.direction_sector_size));
    SetValue(mUI.timeDerivative, params.rate_of_change_in_size_wrt_time);
    SetValue(mUI.distDerivative, params.rate_of_change_in_size_wrt_dist);

    setWindowTitle(name);
}

ParticleEditorWidget::~ParticleEditorWidget()
{
    DEBUG("Destroy ParticleEdtiorWidget");
}

void ParticleEditorWidget::addActions(QToolBar& bar)
{
    bar.addAction(mUI.actionPlay);
    bar.addAction(mUI.actionPause);
    bar.addSeparator();
    bar.addAction(mUI.actionStop);
    bar.addSeparator();
    bar.addAction(mUI.actionSave);
}

void ParticleEditorWidget::addActions(QMenu& menu)
{
    menu.addAction(mUI.actionPlay);
    menu.addAction(mUI.actionPause);
    menu.addSeparator();
    menu.addAction(mUI.actionStop);
    menu.addSeparator();
    menu.addAction(mUI.actionSave);
}

bool ParticleEditorWidget::saveState(Settings& settings) const
{
    settings.saveWidget("Particle", mUI.name);
    settings.saveWidget("Particle", mUI.materials);
    settings.saveWidget("Particle", mUI.transform);
    settings.saveWidget("Particle", mUI.translateX);
    settings.saveWidget("Particle", mUI.translateY);
    settings.saveWidget("Particle", mUI.scaleX);
    settings.saveWidget("Particle", mUI.scaleY);
    settings.saveWidget("Particle", mUI.rotation);
    settings.saveWidget("Particle", mUI.simWidth);
    settings.saveWidget("Particle", mUI.simHeight);
    settings.saveWidget("Particle", mUI.motion);
    settings.saveWidget("Particle", mUI.boundary);
    settings.saveWidget("Particle", mUI.when);
    settings.saveWidget("Particle", mUI.numParticles);
    settings.saveWidget("Particle", mUI.initRect);
    settings.saveWidget("Particle", mUI.initX);
    settings.saveWidget("Particle", mUI.initY);
    settings.saveWidget("Particle", mUI.initWidth);
    settings.saveWidget("Particle", mUI.initHeight);
    settings.saveWidget("Particle", mUI.dirSector);
    settings.saveWidget("Particle", mUI.dirStartAngle);
    settings.saveWidget("Particle", mUI.dirSizeAngle);
    settings.saveWidget("Particle", mUI.minVelocity);
    settings.saveWidget("Particle", mUI.maxVelocity);
    settings.saveWidget("Particle", mUI.minLifetime);
    settings.saveWidget("Particle", mUI.maxLifetime);
    settings.saveWidget("Particle", mUI.minPointsize);
    settings.saveWidget("Particle", mUI.maxPointsize);
    settings.saveWidget("Particle", mUI.canExpire);
    settings.saveWidget("Particle", mUI.growth);
    settings.saveWidget("Particle", mUI.timeDerivative);
    settings.saveWidget("Particle", mUI.distDerivative);
    return true;
}

bool ParticleEditorWidget::loadState(const Settings& settings)
{
    settings.loadWidget("Particle", mUI.name);
    settings.loadWidget("Particle", mUI.materials);
    settings.loadWidget("Particle", mUI.transform);
    settings.loadWidget("Particle", mUI.translateX);
    settings.loadWidget("Particle", mUI.translateY);
    settings.loadWidget("Particle", mUI.scaleX);
    settings.loadWidget("Particle", mUI.scaleY);
    settings.loadWidget("Particle", mUI.rotation);
    settings.loadWidget("Particle", mUI.simWidth);
    settings.loadWidget("Particle", mUI.simHeight);
    settings.loadWidget("Particle", mUI.motion);
    settings.loadWidget("Particle", mUI.boundary);
    settings.loadWidget("Particle", mUI.when);
    settings.loadWidget("Particle", mUI.numParticles);
    settings.loadWidget("Particle", mUI.initRect);
    settings.loadWidget("Particle", mUI.initX);
    settings.loadWidget("Particle", mUI.initY);
    settings.loadWidget("Particle", mUI.initWidth);
    settings.loadWidget("Particle", mUI.initHeight);
    settings.loadWidget("Particle", mUI.dirSector);
    settings.loadWidget("Particle", mUI.dirStartAngle);
    settings.loadWidget("Particle", mUI.dirSizeAngle);
    settings.loadWidget("Particle", mUI.minVelocity);
    settings.loadWidget("Particle", mUI.maxVelocity);
    settings.loadWidget("Particle", mUI.minLifetime);
    settings.loadWidget("Particle", mUI.maxLifetime);
    settings.loadWidget("Particle", mUI.minPointsize);
    settings.loadWidget("Particle", mUI.maxPointsize);
    settings.loadWidget("Particle", mUI.canExpire);
    settings.loadWidget("Particle", mUI.growth);
    settings.loadWidget("Particle", mUI.timeDerivative);
    settings.loadWidget("Particle", mUI.distDerivative);
    return true;
}

void ParticleEditorWidget::reloadShaders()
{
    mUI.widget->reloadShaders();
}

void ParticleEditorWidget::reloadTextures()
{
    mUI.widget->reloadTextures();
}

void ParticleEditorWidget::shutdown()
{
    mUI.widget->dispose();
}

void ParticleEditorWidget::on_actionPause_triggered()
{
    mPaused = true;
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
}

void ParticleEditorWidget::on_actionSave_triggered()
{
    const auto& name = mUI.name->text();
    if (name.isEmpty())
    {
        mUI.name->setFocus();
        return;
    }

    if (mWorkspace->HasParticleSystem(name))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Question);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setText("Workspace already contains particle system by this name. Overwrite?");
        if (msg.exec() == QMessageBox::No)
            return;
    }

    gfx::KinematicsParticleEngine::Params params;
    fillParams(params);
    gfx::KinematicsParticleEngine engine(params);
    app::ParticleSystemResource particle_resource(std::move(engine), name);

    SetProperty(particle_resource, "material", mUI.materials);
    SetProperty(particle_resource, "transform_enabled", mUI.transform);
    SetProperty(particle_resource, "transform_xpos", mUI.translateX);
    SetProperty(particle_resource, "transform_ypos", mUI.translateY);
    SetProperty(particle_resource, "transform_width", mUI.scaleX);
    SetProperty(particle_resource, "transform_height", mUI.scaleY);
    SetProperty(particle_resource, "transform_rotation", mUI.rotation);
    SetProperty(particle_resource, "use_init_rect", mUI.initRect);
    SetProperty(particle_resource, "use_direction_sector", mUI.dirSector);
    SetProperty(particle_resource, "use_growth", mUI.growth);
    SetProperty(particle_resource, "use_lifetime", mUI.canExpire);
    mWorkspace->SaveResource(particle_resource);
    INFO("Saved particle system '%1'", name);
    NOTE("Saved particle system '%1'", name);

    setWindowTitle(name);
}

void ParticleEditorWidget::fillParams(gfx::KinematicsParticleEngine::Params& params) const
{
    params.motion         = GetValue(mUI.motion);
    params.mode           = GetValue(mUI.when);
    params.boundary       = GetValue(mUI.boundary);
    params.num_particles  = GetValue(mUI.numParticles);
    params.max_xpos       = GetValue(mUI.simWidth);
    params.max_ypos       = GetValue(mUI.simHeight);
    //params.min_lifetime   = GetValue(mUI.minLifetime);
    //params.max_lifetime   = GetValue(mUI.maxLifetime);
    params.min_point_size = GetValue(mUI.minPointsize);
    params.max_point_size = GetValue(mUI.maxPointsize);
    params.min_velocity   = GetValue(mUI.minVelocity);
    params.max_velocity   = GetValue(mUI.maxVelocity);

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

    if (mUI.growth->isChecked())
    {
        params.rate_of_change_in_size_wrt_time = GetValue(mUI.timeDerivative);
        params.rate_of_change_in_size_wrt_dist = GetValue(mUI.distDerivative);
    }
    else
    {
        params.rate_of_change_in_size_wrt_time = 0.0f;
        params.rate_of_change_in_size_wrt_dist = 0.0f;
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

    gfx::KinematicsParticleEngine::Params p;
    fillParams(p);
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
    mUI.translateX->setValue(0);
    mUI.translateY->setValue(0);
    mUI.scaleX->setValue(width);
    mUI.scaleY->setValue(height);
    mUI.rotation->setValue(0);
}

void ParticleEditorWidget::on_plus90_clicked()
{
    const auto value = mUI.rotation->value();
    mUI.rotation->setValue(value + 90.0f);
}
void ParticleEditorWidget::on_minus90_clicked()
{
    const auto value = mUI.rotation->value();
    mUI.rotation->setValue(value - 90.0f);
}

void ParticleEditorWidget::paintScene(gfx::Painter& painter, double secs)
{
    if (!mEngine)
        return;

    if (!mPaused)
    {
        mEngine->Update(secs);
        mTime += secs;
    }
    if (!mEngine->IsAlive())
    {
        DEBUG("Particle simulation finished");
        mUI.actionStop->setEnabled(false);
        mUI.actionPause->setEnabled(false);
        mUI.actionPlay->setEnabled(true);
        mUI.curNumParticles->setText("0");
        mEngine.reset();
        return;
    }

    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    gfx::Transform tr;
    if (mUI.transform->isChecked())
    {
        const float angle = qDegreesToRadians(mUI.rotation->value());
        const auto tx = mUI.translateX->value();
        const auto ty = mUI.translateY->value();
        const auto sx = mUI.scaleX->value();
        const auto sy = mUI.scaleY->value();
        tr.Resize(sx, sy);
        tr.Translate(-sx * 0.5, -sy * 0.5);
        tr.Rotate(angle);
        tr.Translate(sx * 0.5 + tx, sy * 0.5 + ty);
        gfx::DrawRectOutline(painter, gfx::FRect(tx, ty, sx, sy),
            gfx::Color::DarkGreen, 3, angle);
    }
    else
    {
        tr.Resize(width, height);
        tr.MoveTo(0, 0);
    }

    // get the material based on the selection in the materials combobox
    const auto& material_name = mUI.materials->currentText();
    if (mMaterialName != material_name)
    {
        mMaterial = mWorkspace->MakeMaterial(material_name);
        mMaterialName = material_name;
    }

    mMaterial->SetRuntime(mTime);

    // render the particle engine
    painter.Draw(*mEngine, tr, *mMaterial);

    mUI.curTime->setText(QString("%1s").arg(mTime));
    mUI.curNumParticles->setText(QString::number(mEngine->GetNumParticlesAlive()));
}

void ParticleEditorWidget::newResourceAvailable(const app::Resource* resource)
{
    // this is simple, just add new resources in the appropriate UI objects.
    if (resource->GetType() == app::Resource::Type::Material)
    {
        mUI.materials->addItem(resource->GetName());
    }
}

void ParticleEditorWidget::resourceToBeDeleted(const app::Resource* resource)
{
    if (resource->GetType() == app::Resource::Type::Material)
    {
        const auto& current_material_name = mUI.materials->currentText();
        const auto carcass_index = mUI.materials->findText(resource->GetName());
        const auto current_index = mUI.materials->findText(current_material_name);
        mUI.materials->blockSignals(true);
        mUI.materials->removeItem(carcass_index);
        if (current_index == carcass_index)
        {
            const auto checkerboard = mUI.materials->findText("Checkerboard");
            mUI.materials->setCurrentIndex(checkerboard);
            WARN("Particle system '%1' uses material '%2' that is deleted.",
                mUI.name->text(), current_material_name);
        }
        mUI.materials->blockSignals(false);
    }
}

} // namespace
