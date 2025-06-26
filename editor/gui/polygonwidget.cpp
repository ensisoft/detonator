// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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
#  include <QAbstractTableModel>
#  include <base64/base64.h>
#include "warnpop.h"

#include <cmath>

#include "base/math.h"
#include "base/utility.h"
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
#include "editor/app/format.h"
#include "editor/gui/polygonwidget.h"
#include "editor/gui/utility.h"
#include "editor/gui/settings.h"
#include "editor/gui/dlgtextedit.h"
#include "editor/gui/drawing.h"

namespace {

constexpr auto Margin = 20; // pixels

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

class ShapeWidget::VertexDataTable : public QAbstractTableModel
{
public:
    virtual void UpdateVertex(const gfx::Vertex2D& vertex, size_t vertex_index) = 0;
    virtual void InsertVertex(const gfx::Vertex2D& vertex, size_t vertex_index, size_t cmd_index) = 0;
    virtual void AddVertices(std::vector<gfx::Vertex2D> vertices) = 0;
    virtual void EraseVertex(size_t vertex_index) = 0;
    virtual void Reset() = 0;
};

class ShapeWidget::Vertex2DTable : public VertexDataTable {
public:
    explicit Vertex2DTable(gfx::tool::PolygonBuilder2D& builder) : mBuilder(builder)
    {}
    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
    }

    bool setData(const QModelIndex& index, const QVariant& variant, int role) override
    {
        const auto row = static_cast<size_t>(index.row());
        const auto col = static_cast<size_t>(index.column());

        bool success = false;
        const float value = variant.toFloat(&success);
        if (!success)
            return false;

        auto& vertex = mBuilder.GetVertex(row);
        if (col == 0)
            vertex.aPosition.x = value;
        else if (col == 1)
            vertex.aPosition.y = value;
        else if (col == 2)
            vertex.aTexCoord.x = value;
        else if (col == 3)
            vertex.aTexCoord.y = value;

        emit dataChanged(this->index(row, 0), this->index(row, 4));
        return true;
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        const auto row = static_cast<size_t>(index.row());
        const auto col = static_cast<size_t>(index.column());
        if (role == Qt::DisplayRole)
        {
            const auto& vertex = mBuilder.GetVertex(row);
            if (col == 0)
                return QString::number(vertex.aPosition.x, 'f', 2);
            else if (col == 1)
                return QString::number(vertex.aPosition.y, 'f', 2);
            else if (col == 2)
                return QString::number(vertex.aTexCoord.x, 'f', 2);
            else if (col == 3)
                return QString::number(vertex.aTexCoord.y, 'f', 2);

        }
        return {};
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
        {
            if (section == 0)
                return "aPosition X";
            else if (section == 1)
                return "aPosition Y";
            else if (section == 2)
                return "aTexCoord X";
            else if (section == 3)
                return "aTexCoord Y";
        }
        return {};
    }
    int rowCount(const QModelIndex&) const override
    {
        return static_cast<int>(mBuilder.GetVertexCount());
    }
    int columnCount(const QModelIndex&) const override
    {
        return 4;
    }

    void UpdateVertex(const gfx::Vertex2D& vertex, size_t vertex_index) override
    {
        const auto row = static_cast<int>(vertex_index);

        mBuilder.UpdateVertex(vertex, vertex_index);

        emit dataChanged(this->index(row, 0), this->index(row, 4));
    }
    void InsertVertex(const gfx::Vertex2D& vertex, size_t vertex_index, size_t cmd_index) override
    {
        const auto first_index = static_cast<int>(vertex_index);
        const auto last_index = static_cast<int>(vertex_index);

        beginInsertRows(QModelIndex(),
            static_cast<int>(first_index), static_cast<int>(last_index));
        mBuilder.InsertVertex(vertex, cmd_index, vertex_index);
        endInsertRows();
    }

