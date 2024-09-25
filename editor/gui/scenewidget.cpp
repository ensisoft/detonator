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

#define LOGTAG "scene"

#include "config.h"

#include "warnpush.h"
#  include <QPoint>
#  include <QMouseEvent>
#  include <QMessageBox>
#  include <QFile>
#  include <QTextStream>
#  include <QAbstractTableModel>
#  include <base64/base64.h>
#  include <glm/glm.hpp>
#  include <glm/gtx/matrix_decompose.hpp>
#include "warnpop.h"

#include <algorithm>
#include <unordered_set>
#include <cmath>

#include "base/assert.h"
#include "base/format.h"
#include "base/math.h"
#include "data/json.h"
#include "base/transform.h"
#include "game/treeop.h"
#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/transform.h"
#include "graphics/drawing.h"
#include "graphics/types.h"
#include "editor/app/eventlog.h"
#include "editor/app/utility.h"
#include "editor/app/workspace.h"
#include "editor/gui/settings.h"
#include "editor/gui/treewidget.h"
#include "editor/gui/treemodel.h"
#include "editor/gui/nerd.h"
#include "editor/gui/tool.h"
#include "editor/gui/scenewidget.h"
#include "editor/gui/scriptwidget.h"
#include "editor/gui/tilemapwidget.h"
#include "editor/gui/utility.h"
#include "editor/gui/drawing.h"
#include "editor/gui/dlgscriptvar.h"
#include "editor/gui/dlgentity.h"
#include "editor/gui/clipboard.h"
#include "editor/gui/playwindow.h"

namespace gui
{

class DlgFindEntity::TableModel : public QAbstractTableModel
{
public:
    TableModel(const game::SceneClass& klass)
      : mScene(klass)
    {}
    virtual QVariant data(const QModelIndex& index, int role) const override
    {
        const auto& placement = mScene.GetPlacement(index.row());
        const auto& entity = placement.GetEntityClass();
        if (role == Qt::DisplayRole)
        {
            if (index.column() == 0) return app::toString(placement.GetName());
            else if (index.column() == 1) {
                if (entity)
                    return app::toString(entity->GetName());
                else return "*Deleted Class*";
            }
        }
        return QVariant();
    }
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
        {
            if (section == 0) return "Name";
            else if (section == 1) return "Class";
        }
        return QVariant();
    }
    virtual int rowCount(const QModelIndex&) const override
    {
        return static_cast<int>(mScene.GetNumNodes());
    }
    virtual int columnCount(const QModelIndex&) const override
    {
        return 2;
    }
private:
    const game::SceneClass& mScene;
};

class DlgFindEntity::TableProxy : public QSortFilterProxyModel
{
public:
    TableProxy(const game::SceneClass& klass)
      : mScene(klass)
    {}

    void SetFilterString(const QString& string)
    { mFilterString = string; }
protected:
    virtual bool filterAcceptsRow(int row, const QModelIndex&) const override
    {
        if (mFilterString.isEmpty())
            return true;

        const auto& node = mScene.GetPlacement(row);
        const auto& name = app::FromUtf8(node.GetName());
        if (name.contains(mFilterString, Qt::CaseInsensitive))
            return true;
        const auto& entity_klass = node.GetEntityClass();
        if (!entity_klass)
            return false;
        const auto& klass_name = app::FromUtf8(entity_klass->GetName());
        return klass_name.contains(mFilterString, Qt::CaseInsensitive);
    }
private:
    QString mFilterString;
    const game::SceneClass& mScene;
};


DlgFindEntity::DlgFindEntity(QWidget* parent, const game::SceneClass& klass)
  : QDialog(parent)
  , mScene(klass)
{
    mModel.reset(new TableModel(mScene));
    mProxy.reset(new TableProxy(mScene));
    mProxy->setSourceModel(mModel.get());

    mUI.setupUi(this);
    mUI.filter->installEventFilter(this);
    mUI.tableView->setModel(mProxy.get());
    mProxy->invalidate();
}

DlgFindEntity::~DlgFindEntity() = default;

void DlgFindEntity::on_btnAccept_clicked()
{
    const auto current = GetSelectedIndex(mUI.tableView);
    if (current.isValid())
        mNode = &mScene.GetPlacement(current.row());

    accept();
}
void DlgFindEntity::on_btnCancel_clicked()
{
    reject();
}

void DlgFindEntity::on_filter_textChanged(const QString&)
{
    mProxy->SetFilterString(GetValue(mUI.filter));
    mProxy->invalidate();
    SelectRow(mUI.tableView, 0);
}

bool DlgFindEntity::eventFilter(QObject* destination, QEvent* event)
{
    if (destination != mUI.filter)
        return false;
    else if (event->type() != QEvent::KeyPress)
        return false;
    else if (mScene.GetNumNodes() == 0)
        return false;

    const auto* key = static_cast<QKeyEvent*>(event);
    const bool ctrl = key->modifiers() & Qt::ControlModifier;
    const bool shift = key->modifiers() & Qt::ShiftModifier;

    int current = GetSelectedRow(mUI.tableView);

    const auto max = GetCount(mUI.tableView);
    if (ctrl && key->key() == Qt::Key_N)
        current = math::wrap(0, max-1, current+1);
    else if (ctrl && key->key() == Qt::Key_P)
        current = math::wrap(0, max-1, current-1);
    else if (key->key() == Qt::Key_Up)
        current = math::wrap(0, max-1, current-1);
    else if (key->key() == Qt::Key_Down)
        current = math::wrap(0, max-1, current+1);
    else return false;

    SelectRow(mUI.tableView, current);
    return true;
}

// todo: refactor this and the similar model from EntityWidget into
// some reusable class
class SceneWidget::ScriptVarModel : public QAbstractTableModel
{
public:
    ScriptVarModel(SceneWidget::State& state) : mState(state)
    {}
    virtual QVariant data(const QModelIndex& index, int role) const override
    {
        const auto& var = mState.scene->GetScriptVar(index.row());
        if (role == Qt::DisplayRole)
        {
            switch (index.column()) {
                //case 0: return app::toString(var.GetType());
                case 0: return app::FromUtf8(var.GetName());
                case 1: return GetScriptVarData(var);
                default: BUG("Unknown script variable data index.");
            }
        }
        return QVariant();
    }
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
        {
            switch (section) {
                //case 0: return "Type";
                case 0: return "Name";
                case 1: return "Value";
                default: BUG("Unknown script variable data index.");
            }
        }
        return QVariant();
    }
    virtual int rowCount(const QModelIndex&) const override
    {
        return static_cast<int>(mState.scene->GetNumScriptVars());
    }
    virtual int columnCount(const QModelIndex&) const override
    {
        return 2;
        //return 3;
    }
    void AddVariable(game::ScriptVar&& var)
    {
        const auto count = static_cast<int>(mState.scene->GetNumScriptVars());
        beginInsertRows(QModelIndex(), count, count);
        mState.scene->AddScriptVar(std::move(var));
        endInsertRows();
    }
    void EditVariable(size_t row, game::ScriptVar&& var)
    {
        mState.scene->SetScriptVar(row, std::move(var));
        emit dataChanged(index(row, 0), index(row, 3));
    }
    void DeleteVariable(size_t row)
    {
        beginRemoveRows(QModelIndex(), row, row);
        mState.scene->DeleteScriptVar(row);
        endRemoveRows();
    }
    void Reset()
    {
        beginResetModel();
        endResetModel();
    }

private:
    QVariant GetScriptVarData(const game::ScriptVar& var) const
    {
        using app::toString;

        switch (var.GetType())
        {
            case game::ScriptVar::Type::Boolean:
                if (!var.IsArray())
                    return var.GetValue<bool>();
                return QString("[0]=%1 ...").arg(var.GetArray<bool>()[0]);
            case game::ScriptVar::Type::String:
                if (!var.IsArray())
                    return app::FromUtf8(var.GetValue<std::string>());
                return QString("[0]='%1' ...").arg(app::FromUtf8(var.GetArray<std::string>()[0]));
            case game::ScriptVar::Type::Float:
                if (!var.IsArray())
                    return QString::number(var.GetValue<float>(), 'f', 2);
                return QString("[0]=%1 ...").arg(QString::number(var.GetArray<float>()[0], 'f', 2));
            case game::ScriptVar::Type::Integer:
                if (!var.IsArray())
                    return var.GetValue<int>();
                return QString("[0]=%1 ...").arg(var.GetArray<int>()[0]);
            case game::ScriptVar::Type::Color:
            {
                if (!var.IsArray())
                {
                    const auto& color = var.GetValue<game::Color4f>();
                    return app::toString(base::ToHex(color));
                }
                const auto& color = var.GetArray<game::Color4f>()[0];
                return app::toString("[0]=%1 ...", base::ToHex(color));
            }

            case game::ScriptVar::Type::Vec2: {
                if (!var.IsArray()) {
                    const auto& val = var.GetValue<glm::vec2>();
                    return QString("[%1,%2]")
                            .arg(QString::number(val.x, 'f', 2))
                            .arg(QString::number(val.y, 'f', 2));
                } else {
                    const auto& val = var.GetArray<glm::vec2>()[0];
                    return QString("[0]=[%1,%2] ...")
                            .arg(QString::number(val.x, 'f', 2))
                            .arg(QString::number(val.y, 'f', 2));
                }
            }
            case game::ScriptVar::Type::Vec3: {
                if (!var.IsArray()) {
                    const auto& val = var.GetValue<glm::vec3>();
                    return QString("[%1,%2,%3]")
                            .arg(QString::number(val.x, 'f', 2))
                            .arg(QString::number(val.y, 'f', 2))
                            .arg(QString::number(val.z, 'f', 2));
                } else {
                    const auto& val = var.GetArray<glm::vec3>()[0];
                    return QString("[0]=[%1,%2,%3] ...")
                            .arg(QString::number(val.x, 'f', 2))
                            .arg(QString::number(val.y, 'f', 2))
                            .arg(QString::number(val.z, 'f', 2));
                }
            }
            case game::ScriptVar::Type::Vec4: {
                if (!var.IsArray()) {
                    const auto& val = var.GetValue<glm::vec4>();
                    return QString("[%1,%2,%3,%4]")
                            .arg(QString::number(val.x, 'f', 2))
                            .arg(QString::number(val.y, 'f', 2))
                            .arg(QString::number(val.z, 'f', 2))
                            .arg(QString::number(val.w, 'f', 2));
                } else {
                    const auto& val = var.GetArray<glm::vec4>()[0];
                    return QString("[0]=[%1,%2,%3,%4] ...")
                            .arg(QString::number(val.x, 'f', 2))
                            .arg(QString::number(val.y, 'f', 2))
                            .arg(QString::number(val.z, 'f', 2))
                            .arg(QString::number(val.w, 'f', 2));
                }
            }
            case game::ScriptVar::Type::EntityNodeReference:
                if (!var.IsArray()) {
                    return "Nil";
                } else {
                    return "[0]=Nil ...";
                }
                break;
            case game::ScriptVar::Type::EntityReference:
                if (!var.IsArray()) {
                    const auto& val = var.GetValue<game::ScriptVar::EntityReference>();
                    if (const auto* node = mState.scene->FindPlacementById(val.id))
                        return app::FromUtf8(node->GetName());
                    return "Nil";
                } else {
                    const auto& val = var.GetArray<game::ScriptVar::EntityReference>()[0];
                    if (const auto* node = mState.scene->FindPlacementById(val.id))
                        return QString("[0]=%1 ...").arg(app::FromUtf8(node->GetName()));
                    return "[0]=Nil ...";
                }
                break;
            case game::ScriptVar::Type::MaterialReference:
                if (!var.IsArray()) {
                    const auto& val = var.GetValue<game::ScriptVar::MaterialReference>();
                    if (const auto& material = mState.workspace->FindMaterialClassById(val.id))
                        return toString(material->GetName());
                    return "Nil";
                } else {
                    const auto& val = var.GetArray<game::ScriptVar::MaterialReference>()[0];
                    if (const auto& material = mState.workspace->FindMaterialClassById(val.id))
                        return toString("[0]=%1 ...", material->GetName());
                    return "[0]=Nil ...";
                }
                break;
        }
        BUG("Unknown ScriptVar type.");
        return QVariant();
    }
private:
    SceneWidget::State& mState;
};

