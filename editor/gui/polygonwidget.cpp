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

#define LOGTAG "gui"

#include "warnpush.h"
#  include <QMessageBox>
#  include <QVector2D>
#  include <base64/base64.h>
#include "warnpop.h"

#include "base/math.h"
#include "editor/app/eventlog.h"
#include "editor/gui/polygonwidget.h"
#include "editor/gui/utility.h"
#include "editor/gui/settings.h"
#include "graphics/material.h"
#include "graphics/drawable.h"
#include "graphics/drawing.h"
#include "graphics/transform.h"

namespace {

std::vector<gfx::PolygonClass::Vertex> MakeVerts(const std::vector<QPoint>& points,
    float width, float height)
{
    std::vector<gfx::PolygonClass::Vertex> verts;
    for (const auto& p : points)
    {
        gfx::PolygonClass::Vertex vertex;
        vertex.aPosition.x = p.x() / width;
        vertex.aPosition.y = p.y() / height * -1.0;
        vertex.aTexCoord.x = p.x() / width;
        vertex.aTexCoord.y = p.y() / height;
        verts.push_back(vertex);
    }
    return verts;
}

} // namespace

namespace gui
{

PolygonWidget::PolygonWidget(app::Workspace* workspace) : mWorkspace(workspace)
{
    DEBUG("Create PolygonWidget");

    mUI.setupUi(this);

    mUI.widget->onPaintScene = std::bind(&PolygonWidget::PaintScene,
        this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onMousePress  = std::bind(&PolygonWidget::OnMousePress,
        this, std::placeholders::_1);
    mUI.widget->onMouseRelease = std::bind(&PolygonWidget::OnMouseRelease,
        this, std::placeholders::_1);
    mUI.widget->onMouseMove = std::bind(&PolygonWidget::OnMouseMove,
        this, std::placeholders::_1);
    mUI.widget->onMouseDoubleClick = std::bind(&PolygonWidget::OnMouseDoubleClick,
        this, std::placeholders::_1);
    mUI.widget->onKeyPress = std::bind(&PolygonWidget::OnKeyPressEvent,
        this, std::placeholders::_1);

    mUI.blueprints->addItems(QStringList(""));
    mUI.blueprints->addItems(workspace->ListUserDefinedMaterials());
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(false);
    mUI.name->setText("My Custom Shape");

    setWindowTitle("My Custom Shape");
    setFocusPolicy(Qt::StrongFocus);

    connect(workspace, &app::Workspace::NewResourceAvailable,
        this, &PolygonWidget::NewResourceAvailable);
    connect(workspace, &app::Workspace::ResourceToBeDeleted,
        this, &PolygonWidget::ResourceToBeDeleted);

    PopulateFromEnum<GridDensity>(mUI.cmbGrid);
    SetValue(mUI.cmbGrid, GridDensity::Grid20x20);
}

PolygonWidget::PolygonWidget(app::Workspace* workspace, const app::Resource& resource) : PolygonWidget(workspace)
{
    DEBUG("Editing custom shape '%1'", resource.GetName());

    mUI.name->setText(resource.GetName());
    GetProperty(resource, "material", mUI.blueprints);
    GetProperty(resource, "draw_alpha", mUI.alpha);

    mPolygon = *resource.GetContent<gfx::PolygonClass>();
    mOriginalHash = mPolygon.GetHash();
    setWindowTitle(mUI.name->text());
}

PolygonWidget::~PolygonWidget()
{
    DEBUG("Destroy PolygonWidget");
}

void PolygonWidget::AddActions(QToolBar& bar)
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
void PolygonWidget::AddActions(QMenu& menu)
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

bool PolygonWidget::SaveState(Settings& settings) const
{
    settings.saveWidget("Polygon", mUI.name);
    settings.saveWidget("Polygon", mUI.blueprints);
    settings.saveWidget("Polygon", mUI.alpha);
    settings.saveWidget("Polygon", mUI.blueprints);
    settings.saveWidget("Polygon", mUI.chkShowGrid);
    settings.saveWidget("Polygon", mUI.cmbGrid);

    // the polygon can already serialize into JSON.
    // so let's use the JSON serialization in the animation
    // and then convert that into base64 string which we can
    // stick in the settings data stream.
    const auto& json = mPolygon.ToJson();
    const auto& base64 = base64::Encode(json.dump(2));
    settings.setValue("Polygon", "content", base64);
    return true;
}
bool PolygonWidget::LoadState(const Settings& settings)
{
    settings.loadWidget("Polygon", mUI.name);
    settings.loadWidget("Polygon", mUI.blueprints);
    settings.loadWidget("Polygon", mUI.blueprints);
    settings.loadWidget("Polygon", mUI.chkShowGrid);
    settings.loadWidget("Polygon", mUI.cmbGrid);
    setWindowTitle(mUI.name->text());

    const std::string& base64 = settings.getValue("Polygon", "content", std::string(""));
    if (base64.empty())
        return true;

    const auto& json = nlohmann::json::parse(base64::Decode(base64));
    auto ret  = gfx::PolygonClass::FromJson(json);
    if (!ret.has_value())
    {
        WARN("Failed to load polygon widget state.");
        return false;
    }
    mPolygon = std::move(ret.value());
    return true;
}

void PolygonWidget::ReloadShaders()
{
    mUI.widget->reloadShaders();
}
void PolygonWidget::ReloadTextures()
{
    mUI.widget->reloadTextures();
}
void PolygonWidget::Shutdown()
{
    mUI.widget->dispose();
}
void PolygonWidget::Render()
{
    mUI.widget->triggerPaint();
}

void PolygonWidget::Update(double secs)
{
    if (mPaused || !mPlaying)
        return;

    mTime += secs;

    const auto& blueprint = mUI.blueprints->currentText();
    if (blueprint.isEmpty())
    {
        mBlueprint.reset();
        mBlueprintName.clear();
    }
    else if (blueprint != mBlueprintName)
    {
        mBlueprint = mWorkspace->MakeMaterial(blueprint);
        mBlueprintName = blueprint;
    }
    if (mBlueprint)
    {
        mBlueprint->SetRuntime(mTime);
    }
}

bool PolygonWidget::ConfirmClose()
{
    const auto hash = mPolygon.GetHash();
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

void PolygonWidget::on_actionPlay_triggered()
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
void PolygonWidget::on_actionPause_triggered()
{
    mPaused = true;
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
}
void PolygonWidget::on_actionStop_triggered()
{
    mUI.actionStop->setEnabled(false);
    mUI.actionPause->setEnabled(false);
    mUI.actionPlay->setEnabled(true);
    mPaused = false;
    mPlaying = false;
}

void PolygonWidget::on_actionSave_triggered()
{
    if (!MustHaveInput(mUI.name))
        return;

    const QString& name = GetValue(mUI.name);
    if (mWorkspace->HasResource(name, app::Resource::Type::CustomShape))
    {
        const auto& resource = mWorkspace->GetResource(name, app::Resource::Type::CustomShape);
        const gfx::PolygonClass* polygon = nullptr;
        resource.GetContent(&polygon);
        if (polygon->GetHash() != mOriginalHash)
        {
            QMessageBox msg(this);
            msg.setIcon(QMessageBox::Question);
            msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msg.setText("Workspace already contains a shape by this name. Overwrite?");
            if (msg.exec() == QMessageBox::No)
                return;
        }
    }

    // update the hash to the new value.
    mOriginalHash = mPolygon.GetHash();

    app::CustomShapeResource resource(mPolygon, name);
    const QString& material = GetValue(mUI.blueprints);
    if (!material.isEmpty())
        SetProperty(resource, "material", mUI.blueprints);
    mWorkspace->SaveResource(resource);

    INFO("Saved shape '%1'", name);
    NOTE("Saved shape '%1'", name);
    setWindowTitle(name);
}

void PolygonWidget::on_actionNewTriangleFan_toggled(bool checked)
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

void PolygonWidget::on_actionClear_triggered()
{
    mPolygon.ClearVertices();
    mPolygon.ClearDrawCommands();
    mUI.actionClear->setEnabled(false);
}

void PolygonWidget::NewResourceAvailable(const app::Resource* resource)
{
    if (resource->GetType() != app::Resource::Type::Material)
        return;

    mUI.blueprints->clear();
    mUI.blueprints->addItems(QStringList(""));
    mUI.blueprints->addItems(mWorkspace->ListUserDefinedMaterials());
    if (mBlueprintName.isEmpty())
        return;

    const auto index = mUI.blueprints->findText(mBlueprintName);
    mUI.blueprints->setCurrentIndex(index);
}

void PolygonWidget::ResourceToBeDeleted(const app::Resource* resource)
{
    if (resource->GetType() != app::Resource::Type::Material)
        return;

    mUI.blueprints->clear();
    mUI.blueprints->addItems(QStringList(""));
    mUI.blueprints->addItems(mWorkspace->ListUserDefinedMaterials());
    if (mBlueprintName.isEmpty())
        return;

    const auto index = mUI.blueprints->findText(mBlueprintName);
    if (index == -1)
    {
        mBlueprint.reset();
        mBlueprintName.clear();
    }
    else
    {
        mUI.blueprints->setCurrentIndex(index);
    }
}

void PolygonWidget::PaintScene(gfx::Painter& painter, double secs)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    gfx::Transform view;
    view.Resize(width, height);

    const auto& blueprint = mUI.blueprints->currentText();
    if (blueprint.isEmpty())
    {
        mBlueprint.reset();
        mBlueprintName.clear();
    }
    else if (mBlueprintName != blueprint)
    {
        mBlueprint = mWorkspace->MakeMaterial(blueprint);
        mBlueprintName = blueprint;
    }

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
            gfx::SolidColor(gfx::Color4f(gfx::Color::LightGray, 0.7f))
                .SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent));
    }


    // draw the polygon we're working on
    const auto alpha = GetValue(mUI.alpha);
    painter.Draw(gfx::Polygon(mPolygon), view,
        gfx::SolidColor(gfx::Color4f(gfx::Color::LightGray, alpha))
            .SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent));

    // visualize the vertices.
    view.Resize(6, 6);
    for (size_t i=0; i<mPolygon.GetNumVertices(); ++i)
    {
        const auto& vert = mPolygon.GetVertex(i);
        const auto x = width * vert.aPosition.x;
        const auto y = height * -vert.aPosition.y;
        view.MoveTo(x, y);
        view.Translate(-3, -3);
        if (mVertexIndex == i)
        {
            painter.Draw(gfx::Circle(), view, gfx::SolidColor(gfx::Color::Green));
        }
        else
        {
            painter.Draw(gfx::Circle(), view, gfx::SolidColor(gfx::Color::HotPink));
        }
    }

    if (!mActive)
        return;

    if (!mPoints.empty())
    {
        const QPoint& a = mPoints.back();
        const QPoint& b = mCurrentPoint;
        gfx::DrawLine(painter, ToGfx(a), ToGfx(b), gfx::Color::HotPink ,2.0f);
    }

    std::vector<QPoint> points = mPoints;
    points.push_back(mCurrentPoint);

    view.Resize(width, height);
    view.MoveTo(0, 0);

    gfx::PolygonClass poly;
    gfx::PolygonClass::DrawCommand cmd;
    cmd.type   = gfx::PolygonClass::DrawType::TriangleFan;
    cmd.offset = 0;
    cmd.count  = points.size();
    poly.AddDrawCommand(MakeVerts(points, width, height), cmd);
    painter.Draw(gfx::Polygon(poly), view,
        gfx::SolidColor(gfx::Color4f(gfx::Color::LightGray, alpha))
            .SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent));
}