    void AddVertices(std::vector<gfx::Vertex2D> vertices) override
    {
        const auto first_index = mBuilder.GetVertexCount();
        const auto last_index = first_index + vertices.size() - 1;

        beginInsertRows(QModelIndex(),
            static_cast<int>(first_index), static_cast<int>(last_index));
        mBuilder.AddVertices(std::move(vertices));
        endInsertRows();
    }
    void EraseVertex(size_t index) override
    {
        beginRemoveRows(QModelIndex(), static_cast<int>(index), static_cast<int>(index));
        mBuilder.EraseVertex(index);
        endRemoveRows();
    }

    void Reset() override
    {
        beginResetModel();
        endResetModel();
    }
private:
    gfx::tool::PolygonBuilder2D& mBuilder;
};


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

    mUI.widget->SetCursorColor(gfx::Color::HotPink, GfxWidget::CursorShape::CrossHair);
    mUI.widget->SetCursorColor(gfx::Color::HotPink, GfxWidget::CursorShape::ArrowCursor);

    auto* menu = new QMenu(this);
    menu->addAction(mUI.actionCustomizeShader);
    menu->addAction(mUI.actionShowShader);
    mUI.btnAddShader->setMenu(menu);

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

    mPolygon.SetName(GetValue(mUI.name));
    mPolygon.SetStatic(GetValue(mUI.staticInstance));
    mOriginalHash = mPolygon.GetHash();

    mTable = std::make_unique<Vertex2DTable>(mBuilder);
    mUI.tableView->setModel(mTable.get());

    // Adwaita theme bugs out and doesn't show the data without this
    // little hack here.
    mTable->Reset();

    QObject::connect(mUI.tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     [this](const QItemSelection&, const QItemSelection&) {
        const auto row = GetSelectedRow(mUI.tableView);
        mVertexIndex = row;
    });

}

