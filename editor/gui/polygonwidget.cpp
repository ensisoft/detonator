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

#include "warnpush.h"
#  include <QMessageBox>
#  include <QVector2D>
#  include <base64/base64.h>
#include "warnpop.h"

#include "base/math.h"
#include "data/json.h"
#include "graphics/material.h"
#include "graphics/material_class.h"
#include "graphics/material_instance.h"
#include "graphics/drawable.h"
#include "graphics/drawing.h"
#include "graphics/transform.h"
#include "graphics/utility.h"
#include "graphics/simple_shape.h"
#include "graphics/polygon_mesh.h"
#include "graphics/guidegrid.h"
#include "editor/app/eventlog.h"
#include "editor/gui/polygonwidget.h"
#include "editor/gui/utility.h"
#include "editor/gui/settings.h"
#include "editor/gui/dlgtextedit.h"
#include "editor/gui/drawing.h"

namespace {

std::vector<gfx::Vertex2D> MakeVerts(const std::vector<QPoint>& points,
                                                     float width, float height)
{
    std::vector<gfx::Vertex2D> verts;
    for (const auto& p : points)
    {
        gfx::Vertex2D vertex;
        vertex.aPosition.x = p.x() / width;
        vertex.aPosition.y = p.y() / height * -1.0;
        vertex.aTexCoord.x = p.x() / width;
        vertex.aTexCoord.y = p.y() / height;
        verts.push_back(vertex);
    }
    return verts;
}

// Map vertex to widget space.
QPoint MapVertexToWidget(const gfx::Vertex2D& vertex, float width, float height)
{
    return QPoint(vertex.aPosition.x * width, vertex.aPosition.y * height * -1.0f);
}

float PointDist(const QPoint& a, const QPoint& b)
{
    const auto& diff = a - b;
    return std::sqrt(diff.x()*diff.x() + diff.y()*diff.y());
}

} // namespace