class SceneWidget::PlaceEntityTool : public MouseTool
{
public:
    PlaceEntityTool(SceneWidget::State& state, std::shared_ptr<const game::EntityClass> klass, bool snap, unsigned grid)
      : mState(state)
      , mClass(klass)
      , mSnapToGrid(snap)
      , mGridSize(grid)
    {
        mEntityIds = mState.workspace->ListUserDefinedEntityIds();
        for (mCurrentEntityIdIndex=0; mCurrentEntityIdIndex<mEntityIds.size(); ++mCurrentEntityIdIndex)
        {
            if (mEntityIds[mCurrentEntityIdIndex] == app::FromUtf8(klass->GetId()))
                break;
        }
    }
    PlaceEntityTool(SceneWidget::State& state, bool snap, unsigned grid)
        : mState(state)
        , mSnapToGrid(snap)
        , mGridSize(grid)
    {
        mEntityIds = mState.workspace->ListUserDefinedEntityIds();
        mClass = mState.workspace->GetEntityClassById(mEntityIds[0]);
        for (unsigned i=0; i<mEntityIds.size(); ++i)
        {
            if (mEntityIds[i] == mState.last_placed_entity)
            {
                mCurrentEntityIdIndex = i;
                mClass = mState.workspace->GetEntityClassById(mEntityIds[i]);
                break;
            }
        }
    }
    virtual void Render(gfx::Painter& painter, gfx::Painter& scene_painter) const override
    {
        const auto& rect = mClass->GetBoundingRect();
        const auto width = rect.GetWidth();
        const auto height = rect.GetHeight();
        const auto right = rect.GetX() + width;
        const auto bottom = rect.GetY() + height;

        gfx::Device* device = painter.GetDevice();
        gfx::Transform model;
        model.Translate(mWorldPos.x, mWorldPos.y);
        mState.renderer.Draw(*mClass, *device, model, nullptr);

        const auto& pos = engine::ProjectPoint(scene_painter.GetProjMatrix(),
                                               scene_painter.GetViewMatrix(),
                                               painter.GetProjMatrix(),
                                               painter.GetViewMatrix(),
                                               glm::vec3(mWorldPos.x + right + 10.0f,
                                                         mWorldPos.y + bottom + 10.0f, 0.0f));
        ShowMessage(mClass->GetName(), gfx::FRect(pos.x, pos.y, 200.0f, 20.0f), painter);
    }
    virtual void MouseMove(const MouseEvent& mickey, gfx::Transform&) override
    {
        mWorldPos = mickey.MapToPlane();
    }
    virtual void MousePress(const MouseEvent& mickey, gfx::Transform& view) override
    {
    }
    virtual bool MouseRelease(const MouseEvent& mickey, gfx::Transform& view) override
    {
        const auto button = mickey->button();
        if (button != Qt::LeftButton)
            return false;

        bool snap = mSnapToGrid;

        // allow control modifier to be used to toggle
        // snap to grid for this placement.
        if (mickey->modifiers() & Qt::ControlModifier)
            snap = !snap;

        if (snap)
        {
            mWorldPos.x = std::round(mWorldPos.x / mGridSize) * mGridSize;
            mWorldPos.y = std::round(mWorldPos.y / mGridSize) * mGridSize;
        }

        const auto name = CreateName();
        game::EntityPlacement node;
        node.SetEntity(mClass);
        node.SetName(name);
        node.SetScale(glm::vec2(1.0f, 1.0f));
        node.SetTranslation(glm::vec2(mWorldPos.x, mWorldPos.y));
        // leave this empty for the class default to take place.
        // node.SetIdleAnimationId(mClass->GetIdleTrackId());
        auto* child = mState.scene->PlaceEntity(std::move(node));
        mState.scene->LinkChild(nullptr, child);
        mState.view->Rebuild();
        mState.view->SelectItemById(child->GetId());
        mState.last_placed_entity = app::FromUtf8(mClass->GetId());
        DEBUG("Added new entity '%1'", name);
        // return false to indicate that another object can be placed.
        // in fact object placement continues until it's cancelled.
        // this makes it quite convenient to place multiple objects
        // in rapid succession.
        return false;
    }
    void SelectNextEntity()
    {
        mCurrentEntityIdIndex = (mCurrentEntityIdIndex + 1) % mEntityIds.size();
        mClass = mState.workspace->GetEntityClassById(mEntityIds[mCurrentEntityIdIndex]);
    }
    void SelectPrevEntity()
    {
        mCurrentEntityIdIndex = mCurrentEntityIdIndex > 0 ? mCurrentEntityIdIndex - 1 : mEntityIds.size() - 1;
        mClass = mState.workspace->GetEntityClassById(mEntityIds[mCurrentEntityIdIndex]);
    }
    void SetWorldPos(const glm::vec2& pos)
    { mWorldPos = pos; }
private:
    std::string CreateName() const
    {
        QString name = mState.workspace->MapResourceIdToName(mEntityIds[mCurrentEntityIdIndex]);
        for (size_t i=0; i<10000; ++i)
        {
            QString suggestion = QString("%1_%2").arg(name).arg(i);
            if (mState.scene->FindPlacementByName(app::ToUtf8(suggestion)) == nullptr)
                return app::ToUtf8(suggestion);
        }
        return "???";
    }
private:
    SceneWidget::State& mState;
    // the current entity position in scene coordinates of the placement
    // based on the mouse position at the time.
    glm::vec2 mWorldPos;
    // entity class for the item we're going to add to scene.
    std::shared_ptr<const game::EntityClass> mClass;
    // true if we want the x,y coords to be aligned on grid size units.
    bool mSnapToGrid = false;
    // the grid size in scene units to align the object onto.
    unsigned mGridSize = 0;
    // the list of of entity ids currently available for cycling through.
    QStringList mEntityIds;
    // the current index into the mEntityIds list.
    unsigned mCurrentEntityIdIndex = 0;
};

SceneWidget::SceneWidget(app::Workspace* workspace) : mUndoStack(3)
{
    DEBUG("Create SceneWidget");

    mState.scene = std::make_shared<game::SceneClass>();

    mRenderTree.reset(new TreeModel(*mState.scene));
    mScriptVarModel.reset(new ScriptVarModel(mState));

    mUI.setupUi(this);
    mUI.scriptVarList->setModel(mScriptVarModel.get());
    QHeaderView* verticalHeader = mUI.scriptVarList->verticalHeader();
    verticalHeader->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader->setDefaultSectionSize(16);
    mUI.tree->SetModel(mRenderTree.get());
    mUI.tree->Rebuild();
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(false);
    mUI.widget->onZoomIn        = std::bind(&SceneWidget::ZoomIn, this);
    mUI.widget->onZoomOut       = std::bind(&SceneWidget::ZoomOut, this);
    mUI.widget->onMouseMove     = std::bind(&SceneWidget::MouseMove, this, std::placeholders::_1);
    mUI.widget->onMousePress    = std::bind(&SceneWidget::MousePress, this, std::placeholders::_1);
    mUI.widget->onMouseRelease  = std::bind(&SceneWidget::MouseRelease, this, std::placeholders::_1);
    mUI.widget->onMouseWheel    = std::bind(&SceneWidget::MouseWheel, this, std::placeholders::_1);
    mUI.widget->onMouseDoubleClick = std::bind(&SceneWidget::MouseDoubleClick, this, std::placeholders::_1);
    mUI.widget->onKeyPress      = std::bind(&SceneWidget::KeyPress, this, std::placeholders::_1);
    mUI.widget->onPaintScene    = std::bind(&SceneWidget::PaintScene, this, std::placeholders::_1,
                                            std::placeholders::_2);
    // the menu for adding entities in the scene.
    mEntities = new QMenu(this);
    mEntities->menuAction()->setIcon(QIcon("level:entity.png"));
    mEntities->menuAction()->setText("Place Entity");

    mState.scene->SetName("My Scene");
    mState.workspace = workspace;
    mState.renderer.SetClassLibrary(workspace);
    mState.renderer.SetEditingMode(true);
    mState.renderer.SetName("SceneWidgetRenderer/" + mState.scene->GetId());
    mState.view = mUI.tree;
    mOriginalHash = mState.scene->GetHash();

    // connect tree widget signals
    connect(mUI.tree, &TreeWidget::currentRowChanged, this, &SceneWidget::TreeCurrentNodeChangedEvent);
    connect(mUI.tree, &TreeWidget::dragEvent,  this, &SceneWidget::TreeDragEvent);
    connect(mUI.tree, &TreeWidget::clickEvent, this, &SceneWidget::TreeClickEvent);
    // connect workspace signals for resource management
    connect(workspace, &app::Workspace::ResourceAdded,   this, &SceneWidget::ResourceAdded);
    connect(workspace, &app::Workspace::ResourceRemoved, this, &SceneWidget::ResourceRemoved);
    connect(workspace, &app::Workspace::ResourceUpdated, this, &SceneWidget::ResourceUpdated);

    PopulateFromEnum<game::SceneClass::SpatialIndex>(mUI.cmbSpatialIndex);
    PopulateFromEnum<engine::GameView::EnumValue>(mUI.cmbPerspective);
    PopulateFromEnum<GridDensity>(mUI.cmbGrid);
    SetValue(mUI.cmbGrid, GridDensity::Grid50x50);
    SetValue(mUI.cmbPerspective, engine::GameView::AxisAligned);
    SetValue(mUI.zoom, 1.0f);
    SetValue(mUI.ID, mState.scene->GetId());
    SetValue(mUI.name, mState.scene->GetName());
    SetVisible(mUI.transform, false);

    RebuildMenus();
    RebuildCombos();

    DisplaySceneProperties();
    DisplayCurrentNodeProperties();
    DisplayCurrentCameraLocation();
    setWindowTitle("My Scene");
}

SceneWidget::SceneWidget(app::Workspace* workspace, const app::Resource& resource)
  : SceneWidget(workspace)
{
    DEBUG("Editing scene '%1'", resource.GetName());
    const game::SceneClass* content = nullptr;
    resource.GetContent(&content);

    *mState.scene = *content;
    mOriginalHash = mState.scene->GetHash();
    mScriptVarModel->Reset();

    GetUserProperty(resource, "zoom", mUI.zoom);
    GetUserProperty(resource, "grid", mUI.cmbGrid);
    GetUserProperty(resource, "snap", mUI.chkSnap);
    GetUserProperty(resource, "perspective", mUI.cmbPerspective);
    GetUserProperty(resource, "show_origin", mUI.chkShowOrigin);
    GetUserProperty(resource, "show_grid", mUI.chkShowGrid);
    GetUserProperty(resource, "show_viewport", mUI.chkShowViewport);
    GetUserProperty(resource, "show_map", mUI.chkShowMap);
    GetUserProperty(resource, "widget", mUI.widget);
    GetUserProperty(resource, "camera_scale_x", mUI.scaleX);
    GetUserProperty(resource, "camera_scale_y", mUI.scaleY);
    GetUserProperty(resource, "camera_rotation", mUI.rotation);
    GetUserProperty(resource, "quadtree_max_items", mUI.spQuadMaxItems);
    GetUserProperty(resource, "quadtree_max_levels", mUI.spQuadMaxLevels);
    GetUserProperty(resource, "densegrid_num_rows", mUI.spDenseGridRows);
    GetUserProperty(resource, "densegrid_num_cols", mUI.spDenseGridCols);
    GetUserProperty(resource, "left_boundary", mUI.spinLeftBoundary);
    GetUserProperty(resource, "right_boundary", mUI.spinRightBoundary);
    GetUserProperty(resource, "top_boundary", mUI.spinTopBoundary);
    GetUserProperty(resource, "bottom_boundary", mUI.spinBottomBoundary);
    GetUserProperty(resource, "camera_offset_x", &mState.camera_offset_x);
    GetUserProperty(resource, "camera_offset_y", &mState.camera_offset_y);
    GetUserProperty(resource, "variables_group", mUI.sceneVariablesGroup);
    GetUserProperty(resource, "bounds_group", mUI.sceneBoundsGroup);
    GetUserProperty(resource, "index_group", mUI.sceneIndexGroup);
    GetUserProperty(resource, "bloom_group", mUI.bloomGroup);
    GetUserProperty(resource, "bloom_threshold", &mBloom.threshold);
    GetUserProperty(resource, "bloom_red",       &mBloom.red);
    GetUserProperty(resource, "bloom_green",     &mBloom.green);
    GetUserProperty(resource, "bloom_blue",      &mBloom.blue);
    GetUserProperty(resource, "main_splitter", mUI.mainSplitter);
    GetUserProperty(resource, "right_splitter", mUI.rightSplitter);

    UpdateResourceReferences();
    DisplayCurrentNodeProperties();
    DisplaySceneProperties();
    DisplayCurrentCameraLocation();

    mRenderTree.reset(new TreeModel(*mState.scene));
    mUI.tree->SetModel(mRenderTree.get());
    mUI.tree->Rebuild();
}