ShapeWidget::ShapeWidget(app::Workspace* workspace, const app::Resource& resource) : ShapeWidget(workspace)
{
    DEBUG("Editing shape '%1'", resource.GetName());

    mPolygon = *resource.GetContent<gfx::PolygonMeshClass>();
    mOriginalHash = mPolygon.GetHash();
    mBuilder.InitFrom(mPolygon);

    QString material;
    GetProperty(resource, "material", &material);
    GetUserProperty(resource, "grid",         mUI.cmbGrid);
    GetUserProperty(resource, "snap_to_grid", mUI.chkSnap);
    GetUserProperty(resource, "show_grid",    mUI.chkShowGrid);
    GetUserProperty(resource, "widget",       mUI.widget);
    GetUserProperty(resource, "splitter",     mUI.splitter);
    GetUserProperty(resource, "kRandom",      mUI.kRandom);

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

void ShapeWidget::InitializeContent()
{
    QTimer::singleShot(10, this, [this]() {
        QList<int> sizes {98, 100};
        //mUI.splitter->setSizes(sizes);
    });
}

void ShapeWidget::SetViewerMode()
{
    SetVisible(mUI.baseProperties, false);
    SetVisible(mUI.lblHelp,        false);
    SetVisible(mUI.blueprints,     false);
    SetVisible(mUI.chkSnap,        false);
    SetVisible(mUI.chkSnap,        false);
    SetVisible(mUI.btnResetBlueprint, false);
    SetVisible(mUI.lblRandom, false);
    SetVisible(mUI.kRandom, false);
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
    settings.SaveWidget("Polygon", mUI.chkShowGrid);
    settings.SaveWidget("Polygon", mUI.chkSnap);
    settings.SaveWidget("Polygon", mUI.cmbGrid);
    settings.SaveWidget("Polygon", mUI.widget);
    settings.SaveWidget("Polygon", mUI.splitter);
    settings.SaveWidget("Polygon", mUI.kRandom);
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
    settings.LoadWidget("Polygon", mUI.chkShowGrid);
    settings.LoadWidget("Polygon", mUI.chkSnap);
    settings.LoadWidget("Polygon", mUI.cmbGrid);
    settings.LoadWidget("Polygon", mUI.widget);
    settings.LoadWidget("Polygon", mUI.splitter);
    settings.LoadWidget("Polygon", mUI.kRandom);

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
    auto clone = mPolygon.Copy();
    auto* poly = dynamic_cast<gfx::PolygonMeshClass*>(clone.get());
    mBuilder.BuildPoly(*poly);

    poly->SetStatic(GetValue(mUI.staticInstance));
    poly->SetName(GetValue(mUI.name));

    if (mOriginalHash == poly->GetHash())
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

void ShapeWidget::on_widgetColor_colorChanged(const QColor& color)
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
    SetUserProperty(resource, "grid",         mUI.cmbGrid);
    SetUserProperty(resource, "snap_to_grid", mUI.chkSnap);
    SetUserProperty(resource, "show_grid",    mUI.chkShowGrid);
    SetUserProperty(resource, "widget",       mUI.widget);
    SetUserProperty(resource, "splitter",     mUI.splitter);
    SetUserProperty(resource, "kRandom",      mUI.kRandom);

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
    const auto widget_width  = mUI.widget->width() - Margin * 2;
    const auto widget_height = mUI.widget->height() - Margin * 2;
    const auto size = std::min(widget_width, widget_height);
    const auto xoffset = Margin + (widget_width - size) / 2;
    const auto yoffset = Margin + (widget_height - size) / 2;
    const auto width = float(size);
    const auto height = float(size);
    painter.SetViewport(xoffset, yoffset, size, size);
    painter.SetProjectionMatrix(gfx::MakeOrthographicProjection(width , height));

    SetValue(mUI.widgetColor, mUI.widget->GetCurrentClearColor());

    // if we have some blueprint then use it on the background.
    if (mBlueprint)
    {
        gfx::Transform view;
        view.Resize(width, height);
        painter.Draw(gfx::Rectangle(), view, *mBlueprint);
    }

    if (GetValue(mUI.chkShowGrid))
    {
        const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
        const auto num_cell_lines = static_cast<unsigned>(grid) - 1;

        gfx::Transform view;
        view.Resize(width, height);
        painter.Draw(gfx::Grid(num_cell_lines, num_cell_lines), view,
                     gfx::CreateMaterialFromColor(gfx::Color::LightGray));
    }
    else
    {
        gfx::Transform view;
        view.Resize(width, height);
        painter.Draw(gfx::Rectangle(gfx::SimpleShapeStyle::Outline), view,
                     gfx::CreateMaterialFromColor(gfx::Color::LightGray));
    }

    const auto alpha = 0.87f;
    static gfx::ColorClass color(gfx::MaterialClass::Type::Color);
    color.SetBaseColor(gfx::Color4f(gfx::Color::LightGray, alpha));
    color.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);

    // draw the polygon we're working on
    {
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
        mesh.SetRandomValue(GetValue(mUI.kRandom));

        gfx::Transform view;
        view.Resize(width, height);
        painter.Draw(mesh, view, gfx::MaterialInstance(color));
    }

    // visualize the vertices.
    {
        gfx::Transform view;
        view.Resize(15, 15);

        for (size_t i = 0; i < mBuilder.GetVertexCount(); ++i)
        {
            const auto& vert = mBuilder.GetVertex(i);
            const auto x = width * vert.aPosition.x;
            const auto y = height * -vert.aPosition.y;
            view.MoveTo(x, y);
            view.Translate(-7.5, -7.5);
            if (mVertexIndex == i)
            {
                painter.Draw(gfx::Circle(), view, gfx::CreateMaterialFromColor(gfx::Color::Green));
                if (!mDragging)
                {
                    const auto time = fmod(base::GetTime(), 1.5) / 1.5;
                    const auto size = 15.0f + time * 50.0f;
                    gfx::Transform model;
                    model.Resize(size, size);
                    model.MoveTo(x, y);
                    model.Translate(-size*0.5f, -size*0.5f);
                    painter.Draw(gfx::Circle(gfx::Circle::Style::Outline), model, gfx::CreateMaterialFromColor(gfx::Color::Green));
                }
            }
            else
            {
                painter.Draw(gfx::Circle(), view, gfx::CreateMaterialFromColor(gfx::Color::HotPink));
            }
        }
    }

    if (painter.GetErrorCount())
    {
        ShowMessage("Shader compile error:", gfx::FPoint(10.0f, 10.0f), painter);
        ShowMessage(painter.GetError(0), gfx::FPoint(10.0f, 30.0f), painter);
    }

    // visualize the current triangle
    if (mActive)
    {
        if (!mPoints.empty())
        {
            const QPoint& a = mPoints.back();
            const QPoint& b = mCurrentPoint;
            gfx::DebugDrawLine(painter, ToGfx(a), ToGfx(b), gfx::Color::HotPink, 2.0f);
        }

        std::vector<QPoint> points = mPoints;
        points.push_back(mCurrentPoint);

        gfx::Transform  view;
        view.Resize(width, height);

        gfx::Geometry::DrawCommand cmd;
        cmd.type = gfx::Geometry::DrawType::TriangleFan;
        cmd.offset = 0;
        cmd.count = points.size();

        gfx::tool::PolygonBuilder2D builder;
        builder.AddVertices(MakeVerts(points, width, height));
        builder.AddDrawCommand(cmd);

        gfx::PolygonMeshClass current(mPolygon.GetId() + "_2");
        builder.BuildPoly(current);
        current.SetStatic(false);

        painter.Draw(gfx::PolygonMeshInstance(current), view, gfx::MaterialInstance(color));
    }
}