namespace gui
{

ShapeWidget::ShapeWidget(app::Workspace* workspace) : mWorkspace(workspace)
{
    DEBUG("Create PolygonWidget");

    mUI.setupUi(this);
    mUI.widget->onPaintScene = std::bind(&ShapeWidget::PaintScene,
        this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onMousePress  = std::bind(&ShapeWidget::OnMousePress,
        this, std::placeholders::_1);
    mUI.widget->onMouseRelease = std::bind(&ShapeWidget::OnMouseRelease,
        this, std::placeholders::_1);
    mUI.widget->onMouseMove = std::bind(&ShapeWidget::OnMouseMove,
        this, std::placeholders::_1);
    mUI.widget->onMouseDoubleClick = std::bind(&ShapeWidget::OnMouseDoubleClick,
        this, std::placeholders::_1);
    mUI.widget->onKeyPress = std::bind(&ShapeWidget::OnKeyPressEvent,
        this, std::placeholders::_1);

    auto* menu = new QMenu(this);
    menu->addAction(mUI.actionCustomizeShader);
    menu->addAction(mUI.actionShowShader);
    mUI.btnAddShader->setMenu(menu);

    mOriginalHash = mPolygon.GetHash();

    SetList(mUI.blueprints, workspace->ListUserDefinedMaterials());
    SetEnabled(mUI.actionPause, false);
    SetEnabled(mUI.actionStop, false);
    SetValue(mUI.name, QString("My Shape"));
    SetValue(mUI.ID, mPolygon.GetId());
    SetValue(mUI.staticInstance, mPolygon.IsStatic());
    SetValue(mUI.shaderFile, "Built-in Shader");
    setWindowTitle(GetValue(mUI.name));
    setFocusPolicy(Qt::StrongFocus);

    PopulateFromEnum<GridDensity>(mUI.cmbGrid);
    SetValue(mUI.cmbGrid, GridDensity::Grid20x20);
}

ShapeWidget::ShapeWidget(app::Workspace* workspace, const app::Resource& resource) : ShapeWidget(workspace)
{
    DEBUG("Editing shape '%1'", resource.GetName());

    mPolygon = *resource.GetContent<gfx::PolygonMeshClass>();
    mOriginalHash = mPolygon.GetHash();
    mBuilder.InitFrom(mPolygon);

    QString material;
    GetProperty(resource, "material", &material);
    GetUserProperty(resource, "alpha",        mUI.alpha);
    GetUserProperty(resource, "grid",         mUI.cmbGrid);
    GetUserProperty(resource, "snap_to_grid", mUI.chkSnap);
    GetUserProperty(resource, "show_grid",    mUI.chkShowGrid);
    GetUserProperty(resource, "widget",       mUI.widget);

    SetValue(mUI.name, resource.GetName());
    SetValue(mUI.ID, mPolygon.GetId());
    SetValue(mUI.staticInstance, mPolygon.IsStatic());
    SetValue(mUI.shaderFile, mPolygon.HasShaderSrc() ? "Customized Shader" : "Built-in Shader");
    SetEnabled(mUI.btnResetShader, mPolygon.HasShaderSrc());
    SetEnabled(mUI.actionClear, mPolygon.HasInlineData());
    SetValue(mUI.blueprints, ListItemId(material));

    if (!material.isEmpty())
    {
        if (mWorkspace->IsValidMaterial(material))
        {
            auto klass = mWorkspace->FindMaterialClassById(app::ToUtf8(material));
            mBlueprint = gfx::CreateMaterialInstance(klass);
        }
        else
        {
            WARN("Material '%1' is no longer available.", material);
            SetValue(mUI.blueprints, -1);
        }
    }
}

ShapeWidget::~ShapeWidget()
{
    DEBUG("Destroy PolygonWidget");
}

QString ShapeWidget::GetId() const
{
    return GetValue(mUI.ID);
}

void ShapeWidget::InitializeSettings(const UISettings& settings)
{
    SetValue(mUI.cmbGrid,     settings.grid);
    SetValue(mUI.chkSnap,     settings.snap_to_grid);
    SetValue(mUI.chkShowGrid, settings.show_grid);
}

void ShapeWidget::SetViewerMode()
{
    SetVisible(mUI.baseProperties, false);
    SetVisible(mUI.lblHelp,        false);
    SetVisible(mUI.blueprints,     false);
    SetVisible(mUI.alpha,          false);
    SetVisible(mUI.chkSnap,        false);
    SetVisible(mUI.chkSnap,        false);
    SetVisible(mUI.btnResetBlueprint, false);
}

void ShapeWidget::AddActions(QToolBar& bar)
{
    bar.addAction(mUI.actionPlay);
    bar.addAction(mUI.actionPause);
    bar.addSeparator();
    bar.addAction(mUI.actionStop);
    bar.addSeparator();
    bar.addAction(mUI.actionSave);
    bar.addSeparator();
    bar.addAction(mUI.actionNewTriangleFan);
    bar.addSeparator();
    bar.addAction(mUI.actionClear);
}
void ShapeWidget::AddActions(QMenu& menu)
{
    menu.addAction(mUI.actionPlay);
    menu.addAction(mUI.actionPause);
    menu.addSeparator();
    menu.addAction(mUI.actionStop);
    menu.addSeparator();
    menu.addAction(mUI.actionSave);
    menu.addSeparator();
    menu.addAction(mUI.actionNewTriangleFan);
    menu.addSeparator();
    menu.addAction(mUI.actionClear);
}

bool ShapeWidget::SaveState(Settings& settings) const
{
    data::JsonObject json;

    mBuilder.BuildPoly(const_cast<gfx::PolygonMeshClass&>(mPolygon));
    mPolygon.IntoJson(json);

    settings.SetValue("Polygon", "content", json);
    settings.SetValue("Polygon", "material", (QString)GetItemId(mUI.blueprints));
    settings.SetValue("Polygon", "hash", mOriginalHash);
    settings.SaveWidget("Polygon", mUI.name);
    settings.SaveWidget("Polygon", mUI.alpha);
    settings.SaveWidget("Polygon", mUI.chkShowGrid);
    settings.SaveWidget("Polygon", mUI.chkSnap);
    settings.SaveWidget("Polygon", mUI.cmbGrid);
    settings.SaveWidget("Polygon", mUI.widget);
    return true;
}
bool ShapeWidget::LoadState(const Settings& settings)
{
    QString material;
    data::JsonObject json;
    settings.GetValue("Polygon", "content", &json);
    settings.GetValue("Polygon", "material", &material);
    settings.GetValue("Polygon", "hash", &mOriginalHash);
    settings.LoadWidget("Polygon", mUI.name);
    settings.LoadWidget("Polygon", mUI.alpha);
    settings.LoadWidget("Polygon", mUI.chkShowGrid);
    settings.LoadWidget("Polygon", mUI.chkSnap);
    settings.LoadWidget("Polygon", mUI.cmbGrid);
    settings.LoadWidget("Polygon", mUI.widget);

    if (!mPolygon.FromJson(json))
        WARN("Failed to restore polygon shape state.");

    SetValue(mUI.ID, mPolygon.GetId());
    SetValue(mUI.staticInstance, mPolygon.IsStatic());
    SetValue(mUI.blueprints, ListItemId(material));
    SetValue(mUI.shaderFile, mPolygon.HasShaderSrc() ? "Customized Shader" : "Built-in Shader");
    SetEnabled(mUI.btnResetShader, mPolygon.HasShaderSrc());
    SetEnabled(mUI.actionClear, mPolygon.HasInlineData());

    mBuilder.InitFrom(mPolygon);

    on_blueprints_currentIndexChanged(0);
    return true;
}

bool ShapeWidget::CanTakeAction(Actions action, const Clipboard* clipboard) const
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
        case Actions::CanZoomIn:
        case Actions::CanZoomOut:
            return false;
        case Actions::CanUndo:
            return false;
    }
    return false;
}