SceneWidget::~SceneWidget()
{
    DEBUG("Destroy SceneWidget");
}

QString SceneWidget::GetId() const
{
    return GetValue(mUI.ID);
}

void SceneWidget::InitializeSettings(const UISettings& settings)
{
    SetValue(mUI.chkSnap,         settings.snap_to_grid);
    SetValue(mUI.chkShowViewport, settings.show_viewport);
    SetValue(mUI.chkShowOrigin,   settings.show_origin);
    SetValue(mUI.chkShowGrid,     settings.show_grid);
    SetValue(mUI.cmbGrid,         settings.grid);
    SetValue(mUI.zoom,            settings.zoom);
}

void SceneWidget::AddActions(QToolBar& bar)
{
    bar.addAction(mUI.actionPlay);
    bar.addAction(mUI.actionPause);
    bar.addAction(mUI.actionStop);
    bar.addSeparator();
    bar.addAction(mUI.actionPreview);
    bar.addSeparator();
    bar.addAction(mUI.actionSave);
    bar.addSeparator();
    bar.addAction(mEntities->menuAction());
    bar.addSeparator();
    bar.addAction(mUI.actionFind);
}
void SceneWidget::AddActions(QMenu& menu)
{
    menu.addAction(mUI.actionPlay);
    menu.addAction(mUI.actionPause);
    menu.addAction(mUI.actionStop);
    menu.addSeparator();
    menu.addAction(mUI.actionPreview);
    menu.addSeparator();
    menu.addAction(mUI.actionSave);
    menu.addSeparator();
    menu.addAction(mEntities->menuAction());
    menu.addAction(mUI.actionFind);
}

bool SceneWidget::SaveState(Settings& settings) const
{
    data::JsonObject json;
    mState.scene->IntoJson(json);
    settings.SetValue("Scene", "content", json);
    settings.SetValue("Scene", "hash", mOriginalHash);
    settings.SetValue("Scene", "camera_offset_x", mState.camera_offset_x);
    settings.SetValue("Scene", "camera_offset_y", mState.camera_offset_y);
    settings.SetValue("Scene", "bloom_threshold", mBloom.threshold);
    settings.SetValue("Scene", "bloom_red",   mBloom.red);
    settings.SetValue("Scene", "bloom_green", mBloom.green);
    settings.SetValue("Scene", "bloom_blue",  mBloom.blue);
    settings.SaveWidget("Scene", mUI.scaleX);
    settings.SaveWidget("Scene", mUI.scaleY);
    settings.SaveWidget("Scene", mUI.rotation);
    settings.SaveWidget("Scene", mUI.chkShowOrigin);
    settings.SaveWidget("Scene", mUI.chkShowGrid);
    settings.SaveWidget("Scene", mUI.chkShowViewport);
    settings.SaveWidget("Scene", mUI.chkSnap);
    settings.SaveWidget("Scene", mUI.cmbGrid);
    settings.SaveWidget("Scene", mUI.chkShowMap);
    settings.SaveWidget("Scene", mUI.zoom);
    settings.SaveWidget("Scene", mUI.widget);
    settings.SaveWidget("Scene", mUI.sceneVariablesGroup);
    settings.SaveWidget("Scene", mUI.sceneBoundsGroup);
    settings.SaveWidget("Scene", mUI.sceneIndexGroup);
    settings.SaveWidget("Scene", mUI.bloomGroup);
    settings.SaveWidget("Scene", mUI.cmbPerspective);
    settings.SaveWidget("Scene", mUI.mainSplitter);
    settings.SaveWidget("Scene", mUI.rightSplitter);
    return true;
}
bool SceneWidget::LoadState(const Settings& settings)
{
    data::JsonObject json;
    settings.GetValue("Scene", "content", &json);
    settings.GetValue("Scene", "hash", &mOriginalHash);
    settings.GetValue("Scene", "camera_offset_x", &mState.camera_offset_x);
    settings.GetValue("Scene", "camera_offset_y", &mState.camera_offset_y);
    settings.GetValue("Scene", "bloom_threshold", &mBloom.threshold);
    settings.GetValue("Scene", "bloom_red",   &mBloom.red);
    settings.GetValue("Scene", "bloom_green", &mBloom.green);
    settings.GetValue("Scene", "bloom_blue",  &mBloom.blue);
    settings.LoadWidget("Scene", mUI.scaleX);
    settings.LoadWidget("Scene", mUI.scaleY);
    settings.LoadWidget("Scene", mUI.rotation);
    settings.LoadWidget("Scene", mUI.chkShowOrigin);
    settings.LoadWidget("Scene", mUI.chkShowGrid);
    settings.LoadWidget("Scene", mUI.chkShowViewport);
    settings.LoadWidget("Scene", mUI.chkSnap);
    settings.LoadWidget("Scene", mUI.cmbGrid);
    settings.LoadWidget("Scene", mUI.chkShowMap);
    settings.LoadWidget("Scene", mUI.zoom);
    settings.LoadWidget("Scene", mUI.widget);
    settings.LoadWidget("Scene", mUI.sceneVariablesGroup);
    settings.LoadWidget("Scene", mUI.sceneBoundsGroup);
    settings.LoadWidget("Scene", mUI.sceneIndexGroup);
    settings.LoadWidget("Scene", mUI.bloomGroup);
    settings.LoadWidget("Scene", mUI.cmbPerspective);
    settings.LoadWidget("Scene", mUI.mainSplitter);
    settings.LoadWidget("Scene", mUI.rightSplitter);

    if (!mState.scene->FromJson(json))
        WARN("Failed to restore scene state.");

    UpdateResourceReferences();
    DisplaySceneProperties();
    DisplayCurrentNodeProperties();
    DisplayCurrentCameraLocation();

    mScriptVarModel->Reset();
    mRenderTree.reset(new TreeModel(*mState.scene));
    mUI.tree->SetModel(mRenderTree.get());
    mUI.tree->Rebuild();
    return true;
}

bool SceneWidget::CanTakeAction(Actions action, const Clipboard* clipboard) const
{
    switch (action)
    {
        case Actions::CanPaste:
            if (clipboard->GetType() == "application/json/scene_node")
                return true;
            return false;
        case Actions::CanCopy:
        case Actions::CanCut:
            if (GetCurrentNode())
                return true;
            return false;
        case Actions::CanUndo:
            return mUndoStack.size() > 1;
        case Actions::CanZoomIn:
            return CanZoomIn(mUI.zoom);
        case Actions::CanZoomOut:
            return CanZoomOut(mUI.zoom);
        case Actions::CanReloadShaders:
        case Actions::CanReloadTextures:
        case Actions::CanScreenshot:
            return true;
    }
    return false;
}

void SceneWidget::Cut(Clipboard& clipboard)
{
    if (auto* node = GetCurrentNode())
    {
        data::JsonObject json;
        const auto& tree = mState.scene->GetRenderTree();
        game::RenderTreeIntoJson(tree, [](data::Writer& writer, const auto* node) {
            node->IntoJson(writer);
        }, json, node);

        clipboard.Clear();
        clipboard.SetType("application/json/scene_node");
        clipboard.SetText(json.ToString());
        NOTE("Copied JSON to application clipboard.");

        mState.scene->DeletePlacement(node);
        mUI.tree->Rebuild();
        mUI.tree->ClearSelection();
    }
}
void SceneWidget::Copy(Clipboard& clipboard) const
{
    if (const auto* node = GetCurrentNode())
    {
        data::JsonObject json;
        const auto& tree = mState.scene->GetRenderTree();
        game::RenderTreeIntoJson(tree, [](data::Writer& writer, const auto* node) {
            node->IntoJson(writer);
         }, json, node);

        clipboard.Clear();
        clipboard.SetType("application/json/scene_node");
        clipboard.SetText(json.ToString());
        NOTE("Copied JSON to application clipboard.");
    }
}
void SceneWidget::Paste(const Clipboard& clipboard)
{
    if (clipboard.IsEmpty())
    {
        NOTE("Clipboard is empty.");
        return;
    }
    else if (clipboard.GetType() != "application/json/scene_node")
    {
        NOTE("No scene node JSON data found in clipboard.");
        return;
    }

    data::JsonObject json;
    auto [success, _] = json.ParseString(clipboard.GetText());
    if (!success)
    {
        NOTE("Clipboard JSON parse failed.");
        return;
    }

    // use a temporary vector in case there's a problem
    std::vector<std::unique_ptr<game::EntityPlacement>> nodes;
    bool error = false;
    game::SceneClass::RenderTree tree;
    game::RenderTreeFromJson(tree, [&nodes, &error](const data::Reader& data) {
        game::EntityPlacement ret;
        if (ret.FromJson(data)) {
            auto node = std::make_unique<game::EntityPlacement>(ret.Clone());
            node->SetName(base::FormatString("Copy of %1", ret.GetName()));
            nodes.push_back(std::move(node));
            return nodes.back().get();
        }
        error = true;
        return (game::EntityPlacement*)nullptr;
    }, json);
    if (error || nodes.empty())
    {
        NOTE("No render tree JSON found.");
        return;
    }

    // if the mouse pointer is not within the widget then adjust
    // the paste location to the center of the widget.
    QPoint mickey = mUI.widget->mapFromGlobal(QCursor::pos());
    if (mickey.x() < 0 || mickey.x() > mUI.widget->width() ||
        mickey.y() < 0 || mickey.y() > mUI.widget->height())
        mickey = QPoint(mUI.widget->width() * 0.5, mUI.widget->height() * 0.5);

    const auto& world_pos = MapWindowCoordinateToWorld(mUI, mState, mickey);

    auto* paste_root = nodes[0].get();
    paste_root->SetTranslation(world_pos);
    tree.LinkChild(nullptr, paste_root);

    // if we got this far, nodes should contain the nodes to be added
    // into the scene and tree should contain their hierarchy.
    for (auto& node : nodes)
    {
        // moving the unique ptr means that node address stays the same
        // thus the tree is still valid!
        node->SetEntity(mState.workspace->FindEntityClassById(node->GetEntityId()));
        mState.scene->PlaceEntity(std::move(node));
    }
    nodes.clear();
    // walk the tree and link the nodes into the scene.
    tree.PreOrderTraverseForEach([&nodes, this, &tree](game::EntityPlacement* node) {
        if (node == nullptr)
            return;
        auto* parent = tree.GetParent(node);
        mState.scene->LinkChild(parent, node);
    });

    mUI.tree->Rebuild();
    mUI.tree->SelectItemById(paste_root->GetId());
}

void SceneWidget::Undo()
{
    if (mUndoStack.size() <= 1)
    {
        NOTE("No undo available.");
        return;
    }
    // if the timer has run the top of the undo stack
    // is the same copy as the actual scene object.
    if (mUndoStack.back().GetHash() == mState.scene->GetHash())
        mUndoStack.pop_back();

    *mState.scene = mUndoStack.back();
    mState.view->Rebuild();
    mUndoStack.pop_back();
    mScriptVarModel->Reset();
    DisplayCurrentNodeProperties();
    DisplaySceneProperties();
    NOTE("Undo!");
}

void SceneWidget::Save()
{
    on_actionSave_triggered();
}

void SceneWidget::ZoomIn()
{
    const float value = GetValue(mUI.zoom);
    SetValue(mUI.zoom, value + 0.1f);
}
void SceneWidget::ZoomOut()
{
    const float value = GetValue(mUI.zoom);
    SetValue(mUI.zoom, value - 0.1f);
}
void SceneWidget::ReloadShaders()
{
    mUI.widget->reloadShaders();
}
void SceneWidget::ReloadTextures()
{
    mUI.widget->reloadTextures();
}
void SceneWidget::Shutdown()
{
    if (mPreview)
    {
        mPreview->Shutdown();
        mPreview->close();
        mPreview.reset();
    }

    mUI.widget->dispose();
}
void SceneWidget::Update(double secs)
{
    if (mPlayState == PlayState::Playing)
    {
        mState.renderer.Update(*mState.scene, mSceneTime, secs);
        mSceneTime += secs;
    }
    mCurrentTime += secs;

    mAnimator.Update(mUI, mState);

}
void SceneWidget::Render()
{
    // call for the widget to paint, it will set its own OpenGL context on this thread
    // and everything should be fine.
    mUI.widget->triggerPaint();
}

void SceneWidget::RunGameLoopOnce()
{
    // WARNING: Calling into PlayWindow will change the OpenGL context on *this* thread
    if (!mPreview)
        return;

    if (mPreview->IsClosed())
    {
        mPreview->SaveState("preview_window");
        mPreview->Shutdown();
        mPreview->close();
        mPreview.reset();
    }
    else
    {
        mPreview->RunGameLoopOnce();
    }
}