void ShapeWidget::OnMousePress(QMouseEvent* mickey)
{
    const auto widget_width  = mUI.widget->width() - Margin * 2;
    const auto widget_height = mUI.widget->height() - Margin * 2;
    const auto size = std::min(widget_width, widget_height);
    const auto xoffset = Margin + (widget_width - size) / 2;
    const auto yoffset = Margin + (widget_height - size) / 2;
    const auto width  = float(size);
    const auto height = float(size);

    const auto& point = mickey->pos() - QPoint(xoffset, yoffset);

    for (size_t i=0; i<mBuilder.GetVertexCount(); ++i)
    {
        const auto& vert = mBuilder.GetVertex(i);
        const auto x = width * vert.aPosition.x;
        const auto y = height * -vert.aPosition.y;
        const auto r = QPoint(x, y) - point;
        const auto l = std::sqrt(r.x() * r.x() + r.y() * r.y());
        if (l <= 10)
        {
            mDragging = true;
            mVertexIndex = i;
            SelectRow(mUI.tableView, mVertexIndex);
            break;
        }
    }
}

void ShapeWidget::OnMouseRelease(QMouseEvent* mickey)
{
    mDragging = false;

    if (!mActive)
        return;

    const auto widget_width  = mUI.widget->width() - Margin * 2;
    const auto widget_height = mUI.widget->height() - Margin * 2;
    const auto size = std::min(widget_width, widget_height);
    const auto xoffset = Margin + (widget_width - size) / 2;
    const auto yoffset = Margin + (widget_height - size) / 2;
    const auto width  = float(size);
    const auto height = float(size);

    const auto& pos = mickey->pos() - QPoint(xoffset, yoffset);

    const auto ctrl = mickey->modifiers() & Qt::ControlModifier;

    bool snap = GetValue(mUI.chkSnap);
    if (ctrl)
        snap = !snap;

    if (snap)
    {
        const GridDensity grid = GetValue(mUI.cmbGrid);
        const auto num_cells = static_cast<float>(grid);

        const auto cell_width = width / num_cells;
        const auto cell_height = height / num_cells;
        const auto x = std::round(pos.x() / cell_width) * cell_width;
        const auto y = std::round(pos.y() / cell_height) * cell_height;
        mPoints.emplace_back(QPoint(x, y));
    }
    else
    {
        mPoints.push_back(pos);
    }
}