void ShapeWidget::ReloadShaders()
{
    mUI.widget->reloadShaders();
}
void ShapeWidget::ReloadTextures()
{
    mUI.widget->reloadTextures();
}
void ShapeWidget::Shutdown()
{
    mUI.widget->dispose();
}
void ShapeWidget::Render()
{
    mUI.widget->triggerPaint();
}

void ShapeWidget::Update(double secs)
{
    if (mPaused || !mPlaying)
        return;

    mTime += secs;

    if (mBlueprint)
    {
        mBlueprint->SetRuntime(mTime);
    }
}

void ShapeWidget::Save()
{
    on_actionSave_triggered();
}

bool ShapeWidget::HasUnsavedChanges() const
{
    if (mOriginalHash == mPolygon.GetHash())
        return false;
    return true;
}

bool ShapeWidget::GetStats(Stats* stats) const
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

void ShapeWidget::on_widgetColor_colorChanged(QColor color)
{
    mUI.widget->SetClearColor(ToGfx(color));
}

void ShapeWidget::on_actionPlay_triggered()
{
    if (mPaused)
    {
        mPaused = false;
        mUI.actionPause->setEnabled(true);
    }
    else
    {
        mUI.actionPlay->setEnabled(false);
        mUI.actionPause->setEnabled(true);
        mUI.actionStop->setEnabled(true);
        mTime   = 0.0f;
        mPaused = false;
        mPlaying = true;
    }
}
void ShapeWidget::on_actionPause_triggered()
{
    mPaused = true;
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
}
void ShapeWidget::on_actionStop_triggered()
{
    mUI.actionStop->setEnabled(false);
    mUI.actionPause->setEnabled(false);
    mUI.actionPlay->setEnabled(true);
    mPaused = false;
    mPlaying = false;
    mTime = 0.0;
}

void ShapeWidget::on_actionSave_triggered()
{
    if (!MustHaveInput(mUI.name))
        return;

    mBuilder.BuildPoly(mPolygon);

    app::CustomShapeResource resource(mPolygon, GetValue(mUI.name));
    SetProperty(resource, "material", (QString)GetItemId(mUI.blueprints));
    SetUserProperty(resource, "alpha",        mUI.alpha);
    SetUserProperty(resource, "grid",         mUI.cmbGrid);
    SetUserProperty(resource, "snap_to_grid", mUI.chkSnap);
    SetUserProperty(resource, "show_grid",    mUI.chkShowGrid);
    SetUserProperty(resource, "widget",       mUI.widget);

    mWorkspace->SaveResource(resource);
    mOriginalHash = mPolygon.GetHash();
}

void ShapeWidget::on_actionNewTriangleFan_toggled(bool checked)
{
    if (checked)
    {
        mActive = true;
    }
    else
    {
        mPoints.clear();
        mActive = false;
    }
}

void ShapeWidget::on_actionShowShader_triggered()
{
    auto* device = mUI.widget->GetDevice();

    gfx::Drawable::Environment environment;
    environment.editing_mode  = false; // we want to see the shader as it will be, so using false here
    environment.instanced_draw = false;
    const auto& source = mPolygon.GetShader(environment, *device);

    DlgTextEdit dlg(this);
    dlg.SetText(source.GetSource(), "GLSL");
    dlg.SetReadOnly(true);
    dlg.SetTitle("Shader Source");
    dlg.LoadGeometry(mWorkspace, "polygon-shader-source-dialog-geometry");
    dlg.execFU();
    dlg.SaveGeometry(mWorkspace, "polygon-shader-source-dialog-geometry");
}