bool SceneWidget::HasUnsavedChanges() const
{
    if (mOriginalHash == mState.scene->GetHash())
        return false;
    return true;
}

bool SceneWidget::OnEscape()
{
    if (mCurrentTool)
    {
        mCurrentTool.reset();
        UncheckPlacementActions();
    }
    else if (mUI.tree->GetSelectedItem())
    {
        mUI.tree->ClearSelection();
    }
    else
    {
        on_btnViewReset_clicked();
    }
    return true;
}

void SceneWidget::Refresh()
{
    if (mPreview && !mPreview->IsClosed())
    {
        mPreview->NonGameTick();
    }

    // don't take an undo snapshot while the mouse tool is in
    // action.
    if (mCurrentTool)
        return;
    // don't take an undo snapshot while the node name is being
    // edited.
    if (mUI.nodeName->hasFocus())
        return;

    if (mUndoStack.empty())
    {
        mUndoStack.push_back(*mState.scene);
    }
    const auto curr_hash = mState.scene->GetHash();
    const auto undo_hash = mUndoStack.back().GetHash();
    if (curr_hash != undo_hash)
    {
        mUndoStack.push_back(*mState.scene);
        DEBUG("Created undo copy. stack size: %1", mUndoStack.size());
    }
}

bool SceneWidget::GetStats(Stats* stats) const
{
    stats->time  = mSceneTime;
    stats->graphics.valid = true;
    stats->graphics.fps   = mUI.widget->getCurrentFPS();
    stats->graphics.vsync = mUI.widget->haveVSYNC();
    const auto& dev_stats = mUI.widget->getDeviceResourceStats();
    stats->device.static_vbo_mem_alloc    = dev_stats.static_vbo_mem_alloc;
    stats->device.static_vbo_mem_use      = dev_stats.static_vbo_mem_use;
    stats->device.dynamic_vbo_mem_alloc   = dev_stats.dynamic_vbo_mem_alloc;
    stats->device.dynamic_vbo_mem_use     = dev_stats.dynamic_vbo_mem_use;
    stats->device.streaming_vbo_mem_use   = dev_stats.streaming_vbo_mem_use;
    stats->device.streaming_vbo_mem_alloc = dev_stats.streaming_vbo_mem_alloc;
    return true;
}

QImage SceneWidget::TakeScreenshot() const
{
    return mUI.widget->TakeSreenshot();
}

void SceneWidget::on_name_textChanged(const QString&)
{
    mState.scene->SetName(GetValue(mUI.name));
}

void SceneWidget::on_cmbScripts_currentIndexChanged(int)
{
    mState.scene->SetScriptFileId(GetItemId(mUI.cmbScripts));
    SetEnabled(mUI.btnEditScript, true);
}

void SceneWidget::on_cmbTilemaps_currentIndexChanged(int)
{
    mState.scene->SetTilemapId(GetItemId(mUI.cmbTilemaps));
    SetEnabled(mUI.btnEditMap, true);
}

void SceneWidget::on_cmbSpatialIndex_currentIndexChanged(int)
{
    // Set the values based on what is currently in UI
    SetSpatialIndexParams();
    // then display appropriately (enable/disable the right stuff)
    DisplaySceneProperties();
}

void SceneWidget::on_spQuadMaxLevels_valueChanged(int)
{
    SetSpatialIndexParams();
}
void SceneWidget::on_spQuadMaxItems_valueChanged(int)
{
    SetSpatialIndexParams();
}

void SceneWidget::on_spDenseGridRows_valueChanged(int)
{
    SetSpatialIndexParams();
}
void SceneWidget::on_spDenseGridCols_valueChanged(int)
{
    SetSpatialIndexParams();
}
void SceneWidget::on_spinLeftBoundary_valueChanged(bool has_value, double value)
{
    SetSceneBoundary();
}
void SceneWidget::on_spinRightBoundary_valueChanged(bool has_value, double value)
{
    SetSceneBoundary();
}
void SceneWidget::on_spinTopBoundary_valueChanged(bool has_value, double value)
{
    SetSceneBoundary();
}
void SceneWidget::on_spinBottomBoundary_valueChanged(bool has_value, double value)
{
    SetSceneBoundary();
}

void SceneWidget::on_chkEnableBloom_stateChanged(int)
{
    if (GetValue(mUI.chkEnableBloom))
    {
        mState.scene->SetBloom(mBloom);
    }
    else
    {
        if (auto* bloom = mState.scene->GetBloom())
            mBloom = *bloom;
        mState.scene->ResetBloom();
    }
    DisplaySceneProperties();
}

void SceneWidget::on_bloomThresholdSpin_valueChanged(double value)
{
    if (auto* bloom = mState.scene->GetBloom())
    {
        bloom->threshold = value;
        SetValue(mUI.bloomThresholdSlide, value);
    }
}

void SceneWidget::on_bloomRSpin_valueChanged(double value)
{
    if (auto* bloom = mState.scene->GetBloom())
    {
        bloom->red = value;
        SetValue(mUI.bloomRSlide, value);
    }
}
void SceneWidget::on_bloomGSpin_valueChanged(double value)
{
    if (auto* bloom = mState.scene->GetBloom())
    {
        bloom->green = value;
        SetValue(mUI.bloomGSlide, value);
    }
}
void SceneWidget::on_bloomBSpin_valueChanged(double value)
{
    if (auto* bloom = mState.scene->GetBloom())
    {
        bloom->blue = value;
        SetValue(mUI.bloomBSlide, value);
    }
}

void SceneWidget::on_bloomThresholdSlide_valueChanged(double value)
{
    if (auto* bloom = mState.scene->GetBloom())
    {
        bloom->threshold = value;
        SetValue(mUI.bloomThresholdSpin, value);
    }
}
void SceneWidget::on_bloomRSlide_valueChanged(double value)
{
    if (auto* bloom = mState.scene->GetBloom())
    {
        bloom->red = value;
        SetValue(mUI.bloomRSpin, value);
    }
}
void SceneWidget::on_bloomGSlide_valueChanged(double value)
{
    if (auto* bloom = mState.scene->GetBloom())
    {
        bloom->green = value;
        SetValue(mUI.bloomGSpin, value);
    }
}
void SceneWidget::on_bloomBSlide_valueChanged(double value)
{
    if (auto* bloom = mState.scene->GetBloom())
    {
        bloom->blue = value;
        SetValue(mUI.bloomBSpin, value);
    }
}


void SceneWidget::on_actionPlay_triggered()
{
    mPlayState = PlayState::Playing;
    mUI.actionPlay->setEnabled(false);
    mUI.actionPause->setEnabled(true);
    mUI.actionStop->setEnabled(true);
}
void SceneWidget::on_actionPause_triggered()
{
    mPlayState = PlayState::Paused;
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(true);
}
void SceneWidget::on_actionStop_triggered()
{
    mSceneTime = 0.0f;
    mPlayState = PlayState::Stopped;
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(false);
}
void SceneWidget::on_actionSave_triggered()
{
    if (!MustHaveInput(mUI.name))
        return;

    app::SceneResource resource(mState.scene, GetValue(mUI.name));
    SetUserProperty(resource, "camera_offset_x", mState.camera_offset_x);
    SetUserProperty(resource, "camera_offset_y", mState.camera_offset_y);
    SetUserProperty(resource, "camera_scale_x", mUI.scaleX);
    SetUserProperty(resource, "camera_scale_y", mUI.scaleY);
    SetUserProperty(resource, "camera_rotation", mUI.rotation);
    SetUserProperty(resource, "zoom", mUI.zoom);
    SetUserProperty(resource, "grid", mUI.cmbGrid);
    SetUserProperty(resource, "snap", mUI.chkSnap);
    SetUserProperty(resource, "perspective", mUI.cmbPerspective);
    SetUserProperty(resource, "show_origin", mUI.chkShowOrigin);
    SetUserProperty(resource, "show_grid", mUI.chkShowGrid);
    SetUserProperty(resource, "show_viewport", mUI.chkShowViewport);
    SetUserProperty(resource, "show_map", mUI.chkShowMap);
    SetUserProperty(resource, "widget", mUI.widget);
    SetUserProperty(resource, "quadtree_max_items", mUI.spQuadMaxItems);
    SetUserProperty(resource, "quadtree_max_levels", mUI.spQuadMaxLevels);
    SetUserProperty(resource, "densegrid_num_rows", mUI.spDenseGridRows);
    SetUserProperty(resource, "densegrid_num_cols", mUI.spDenseGridCols);
    SetUserProperty(resource, "left_boundary", mUI.spinLeftBoundary);
    SetUserProperty(resource, "right_boundary", mUI.spinRightBoundary);
    SetUserProperty(resource, "top_boundary", mUI.spinTopBoundary);
    SetUserProperty(resource, "bottom_boundary", mUI.spinBottomBoundary);
    SetUserProperty(resource, "variables_group", mUI.sceneVariablesGroup);
    SetUserProperty(resource, "bounds_group", mUI.sceneBoundsGroup);
    SetUserProperty(resource, "index_group", mUI.sceneIndexGroup);
    SetUserProperty(resource, "bloom_group", mUI.bloomGroup);
    SetUserProperty(resource, "bloom_threshold", mBloom.threshold);
    SetUserProperty(resource, "bloom_red",       mBloom.red);
    SetUserProperty(resource, "bloom_green",     mBloom.green);
    SetUserProperty(resource, "bloom_blue",      mBloom.blue);
    SetUserProperty(resource, "main_splitter", mUI.mainSplitter);
    SetUserProperty(resource, "right_splitter", mUI.rightSplitter);

    mState.workspace->SaveResource(resource);
    mOriginalHash = mState.scene->GetHash();
}

void SceneWidget::on_actionFind_triggered()
{
    DlgFindEntity dlg(this, *mState.scene);
    if (dlg.exec() == QDialog::Rejected)
        return;

    const auto* node = dlg.GetNode();
    if (node == nullptr)
        return;

    FindNode(node);
    mUI.tree->SelectItemById(node->GetId());
    mUI.widget->setFocus();
}

void SceneWidget::on_actionEditEntityClass_triggered()
{
    on_btnEditEntity_clicked();
}

void SceneWidget::on_actionPreview_triggered()
{
    if (mPreview)
    {
        mPreview->ActivateWindow();
    }
    else
    {
        auto preview = std::make_unique<PlayWindow>(*mState.workspace);
        preview->LoadState("preview_window", this);
        preview->ShowWithWAR();
        preview->LoadPreview(mState.scene);
        mPreview = std::move(preview);
    }
}

void SceneWidget::on_actionNodeEdit_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        auto klass = node->GetEntityClass();
        if (!klass)
            return;

        DlgEntity dlg(this, mState.workspace, *klass, *node);
        dlg.exec();

        node->ClearStaleScriptValues(*klass);
    }
}

void SceneWidget::on_actionNodeDelete_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        mState.scene->DeletePlacement(node);

        mUI.tree->Rebuild();
        mUI.tree->ClearSelection();
    }
}

void SceneWidget::on_actionNodeBreakLink_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        node->SetParentRenderTreeNodeId("");
        mState.scene->BreakChild(node);
        mState.scene->LinkChild(nullptr, node);
        mUI.tree->Rebuild();
    }
}

void SceneWidget::on_actionNodeDuplicate_triggered()
{
    if (const auto* node = GetCurrentNode())
    {
        auto* dupe = mState.scene->DuplicatePlacement(node);
        // update the translation for the parent of the new hierarchy
        // so that it's possible to tell it apart from the source of the copy.
        dupe->SetTranslation(node->GetTranslation() * 1.2f);

        mState.view->Rebuild();
        mState.view->SelectItemById(dupe->GetId());
    }
}

void SceneWidget::on_actionNodeMoveUpLayer_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        const int layer = node->GetLayer();
        node->SetLayer(layer + 1);
    }
    DisplayCurrentNodeProperties();
}
void SceneWidget::on_actionNodeMoveDownLayer_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        const int layer = node->GetLayer();
        node->SetLayer(layer - 1);
    }
    DisplayCurrentNodeProperties();
}

void SceneWidget::on_actionNodeFind_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        FindNode(node);
        mUI.tree->SelectItemById(node->GetId());
        mUI.widget->setFocus();
    }
}

