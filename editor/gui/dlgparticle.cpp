// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#include "data/json.h"

#include "graphics/drawable.h"
#include "graphics/material.h"
#include "graphics/material_class.h"
#include "graphics/material_instance.h"
#include "editor/app/eventlog.h"
#include "editor/app/workspace.h"
#include "editor/gui/dlgparticle.h"
#include "editor/gui/utility.h"
#include "editor/gui/drawing.h"
#include "editor/gui/translation.h"

namespace gui
{

DlgParticle::DlgParticle(QWidget* parent, const app::Workspace* workspace)
  : QDialog(parent)
  , mWorkspace(workspace)
{
    mUI.setupUi(this);

    setMouseTracking(true);

    // do the graphics dispose in finished handler which is triggered
    // regardless whether we do accept/reject or the user clicks the X
    // or presses Esc.
    connect(this, &QDialog::finished, mUI.widget, &GfxWidget::Dispose);

    mUI.widget->onPaintScene = std::bind(&DlgParticle::PaintScene, this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onInitScene = [&](unsigned, unsigned) {
        mUI.widget->StartPaintTimer();
    };
    mUI.widget->onKeyPress = std::bind(&DlgParticle::KeyPress, this, std::placeholders::_1);
    mUI.widget->onMousePress = std::bind(&DlgParticle::MousePress, this, std::placeholders::_1);
    mUI.widget->onMouseWheel = std::bind(&DlgParticle::MouseWheel, this, std::placeholders::_1);
    mUI.widget->onMouseDoubleClick = std::bind(&DlgParticle::MouseDoubleClick, this, std::placeholders::_1);

    PopulatePresetParticleList(mUI.cmbParticle);

    PopulateFromEnum<gfx::ParticleEngineClass::CoordinateSpace>(mUI.cmbSpace);
    PopulateFromEnum<gfx::ParticleEngineClass::Motion>(mUI.cmbMotion);
    PopulateFromEnum<gfx::ParticleEngineClass::SpawnPolicy>(mUI.cmbEmission);

    on_cmbParticle_currentIndexChanged(0);
}
DlgParticle::~DlgParticle() = default;

void DlgParticle::on_btnAccept_clicked()
{
    accept();
}
void DlgParticle::on_btnCancel_clicked()
{
    reject();
}

void DlgParticle::on_cmbParticle_currentIndexChanged(int)
{
    const auto& file = mWorkspace->MapFileToFilesystem(GetItemId(mUI.cmbParticle));

    data::JsonFile json;
    const auto [success, error] = json.Load(file);
    if (!success)
        return;
    const auto& root = json.GetRootObject();
    const auto& particle_chunk = root.GetReadChunk("particle");
    if (!particle_chunk)
        return;

    auto klass = std::make_shared<gfx::ParticleEngineClass>();
    if (!klass->FromJson(*particle_chunk))
        return;

    if (root.HasValue("material-id"))
    {
        std::string materialId;
        root.Read("material-id", &materialId);
        if (!mWorkspace->IsValidMaterial(materialId))
            return;

        mMaterialClass = mWorkspace->GetMaterialClassById(materialId);
    }
    else
    {
        const auto& material = root.GetReadChunk("material");
        if (!material)
            return;
        auto material_class = gfx::MaterialClass::ClassFromJson(*material);
        if (!material_class)
            return;
        mMaterialClass = std::move(material_class);
    }
    mParticleClass = std::move(klass);
    mMaterial.reset();
    mEngine.reset();

    root.Read("viz_width", &mVizWidth);
    root.Read("viz_height", &mVizHeight);

    const auto& params = mParticleClass->GetParams();

    SetValue(mUI.cmbSpace, params.coordinate_space);
    SetValue(mUI.cmbMotion, params.motion);
    SetValue(mUI.cmbEmission, params.mode);
    DEBUG("Loaded preset particle system. [file='%1']", file);
}

void DlgParticle::PaintScene(gfx::Painter& painter, double dt)
{
    if (!mMaterial)
    {
        if (!mMaterialClass)
        {
            ShowError("The material class failed to load.", Point2Df(20.0f, 20.0f), painter);
            return;
        }
        mMaterial = gfx::CreateMaterialInstance(mMaterialClass);
    }
    if (!mEngine)
    {
        if (!mParticleClass)
        {
            ShowError("The particle class failed to load.", Point2Df(20.0f, 20.0f), painter);
            return;
        }
        mEngine = std::make_unique<gfx::ParticleEngineInstance>(mParticleClass);
    }

    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    const auto viz_width  = mVizWidth;
    const auto viz_height = mVizHeight;

    gfx::Transform transform;
    transform.Resize(viz_width, viz_height);
    transform.Translate(width*0.5, height*0.5);
    transform.Translate(-viz_width * 0.5, -viz_height * 0.5);

    const auto model_to_world = transform.GetAsMatrix();
    const auto world_matrix = glm::mat4(1.0f);

    gfx::ParticleEngineInstance::Environment env;
    env.editing_mode = false;
    env.pixel_ratio  = glm::vec2{1.0, 1.0};
    env.model_matrix = &model_to_world;
    env.world_matrix = &world_matrix; // todo: would need this for dimetric projection
    if (!mEngine->IsAlive())
        mEngine->Restart(env);

    mEngine->Update(env, dt);

    mMaterial->Update(dt);

    painter.Draw(*mEngine, transform, *mMaterial);

    if (mEngine->GetParams().mode == gfx::ParticleEngineClass::SpawnPolicy::Command)
    {
        if (mEngine->GetNumParticlesAlive() == 0)
            ShowMessage("Click to emit particles!", painter);
    }

    if (mEmitOnce)
    {
        gfx::Drawable::Command cmd;
        cmd.name = "EmitParticles";
        // no count here.
        mEngine->Execute(env, cmd);
    }
    mEmitOnce = false;
}

void DlgParticle::MousePress(QMouseEvent* mickey)
{
    if (!mEngine)
        return;

    // using mouse press to emit particles avoids the
    // issue that the keyboard focus is not on the gfx widget..
    if (mEngine->GetParams().mode == gfx::ParticleEngineClass::SpawnPolicy::Command)
    {
        mEmitOnce = true;
    }
}
void DlgParticle::MouseDoubleClick(QMouseEvent* mickey)
{

}
void DlgParticle::MouseWheel(QWheelEvent* wheel)
{

}
bool DlgParticle::KeyPress(QKeyEvent* key)
{
    if (!mEngine)
        return false;

    if (mEngine->GetParams().mode == gfx::ParticleEngineClass::SpawnPolicy::Command)
    {
        mEmitOnce = true;
    }
    return true;
}

} //namespace