void ShapeWidget::on_actionCustomizeShader_triggered()
{
    if (mShaderEditor)
    {
        mShaderEditor->activateWindow();
        return;
    }

    mCustomizedSource = mPolygon.GetShaderSrc();
    if (!mPolygon.HasShaderSrc())
    {
        mPolygon.SetShaderSrc(R"(
// this is your custom vertex transform function.
// you can modify the incoming vertex data here as want.
void CustomVertexTransform(inout VertexData vs) {

   // your code here

})");
    }

    mShaderEditor = new DlgTextEdit(this);
    mShaderEditor->LoadGeometry(mWorkspace, "polygon-shader-editor-geometry");
    mShaderEditor->SetText(mPolygon.GetShaderSrc(), "GLSL");
    mShaderEditor->SetTitle("Shader Source");
    mShaderEditor->EnableApply(true);
    mShaderEditor->showFU();
    mShaderEditor->finished = [this](int ret) {
        if (ret == QDialog::Rejected)
            mPolygon.SetShaderSrc(mCustomizedSource);
        else if (ret == QDialog::Accepted)
            mPolygon.SetShaderSrc(mShaderEditor->GetText());
        mShaderEditor->SaveGeometry(mWorkspace, "polygon-shader-editor-geometry");
        mShaderEditor->deleteLater();
        mShaderEditor = nullptr;

        SetValue(mUI.shaderFile, mPolygon.HasShaderSrc() ? "Customized Shader" : "Built-in Shader");
        SetEnabled(mUI.btnResetShader, mPolygon.HasShaderSrc());

        mUI.widget->GetPainter()->ClearErrors();
    };
    mShaderEditor->apply = [this]() {
        mPolygon.SetShaderSrc(mShaderEditor->GetText());

        mUI.widget->GetPainter()->ClearErrors();
    };
}

void ShapeWidget::on_actionClear_triggered()
{
    mBuilder.ClearVertices();
    mBuilder.ClearDrawCommands();
    mUI.actionClear->setEnabled(false);
}

void ShapeWidget::on_blueprints_currentIndexChanged(int)
{
    mBlueprint.reset();
    if (mUI.blueprints->currentIndex() == -1)
        return;

    auto klass = mWorkspace->GetMaterialClassById(GetItemId(mUI.blueprints));
    mBlueprint = gfx::CreateMaterialInstance(klass);
}

void ShapeWidget::on_btnResetShader_clicked()
{
    mPolygon.SetShaderSrc(std::string{});
    SetValue(mUI.shaderFile, "Built-In Shader");
    SetEnabled(mUI.btnResetShader, false);

    mUI.widget->GetPainter()->ClearErrors();
}

void ShapeWidget::on_btnResetBlueprint_clicked()
{
    SetValue(mUI.blueprints, -1);
    mBlueprint.reset();
}

void ShapeWidget::on_staticInstance_stateChanged(int)
{
    mBuilder.SetStatic(GetValue(mUI.staticInstance));
}

void ShapeWidget::OnAddResource(const app::Resource* resource)
{
    if (resource->GetType() != app::Resource::Type::Material)
        return;

    SetList(mUI.blueprints, mWorkspace->ListUserDefinedMaterials());
}

void ShapeWidget::OnRemoveResource(const app::Resource* resource)
{
    if (resource->GetType() != app::Resource::Type::Material)
        return;

    SetList(mUI.blueprints, mWorkspace->ListUserDefinedMaterials());
    if (mBlueprint && mBlueprint->GetClassId() == resource->GetIdUtf8())
        mBlueprint.reset();
}