void SceneWidget::on_actionEntityVarRef_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        std::vector<ResourceListItem> entities;
        std::vector<ResourceListItem> nodes;
        for (size_t i = 0; i < mState.scene->GetNumNodes(); ++i)
        {
            const auto& placement = mState.scene->GetPlacement(i);
            ResourceListItem item;
            item.name = placement.GetName();
            item.id   = placement.GetId();
            entities.push_back(std::move(item));
        }
        QString name = app::FromUtf8(node->GetName());
        name = name.replace(' ', '_');
        name = name.toLower();
        game::ScriptVar::EntityReference  ref;
        ref.id = node->GetId();

        game::ScriptVar var(app::ToUtf8(name), ref);
        var.SetPrivate(true);
        DlgScriptVar dlg(nodes, entities, mState.workspace->ListAllMaterials(),
                         this, var);
        if (dlg.exec() == QDialog::Rejected)
            return;

        mScriptVarModel->AddVariable(std::move(var));
        SetEnabled(mUI.btnEditScriptVar, true);
        SetEnabled(mUI.btnDeleteScriptVar, true);
    }
}

void SceneWidget::on_actionScriptVarAdd_triggered()
{
    on_btnNewScriptVar_clicked();
}
void SceneWidget::on_actionScriptVarDel_triggered()
{
    on_btnDeleteScriptVar_clicked();
}
void SceneWidget::on_actionScriptVarEdit_triggered()
{
    on_btnEditScriptVar_clicked();
}

void SceneWidget::on_btnEditScript_clicked()
{
    const auto& id = (QString)GetItemId(mUI.cmbScripts);
    if (id.isEmpty())
        return;
    emit OpenResource(id);
}

void SceneWidget::on_btnResetScript_clicked()
{
    mState.scene->ResetScriptFile();
    SetValue(mUI.cmbScripts, -1);
    SetEnabled(mUI.btnEditScript, false);
}
void SceneWidget::on_btnAddScript_clicked()
{
    app::Script script;
    // use the script ID as the file name so that we can
    // avoid naming clashes and always find the correct lua
    // file even if the entity is later renamed.
    const auto& uri  = app::toString("ws://lua/%1.lua", script.GetId());
    const auto& file = mState.workspace->MapFileToFilesystem(uri);
    const auto& name = GetValue(mUI.name);

    if (app::FileExists(file))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Question);
        msg.setWindowTitle(tr("File already exists"));
        msg.setText(tr("Overwrite existing script file?\n%1").arg(file));
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
        if (msg.exec() == QMessageBox::Cancel)
            return;
    }

    QString source = GenerateSceneScriptSource(name);

    QFile::FileError err_val = QFile::FileError::NoError;
    QString err_str;
    if (!app::WriteTextFile(file, source, &err_val, &err_str))
    {
        ERROR("Failed to write file. [file='%1', err_val=%2, err_str='%3']", file, err_val, err_str);
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowTitle("Error Occurred");
        msg.setText(tr("Failed to write the script file. [%1]").arg(err_str));
        msg.setStandardButtons(QMessageBox::Ok);
        msg.exec();
        return;
    }

    script.SetFileURI(uri);
    app::ScriptResource resource(script, name);
    mState.workspace->SaveResource(resource);
    mState.scene->SetScriptFileId(script.GetId());

    ScriptWidget* widget = new ScriptWidget(mState.workspace, resource);
    emit OpenNewWidget(widget);

    SetValue(mUI.cmbScripts, ListItemId(script.GetId()));
    SetEnabled(mUI.btnEditScript, true);
}

void SceneWidget::on_btnEditMap_clicked()
{
    const auto& id = (QString)GetItemId(mUI.cmbTilemaps);
    if (id.isEmpty())
        return;

    emit OpenResource(id);
}
void SceneWidget::on_btnAddMap_clicked()
{
    auto* widget = new TilemapWidget(mState.workspace);
    widget->Save();
    emit OpenNewWidget(widget);

    QString id = widget->GetId();
    mState.scene->SetTilemapId(app::ToUtf8(id));
    SetValue(mUI.cmbTilemaps, ListItemId(id));
    SetEnabled(mUI.btnEditMap, true);
}

void SceneWidget::on_btnResetMap_clicked()
{
    mState.scene->ResetTilemap();
    SetValue(mUI.cmbTilemaps, -1);
    SetEnabled(mUI.btnEditMap, false);
    mTilemap.reset();
}

void SceneWidget::on_btnNewScriptVar_clicked()
{
    std::vector<ResourceListItem> entities;
    std::vector<ResourceListItem> nodes;
    for (size_t i=0; i<mState.scene->GetNumNodes(); ++i)
    {
        const auto& node = mState.scene->GetPlacement(i);
        ResourceListItem item;
        item.name = node.GetName();
        item.id   = node.GetId();
        entities.push_back(std::move(item));
    }

    game::ScriptVar var("My_Var", std::string(""));
    var.SetPrivate(true);
    DlgScriptVar dlg(nodes, entities, mState.workspace->ListAllMaterials(),
                     this, var);
    if (dlg.exec() == QDialog::Rejected)
        return;

    mScriptVarModel->AddVariable(std::move(var));
    SetEnabled(mUI.btnEditScriptVar, true);
    SetEnabled(mUI.btnDeleteScriptVar, true);
}
void SceneWidget::on_btnEditScriptVar_clicked()
{
    auto items = mUI.scriptVarList->selectionModel()->selectedRows();
    if (items.isEmpty())
        return;

    std::vector<ResourceListItem> entities;
    std::vector<ResourceListItem> nodes;
    for (size_t i=0; i<mState.scene->GetNumNodes(); ++i)
    {
        const auto& node = mState.scene->GetPlacement(i);
        ResourceListItem item;
        item.name = node.GetName();
        item.id   = node.GetId();
        entities.push_back(std::move(item));
    }

    // single selection for now.
    const auto index = items[0];
    game::ScriptVar var = mState.scene->GetScriptVar(index.row());
    DlgScriptVar dlg(nodes, entities, mState.workspace->ListAllMaterials(),
                     this, var);
    if (dlg.exec() == QDialog::Rejected)
        return;

    mScriptVarModel->EditVariable(index.row(), std::move(var));
}
void SceneWidget::on_btnDeleteScriptVar_clicked()
{
    auto items = mUI.scriptVarList->selectionModel()->selectedRows();
    if (items.isEmpty())
        return;

    // single selection for now.
    const auto index = items[0];
    mScriptVarModel->DeleteVariable(index.row());
    const auto vars = mState.scene->GetNumScriptVars();
    SetEnabled(mUI.btnEditScriptVar, vars > 0);
    SetEnabled(mUI.btnDeleteScriptVar, vars > 0);
}

void SceneWidget::on_btnViewPlus90_clicked()
{
    mAnimator.Plus90(mUI, mState);
}

void SceneWidget::on_btnViewMinus90_clicked()
{
    mAnimator.Minus90(mUI, mState);
}

void SceneWidget::on_btnViewReset_clicked()
{
    mAnimator.Reset(mUI, mState);
    SetValue(mUI.scaleX,     1.0f);
    SetValue(mUI.scaleY,     1.0f);
}

void SceneWidget::on_btnMoreViewportSettings_clicked()
{
    const auto visible = mUI.transform->isVisible();
    SetVisible(mUI.transform, !visible);
    if (!visible)
        mUI.btnMoreViewportSettings->setArrowType(Qt::ArrowType::DownArrow);
    else mUI.btnMoreViewportSettings->setArrowType(Qt::ArrowType::UpArrow);
}

void SceneWidget::on_widgetColor_colorChanged(QColor color)
{
    mUI.widget->SetClearColor(ToGfx(color));
}

void SceneWidget::on_nodeName_textChanged(const QString& text)
{
    TreeWidget::TreeItem* item = mUI.tree->GetSelectedItem();
    if (item == nullptr)
        return;
    else if (!item->GetUserData())
        return;
    auto* node = static_cast<game::EntityPlacement*>(item->GetUserData());
    node->SetName(app::ToUtf8(text));
    item->SetText(text);
    mUI.tree->Update();
}

void SceneWidget::on_nodeEntity_currentIndexChanged(const QString& name)
{
    if (auto* node = GetCurrentNode())
    {
        auto klass = mState.workspace->GetEntityClassById(GetItemId(mUI.nodeEntity));
        node->SetEntity(klass);

        const auto visible_in_game = node->TestFlag(game::EntityPlacement::Flags::VisibleInGame);
        const auto visible_in_editor = node->TestFlag(game::EntityPlacement::Flags::VisibleInEditor);
        // reset the entity instance parameters to defaults since the
        // entity class has changed. only save the flags that are changed
        // through this editor UI.
        node->ResetEntityParams();
        node->SetFlag(game::EntityPlacement::Flags::VisibleInGame, visible_in_game);
        node->SetFlag(game::EntityPlacement::Flags::VisibleInEditor, visible_in_editor);
        NOTE("Entity parameters were reset to default.");
    }
}

void SceneWidget::on_nodeLayer_valueChanged(int layer)
{
    if (auto* node = GetCurrentNode())
    {
        node->SetLayer(layer);
    }
}

void SceneWidget::on_nodeLink_currentIndexChanged(const QString&)
{
    if (auto* node = GetCurrentNode())
    {
        const auto& id = mUI.nodeLink->currentData().toString();
        node->SetParentRenderTreeNodeId(app::ToUtf8(id));
    }
}

void SceneWidget::on_nodeIsVisible_stateChanged(int)
{
    if (auto* node = GetCurrentNode())
    {
        node->SetFlag(game::EntityPlacement::Flags::VisibleInGame, GetValue(mUI.nodeIsVisible));
    }
}
void SceneWidget::on_nodeTranslateX_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto translate = node->GetTranslation();
        translate.x = value;
        node->SetTranslation(translate);
    }
}
void SceneWidget::on_nodeTranslateY_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto translate = node->GetTranslation();
        translate.y = value;
        node->SetTranslation(translate);
    }
}
void SceneWidget::on_nodeScaleX_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto scale = node->GetScale();
        scale.x = value;
        node->SetScale(scale);
    }
}
void SceneWidget::on_nodeScaleY_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto scale = node->GetScale();
        scale.y = value;
        node->SetScale(scale);
    }
}
void SceneWidget::on_nodeRotation_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        node->SetRotation(qDegreesToRadians(value));
    }
}

void SceneWidget::on_btnEditEntity_clicked()
{
    if (auto* node = GetCurrentNode())
    {
        const auto& id = (QString)GetItemId(mUI.nodeEntity);
        if (id.isEmpty())
            return;

        emit OpenResource(id);
    }
}

void SceneWidget::on_btnEntityParams_clicked()
{
    if (auto* node = GetCurrentNode())
    {
        auto klass = node->GetEntityClass();
        if (!klass)
            return;

        DlgEntity dlg(this, mState.workspace, *klass, *node);
        dlg.exec();

        node->ClearStaleScriptValues(*klass);
    }
}

void SceneWidget::on_btnNodePlus90_clicked()
{
    if (auto* node = GetCurrentNode())
    {
        const float value = GetValue(mUI.nodeRotation);
        // careful, triggers value changed event.
        mUI.nodeRotation->setValue(math::clamp(-180.0f, 180.0f, value + 90.0f));
    }
}
void SceneWidget::on_btnNodeMinus90_clicked()
{
    if (auto* node = GetCurrentNode())
    {
        const float value = GetValue(mUI.nodeRotation);
        // careful, triggers value changed event.
        mUI.nodeRotation->setValue(math::clamp(-180.0f, 180.0f, value - 90.0f));
    }
}

void SceneWidget::on_tree_customContextMenuRequested(QPoint)
{
    const auto& tree = mState.scene->GetRenderTree();
    const auto* node = GetCurrentNode();
    mUI.actionNodeDuplicate->setEnabled(node != nullptr);
    mUI.actionNodeDelete->setEnabled(node != nullptr);
    mUI.actionNodeMoveDownLayer->setEnabled(node != nullptr);
    mUI.actionNodeMoveUpLayer->setEnabled(node != nullptr);
    mUI.actionNodeBreakLink->setEnabled(node != nullptr && tree.GetParent(node));
    mUI.actionNodeEdit->setEnabled(node != nullptr);
    mUI.actionNodeFind->setEnabled(node != nullptr);
    mUI.actionEntityVarRef->setEnabled(node != nullptr);
    mUI.actionEditEntityClass->setEnabled(node != nullptr);

    QMenu menu(this);
    menu.addAction(mUI.actionNodeMoveUpLayer);
    menu.addAction(mUI.actionNodeMoveDownLayer);
    menu.addAction(mUI.actionNodeDuplicate);
    menu.addAction(mUI.actionNodeBreakLink);
    menu.addAction(mUI.actionNodeFind);
    menu.addSeparator();
    menu.addAction(mUI.actionNodeEdit);
    menu.addAction(mUI.actionEditEntityClass);
    menu.addSeparator();
    menu.addAction(mUI.actionEntityVarRef);
    menu.addSeparator();
    menu.addAction(mUI.actionNodeDelete);
    menu.exec(QCursor::pos());
}