void ShapeWidget::OnMouseMove(QMouseEvent* mickey)
{
    const auto widget_width  = mUI.widget->width() - 2 * Margin;
    const auto widget_height = mUI.widget->height() - 2 * Margin;
    const auto size = std::min(widget_width, widget_height);
    const auto xoffset = Margin + (widget_width - size) / 2;
    const auto yoffset = Margin + (widget_height - size) / 2;
    const auto width  = float(size);
    const auto height = float(size);
    const auto& pos = mickey->pos() - QPoint(xoffset, yoffset);

    const auto ctrl = mickey->modifiers() & Qt::ControlModifier;

    if (mDragging)
    {
        bool snap = GetValue(mUI.chkSnap);
        if (ctrl)
            snap = !snap;

        auto vertex = mBuilder.GetVertex(mVertexIndex);
        if (snap)
        {
            const GridDensity grid = GetValue(mUI.cmbGrid);
            const auto num_cells = static_cast<float>(grid);

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
            const auto dx = float(mCurrentPoint.x() - pos.x());
            const auto dy = float(mCurrentPoint.y() - pos.y());
            vertex.aPosition.x -= (dx / width);
            vertex.aPosition.y += (dy / height);
            mCurrentPoint = pos;
        }
        vertex.aTexCoord.x =  vertex.aPosition.x;
        vertex.aTexCoord.y = -vertex.aPosition.y;
        mTable->UpdateVertex(vertex, mVertexIndex);
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
    const auto widget_width  = mUI.widget->width() - Margin * 2;
    const auto widget_height = mUI.widget->height() - Margin * 2;
    const auto size = std::min(widget_width, widget_height);
    const auto xoffset = Margin + (widget_width - size) / 2;
    const auto yoffset = Margin + (widget_height - size) / 2;
    const auto width  = float(size);
    const auto height = float(size);
    const auto& pos = mickey->pos() - QPoint(xoffset, yoffset);
    const auto ctrl = mickey->modifiers() & Qt::ControlModifier;

    const auto num_vertices = mBuilder.GetVertexCount();

    QPoint point = pos;
    bool snap = GetValue(mUI.chkSnap);
    if (ctrl)
        snap = !snap;

    if (snap)
    {
        const GridDensity grid = GetValue(mUI.cmbGrid);
        const auto num_cells = static_cast<float>(grid);

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
        mTable->InsertVertex(vertex, cmd_vertex_index + 1, cmd_index);
    else mTable->InsertVertex(vertex, cmd_vertex_index, cmd_index);
}

bool ShapeWidget::OnKeyPressEvent(QKeyEvent* key)
{
    const auto widget_width  = mUI.widget->width() - Margin * 2;
    const auto widget_height = mUI.widget->height() - Margin * 2;
    const auto size = std::min(widget_width, widget_height);
    const auto width  = size;
    const auto height = size;

    if (key->key() == Qt::Key_Escape && mActive)
    {
        gfx::Geometry::DrawCommand cmd;
        cmd.type   = gfx::Geometry::DrawType::TriangleFan;
        cmd.count  = mPoints.size();
        cmd.offset = mBuilder.GetVertexCount();
        mTable->AddVertices(MakeVerts(mPoints, width, height));
        mBuilder.AddDrawCommand(cmd);
        mPoints.clear();

        mUI.actionNewTriangleFan->setChecked(false);
        mUI.actionClear->setEnabled(true);
        mActive = false;
    }
    else if (key->key() == Qt::Key_Delete ||
             key->key() == Qt::Key_D)
    {
        if (mVertexIndex < mBuilder.GetVertexCount())
        {
            mTable->EraseVertex(mVertexIndex);
            mVertexIndex = 0xffffff;
            ClearSelection(mUI.tableView);
        }

        if (mBuilder.GetVertexCount() == 0)
            mUI.actionClear->setEnabled(false);
    }
    else if (key->key() == Qt::Key_E)
    {

    }
    else  return false;

    return true;
}


} // namespace