void ShapeWidget::PaintScene(gfx::Painter& painter, double secs)
{
    const auto widget_width  = mUI.widget->width();
    const auto widget_height = mUI.widget->height();
    const auto size = std::min(widget_width, widget_height);
    const auto xoffset = (widget_width - size) / 2;
    const auto yoffset = (widget_height - size) / 2;
    const auto width = size;
    const auto height = size;
    painter.SetViewport(xoffset, yoffset, size, size);
    painter.SetProjectionMatrix(gfx::MakeOrthographicProjection(width , height));

    SetValue(mUI.widgetColor, mUI.widget->GetCurrentClearColor());

    gfx::Transform view;
    // fiddle with the view transform in order to avoid having
    // some content (fragments) get clipped against the viewport.
    view.Resize(width-2, height-2);
    view.MoveTo(1, 1);

    // if we have some blueprint then use it on the background.
    if (mBlueprint)
    {
        painter.Draw(gfx::Rectangle(), view, *mBlueprint);
    }

    if (GetValue(mUI.chkShowGrid))
    {
        const GridDensity grid = GetValue(mUI.cmbGrid);
        const unsigned num_cell_lines = static_cast<unsigned>(grid) - 1;
        painter.Draw(gfx::Grid(num_cell_lines, num_cell_lines), view,
                     gfx::CreateMaterialFromColor(gfx::Color::LightGray));
    }
    else
    {
        painter.Draw(gfx::Rectangle(gfx::SimpleShapeStyle::Outline), view,
                     gfx::CreateMaterialFromColor(gfx::Color::LightGray));
    }

    // draw the polygon we're working on
    const auto alpha = GetValue(mUI.alpha);
    static gfx::ColorClass color(gfx::MaterialClass::Type::Color);
    color.SetBaseColor(gfx::Color4f(gfx::Color::LightGray, alpha));
    color.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);

    // hack hack if we have the main window preview window displaying this
    // same custom shape then we have a competition of hash values used
    // to compare the polygon data content against the content in the GPU
    // buffer. And the competition is between the class object stored in
    // the workspace that has the same Class ID but different content hash
    // and *this* polygon class instance here that is a copy but maps to
    // the same class ID but with different hash (because it has different
    // content when it's being edited).
    // so hack around this problem by adding a suffix here.
    gfx::PolygonMeshClass poly(mPolygon.GetId() + "_1");
    poly.SetShaderSrc(mPolygon.GetShaderSrc());
    poly.SetName(mPolygon.GetName());
    mBuilder.BuildPoly(poly);

    // set to true since we're constructing this polygon on every frame
    // without this we'll eat all the static vertex/index buffers. argh!
    poly.SetDynamic(true);

    gfx::PolygonMeshInstance mesh(poly);
    mesh.SetTime(mTime);
    mesh.SetRandomValue(0.123423); // random!

    painter.Draw(mesh, view, gfx::MaterialInstance(color));

    // visualize the vertices.
    view.Resize(6, 6);
    for (size_t i=0; i<mBuilder.GetNumVertices(); ++i)
    {
        const auto& vert = mBuilder.GetVertex(i);
        const auto x = width * vert.aPosition.x;
        const auto y = height * -vert.aPosition.y;
        view.MoveTo(x+1, y+1);
        view.Translate(-3, -3);
        if (mVertexIndex == i)
        {
            painter.Draw(gfx::Circle(), view, gfx::CreateMaterialFromColor(gfx::Color::Green));
        }
        else
        {
            painter.Draw(gfx::Circle(), view, gfx::CreateMaterialFromColor(gfx::Color::HotPink));
        }
    }

    if (painter.GetErrorCount())
    {
        ShowMessage("Shader compile error:", gfx::FPoint(10.0f, 10.0f), painter);
        ShowMessage(painter.GetError(0), gfx::FPoint(10.0f, 30.0f), painter);
    }

    if (!mActive)
        return;

    if (!mPoints.empty())
    {
        const QPoint& a = mPoints.back();
        const QPoint& b = mCurrentPoint;
        gfx::DebugDrawLine(painter, ToGfx(a), ToGfx(b), gfx::Color::HotPink, 2.0f);
    }

    std::vector<QPoint> points = mPoints;
    points.push_back(mCurrentPoint);

    view.Resize(width-2, height-2);
    view.MoveTo(1, 1);

    gfx::Geometry::DrawCommand cmd;
    cmd.type   = gfx::Geometry::DrawType::TriangleFan;
    cmd.offset = 0;
    cmd.count  = points.size();

    gfx::tool::PolygonBuilder builder;
    builder.AddVertices(MakeVerts(points, width, height));
    builder.AddDrawCommand(cmd);

    gfx::PolygonMeshClass current(mPolygon.GetId() + "_2");
    builder.BuildPoly(current);
    current.SetStatic(false);

    painter.Draw(gfx::PolygonMeshInstance(current), view, gfx::MaterialInstance(color));
}