void SceneWidget::on_scriptVarList_customContextMenuRequested(QPoint)
{
    QMenu menu(this);
    menu.addAction(mUI.actionScriptVarAdd);
    menu.addAction(mUI.actionScriptVarEdit);
    menu.addAction(mUI.actionScriptVarDel);
    menu.exec(QCursor::pos());
}

void SceneWidget::PlaceAnyEntity()
{
    const auto snap = (bool)GetValue(mUI.chkSnap);
    const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
    const auto grid_size = (unsigned)grid;
    auto tool = std::make_unique<PlaceEntityTool>(mState, snap, grid_size);
    tool->SetWorldPos(MapWindowCoordinateToWorld(mUI, mState, mUI.widget->mapFromGlobal(QCursor::pos())));
    mCurrentTool = std::move(tool);
}

void SceneWidget::PlaceNewEntity()
{
    const auto* action = qobject_cast<QAction*>(sender());
    const auto klassid = action->data().toString();
    auto entity = mState.workspace->GetEntityClassById(klassid);

    const auto snap = (bool)GetValue(mUI.chkSnap);
    const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
    const auto grid_size = (unsigned)grid;
    mCurrentTool = std::make_unique<PlaceEntityTool>(mState, entity, snap, grid_size);
}

void SceneWidget::TreeCurrentNodeChangedEvent()
{
    DisplayCurrentNodeProperties();
}
void SceneWidget::TreeDragEvent(TreeWidget::TreeItem* item, TreeWidget::TreeItem* target)
{
    auto& tree = mState.scene->GetRenderTree();
    auto* src_value = static_cast<game::EntityPlacement*>(item->GetUserData());
    auto* dst_value = static_cast<game::EntityPlacement*>(target->GetUserData());

    // check if we're trying to drag a parent onto its own child
    if (game::SearchChild(tree, dst_value, src_value))
        return;

    const bool retain_world_transform = true;
    mState.scene->ReparentChild(dst_value, src_value, retain_world_transform);

}
void SceneWidget::TreeClickEvent(TreeWidget::TreeItem* item)
{
    if (auto* node = GetCurrentNode())
    {
        const bool visibility = !node->TestFlag(game::EntityPlacement::Flags::VisibleInEditor);
        node->SetFlag(game::EntityPlacement::Flags::VisibleInEditor, visibility);
        item->SetIcon(visibility ? QIcon("icons:eye.png") : QIcon("icons:crossed_eye.png"));
        mUI.tree->Update();
    }
}

void SceneWidget::ResourceAdded(const app::Resource* resource)
{
    RebuildCombos();
    RebuildMenus();
    DisplaySceneProperties();
    DisplayCurrentNodeProperties();
}
void SceneWidget::ResourceRemoved(const app::Resource* resource)
{
    UpdateResourceReferences();
    RebuildCombos();
    RebuildMenus();
    DisplayCurrentNodeProperties();
    DisplaySceneProperties();
}
void SceneWidget::ResourceUpdated(const app::Resource* resource)
{
    mState.renderer.ClearPaintState();

    RebuildCombos();
    RebuildMenus();

    if (!resource->IsTilemap())
        return;

    if (!mState.scene->HasTilemap())
        return;

    const auto& mapId = mState.scene->GetTilemapId();
    if (mapId != resource->GetIdUtf8())
        return;

    // if the tilemap this scene refers to was just modified
    // then force a re-load by resetting the map object.
    mTilemap.reset();
}

void SceneWidget::PaintScene(gfx::Painter& painter, double /*secs*/)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto zoom   = (float)GetValue(mUI.zoom);
    const auto xs     = (float)GetValue(mUI.scaleX);
    const auto ys     = (float)GetValue(mUI.scaleY);
    const auto grid   = (GridDensity)GetValue(mUI.cmbGrid);
    const auto view   = (engine::GameView::EnumValue)GetValue(mUI.cmbPerspective);

    SetValue(mUI.widgetColor, mUI.widget->GetCurrentClearColor());

    gfx::Device* device = painter.GetDevice();

    // painter for drawing in the tile domain/space. If perspective is axis aligned
    // then this is the same as the scene painter below, but these are always
    // conceptually different painters in different domains.
    gfx::Painter tile_painter(device);
    tile_painter.SetViewMatrix(CreateViewMatrix(mUI, mState, view));
    tile_painter.SetProjectionMatrix(CreateProjectionMatrix(mUI, engine::Projection::Orthographic));
    tile_painter.SetPixelRatio(glm::vec2{xs*zoom, ys*zoom});
    tile_painter.SetViewport(0, 0, width, height);
    tile_painter.SetSurfaceSize(width, height);
    tile_painter.SetEditingMode(true);

    gfx::Painter scene_painter(device);
    scene_painter.SetViewMatrix(CreateViewMatrix(mUI, mState, engine::GameView::AxisAligned));
    scene_painter.SetProjectionMatrix(CreateProjectionMatrix(mUI, engine::Projection::Orthographic));
    scene_painter.SetPixelRatio(glm::vec2{xs*zoom, ys*zoom});
    scene_painter.SetViewport(0, 0, width, height);
    scene_painter.SetSurfaceSize(width, height);
    scene_painter.SetEditingMode(true);

    // render endless background grid.
    if (GetValue(mUI.chkShowGrid))
    {
        DrawCoordinateGrid(scene_painter, tile_painter, grid, zoom, xs, ys, width, height, view);
    }

    // render the actual scene
    {
        if (mState.scene->HasTilemap())
        {
            const auto& mapId = mState.scene->GetTilemapId();
            if (!mTilemap || mTilemap->GetClassId() != mapId)
            {
                auto klass = mState.workspace->GetTilemapClassById(mapId);
                mTilemap = game::CreateTilemap(klass);
                mTilemap->Load(*mState.workspace);
            }
        }

        // setup a viewport rect for culling draw packets against
        // draw packets which don't intersect with the viewrect are culled
        // for improved perf.

        //const game::FRect viewport(0, 0, width, height);

        // todo: reimplement culling
        const game::FRect viewport(0, 0, 0, 0);

        DrawHook hook(GetCurrentNode(), viewport);
        hook.SetIsPlaying(mPlayState == PlayState::Playing);
        hook.SetDrawVectors(true);
        hook.SetViewMatrix(CreateViewMatrix(mUI, mState, engine::GameView::AxisAligned));

        engine::Renderer::Camera camera;
        camera.position.x = mState.camera_offset_x;
        camera.position.y = mState.camera_offset_y;
        camera.rotation   = GetValue(mUI.rotation);
        camera.scale.x    = xs * zoom;
        camera.scale.y    = ys * zoom;
        camera.viewport   = game::FRect(-width*0.5f, -height*0.5f, width, height);
        mState.renderer.SetCamera(camera);

        engine::Renderer::Surface surface;
        surface.viewport = gfx::IRect(0, 0, width, height);
        surface.size     = gfx::USize(width, height);
        mState.renderer.SetSurface(surface);

        // we don't have an UI to control the individual map layers in the
        // scene widget so only expose a "master" flag that controls the map
        // visibility in the scene overall and then the layers are controlled
        // by the map klass setting (which are available currently only in the
        // map editor...)
        const bool show_map = GetValue(mUI.chkShowMap);
        const auto* map = show_map ? mTilemap.get() : nullptr;

        mState.renderer.BeginFrame();
        mState.renderer.Draw(*mState.scene,  map, *device, &hook);

        if (mCurrentTool)
            mCurrentTool->Render(painter, scene_painter);

        // Remember that the tool can also render using the renderer.
        // If that happens after the call to "EndFrame" the renderer
        // resources are lost in EndFrame and the tool's render call will
        // end up recreating them again.
        mState.renderer.EndFrame();
    }

    for (size_t i=0; i<mState.scene->GetNumNodes(); ++i)
    {
        const auto& node = mState.scene->GetPlacement(i);
        const auto& entity_klass = node.GetEntityClass();
        if (entity_klass)
            continue;
        const auto& pos = mState.scene->MapCoordsFromNodeBox(0.0f, 0.0f, &node);
        ShowError(base::FormatString("%1 Missing entity reference!", node.GetName()), gfx::FPoint(pos.x, pos.y), scene_painter);
    }

    if (mState.scene->GetNumNodes() == 0)
    {
        ShowInstruction(
            "Create a new scene where game play takes place.\n\n"
            "INSTRUCTIONS\n"
            "1. Select 'Place Entity' in the main tool bar above.\n"
            "2. Move the mouse to place the entity into the scene.\n"
            "3. Use the mouse wheel to scroll through the entities.\n"
            "4. Press 'Escape' to quit placing entities.\n\n\n"
            "Hit 'Play' to animate materials and shapes.\n"
            "Hit 'Test Run' to test the scene.\n"
            "Hit 'Save' to save the scene.",
            gfx::FRect(0, 0, width, height),
            painter, 28
        );
        return;
    }

    // right arrow
    if (GetValue(mUI.chkShowOrigin))
    {
        gfx::Transform view;
        DrawBasisVectors(tile_painter, view);
    }

    if (GetValue(mUI.chkShowViewport))
    {
        gfx::Transform view;
        MakeViewTransform(mUI, mState, view);
        const auto& settings = mState.workspace->GetProjectSettings();
        const float game_width = settings.viewport_width;
        const float game_height = settings.viewport_height;
        DrawViewport(painter, view, game_width, game_height, width, height);
    }
    PrintMousePos(mUI, mState, painter, view,
                  engine::Projection::Orthographic);
}

void SceneWidget::MouseMove(QMouseEvent* event)
{
    if (mCurrentTool)
    {
        const MouseEvent mickey(event, mUI, mState);

        mCurrentTool->MouseMove(mickey);
        // update the properties that might have changed as the result of application
        // of the current tool.
        DisplayCurrentCameraLocation();
        DisplayCurrentNodeProperties();
    }
}
void SceneWidget::MousePress(QMouseEvent* event)
{
    const MouseEvent mickey(event, mUI, mState);

    if (!mCurrentTool && (mickey->button() == Qt::LeftButton))
    {
        const auto snap = (bool)GetValue(mUI.chkSnap);
        const auto grid_type = (GridDensity)GetValue(mUI.cmbGrid);
        const auto grid_size = static_cast<unsigned>(grid_type);
        const auto& click_point = mickey->pos();

        // if we have a current node see if the mouse click point is in the special
        // areas that are used to select a tool for resizing/rotating the node.
        // the visualization of these is in the editor/gui/drawing.cpp/.h files.
        if (auto* current = GetCurrentNode())
        {
            const auto& entity_klass = current->GetEntityClass();
            if (!entity_klass)
                return;
            const auto& box  = entity_klass->GetBoundingRect();

            const base::Transform model(mState.scene->FindEntityTransform(current));

            const auto hotspot = TestToolHotspot(mUI, mState, model, box, click_point);
            if (hotspot == ToolHotspot::Resize)
                mCurrentTool.reset(new ScaleRenderTreeNodeTool(*mState.scene, current));
            else if (hotspot == ToolHotspot::Rotate)
                mCurrentTool.reset(new RotateRenderTreeNodeTool(*mState.scene, current));
            else if (hotspot == ToolHotspot::Remove)
                mCurrentTool.reset(new MoveRenderTreeNodeTool(*mState.scene, current, snap, grid_size, GetValue(mUI.cmbPerspective)));
            else mUI.tree->ClearSelection();

        }

        // pick another node
        if (!GetCurrentNode())
        {
            if (auto* selection = SelectNode(click_point))
            {
                const auto& entity_klass = selection->GetEntityClass();
                if (!entity_klass)
                    return;
                const auto& box  = entity_klass->GetBoundingRect();

                const base::Transform model(mState.scene->FindEntityTransform(selection));

                const auto hotspot = TestToolHotspot(mUI, mState, model, box, click_point);
                if (hotspot == ToolHotspot::Resize)
                    mCurrentTool.reset(new ScaleRenderTreeNodeTool(*mState.scene, selection));
                else if (hotspot == ToolHotspot::Rotate)
                    mCurrentTool.reset(new RotateRenderTreeNodeTool(*mState.scene, selection));
                else if (hotspot == ToolHotspot::Remove)
                    mCurrentTool.reset(new MoveRenderTreeNodeTool(*mState.scene, selection, snap, grid_size, GetValue(mUI.cmbPerspective)));

                mUI.tree->SelectItemById(selection->GetId());
            }
        }
    }
    else if (!mCurrentTool && (mickey->button() == Qt::RightButton))
    {
        mCurrentTool.reset(new PerspectiveCorrectCameraTool(mUI, mState));
    }

    if (mCurrentTool)
        mCurrentTool->MousePress(mickey);
}
void SceneWidget::MouseRelease(QMouseEvent* event)
{
    if (!mCurrentTool)
        return;

    const MouseEvent mickey(event, mUI, mState);

    if (mCurrentTool->MouseRelease(mickey))
    {
        mCurrentTool.reset();
        UncheckPlacementActions();
        DisplayCurrentNodeProperties();
    }
}

