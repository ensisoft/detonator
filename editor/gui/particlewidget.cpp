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
#include "warnpop.h"

#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/transform.h"
#include "graphics/drawing.h"
#include "graphics/types.h"

#include "app/eventlog.h"
#include "app/utility.h"
#include "particlewidget.h"
#include "settings.h"

namespace gui
{

ParticleEditorWidget::ParticleEditorWidget()
{
    DEBUG("Create ParticleEditorWidget");

    mUI.setupUi(this);
    mUI.widget->setFramerate(60);
    mUI.widget->onPaintScene = std::bind(&ParticleEditorWidget::paintScene, 
        this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onInitScene = [&](unsigned w, unsigned h) {
        mUI.scaleX->setValue(w);
        mUI.scaleY->setValue(h);
    };
    const auto w = mUI.widget->width();
    const auto h = mUI.widget->height();
    mUI.scaleX->setValue(w);
    mUI.scaleY->setValue(h);
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(false);
}

ParticleEditorWidget::~ParticleEditorWidget()
{
    DEBUG("Destroy ParticleEdtiorWidget");

    mUI.widget->dispose();
}

void ParticleEditorWidget::addActions(QToolBar& bar)
{
    bar.addAction(mUI.actionPlay);
    bar.addAction(mUI.actionPause);
    bar.addSeparator();
    bar.addAction(mUI.actionStop);
}

void ParticleEditorWidget::addActions(QMenu& menu)
{
    menu.addAction(mUI.actionPlay);
    menu.addAction(mUI.actionPause);
    menu.addSeparator();
    menu.addAction(mUI.actionStop);
}

bool ParticleEditorWidget::saveState(Settings& settings) const
{
    // todo:
    return true;
}

bool ParticleEditorWidget::loadState(const Settings& settings)
{
    // todo:
    return true;
}

void ParticleEditorWidget::on_actionPause_triggered()
{
    mPaused = true;
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
}

void ParticleEditorWidget::on_actionPlay_triggered()
{
    if (mPaused)
    {
        mPaused = false;
        mUI.actionPause->setEnabled(true);
        return;
    }

    const auto& motion = mUI.motion->currentText();
    const auto& boundary = mUI.boundary->currentText();
    const auto& when = mUI.when->currentText();

    gfx::KinematicsParticleEngine::Params p;
    p.num_particles  = mUI.numParticles->value();
    p.max_xpos       = mUI.simWidth->value();
    p.max_ypos       = mUI.simHeight->value();
    p.num_particles  = mUI.numParticles->value();
    p.min_lifetime   = mUI.minLifetime->value();
    p.max_lifetime   = mUI.maxLifetime->value();
    p.min_point_size = mUI.minPointsize->value();
    p.max_point_size = mUI.maxPointsize->value();
    p.min_velocity   = mUI.minVelocity->value();
    p.max_velocity   = mUI.maxVelocity->value();

    if (mUI.initRect->isChecked())
    {
        p.init_rect_xpos = mUI.initX->value();
        p.init_rect_ypos = mUI.initY->value();
        p.init_rect_width = mUI.initWidth->value();
        p.init_rect_height = mUI.initHeight->value();
    }
    else
    {
        p.init_rect_xpos = 0.0f;
        p.init_rect_ypos = 0.0f;
        p.init_rect_width = p.max_xpos;
        p.init_rect_height = p.max_ypos;
    }
    if (mUI.dirSector->isChecked())
    {
        p.direction_sector_start_angle = qDegreesToRadians(mUI.dirStartAngle->value());
        p.direction_sector_size        = qDegreesToRadians(mUI.dirSizeAngle->value());
    }
    else
    {   
        p.direction_sector_start_angle = 0.0f;
        p.direction_sector_size        = qDegreesToRadians(360.0f);        
    }


    if (when == "Once")
        p.mode = gfx::KinematicsParticleEngine::SpawnPolicy::Once;
    else if (when == "Maintain")
        p.mode = gfx::KinematicsParticleEngine::SpawnPolicy::Maintain;
    else if (when == "Continuous")
        p.mode = gfx::KinematicsParticleEngine::SpawnPolicy::Continuous;
    
    mEngine.reset(new gfx::KinematicsParticleEngine(p));
    if (motion == "Linear")
        mEngine->SetMotion(gfx::DefaultKinematicsParticleUpdatePolicy::Motion::Linear);
    if (boundary == "Clamp")
        mEngine->SetBoundaryPolicy(gfx::DefaultKinematicsParticleUpdatePolicy::BoundaryPolicy::Clamp);
    else if (boundary == "Wrap")
        mEngine->SetBoundaryPolicy(gfx::DefaultKinematicsParticleUpdatePolicy::BoundaryPolicy::Wrap);
    else if (boundary == "Kill")
        mEngine->SetBoundaryPolicy(gfx::DefaultKinematicsParticleUpdatePolicy::BoundaryPolicy::Kill);

    if (mUI.growth->isChecked())
    {
        const auto dSdT = mUI.timeDerivative->value();
        const auto dSdD = mUI.distDerivative->value();
        mEngine->SetGrowthWithRespectToTime(dSdT);
        mEngine->SetGrowthWithRespectToDistance(dSdD);
    }

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

void ParticleEditorWidget::on_browseTexture_clicked()
{
    const auto& list = QFileDialog::getOpenFileNames(this, 
        tr("Select Image File"), "", tr("Images (*.png *.jpeg *.jpg)"));
    if (list.isEmpty())
        return;
    if (list.count() > 1)
        mUI.filename->setText("Multiple files");
    else mUI.filename->setText(list[0]);
    mTextures = list;
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

    mUI.curTime->setText(QString("%1s").arg(mTime));
    mUI.curNumParticles->setText(QString::number(mEngine->GetNumParticlesAlive()));

    const auto& type    = mUI.surfaceType->currentText();
    const auto& texture = mUI.filename->text(); 
    const auto& color = mUI.color->color();
    const float a  = color.alphaF();
    const float r  = color.redF();
    const float g  = color.greenF();
    const float b  = color.blueF();      
    
    gfx::Material::SurfaceType surface;
    if (type == "Opaque")
        surface = gfx::Material::SurfaceType::Opaque;
    else if (type == "Transparent")
        surface = gfx::Material::SurfaceType::Transparent;
    else if (type == "Emissive")
        surface = gfx::Material::SurfaceType::Emissive;

    if (mTextures.count() == 0)
    {
        painter.Draw(*mEngine, tr,  
            gfx::SolidColor(gfx::Color4f(r, g, b, a)).SetSurfaceType(surface));
            
    }
    else if (mTextures.count() == 1)
    {
        painter.Draw(*mEngine, tr, 
            gfx::TextureMap(app::strToEngine(texture))
            .SetBaseColor(gfx::Color4f(r, g, b, a))
            .SetSurfaceType(surface));
    }
    else
    {
        gfx::SpriteSet sprite;
        for (const auto& s : mTextures) 
        {
            sprite.AddTexture(app::strToEngine(s));
        }
        sprite.SetFps(10)
            .SetAppRuntime(mTime)
            .SetBaseColor(gfx::Color4f(r, g, b, a))
            .SetSurfaceType(surface);
        painter.Draw(*mEngine, tr, sprite);
    }
}

} // namespace