void ShapeWidget::OnMousePress(QMouseEvent* mickey)
{
    const auto widget_width  = mUI.widget->width();
    const auto widget_height = mUI.widget->height();
    const auto size = std::min(widget_width, widget_height);
    const auto xoffset = (widget_width - size) / 2;
    const auto yoffset = (widget_height - size) / 2;
    const auto width = size;
    const auto height = size;

    const auto& point = mickey->pos() - QPoint(xoffset, yoffset);

    for (size_t i=0; i<mBuilder.GetNumVertices(); ++i)
    {
        const auto& vert = mBuilder.GetVertex(i);
        const auto x = width * vert.aPosition.x;
        const auto y = height * -vert.aPosition.y;
        const auto r = QPoint(x, y) - point;
        const auto l = std::sqrt(r.x() * r.x() + r.y() * r.y());
        if (l <= 5)
        {
            mDragging = true;
            mVertexIndex = i;
            break;
        }
    }
}

void ShapeWidget::OnMouseRelease(QMouseEvent* mickey)
{
    mDragging = false;

    if (!mActive)
        return;

    const auto widget_width  = mUI.widget->width();
    const auto widget_height = mUI.widget->height();
    const auto size = std::min(widget_width, widget_height);
    const auto xoffset = (widget_width - size) / 2;
    const auto yoffset = (widget_height - size) / 2;
    const auto width  = size;
    const auto height = size;

    const auto& pos = mickey->pos() - QPoint(xoffset, yoffset);

    if (GetValue(mUI.chkSnap))
    {
        const GridDensity grid = GetValue(mUI.cmbGrid);
        const float num_cells = static_cast<float>(grid);

        const auto cell_width = width / num_cells;
        const auto cell_height = height / num_cells;
        const auto x = std::round(pos.x() / cell_width) * cell_width;
        const auto y = std::round(pos.y() / cell_height) * cell_height;
        mPoints.push_back(QPoint(x, y));
    }
    else
    {
        mPoints.push_back(pos);
    }
}

void ShapeWidget::OnMouseMove(QMouseEvent* mickey)
{
    const auto widget_width  = mUI.widget->width();
    const auto widget_height = mUI.widget->height();
    const auto size = std::min(widget_width, widget_height);
    const auto xoffset = (widget_width - size) / 2;
    const auto yoffset = (widget_height - size) / 2;
    const auto width  = size;
    const auto height = size;
    const auto& pos = mickey->pos() - QPoint(xoffset, yoffset);

    if (mDragging)
    {
        auto vertex = mBuilder.GetVertex(mVertexIndex);
        if (GetValue(mUI.chkSnap))
        {
            const GridDensity grid = GetValue(mUI.cmbGrid);
            const float num_cells = static_cast<float>(grid);

            const auto cell_width = width / num_cells;
            const auto cell_height = height / num_cells;
            const auto new_x = std::round(pos.x() / cell_width) * cell_width;
            const auto new_y = std::round(pos.y() / cell_height) * cell_height;
            const auto old_x = std::round((vertex.aPosition.x * width) / cell_width) * cell_width;
            const auto old_y = std::round((vertex.aPosition.y * -height) / cell_height) * cell_height;
            vertex.aPosition.x = new_x / width;
            vertex.aPosition.y = new_y / -height;
            if (new_x != old_x || new_y != old_y)
            {
                const QPoint& snap_point =  QPoint(new_x, new_y);
                QCursor::setPos(mUI.widget->mapToGlobal(snap_point + QPoint(xoffset, yoffset)));
                mCurrentPoint = snap_point;
            }
        }
        else
        {
            const float dx = (mCurrentPoint.x() - pos.x());
            const float dy = (mCurrentPoint.y() - pos.y());
            vertex.aPosition.x -= (dx / width);
            vertex.aPosition.y += (dy / height);
            mCurrentPoint = pos;
        }
        vertex.aTexCoord.x  = vertex.aPosition.x;
        vertex.aTexCoord.y  = -vertex.aPosition.y;
        mBuilder.UpdateVertex(vertex, mVertexIndex);
    }
    else
    {
        mCurrentPoint = pos;
    }
}