void SceneWidget::MouseDoubleClick(QMouseEvent* mickey)
{
    // double-click is preceded by a regular click event and quick
    // googling suggests that there's really no way to filter out
    // single click when trying to react only to double-click other
    // than to set a timer (which adds latency).
    // Going to simply discard any tool selection here on double click.
    mCurrentTool.reset();

    auto scene_node = SelectNode(mickey->pos());
    if (!scene_node)
        return;

    auto entity_klass = scene_node->GetEntityClass();
    if (!entity_klass)
        return;

    DlgEntity dlg(this, mState.workspace, *entity_klass, *scene_node);
    dlg.exec();

    scene_node->ClearStaleScriptValues(*entity_klass);
}

void SceneWidget::MouseWheel(QWheelEvent* wheel)
{
    if (!mCurrentTool)
        return;
    if (auto* place = dynamic_cast<PlaceEntityTool*>(mCurrentTool.get()))
    {
        const QPoint &num_degrees = wheel->angleDelta() / 8;
        const QPoint &num_steps = num_degrees / 15;
        // only consider the wheel scroll steps on the vertical axis.
        // if steps are positive the wheel is scrolled away from the user
        // and if steps are negative the wheel is scrolled towards the user.
        const int num_vertical_steps = num_steps.y();
        for (int i=0; i<std::abs(num_vertical_steps); ++i)
        {
            if (num_vertical_steps > 0)
                place->SelectNextEntity();
            else place->SelectPrevEntity();
        }
    }
}

bool SceneWidget::KeyPress(QKeyEvent* key)
{
    // handle key press events coming from the gfx widget

    if (mCurrentTool && mCurrentTool->KeyPress(key))
        return true;

    switch (key->key())
    {
        case Qt::Key_Delete:
            on_actionNodeDelete_triggered();
            break;
        case Qt::Key_Left:
            TranslateCurrentNode(20.0f, 0.0f);
            break;
        case Qt::Key_Right:
            TranslateCurrentNode(-20.0f, 0.0f);
            break;
        case Qt::Key_Up:
            TranslateCurrentNode(0.0f, -20.0f);
            break;
        case Qt::Key_Down:
            TranslateCurrentNode(0.0f, 20.0f);
            break;
        case Qt::Key_Escape:
            OnEscape();
            break;
        default:
            return false;
    }
    return true;
}

void SceneWidget::DisplayCurrentNodeProperties()
{
    SetValue(mUI.nodeID, QString(""));
    SetValue(mUI.nodeName, QString(""));
    SetValue(mUI.nodeIsVisible, true);
    SetValue(mUI.nodeTranslateX, 0.0f);
    SetValue(mUI.nodeTranslateY, 0.0f);
    SetValue(mUI.nodeScaleX, 1.0f);
    SetValue(mUI.nodeScaleY, 1.0f);
    SetValue(mUI.nodeRotation, 0.0f);
    SetValue(mUI.nodeEntity, "");
    SetValue(mUI.nodeLayer, 0);
    SetList(mUI.nodeLink, QStringList());
    SetValue(mUI.nodeLink, QString(""));

    SetEnabled(mUI.nodeProperties, false);
    SetEnabled(mUI.nodeTransform, false);

    if (const auto* node = GetCurrentNode())
    {
        SetEnabled(mUI.nodeProperties, true);
        SetEnabled(mUI.nodeTransform, true);

        const auto& translate = node->GetTranslation();
        const auto& scale = node->GetScale();
        SetValue(mUI.nodeID, node->GetId());
        SetValue(mUI.nodeName, node->GetName());
        SetValue(mUI.nodeEntity, ListItemId(node->GetEntityId()));
        SetValue(mUI.nodeLayer, node->GetLayer());
        SetValue(mUI.nodeIsVisible, node->TestFlag(game::EntityPlacement::Flags::VisibleInGame));
        SetValue(mUI.nodeTranslateX, translate.x);
        SetValue(mUI.nodeTranslateY, translate.y);
        SetValue(mUI.nodeScaleX, scale.x);
        SetValue(mUI.nodeScaleY, scale.y);
        SetValue(mUI.nodeRotation, qRadiansToDegrees(node->GetRotation()));

        int link_index = -1;
        if (const auto* parent = mState.scene->GetRenderTree().GetParent(node))
        {
            const auto& klass = parent->GetEntityClass();
            if (!klass)
                return;
            for (size_t i=0; i<klass->GetNumNodes(); ++i)
            {
                const auto& tree = klass->GetRenderTree();
                const auto& link = klass->GetNode(i);
                //if (tree.GetParent(&link) == nullptr)
                {
                    const auto name = app::FromUtf8(link.GetName());
                    const auto id   = app::FromUtf8(link.GetId());
                    QSignalBlocker s(mUI.nodeLink);
                    mUI.nodeLink->addItem(name, id);
                    if (link.GetId() == node->GetParentRenderTreeNodeId())
                        link_index = static_cast<int>(i);
                }
            }
            SetValue(mUI.nodeLink, link_index);
            SetEnabled(mUI.nodeLink, true);
        }
        else
        {
            SetEnabled(mUI.nodeLink, false);
        }
    }
}

void SceneWidget::DisplaySceneProperties()
{
    const auto vars = mState.scene->GetNumScriptVars();
    SetEnabled(mUI.btnEditScriptVar, vars > 0);
    SetEnabled(mUI.btnDeleteScriptVar, vars > 0);
    SetValue(mUI.name, mState.scene->GetName());
    SetValue(mUI.ID, mState.scene->GetId());
    SetValue(mUI.cmbScripts, ListItemId(mState.scene->GetScriptFileId()));
    SetValue(mUI.cmbTilemaps, ListItemId(mState.scene->GetTilemapId()));
    SetValue(mUI.cmbSpatialIndex, mState.scene->GetDynamicSpatialIndex());
    SetEnabled(mUI.btnEditScript, mState.scene->HasScriptFile());
    SetEnabled(mUI.btnEditMap, mState.scene->HasTilemap());

    mUI.spinLeftBoundary->ClearValue();
    mUI.spinRightBoundary->ClearValue();
    mUI.spinTopBoundary->ClearValue();
    mUI.spinBottomBoundary->ClearValue();

    const auto index = mState.scene->GetDynamicSpatialIndex();
    if (index == game::SceneClass::SpatialIndex::Disabled)
    {
        SetEnabled(mUI.spQuadMaxItems,  false);
        SetEnabled(mUI.spQuadMaxLevels, false);
        SetEnabled(mUI.spDenseGridCols, false);
        SetEnabled(mUI.spDenseGridRows, false);
    }
    else if (index == game::SceneClass::SpatialIndex::QuadTree)
    {
        SetEnabled(mUI.spQuadMaxItems,  true);
        SetEnabled(mUI.spQuadMaxLevels, true);
        SetEnabled(mUI.spDenseGridCols, false);
        SetEnabled(mUI.spDenseGridRows, false);
    }
    else if (index == game::SceneClass::SpatialIndex::DenseGrid)
    {
        SetEnabled(mUI.spQuadMaxItems,  false);
        SetEnabled(mUI.spQuadMaxLevels, false);
        SetEnabled(mUI.spDenseGridCols, true);
        SetEnabled(mUI.spDenseGridRows, true);
    }

    if (const auto* ptr = mState.scene->GetQuadTreeArgs())
    {
        SetValue(mUI.spQuadMaxLevels, ptr->max_levels);
        SetValue(mUI.spQuadMaxItems,  ptr->max_items);
    }

    if (const auto* ptr = mState.scene->GetDenseGridArgs())
    {
        SetValue(mUI.spDenseGridRows, ptr->num_rows);
        SetValue(mUI.spDenseGridCols, ptr->num_cols);
    }

    if (const auto* ptr = mState.scene->GetLeftBoundary())
    {
        SetValue(mUI.spinLeftBoundary, *ptr);
    }
    if (const auto* ptr = mState.scene->GetRightBoundary())
    {
        SetValue(mUI.spinRightBoundary, *ptr);
    }
    if (const auto* ptr = mState.scene->GetTopBoundary())
    {
        SetValue(mUI.spinTopBoundary, *ptr);
    }
    if (const auto* ptr = mState.scene->GetBottomBoundary())
    {
        SetValue(mUI.spinBottomBoundary, *ptr);
    }

    if (const auto* bloom = mState.scene->GetBloom())
    {
        SetValue(mUI.chkEnableBloom, true);

        SetValue(mUI.bloomThresholdSpin,  bloom->threshold);
        SetValue(mUI.bloomThresholdSlide, bloom->threshold);
        SetValue(mUI.bloomRSpin,          bloom->red);
        SetValue(mUI.bloomRSlide,         bloom->red);
        SetValue(mUI.bloomGSpin,          bloom->green);
        SetValue(mUI.bloomGSlide,         bloom->green);
        SetValue(mUI.bloomBSpin,          bloom->blue);
        SetValue(mUI.bloomBSlide,         bloom->blue);
        SetEnabled(mUI.bloomThresholdSpin,  true);
        SetEnabled(mUI.bloomThresholdSlide, true);
        SetEnabled(mUI.bloomRSpin,          true);
        SetEnabled(mUI.bloomRSlide,         true);
        SetEnabled(mUI.bloomGSpin,          true);
        SetEnabled(mUI.bloomGSlide,         true);
        SetEnabled(mUI.bloomBSpin,          true);
        SetEnabled(mUI.bloomBSlide,         true);
    }
    else
    {
        SetValue(mUI.chkEnableBloom, false);

        SetValue(mUI.bloomThresholdSpin,  mBloom.threshold);
        SetValue(mUI.bloomThresholdSlide, mBloom.threshold);
        SetValue(mUI.bloomRSpin,          mBloom.red);
        SetValue(mUI.bloomRSlide,         mBloom.red);
        SetValue(mUI.bloomGSpin,          mBloom.green);
        SetValue(mUI.bloomGSlide,         mBloom.green);
        SetValue(mUI.bloomBSpin,          mBloom.blue);
        SetValue(mUI.bloomBSlide,         mBloom.blue);
        SetEnabled(mUI.bloomThresholdSpin,  false);
        SetEnabled(mUI.bloomThresholdSlide, false);
        SetEnabled(mUI.bloomRSpin,          false);
        SetEnabled(mUI.bloomRSlide,         false);
        SetEnabled(mUI.bloomGSpin,          false);
        SetEnabled(mUI.bloomGSlide,         false);
        SetEnabled(mUI.bloomBSpin,          false);
        SetEnabled(mUI.bloomBSlide,         false);
    }
}

void SceneWidget::DisplayCurrentCameraLocation()
{
    SetValue(mUI.translateX, -mState.camera_offset_x);
    SetValue(mUI.translateY, -mState.camera_offset_y);
}

void SceneWidget::UncheckPlacementActions()
{
    mUI.actionNodePlace->setChecked(false);
}

void SceneWidget::TranslateCurrentNode(float dx, float dy)
{
    if (auto* node = GetCurrentNode())
    {
        auto pos = node->GetTranslation();
        pos.x += dx;
        pos.y += dy;
        node->SetTranslation(pos);
        SetValue(mUI.nodeTranslateX, pos.x);
        SetValue(mUI.nodeTranslateY, pos.y);
    }
}

void SceneWidget::TranslateCamera(float dx, float dy)
{
    mState.camera_offset_x += dx;
    mState.camera_offset_y += dy;
    DisplayCurrentCameraLocation();
}

void SceneWidget::RebuildMenus()
{
    mEntities->clear();

    const auto& entities = mState.workspace->ListUserDefinedEntities();
    if (entities.empty())
    {
        SetEnabled(mEntities, false);
        return;
    }

    QAction* action = mEntities->addAction("Any Entity");
    action->setIcon(QIcon("icons:entity.png"));
    action->setShortcut(QKeySequence(Qt::ControlModifier | Qt::Key_A));
    connect(action, &QAction::triggered, this, &SceneWidget::PlaceAnyEntity);
    mEntities->addSeparator();

    for (const auto& resource : entities)
    {
        QAction* action = mEntities->addAction(resource.name);
        action->setData(resource.id);
        connect(action, &QAction::triggered, this, &SceneWidget::PlaceNewEntity);
    }
    SetEnabled(mEntities, true);
}