void PolygonWidget::OnMousePress(QMouseEvent* mickey)
{
    const auto width = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto& point = mickey->pos();
    for (size_t i=0; i<mPolygon.GetNumVertices(); ++i)
    {
        const auto& vert = mPolygon.GetVertex(i);
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

void PolygonWidget::OnMouseRelease(QMouseEvent* mickey)
{
    mDragging = false;

    if (!mActive)
        return;

    if (GetValue(mUI.chkSnap))
    {
        const GridDensity grid = GetValue(mUI.cmbGrid);
        const float num_cells = static_cast<float>(grid);

        const auto width  = mUI.widget->width();
        const auto height = mUI.widget->height();
        const auto& pos = mickey->pos();
        const auto cell_width = width / num_cells;
        const auto cell_height = height / num_cells;
        const auto x = std::round(pos.x() / cell_width) * cell_width;
        const auto y = std::round(pos.y() / cell_height) * cell_height;
        mPoints.push_back(QPoint(x, y));
    }
    else
    {
        mPoints.push_back(mickey->pos());
    }
}

void PolygonWidget::OnMouseMove(QMouseEvent* mickey)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto& pos = mickey->pos();

    if (mDragging)
    {
        auto vertex = mPolygon.GetVertex(mVertexIndex);
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
                QCursor::setPos(mUI.widget->mapToGlobal(snap_point));
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
        mPolygon.UpdateVertex(vertex, mVertexIndex);
    }
    else
    {
        mCurrentPoint = pos;
    }
}

void PolygonWidget::OnMouseDoubleClick(QMouseEvent* mickey)
{
    // find the vertex closes to the click point
    // use polygon winding order to figure out whether
    // the new vertex should come before or after the closest vertex

    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto num_vertices = mPolygon.GetNumVertices();

    QPoint point = mickey->pos();
    if (GetValue(mUI.chkSnap))
    {
        const GridDensity grid = GetValue(mUI.cmbGrid);
        const float num_cells = static_cast<float>(grid);

        const auto& pos = mickey->pos();
        const auto cell_width = width / num_cells;
        const auto cell_height = height / num_cells;
        const auto x = std::round(pos.x() / cell_width) * cell_width;
        const auto y = std::round(pos.y() / cell_height) * cell_height;
        point = QPoint(x, y);
    }

    // find the closest vertex.
    float distance = std::numeric_limits<float>::max();
    size_t index = 0;
    for (size_t i=0; i<num_vertices; ++i)
    {
        const auto& vert = mPolygon.GetVertex(i);
        const auto x = width * vert.aPosition.x;
        const auto y = height * -vert.aPosition.y;
        const auto r = QPoint(x, y) - point;
        const auto len = std::sqrt(r.x()*r.x() + r.y()*r.y());
        if (len < distance)
        {
            index = i;
            distance = len;
        }
    }

    // not found ?
    if (index == num_vertices)
        return;

    DEBUG("Closest vertex index %1",index);

    // figure out which draw command this vertex belongs to.
    // note that there could be multiple draw commands but for
    // now we assume there's only one.
    gfx::PolygonClass::DrawCommand cmd;
    for (size_t i=0; i<mPolygon.GetNumDrawCommands(); ++i)
    {
        const auto& c = mPolygon.GetDrawCommand(i);
        if (index >= c.offset &&  index < c.offset + c.count)
        {
            cmd = c;
            break;
        }
    }
    index = index - cmd.offset;
    const auto prev = 0; //index > 0 ? index - 1 : cmd.count - 1;
    const auto next = (index + 1) % cmd.count;

    gfx::Vec2 aPosition;
    aPosition.x = point.x() / (float)width;
    aPosition.y = point.y() / -(float)height;

    const auto winding = math::FindTriangleWindingOrder(mPolygon.GetVertex(prev + cmd.offset).aPosition,
        mPolygon.GetVertex(index + cmd.offset).aPosition,
        mPolygon.GetVertex(next + cmd.offset).aPosition);

    const auto before = math::FindTriangleWindingOrder(mPolygon.GetVertex(prev + cmd.offset).aPosition,
        aPosition, mPolygon.GetVertex(index + cmd.offset).aPosition);
    const auto after = math::FindTriangleWindingOrder(mPolygon.GetVertex(prev + cmd.offset).aPosition,
        mPolygon.GetVertex(index + cmd.offset).aPosition, aPosition);

    // by default before the current (closest) vertex.
    size_t insert_position = index + cmd.offset;

    if (winding != math::TriangleWindingOrder::Undetermined)
    {
        if (before == winding)
            insert_position = index + cmd.offset;
        else if (after == winding)
            insert_position = index + cmd.offset + 1;
    }
    gfx::PolygonClass::Vertex vertex;
    vertex.aPosition = aPosition;
    vertex.aTexCoord.x = vertex.aPosition.x;
    vertex.aTexCoord.y = -vertex.aPosition.y;
    mPolygon.InsertVertex(vertex, insert_position);
}

bool PolygonWidget::OnKeyPressEvent(QKeyEvent* key)
{
    if (key->key() == Qt::Key_Escape && mActive)
    {
        const auto width  = mUI.widget->width();
        const auto height = mUI.widget->height();

        gfx::PolygonClass::DrawCommand cmd;
        cmd.type   = gfx::PolygonClass::DrawType::TriangleFan;
        cmd.count  = mPoints.size();
        cmd.offset = mPolygon.GetNumVertices();
        mPolygon.AddDrawCommand(MakeVerts(mPoints, width, height), cmd);
        mPoints.clear();

        mUI.actionNewTriangleFan->setChecked(false);
        mUI.actionClear->setEnabled(true);
        mActive = false;
    }
    else if (key->key() == Qt::Key_Delete ||
             key->key() == Qt::Key_D)
    {
        if (mVertexIndex < mPolygon.GetNumVertices())
            mPolygon.EraseVertex(mVertexIndex);
        if (mPolygon.GetNumVertices() == 0)
            mUI.actionClear->setEnabled(false);
    }
    else if (key->key() == Qt::Key_E)
    {

    }
    else  return false;

    return true;
}


} // namespace