void ShapeWidget::OnMouseDoubleClick(QMouseEvent* mickey)
{
    // find the vertex closes to the click point
    // use polygon winding order to figure out whether
    // the new vertex should come before or after the closest vertex
    const auto widget_width  = mUI.widget->width();
    const auto widget_height = mUI.widget->height();
    const auto size = std::min(widget_width, widget_height);
    const auto xoffset = (widget_width - size) / 2;
    const auto yoffset = (widget_height - size) / 2;
    const auto width  = size;
    const auto height = size;
    const auto& pos = mickey->pos() - QPoint(xoffset, yoffset);

    const auto num_vertices = mBuilder.GetNumVertices();

    QPoint point = pos;
    if (GetValue(mUI.chkSnap))
    {
        const GridDensity grid = GetValue(mUI.cmbGrid);
        const float num_cells = static_cast<float>(grid);

        const auto cell_width = width / num_cells;
        const auto cell_height = height / num_cells;
        const auto x = std::round(pos.x() / cell_width) * cell_width;
        const auto y = std::round(pos.y() / cell_height) * cell_height;
        point = QPoint(x, y);
    }

    // find the vertex closest to the click point.
    float distance = std::numeric_limits<float>::max();
    size_t vertex_index = 0;
    for (size_t i=0; i<num_vertices; ++i)
    {
        const auto vert = MapVertexToWidget(mBuilder.GetVertex(i), width, height);
        const auto len  = PointDist(point, vert);
        if (len < distance)
        {
            vertex_index = i;
            distance = len;
        }
    }

    // not found ?
    if (vertex_index == num_vertices)
        return;
    const auto cmd_index = mBuilder.FindDrawCommand(vertex_index);
    if (cmd_index == std::numeric_limits<size_t>::max())
        return;

    DEBUG("Closest vertex: index %1 draw cmd %2", vertex_index, cmd_index);

    // degenerate left over triangle ?
    const auto& cmd = mBuilder.GetDrawCommand(cmd_index);
    if (cmd.count < 3)
        return;

    const auto cmd_vertex_index = vertex_index - cmd.offset;

    gfx::Vertex2D vertex;
    vertex.aPosition.x = point.x() / (float)width;
    vertex.aPosition.y = point.y() / (float)height * -1.0f;
    vertex.aTexCoord.x = vertex.aPosition.x;
    vertex.aTexCoord.y = -vertex.aPosition.y;

    // currently back face culling is enabled and the winding
    // order is set to CCW. That means that in order to
    // determine where the new vertex should be placed within
    // the draw command we can check the winding order.
    // We have two options:
    // root -> closest -> new vertex
    // root -> new vertex -> closest
    //
    // One of the above should have a CCW winding order and
    // give us the info where to place the new vertex.
    // note that there's still a possibility for a degenerate
    // case (such as when any of the two vertices are collinear)
    // and this isn't properly handled yet.
    const auto first = mBuilder.GetVertex(cmd.offset);
    const auto closest = mBuilder.GetVertex(vertex_index);
    const auto winding = math::FindTriangleWindingOrder(first.aPosition, closest.aPosition, vertex.aPosition);
    if (winding == math::TriangleWindingOrder::CounterClockwise)
         mBuilder.InsertVertex(vertex, cmd_index, cmd_vertex_index + 1);
    else mBuilder.InsertVertex(vertex, cmd_index, cmd_vertex_index);
}

bool ShapeWidget::OnKeyPressEvent(QKeyEvent* key)
{
    const auto widget_width  = mUI.widget->width();
    const auto widget_height = mUI.widget->height();
    const auto size = std::min(widget_width, widget_height);
    const auto width  = size;
    const auto height = size;

    if (key->key() == Qt::Key_Escape && mActive)
    {
        gfx::Geometry::DrawCommand cmd;
        cmd.type   = gfx::Geometry::DrawType::TriangleFan;
        cmd.count  = mPoints.size();
        cmd.offset = mBuilder.GetNumVertices();
        mBuilder.AddVertices(MakeVerts(mPoints, width, height));
        mBuilder.AddDrawCommand(cmd);
        mPoints.clear();

        mUI.actionNewTriangleFan->setChecked(false);
        mUI.actionClear->setEnabled(true);
        mActive = false;
    }
    else if (key->key() == Qt::Key_Delete ||
             key->key() == Qt::Key_D)
    {
        if (mVertexIndex < mBuilder.GetNumVertices())
            mBuilder.EraseVertex(mVertexIndex);
        if (mBuilder.GetNumVertices() == 0)
            mUI.actionClear->setEnabled(false);
    }
    else if (key->key() == Qt::Key_E)
    {

    }
    else  return false;

    return true;
}


} // namespace
