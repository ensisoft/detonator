// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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
#  include <QFileDialog>
#  include <QMessageBox>
#include "warnpop.h"

#include "editor/gui/dlgmodelimport.h"
#include "editor/gui/drawing.h"
#include "editor/app/workspace.h"
#include "graphics/shaderpass.h"
#include "graphics/painter.h"
#include "graphics/utility.h"

namespace gui
{

DlgModelImport::DlgModelImport(QWidget* parent, app::Workspace* workspace)
  : QDialog(parent)
  , mWorkspace(workspace)
{
    mUI.setupUi(this);
    mUI.drawables->setModel(mImporter.GetDrawableModel());
    mUI.materials->setModel(mImporter.GetMaterialModel());

    setMouseTracking(true);

    // do the graphics dispose in finished handler which is triggered
    // regardless whether we do accept/reject or the user clicks the X
    // or presses Esc.
    connect(this, &QDialog::finished, mUI.widget, &GfxWidget::dispose);
    // render on timer
    connect(&mTimer, &QTimer::timeout, mUI.widget, &GfxWidget::triggerPaint);

    mUI.widget->onInitScene = [&](unsigned, unsigned) {
        mTimer.setInterval(1000.0/60.0);
        mTimer.start();
    };
    mUI.widget->onZoomOut = [this]() {
        float zoom = GetValue(mUI.zoom);
        zoom = math::clamp(0.1f, 5.0f, zoom-0.1f);
        SetValue(mUI.zoom, zoom);
    };

    mUI.widget->onZoomIn = [this]() {
        float zoom = GetValue(mUI.zoom);
        zoom = math::clamp(0.1f, 5.0f, zoom+0.1f);
        SetValue(mUI.zoom, zoom);
    };

    mUI.widget->onPaintScene       = std::bind(&DlgModelImport::PaintScene, this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onKeyPress         = std::bind(&DlgModelImport::KeyPress, this, std::placeholders::_1);
    mUI.widget->onMousePress       = std::bind(&DlgModelImport::MousePress, this, std::placeholders::_1);
    mUI.widget->onMouseWheel       = std::bind(&DlgModelImport::MouseWheel, this, std::placeholders::_1);
    mUI.widget->onMouseDoubleClick = std::bind(&DlgModelImport::MouseDoubleClick, this, std::placeholders::_1);

    PopulateFromEnum<GridDensity>(mUI.cmbGrid);
    SetValue(mUI.cmbGrid, GridDensity::Grid50x50);
    SetValue(mUI.zoom, 1.0f);
}

void DlgModelImport::LoadGeometry()
{
    QByteArray geometry;
    if (GetUserProperty(*mWorkspace, "dlg-model-import-geometry", &geometry))
        restoreGeometry(geometry);
}
void DlgModelImport::LoadState()
{
    QString file;

    GetUserProperty(*mWorkspace, "dlg-model-import-color", mUI.widget);
    GetUserProperty(*mWorkspace, "dlg-model-import-zoom", mUI.zoom);
    GetUserProperty(*mWorkspace, "dlg-model-import-grid", mUI.cmbGrid);
    GetUserProperty(*mWorkspace, "dlg-model-import-file", &file);

    if (file.isEmpty())
        return;

    LoadModel(file);
}

void DlgModelImport::SaveState()
{
    QString file = GetValue(mUI.modelFile);
    SetUserProperty(*mWorkspace, "dlg-model-import-geometry", saveGeometry());
    SetUserProperty(*mWorkspace, "dlg-model-import-file", file);
    SetUserProperty(*mWorkspace, "dlg-model-import-color", mUI.widget);
    SetUserProperty(*mWorkspace, "dlg-model-import-zoom", mUI.zoom);
    SetUserProperty(*mWorkspace, "dlg-model-import-grid", mUI.cmbGrid);
}

void DlgModelImport::on_btnFile_clicked()
{
    const QString previous = GetValue(mUI.modelFile);

    const auto& file = QFileDialog::getOpenFileName(this,
        tr("Select Model File"), previous,
        tr("3D Models (*.blend *.fbx *.obj)"));
    if (file.isEmpty())
        return;

    LoadModel(file);
}

void DlgModelImport::on_btnClose_clicked()
{
    SaveState();

    close();
}

void DlgModelImport::on_widgetColor_colorChanged(const QColor& color)
{
    mUI.widget->SetClearColor(color);
}

void DlgModelImport::PaintScene(gfx::Painter& painter, double secs)
{
    SetValue(mUI.widgetColor, mUI.widget->GetCurrentClearColor());

    const auto surf_width  = (float)mUI.widget->width();
    const auto surf_height = (float)mUI.widget->height();
    const auto surf_aspect = surf_width / surf_height;
    const auto grid_type   = (GridDensity)GetValue(mUI.cmbGrid);
    const auto grid_size   = (float)grid_type;

    painter.SetViewport(0, 0, surf_width, surf_height);

    const auto mesh = mImporter.GetMesh();
    if (!mesh)
    {
        ShowInstruction(
            "View the contents of a 3D model file.\n"
            "The contents can be viewed visually and textually.\n\n"
            "INSTRUCTIONS\n"
            "1. Select a model (.FBX, .OBJ) model file.\n"
            "2. Click the import tab to import the model.\n",
            gfx::FRect(0, 0, surf_width, surf_height), painter);
        return;
    }


    gfx::Painter p(painter);
    p.ResetViewMatrix();
    p.SetProjectionMatrix(gfx::MakePerspectiveProjection(gfx::FDegrees(45.0f), surf_aspect, 1.0f, 100.0f));
    p.ClearDepth(1.0f);

    gfx::detail::GenericShaderProgram program;

    gfx::Painter::DrawState state;
    state.depth_test   = gfx::Painter::DepthTest::LessOrEQual;
    state.stencil_func = gfx::Painter::StencilFunc::Disabled;
    state.write_color  = true;

    const auto t = base::GetTime();

    const auto& min = mImporter.GetMinVector();
    const auto& max = mImporter.GetMaxVector();

    const auto height = max.y - min.y;
    const auto width  = max.x - min.x;
    const auto depth  = max.z - min.z;
    ASSERT(width >= 0.0f && height >= 0.0f && depth >= 0.0f);

    const auto biggest_dimension = std::max( { height, width, depth } );
    // dim * scale = grid_size
    const auto scale = grid_size / biggest_dimension;

    const auto center = glm::vec3 {
            min.x + width*0.5f,
            min.y + height*0.5f,
            min.z + depth*0.5f
    };

    const float zoom = GetValue(mUI.zoom);

    gfx::Transform transform;
    transform.RotateAroundY(std::sin(t));
    transform.MoveTo(0.0f, -5.0f, -30.0f);

    transform.Push();
       transform.Scale(0.1f, 0.1f, 0.1f);
       transform.Scale(zoom, zoom, zoom);
       transform.Scale(scale, scale, scale);

    if (GetValue(mUI.chkShowGrid))
    {
        // 10x10 grid
        const auto grid_scale = grid_size * 10.0f;

        transform.Push();
            transform.Resize(grid_scale, 0.0f, grid_scale);
            transform.Push();
                transform.Translate(-0.5f, -0.5f);
                transform.RotateAroundX(gfx::FDegrees(90.0f));

        // 10x10 grid
        p.Draw(gfx::Grid(9, 9), transform,
               gfx::CreateMaterialFromColor(gfx::Color4f(gfx::Color::LightGray, 0.75f)),
               state, program);

        transform.Pop();
        transform.Pop();

    }

    // debug, reference cube
    if (false)
    {
        transform.Push();
            transform.Resize(grid_size, grid_size, grid_size);
            transform.Push();
                transform.Translate(0.0f, 0.5f, 0.0f);

        p.Draw(gfx::Cube(), transform, gfx::CreateMaterialFromColor(gfx::Color::DarkRed), state, program);

        transform.Pop();
        transform.Pop();
    }


    //draw the model
    if (true)
    {
        for (auto& draw : mDrawState)
        {
            p.Draw(*draw.drawable, transform, *draw.material, state, program);
            if (draw.material->HasError())
            {
                // we likely have an issues such as textures not being found.
                // replace the material with simpler material.
                auto klass = gfx::CreateMaterialClassFromColor(gfx::Color::DarkGray);
                draw.material = std::make_unique<gfx::MaterialInstance>(klass);
            }
        }
    }
}

void DlgModelImport::MousePress(QMouseEvent* mickey)
{

}
void DlgModelImport::MouseDoubleClick(QMouseEvent* mickey)
{

}
void DlgModelImport::MouseWheel(QWheelEvent* wheel)
{

}
bool DlgModelImport::KeyPress(QKeyEvent* key)
{
    return false;
}

void DlgModelImport::LoadModel(const QString& file)
{
    if (!mImporter.LoadModel(file))
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Critical);
        msg.setText("The selected model file could not be loaded.");
        msg.exec();
        return;
    }

    mDrawState.clear();

    const auto& mesh = mImporter.GetMesh();

    for (size_t i=0; i<mImporter.GetDrawableCount(); ++i)
    {
        const auto& drawable_info = mImporter.GetDrawable(i);
        const auto* material_info = mImporter.FindMaterial(drawable_info.material);
        ASSERT(material_info);

        auto drawable = std::make_unique<gfx::PolygonMeshInstance>(mesh, drawable_info.name);
        auto material = std::make_unique<gfx::MaterialInstance>(material_info->klass);

        DrawablePair pair;
        pair.material = std::move(material);
        pair.drawable = std::move(drawable);
        mDrawState.push_back(std::move(pair));
    }

    SetValue(mUI.modelFile, file);
}


} // namespace