void SceneWidget::RebuildCombos()
{
    SetList(mUI.nodeEntity, mState.workspace->ListUserDefinedEntities());
    SetList(mUI.cmbScripts, mState.workspace->ListUserDefinedScripts());
    SetList(mUI.cmbTilemaps, mState.workspace->ListUserDefinedMaps());
}

void SceneWidget::UpdateResourceReferences()
{
    for (size_t i=0; i<mState.scene->GetNumNodes(); ++i)
    {
        auto& node = mState.scene->GetPlacement(i);
        auto klass = mState.workspace->FindEntityClassById(node.GetEntityId());
        if (!klass)
        {
            WARN("Scene node refers to an entity that is no longer available. [node='%1']", node.GetName());
            node.ResetEntity();
            node.ResetEntityParams();
            continue;
        }
        // clear any script value that is no longer part of the entity class.
        node.ClearStaleScriptValues(*klass);
        // resolve the runtime entity klass object reference.
        node.SetEntity(klass);
    }
    mState.renderer.ClearPaintState();

    if (mState.scene->HasScriptFile())
    {
        const auto& scriptId = mState.scene->GetScriptFileId();
        if (!mState.workspace->IsValidScript(scriptId))
        {
            WARN("Scene script is no longer available. [script='%1']", scriptId);
            mState.scene->ResetScriptFile();
            SetEnabled(mUI.btnEditScript, false);
        }
    }
    if (mState.scene->HasTilemap())
    {
        const auto& mapId = mState.scene->GetTilemapId();
        if (!mState.workspace->IsValidTilemap(mapId))
        {
            WARN("Scene tilemap is no longer available. [map='%1']", mapId);
            mState.scene->ResetTilemap();
            SetEnabled(mUI.btnEditMap, false);
        }
    }
}

void SceneWidget::SetSpatialIndexParams()
{
    mState.scene->SetDynamicSpatialIndex(GetValue(mUI.cmbSpatialIndex));

    if (const auto* ptr = mState.scene->GetQuadTreeArgs())
    {
        auto args = *ptr;
        args.max_levels = GetValue(mUI.spQuadMaxLevels);
        args.max_items  = GetValue(mUI.spQuadMaxItems);
        mState.scene->SetDynamicSpatialIndexArgs(args);
    }
    if (const auto* ptr = mState.scene->GetDenseGridArgs())
    {
        auto args = *ptr;
        args.num_cols = GetValue(mUI.spDenseGridCols);
        args.num_rows = GetValue(mUI.spDenseGridRows);
        mState.scene->SetDynamicSpatialIndexArgs(args);
    }
}

void SceneWidget::SetSceneBoundary()
{
    mState.scene->ResetLeftBoundary();
    mState.scene->ResetRightBoundary();
    mState.scene->ResetTopBoundary();
    mState.scene->ResetBottomBoundary();

    if (const auto& left = mUI.spinLeftBoundary->GetValue())
        mState.scene->SetLeftBoundary(left.value());
    if (const auto& right = mUI.spinRightBoundary->GetValue())
        mState.scene->SetRightBoundary(right.value());
    if (const auto& top = mUI.spinTopBoundary->GetValue())
        mState.scene->SetTopBoundary(top.value());
    if (const auto& bottom = mUI.spinBottomBoundary->GetValue())
        mState.scene->SetBottomBoundary(bottom.value());
}

void SceneWidget::FindNode(const game::EntityPlacement* node)
{
    const auto& entity_klass = node->GetEntityClass();
    if (!entity_klass)
        return;

    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto zoom   = (float)GetValue(mUI.zoom);
    const auto xs     = (float)GetValue(mUI.scaleX);
    const auto ys     = (float)GetValue(mUI.scaleY);
    const game::FRect viewport(-width*0.5f, -height*0.5f, width, height);

    const auto view = engine::CreateModelViewMatrix(engine::GameView::AxisAligned,
                                                    glm::vec2 { 0.0f, 0.0f },
                                                    glm::vec2 { xs * zoom, ys * zoom },
                                                    GetValue(mUI.rotation));

    const auto proj = engine::CreateProjectionMatrix(engine::Projection::Orthographic, viewport);

    const auto& node_world_pos = mState.scene->MapCoordsFromNodeBox(0.0f, 0.0f, node);
    const auto& node_view_pos  = view * glm::vec4(node_world_pos, 0.0f, 1.0f);
    const auto& node_clip_pos  = proj * node_view_pos;

    const auto& clip_translation = node_clip_pos - glm::vec4 {0.0f, 0.0f, 0.0f, 1.0f };
    const auto& view_translation = glm::inverse(proj * view) * clip_translation;
    const auto& cam_pos = glm::vec2 { view_translation.x, view_translation.y };
    // the above is incorrect, the jump will reset rotation to zero as a workaround for now.
    mAnimator.Jump(mUI, mState, cam_pos);
}

game::EntityPlacement* SceneWidget::SelectNode(const QPoint& click_point)
{
    const glm::vec2 world_pos = MapWindowCoordinateToWorld(mUI, mState, click_point);

    std::vector<game::EntityPlacement*> hit_nodes;
    std::vector<glm::vec2> hit_positions;
    mState.scene->CoarseHitTest(world_pos, &hit_nodes, &hit_positions);
    if (hit_nodes.empty())
        return nullptr;

    // per pixel selection based on the idea that we re-render the
    // objects returned by the coarse hit test with different colors
    // and then read back the color of the pixel under the click point
    // and see which object/node the color maps back to.
    class DrawHook : public engine::SceneClassDrawHook
    {
    public:
        DrawHook(const std::vector<game::EntityPlacement*>& hits) : mHits(hits)
        {
            for (unsigned i=0; i<hits.size(); ++i)
            {
                const unsigned rgb = i * 100 + 100;
                const unsigned r = (rgb >> 16) & 0xff;
                const unsigned g = (rgb >> 8)  & 0xff;
                const unsigned b = (rgb >> 0)  & 0xff;
                mColors.push_back(gfx::Color4f(r, g, b, 0xff));
            }
        }
        virtual bool FilterEntity(const game::EntityPlacement& placement) override
        {
            // filter out nodes that are currently not visible.
            // probably don't want to select any of those.
            if (!placement.TestFlag(game::EntityPlacement::Flags::VisibleInEditor))
                return false;
            else if (placement.IsBroken())
                return false;

            for (const auto* n : mHits)
                if (n == &placement) return true;
            return false;
        }
        virtual void BeginDrawEntity(const game::EntityPlacement& placement) override
        {
            for (size_t i=0; i<mHits.size(); ++i)
            {
                if (mHits[i] == &placement)
                {
                    mColorIndex = i;
                    return;
                }
            }
        }
        virtual bool InspectPacket(const game::EntityPlacement& placement, engine::DrawPacket& draw) override
        {
            ASSERT(mColorIndex < mColors.size());
            draw.material = gfx::CreateMaterialInstance(gfx::CreateMaterialClassFromColor(mColors[mColorIndex]));
            return true;
        }

    private:
        const std::vector<game::EntityPlacement*>& mHits;
        std::vector<gfx::Color4f> mColors;
        std::size_t mColorIndex = 0;
    };

    auto* device = mUI.widget->GetDevice();

    const auto zoom   = (float)GetValue(mUI.zoom);
    const auto xs     = (float)GetValue(mUI.scaleX);
    const auto ys     = (float)GetValue(mUI.scaleY);
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto perspective = engine::GameView::AxisAligned;

    engine::Renderer::Camera camera;
    camera.position.x = mState.camera_offset_x;
    camera.position.y = mState.camera_offset_y;
    camera.rotation   = GetValue(mUI.rotation);
    camera.scale.x    = xs * zoom;
    camera.scale.y    = ys * zoom;
    camera.viewport   = game::FRect(-width*0.5f, -height*0.5f, width, height);
    mState.renderer.SetCamera(camera);

    engine::Renderer::Surface surface;
    surface.viewport = gfx::IRect(0, 0, width, height);
    surface.size     = gfx::USize(width, height);
    mState.renderer.SetSurface(surface);

    DrawHook hook(hit_nodes);
    mState.renderer.Draw(*mState.scene, nullptr, *device, &hook);

    {
        // for debugging.
        //auto bitmap = device.ReadColorBuffer(mUI.widget->width(), mUI.widget->height());
        //gfx::WritePNG(bitmap, "/tmp/click-test-debug.png");
    }

    const auto surface_width  = mUI.widget->width();
    const auto surface_height = mUI.widget->height();
    const auto& bitmap = device->ReadColorBuffer(click_point.x(), surface_height - click_point.y(), 1, 1);
    const auto& pixel  = bitmap.GetPixel(0, 0);
    for (unsigned i=0; i<hit_nodes.size(); ++i)
    {
        const unsigned rgb = i * 100 + 100;
        const unsigned r = (rgb >> 16) & 0xff;
        const unsigned g = (rgb >> 8) & 0xff;
        const unsigned b = (rgb >> 0) & 0xff;
        if ((pixel.r == r) && (pixel.g == g) && (pixel.b == b))
        {
            return hit_nodes[i];
        }
    }

    // // select the top most node.
    auto* hit = hit_nodes[0];
    auto  pos = hit_positions[0];
    int layer = hit->GetLayer();
    for (size_t i=1; i<hit_nodes.size(); ++i)
    {
        if (hit_nodes[i]->TestFlag(game::EntityPlacement::Flags::VisibleInEditor) &&
            hit_nodes[i]->GetLayer() >= layer)
        {
            hit = hit_nodes[i];
            pos = hit_positions[i];
            layer = hit->GetLayer();
        }
    }
    if (!hit->TestFlag(game::EntityPlacement::Flags::VisibleInEditor))
        return nullptr;
    return hit;
}

game::EntityPlacement* SceneWidget::GetCurrentNode()
{
    TreeWidget::TreeItem* item = mUI.tree->GetSelectedItem();
    if (item == nullptr)
        return nullptr;
    else if (!item->GetUserData())
        return nullptr;
    return static_cast<game::EntityPlacement*>(item->GetUserData());
}

const game::EntityPlacement* SceneWidget::GetCurrentNode() const
{
    const TreeWidget::TreeItem* item = mUI.tree->GetSelectedItem();
    if (item == nullptr)
        return nullptr;
    else if (!item->GetUserData())
        return nullptr;
    return static_cast<const game::EntityPlacement*>(item->GetUserData());
}

QString GenerateSceneScriptSource(QString scene)
{
    scene = app::GenerateScriptVarName(scene);

    QString source(R"(
-- Scene '%1' script.
-- This script will be called for every instance of '%1' during gameplay.
-- You're free to delete functions you don't need.

-- Called when the scene begins play.
-- Map may be nil if the scene has no map set.
function BeginPlay(%1, map)
end

-- Called when the scene ends play.
-- Map may be nil if the scene has no map set.
function EndPlay(%1, map)
end

-- Called when a new entity has been spawned in the scene.
-- This function will be called before entity BeginPlay.
function SpawnEntity(%1, map, entity)
end

-- Called when an entity has been killed from the scene.
-- This function will be called before entity EndPlay.
function KillEntity(%1, map, entity)
end

-- Called on every low frequency game tick.
function Tick(%1, game_time, dt)
end

-- Called on every iteration of game loop.
function Update(%1, game_time, dt)
end

-- Physics collision callback on contact begin.
-- This is called when an entity node begins contact with another
-- entity node in the scene.
function OnBeginContact(%1, entity, entity_node, other, other_node)
end

-- Physics collision callback on end contact.
-- This is called when an entity node ends contact with another
-- entity node in the scene.
function OnEndContact(%1, entity, entity_node, other, other_node)
end

-- Called on keyboard key down events.
function OnKeyDown(%1, symbol, modifier_bits)
end

-- Called on keyboard key up events.
function OnKeyUp(%1, symbol, modifier_bits)
end

-- Called on mouse button press events.
function OnMousePress(%1, mouse)
end

-- Called on mouse button release events.
function OnMouseRelease(%1, mouse)
end

-- Called on mouse move events.
function OnMouseMove(%1, mouse)
end

-- Called on game events.
function OnGameEvent(%1, event)
end

-- Called on entity timer events.
function OnEntityTimer(%1, entity, timer, jitter)
end

-- Called on posted entity events.
function OnEntityEvent(%1, entity, event)
end

-- Called on UI open event.
function OnUIOpen(%1, ui)
end

-- Called on UI close event.
function OnUIClose(%1, ui, result)
end

--Called on UI action event.
function OnUIAction(%1, ui, action)
end
    )");

    return source.replace("%1", scene);
}

} // bui
