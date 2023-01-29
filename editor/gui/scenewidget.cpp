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
#  include <base64/base64.h>
#  include <glm/glm.hpp>
#  include <glm/gtx/matrix_decompose.hpp>
#include "warnpop.h"

#include <algorithm>
#include <unordered_set>
#include <cmath>

#include "editor/app/eventlog.h"
#include "editor/app/utility.h"
#include "editor/app/workspace.h"
#include "editor/gui/settings.h"
#include "editor/gui/treewidget.h"
#include "editor/gui/treemodel.h"
#include "editor/gui/tool.h"
#include "editor/gui/scenewidget.h"
#include "editor/gui/scriptwidget.h"
#include "editor/gui/tilemapwidget.h"
#include "editor/gui/utility.h"
#include "editor/gui/drawing.h"
#include "editor/gui/dlgscriptvar.h"
#include "editor/gui/dlgentity.h"
#include "editor/gui/clipboard.h"
#include "base/assert.h"
#include "base/format.h"
#include "base/math.h"
#include "data/json.h"
#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/transform.h"
#include "graphics/drawing.h"
#include "graphics/types.h"
#include "game/treeop.h"

namespace gui
{

// todo: refactor this and the similar model from EntityWidget into
// some reusable class
class SceneWidget::ScriptVarModel : public QAbstractTableModel
{
public:
    ScriptVarModel(game::SceneClass& klass) : mClass(klass)
    {}
    virtual QVariant data(const QModelIndex& index, int role) const override
    {
        const auto& var = mClass.GetScriptVar(index.row());
        if (role == Qt::DisplayRole)
        {
            switch (index.column()) {
                case 0: return app::toString(var.GetType());
                case 1: return app::FromUtf8(var.GetName());
                case 2: return GetScriptVarData(var);
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
                case 0: return "Type";
                case 1: return "Name";
                case 2: return "Value";
                default: BUG("Unknown script variable data index.");
            }
        }
        return QVariant();
    }
    virtual int rowCount(const QModelIndex&) const override
    {
        return static_cast<int>(mClass.GetNumScriptVars());
    }
    virtual int columnCount(const QModelIndex&) const override
    {
        return 3;
    }
    void AddVariable(game::ScriptVar&& var)
    {
        const auto count = static_cast<int>(mClass.GetNumScriptVars());
        beginInsertRows(QModelIndex(), count, count);
        mClass.AddScriptVar(std::move(var));
        endInsertRows();
    }
    void EditVariable(size_t row, game::ScriptVar&& var)
    {
        mClass.SetScriptVar(row, std::move(var));
        emit dataChanged(index(row, 0), index(row, 3));
    }
    void DeleteVariable(size_t row)
    {
        beginRemoveRows(QModelIndex(), row, row);
        mClass.DeleteScriptVar(row);
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
            case game::ScriptVar::Type::Vec2: {
                if (!var.IsArray()) {
                    const auto& val = var.GetValue<glm::vec2>();
                    return QString("%1,%2")
                            .arg(QString::number(val.x, 'f', 2))
                            .arg(QString::number(val.y, 'f', 2));
                } else {
                    const auto& val = var.GetArray<glm::vec2>()[0];
                    return QString("[0]=%1,%2 ...")
                            .arg(QString::number(val.x, 'f', 2))
                            .arg(QString::number(val.y, 'f', 2));
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
                    if (const auto* node = mClass.FindNodeById(val.id))
                        return app::FromUtf8(node->GetName());
                    return "Nil";
                } else {
                    const auto& val = var.GetArray<game::ScriptVar::EntityReference>()[0];
                    if (const auto* node = mClass.FindNodeById(val.id))
                        return QString("[0]=%1 ...").arg(app::FromUtf8(node->GetName()));
                    return "[0]=Nil ...";
                }
                break;
        }
        BUG("Unknown ScriptVar type.");
        return QVariant();
    }
private:
    game::SceneClass& mClass;
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
    virtual void Render(gfx::Painter& painter, gfx::Transform& view) const override
    {
        DrawHook hook;

        view.Push();
            view.Translate(mWorldPos.x, mWorldPos.y);
            mState.renderer.Draw(*mClass, painter, view, &hook);
        view.Pop();

        ShowMessage(mClass->GetName(), gfx::FRect(40 + mMousePos.x(),
                                                  40 + mMousePos.y(), 200, 20), painter);
    }
    virtual void MouseMove(QMouseEvent* mickey, gfx::Transform& view) override
    {
        const auto& view_to_scene   = glm::inverse(view.GetAsMatrix());
        const auto& mouse_pos_view  = ToVec4(mickey->pos());
        const auto& mouse_pos_scene = view_to_scene * mouse_pos_view;
        mWorldPos = mouse_pos_scene;
        mMousePos = mickey->pos();
    }
    virtual void MousePress(QMouseEvent* mickey, gfx::Transform& view) override
    {
    }
    virtual bool MouseRelease(QMouseEvent* mickey, gfx::Transform& view) override
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
        game::SceneNodeClass node;
        node.SetEntity(mClass);
        node.SetName(name);
        node.SetScale(glm::vec2(1.0f, 1.0f));
        node.SetTranslation(glm::vec2(mWorldPos.x, mWorldPos.y));
        // leave this empty for the class default to take place.
        // node.SetIdleAnimationId(mClass->GetIdleTrackId());
        auto* child = mState.scene.AddNode(std::move(node));
        mState.scene.LinkChild(nullptr, child);
        mState.view->Rebuild();
        mState.view->SelectItemById(app::FromUtf8(child->GetId()));
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
    void AdjustPlacementPosition(const QPoint& mouse_pos, gfx::Transform& view)
    {
        const auto& view_to_scene   = glm::inverse(view.GetAsMatrix());
        const auto& mouse_pos_view  = ToVec4(mouse_pos);
        const auto& mouse_pos_scene = view_to_scene * mouse_pos_view;
        mWorldPos = mouse_pos_scene;
    }
private:
    std::string CreateName() const
    {
        QString name = mState.workspace->MapResourceIdToName(mEntityIds[mCurrentEntityIdIndex]);
        for (size_t i=0; i<10000; ++i)
        {
            QString suggestion = QString("%1_%2").arg(name).arg(i);
            if (mState.scene.FindNodeByName(app::ToUtf8(suggestion)) == nullptr)
                return app::ToUtf8(suggestion);
        }
        return "???";
    }
private:
    SceneWidget::State& mState;
    // the current entity position in scene coordinates of the placement
    // based on the mouse position at the time.
    glm::vec4 mWorldPos;
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
    // mouse position in window coordinates
    QPoint mMousePos;
};

SceneWidget::SceneWidget(app::Workspace* workspace) : mUndoStack(3)
{
    DEBUG("Create SceneWidget");

    mRenderTree.reset(new TreeModel(mState.scene));
    mScriptVarModel.reset(new ScriptVarModel(mState.scene));

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
    mUI.widget->onInitScene     = std::bind(&SceneWidget::InitScene, this, std::placeholders::_1,
                                            std::placeholders::_2);
    // the menu for adding entities in the scene.
    mEntities = new QMenu(this);
    mEntities->menuAction()->setIcon(QIcon("icons:entity.png"));
    mEntities->menuAction()->setText("Entities");

    mState.scene.SetName("My Scene");
    mState.workspace = workspace;
    mState.renderer.SetClassLibrary(workspace);
    mState.renderer.SetEditingMode(true);
    mState.view = mUI.tree;
    mOriginalHash = mState.scene.GetHash();

    // connect tree widget signals
    connect(mUI.tree, &TreeWidget::currentRowChanged, this, &SceneWidget::TreeCurrentNodeChangedEvent);
    connect(mUI.tree, &TreeWidget::dragEvent,  this, &SceneWidget::TreeDragEvent);
    connect(mUI.tree, &TreeWidget::clickEvent, this, &SceneWidget::TreeClickEvent);
    // connect workspace signals for resource management
    connect(workspace, &app::Workspace::NewResourceAvailable, this, &SceneWidget::NewResourceAvailable);
    connect(workspace, &app::Workspace::ResourceToBeDeleted,  this, &SceneWidget::ResourceToBeDeleted);
    connect(workspace, &app::Workspace::ResourceUpdated,      this, &SceneWidget::ResourceUpdated);

    PopulateFromEnum<game::SceneClass::SpatialIndex>(mUI.cmbSpatialIndex);
    PopulateFromEnum<GridDensity>(mUI.cmbGrid);
    SetValue(mUI.cmbGrid, GridDensity::Grid50x50);
    SetValue(mUI.zoom, 1.0f);
    SetValue(mUI.ID, mState.scene.GetId());
    SetValue(mUI.name, mState.scene.GetName());

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

    mState.scene = *content;
    mOriginalHash = mState.scene.GetHash();
    mScriptVarModel->Reset();

    GetUserProperty(resource, "zoom", mUI.zoom);
    GetUserProperty(resource, "grid", mUI.cmbGrid);
    GetUserProperty(resource, "snap", mUI.chkSnap);
    GetUserProperty(resource, "show_origin", mUI.chkShowOrigin);
    GetUserProperty(resource, "show_grid", mUI.chkShowGrid);
    GetUserProperty(resource, "show_viewport", mUI.chkShowViewport);
    GetUserProperty(resource, "widget", mUI.widget);
    GetUserProperty(resource, "camera_scale_x", mUI.scaleX);
    GetUserProperty(resource, "camera_scale_y", mUI.scaleY);
    GetUserProperty(resource, "camera_rotation", mUI.rotation);
    GetUserProperty(resource, "spatial_rect_xpos", mUI.spRectX);
    GetUserProperty(resource, "spatial_rect_ypos", mUI.spRectY);
    GetUserProperty(resource, "spatial_rect_width", mUI.spRectW);
    GetUserProperty(resource, "spatial_rect_height", mUI.spRectH);
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
    mCameraWasLoaded = true;

    UpdateResourceReferences();
    DisplayCurrentNodeProperties();
    DisplaySceneProperties();
    DisplayCurrentCameraLocation();

    mRenderTree.reset(new TreeModel(mState.scene));
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

void SceneWidget::Initialize(const UISettings& settings)
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
    bar.addSeparator();
    bar.addAction(mUI.actionStop);
    bar.addSeparator();
    bar.addAction(mUI.actionSave);
    bar.addSeparator();
    bar.addAction(mUI.actionNodePlace);
    bar.addSeparator();
    bar.addAction(mEntities->menuAction());
}
void SceneWidget::AddActions(QMenu& menu)
{
    menu.addAction(mUI.actionPlay);
    menu.addAction(mUI.actionPause);
    menu.addSeparator();
    menu.addAction(mUI.actionStop);
    menu.addSeparator();
    menu.addAction(mUI.actionSave);
    menu.addSeparator();
    menu.addAction(mUI.actionNodePlace);
    menu.addSeparator();
    menu.addAction(mEntities->menuAction());
}

bool SceneWidget::SaveState(Settings& settings) const
{
    data::JsonObject json;
    mState.scene.IntoJson(json);
    settings.SetValue("Scene", "content", json);
    settings.SetValue("Scene", "hash", mOriginalHash);
    settings.SetValue("Scene", "camera_offset_x", mState.camera_offset_x);
    settings.SetValue("Scene", "camera_offset_y", mState.camera_offset_y);
    settings.SaveWidget("Scene", mUI.scaleX);
    settings.SaveWidget("Scene", mUI.scaleY);
    settings.SaveWidget("Scene", mUI.rotation);
    settings.SaveWidget("Scene", mUI.chkShowOrigin);
    settings.SaveWidget("Scene", mUI.chkShowGrid);
    settings.SaveWidget("Scene", mUI.chkShowViewport);
    settings.SaveWidget("Scene", mUI.chkSnap);
    settings.SaveWidget("Scene", mUI.cmbGrid);
    settings.SaveWidget("Scene", mUI.zoom);
    settings.SaveWidget("Scene", mUI.widget);
    return true;
}
bool SceneWidget::LoadState(const Settings& settings)
{
    data::JsonObject json;
    settings.GetValue("Scene", "content", &json);
    settings.GetValue("Scene", "hash", &mOriginalHash);
    settings.GetValue("Scene", "camera_offset_x", &mState.camera_offset_x);
    settings.GetValue("Scene", "camera_offset_y", &mState.camera_offset_y);
    mCameraWasLoaded = true;

    settings.LoadWidget("Scene", mUI.scaleX);
    settings.LoadWidget("Scene", mUI.scaleY);
    settings.LoadWidget("Scene", mUI.rotation);
    settings.LoadWidget("Scene", mUI.chkShowOrigin);
    settings.LoadWidget("Scene", mUI.chkShowGrid);
    settings.LoadWidget("Scene", mUI.chkShowViewport);
    settings.LoadWidget("Scene", mUI.chkSnap);
    settings.LoadWidget("Scene", mUI.cmbGrid);
    settings.LoadWidget("Scene", mUI.zoom);
    settings.LoadWidget("Scene", mUI.widget);

    auto ret  = game::SceneClass::FromJson(json);
    mState.scene  = std::move(ret.value());

    UpdateResourceReferences();
    DisplaySceneProperties();
    DisplayCurrentNodeProperties();
    DisplayCurrentCameraLocation();

    mScriptVarModel->Reset();
    mRenderTree.reset(new TreeModel(mState.scene));
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
        case Actions::CanReloadShaders:
        case Actions::CanReloadTextures:
            return true;
    }
    BUG("Unhandled action query.");
    return false;
}

void SceneWidget::Cut(Clipboard& clipboard)
{
    if (auto* node = GetCurrentNode())
    {
        data::JsonObject json;
        const auto& tree = mState.scene.GetRenderTree();
        game::RenderTreeIntoJson(tree, [](data::Writer& writer, const auto* node) {
            node->IntoJson(writer);
        }, json, node);
        clipboard.SetType("application/json/scene_node");
        clipboard.SetText(json.ToString());

        NOTE("Copied JSON to application clipboard.");
        DEBUG("Copied scene node '%1' ('%2') to the clipboard.", node->GetId(), node->GetName());

        mState.scene.DeleteNode(node);
        mUI.tree->Rebuild();
        mUI.tree->ClearSelection();
    }
}
void SceneWidget::Copy(Clipboard& clipboard) const
{
    if (const auto* node = GetCurrentNode())
    {
        data::JsonObject json;
        const auto& tree = mState.scene.GetRenderTree();
        game::RenderTreeIntoJson(tree, [](data::Writer& writer, const auto* node) {
            node->IntoJson(writer);
         }, json, node);
        clipboard.SetType("application/json/scene_node");
        clipboard.SetText(json.ToString());

        NOTE("Copied JSON to application clipboard.");
        DEBUG("Copied scene node '%1' ('%2') to the clipboard.", node->GetId(), node->GetName());
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
    std::vector<std::unique_ptr<game::SceneNodeClass>> nodes;
    bool error = false;
    game::SceneClass::RenderTree tree;
    game::RenderTreeFromJson(tree, [&nodes, &error](const data::Reader& data) {
        auto ret = game::SceneNodeClass::FromJson(data);
        if (ret.has_value()) {
            auto node = std::make_unique<game::SceneNodeClass>(ret->Clone());
            node->SetName(base::FormatString("Copy of %1", ret->GetName()));
            nodes.push_back(std::move(node));
            return nodes.back().get();
        }
        error = true;
        return (game::SceneNodeClass*)nullptr;
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

    gfx::Transform view;
    view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
    view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
    view.Rotate(qDegreesToRadians((float)GetValue(mUI.rotation)));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);
    const auto& view_to_scene   = glm::inverse(view.GetAsMatrix());
    const auto& mouse_pos_view  = ToVec4(mickey);
    const auto& mouse_pos_scene = view_to_scene * mouse_pos_view;

    auto* paste_root = nodes[0].get();
    paste_root->SetTranslation(glm::vec2(mouse_pos_scene.x, mouse_pos_scene.y));
    tree.LinkChild(nullptr, paste_root);

    // if we got this far, nodes should contain the nodes to be added
    // into the scene and tree should contain their hierarchy.
    for (auto& node : nodes)
    {
        // moving the unique ptr means that node address stays the same
        // thus the tree is still valid!
        node->SetEntity(mState.workspace->FindEntityClassById(node->GetEntityId()));
        mState.scene.AddNode(std::move(node));
    }
    nodes.clear();
    // walk the tree and link the nodes into the scene.
    tree.PreOrderTraverseForEach([&nodes, this, &tree](game::SceneNodeClass* node) {
        if (node == nullptr)
            return;
        auto* parent = tree.GetParent(node);
        mState.scene.LinkChild(parent, node);
    });

    mUI.tree->Rebuild();
    mUI.tree->SelectItemById(app::FromUtf8(paste_root->GetId()));
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
    if (mUndoStack.back().GetHash() == mState.scene.GetHash())
        mUndoStack.pop_back();

    mState.scene = mUndoStack.back();
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
    mUI.widget->dispose();
}
void SceneWidget::Update(double secs)
{
    if (mPlayState == PlayState::Playing)
    {
        mState.renderer.Update(mState.scene, mSceneTime, secs);
        mSceneTime += secs;
    }
    mCurrentTime += secs;
}
void SceneWidget::Render()
{
    mUI.widget->triggerPaint();
}

bool SceneWidget::HasUnsavedChanges() const
{
    if (mOriginalHash == mState.scene.GetHash())
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
    else mUI.tree->ClearSelection();
    return true;
}

void SceneWidget::Refresh()
{
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
        mUndoStack.push_back(mState.scene);
    }
    const auto curr_hash = mState.scene.GetHash();
    const auto undo_hash = mUndoStack.back().GetHash();
    if (curr_hash != undo_hash)
    {
        mUndoStack.push_back(mState.scene);
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

void SceneWidget::on_name_textChanged(const QString&)
{
    mState.scene.SetName(GetValue(mUI.name));
}

void SceneWidget::on_cmbScripts_currentIndexChanged(int)
{
    mState.scene.SetScriptFileId(GetItemId(mUI.cmbScripts));
    SetEnabled(mUI.btnEditScript, true);
}

void SceneWidget::on_cmbTilemaps_currentIndexChanged(int)
{
    mState.scene.SetTilemapId(GetItemId(mUI.cmbTilemaps));
    SetEnabled(mUI.btnEditMap, true);
}

void SceneWidget::on_cmbSpatialIndex_currentIndexChanged(int)
{
    // Set the values based on what is currently in UI
    SetSpatialIndexParams();
    // then display appropriately (enable/disable the right stuff)
    DisplaySceneProperties();
}

void SceneWidget::on_spRectX_valueChanged(double value)
{
    SetSpatialIndexParams();
}
void SceneWidget::on_spRectY_valueChanged(double value)
{
    SetSpatialIndexParams();
}
void SceneWidget::on_spRectW_valueChanged(double value)
{
    SetSpatialIndexParams();
}
void SceneWidget::on_spRectH_valueChanged(double value)
{
    SetSpatialIndexParams();
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
void SceneWidget::on_chkLeftBoundary_stateChanged(int)
{
    SetEnabled(mUI.spinLeftBoundary, GetValue(mUI.chkLeftBoundary));
    SetSceneBoundary();
}
void SceneWidget::on_chkRightBoundary_stateChanged(int)
{
    SetEnabled(mUI.spinRightBoundary, GetValue(mUI.chkRightBoundary));
    SetSceneBoundary();
}
void SceneWidget::on_chkTopBoundary_stateChanged(int)
{
    SetEnabled(mUI.spinTopBoundary, GetValue(mUI.chkTopBoundary));
    SetSceneBoundary();
}
void SceneWidget::on_chkBottomBoundary_stateChanged(int)
{
    SetEnabled(mUI.spinBottomBoundary, GetValue(mUI.chkBottomBoundary));
    SetSceneBoundary();
}
void SceneWidget::on_spinLeftBoundary_valueChanged(double value)
{
    SetSceneBoundary();
}
void SceneWidget::on_spinRightBoundary_valueChanged(double value)
{
    SetSceneBoundary();
}
void SceneWidget::on_spinTopBoundary_valueChanged(double value)
{
    SetSceneBoundary();
}
void SceneWidget::on_spinBottomBoundary_valueChanged(double value)
{
    SetSceneBoundary();
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
    SetUserProperty(resource, "show_origin", mUI.chkShowOrigin);
    SetUserProperty(resource, "show_grid", mUI.chkShowGrid);
    SetUserProperty(resource, "show_viewport", mUI.chkShowViewport);
    SetUserProperty(resource, "widget", mUI.widget);
    SetUserProperty(resource, "spatial_rect_xpos", mUI.spRectX);
    SetUserProperty(resource, "spatial_rect_ypos", mUI.spRectY);
    SetUserProperty(resource, "spatial_rect_width", mUI.spRectW);
    SetUserProperty(resource, "spatial_rect_height", mUI.spRectH);
    SetUserProperty(resource, "quadtree_max_items", mUI.spQuadMaxItems);
    SetUserProperty(resource, "quadtree_max_levels", mUI.spQuadMaxLevels);
    SetUserProperty(resource, "densegrid_num_rows", mUI.spDenseGridRows);
    SetUserProperty(resource, "densegrid_num_cols", mUI.spDenseGridCols);
    SetUserProperty(resource, "left_boundary", mUI.spinLeftBoundary);
    SetUserProperty(resource, "right_boundary", mUI.spinRightBoundary);
    SetUserProperty(resource, "top_boundary", mUI.spinTopBoundary);
    SetUserProperty(resource, "bottom_boundary", mUI.spinBottomBoundary);

    mState.workspace->SaveResource(resource);
    mOriginalHash = mState.scene.GetHash();
}

void SceneWidget::on_actionNodeEdit_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        auto klass = node->GetEntityClass();
        if (!klass)
        {
            NOTE("Node has no entity klass set.");
            return;
        }
        DlgEntity dlg(this, *klass, *node);
        dlg.exec();
    }
}

void SceneWidget::on_actionNodeDelete_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        mState.scene.DeleteNode(node);

        mUI.tree->Rebuild();
        mUI.tree->ClearSelection();
    }
}

void SceneWidget::on_actionNodeBreakLink_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        node->SetParentRenderTreeNodeId("");
        mState.scene.BreakChild(node);
        mState.scene.LinkChild(nullptr, node);
        mUI.tree->Rebuild();
    }
}

void SceneWidget::on_actionNodePlace_triggered()
{
    const auto snap = (bool)GetValue(mUI.chkSnap);
    const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
    const auto grid_size = (unsigned)grid;
    auto tool = std::make_unique<PlaceEntityTool>(mState, snap, grid_size);

    gfx::Transform view;
    MakeViewTransform(mUI, mState, view);

    tool->AdjustPlacementPosition(mUI.widget->mapFromGlobal(QCursor::pos()), view);

    mCurrentTool = std::move(tool);
    mUI.actionNodePlace->setChecked(true);
}

void SceneWidget::on_actionNodeDuplicate_triggered()
{
    if (const auto* node = GetCurrentNode())
    {
        auto* dupe = mState.scene.DuplicateNode(node);
        // update the translation for the parent of the new hierarchy
        // so that it's possible to tell it apart from the source of the copy.
        dupe->SetTranslation(node->GetTranslation() * 1.2f);

        mState.view->Rebuild();
        mState.view->SelectItemById(app::FromUtf8(dupe->GetId()));
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

void SceneWidget::on_actionEntityVarRef_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        std::vector<ResourceListItem> entities;
        std::vector<ResourceListItem> nodes;
        for (size_t i = 0; i < mState.scene.GetNumNodes(); ++i)
        {
            const auto& node = mState.scene.GetNode(i);
            ResourceListItem item;
            item.name = app::FromUtf8(node.GetName());
            item.id   = app::FromUtf8(node.GetId());
            entities.push_back(std::move(item));
        }
        QString name = app::FromUtf8(node->GetName());
        name = name.replace(' ', '_');
        name = name.toLower();
        game::ScriptVar::EntityReference  ref;
        ref.id = node->GetId();

        game::ScriptVar var(app::ToUtf8(name), ref);
        DlgScriptVar dlg(nodes, entities, this, var);
        if (dlg.exec() == QDialog::Rejected)
            return;

        mScriptVarModel->AddVariable(std::move(var));
        SetEnabled(mUI.btnEditScriptVar, true);
        SetEnabled(mUI.btnDeleteScriptVar, true);
    }
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
    mState.scene.ResetScriptFile();
    SetValue(mUI.cmbScripts, -1);
    SetEnabled(mUI.btnEditScript, false);
}
void SceneWidget::on_btnAddScript_clicked()
{
    app::Script script;
    // use the script ID as the file name so that we can
    // avoid naming clashes and always find the correct lua
    // file even if the entity is later renamed.
    const auto& filename = app::FromUtf8(script.GetId());
    const auto& fileuri  = QString("ws://lua/%1.lua").arg(filename);
    const auto& filepath = mState.workspace->MapFileToFilesystem(fileuri);
    const auto& name = GetValue(mUI.name);
    const QFileInfo info(filepath);
    if (info.exists())
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Question);
        msg.setWindowTitle(tr("File already exists"));
        msg.setText(tr("Overwrite existing script file?\n%1").arg(filepath));
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
        if (msg.exec() == QMessageBox::Cancel)
            return;
    }

    QFile io;
    io.setFileName(filepath);
    if (!io.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        ERROR("Failed to open '%1' for writing (%2)", filepath, io.error());
        ERROR(io.errorString());
        QMessageBox msg;
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowTitle(tr("Error Occurred"));
        msg.setText(tr("There was a problem creating the script file.\n%1").arg(io.errorString()));
        msg.setStandardButtons(QMessageBox::Ok);
        return;
    }
    QString var = name;
    var.replace(' ', '_');
    var = var.toLower();

    // TODO: refactor this and a dupe from MainWindow into a single place.
    QTextStream stream(&io);
    stream.setCodec("UTF-8");
    stream << QString("-- Scene '%1' script.\n\n").arg(name);
    stream << QString("-- This script will be called for every instance of '%1'\n"
                      "-- during gameplay.\n").arg(name);
    stream << "-- You're free to delete functions you don't need.\n\n";
    stream << "-- Called when the scene begins play.\n";
    stream << QString("function BeginPlay(%1)\nend\n\n").arg(var);
    stream << "-- Called when the scene ends play.\n";
    stream << QString("function EndPlay(%1)\nend\n\n").arg(var);
    stream << "-- Called when a new entity has been spawned in the scene.\n";
    stream << QString("function SpawnEntity(%1, entity)\n\nend\n\n").arg(var);
    stream << "-- Called when an entity has been killed from the scene.\n";
    stream << QString("function KillEntity(%1, entity)\n\nend\n\n").arg(var);
    stream << "-- Called on every low frequency game tick.\n";
    stream << QString("function Tick(%1, game_time, dt)\n\nend\n\n").arg(var);
    stream << "-- Called on every iteration of game loop.\n";
    stream << QString("function Update(%1, game_time, dt)\n\nend\n\n").arg(var);
    stream << "-- Called on collision events with other objects.\n";
    stream << QString("function OnBeginContact(%1, entity, entity_node, other, other_node)\nend\n\n").arg(var);
    stream << "-- Called on collision events with other objects.\n";
    stream << QString("function OnEndContact(%1, entity, entity_node, other, other_node)\nend\n\n").arg(var);
    stream << "-- Called on key down events.\n";
    stream << QString("function OnKeyDown(%1, symbol, modifier_bits)\nend\n\n").arg(var);
    stream << "-- Called on key up events.\n";
    stream << QString("function OnKeyUp(%1, symbol, modifier_bits)\nend\n\n").arg(var);
    stream << "-- Called on mouse button press events.\n";
    stream << QString("function OnMousePress(%1, mouse)\nend\n\n").arg(var);
    stream << "-- Called on mouse button release events.\n";
    stream << QString("function OnMouseRelease(%1, mouse)\nend\n\n").arg(var);
    stream << "-- Called on mouse move events.\n";
    stream << QString("function OnMouseMove(%1, mouse)\nend\n\n").arg(var);
    stream << "-- Called on game events.\n";
    stream << QString("function OnGameEvent(%1, event)\nend\n\n").arg(var);
    stream << "-- Called on entity timer events.\n";
    stream << QString("function OnEntityTimer(%1, entity, timer, jitter)\nend\n\n").arg(var);
    stream << "-- Called on posted entity events.\n";
    stream << QString("function OnEntityEvent(%1, entity, event)\nend\n\n").arg(var);
    stream << "-- Called on UI open event.\n";
    stream << QString("function OnUIOpen(%1, ui)\nend\n\n").arg(var);
    stream << "-- Called on UI close event.\n";
    stream << QString("function OnUIClose(%1, ui, result)\nend\n\n").arg(var);
    stream << "--Called on UI action event.\n";
    stream << QString("function OnUIAction(%1, ui, action)\nend\n\n").arg(var);

    io.flush();
    io.close();

    script.SetFileURI(app::ToUtf8(fileuri));
    app::ScriptResource resource(script, name);
    mState.workspace->SaveResource(resource);
    mState.scene.SetScriptFileId(script.GetId());

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
    mState.scene.SetTilemapId(app::ToUtf8(id));
    SetValue(mUI.cmbTilemaps, ListItemId(id));
    SetEnabled(mUI.btnEditMap, true);
}

void SceneWidget::on_btnResetMap_clicked()
{
    mState.scene.ResetTilemap();
    SetValue(mUI.cmbTilemaps, -1);
    SetEnabled(mUI.btnEditMap, false);
    mTilemap.reset();
}

void SceneWidget::on_btnNewScriptVar_clicked()
{
    std::vector<ResourceListItem> entities;
    std::vector<ResourceListItem> nodes;
    for (size_t i=0; i<mState.scene.GetNumNodes(); ++i)
    {
        const auto& node = mState.scene.GetNode(i);
        ResourceListItem item;
        item.name = app::FromUtf8(node.GetName());
        item.id   = app::FromUtf8(node.GetId());
        entities.push_back(std::move(item));
    }

    game::ScriptVar var("My_Var", std::string(""));
    DlgScriptVar dlg(nodes, entities, this, var);
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
    for (size_t i=0; i<mState.scene.GetNumNodes(); ++i)
    {
        const auto& node = mState.scene.GetNode(i);
        ResourceListItem item;
        item.name = app::FromUtf8(node.GetName());
        item.id   = app::FromUtf8(node.GetId());
        entities.push_back(std::move(item));
    }

    // single selection for now.
    const auto index = items[0];
    game::ScriptVar var = mState.scene.GetScriptVar(index.row());
    DlgScriptVar dlg(nodes, entities, this, var);
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
    const auto vars = mState.scene.GetNumScriptVars();
    SetEnabled(mUI.btnEditScriptVar, vars > 0);
    SetEnabled(mUI.btnDeleteScriptVar, vars > 0);
}

void SceneWidget::on_btnViewPlus90_clicked()
{
    const float value = GetValue(mUI.rotation);
    mUI.rotation->setValue(math::clamp(-180.0f, 180.0f, value + 90.0f));
    mViewTransformRotation  = value;
    mViewTransformStartTime = mCurrentTime;
}

void SceneWidget::on_btnViewMinus90_clicked()
{
    const float value = GetValue(mUI.rotation);
    mUI.rotation->setValue(math::clamp(-180.0f, 180.0f, value - 90.0f));
    mViewTransformRotation  = value;
    mViewTransformStartTime = mCurrentTime;
}

void SceneWidget::on_btnViewReset_clicked()
{
    const auto width = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto rotation = mUI.rotation->value();
    mState.camera_offset_x = width * 0.5f;
    mState.camera_offset_y = height * 0.5f;
    mViewTransformRotation = rotation;
    mViewTransformStartTime = mCurrentTime;
    // set new camera offset to the center of the widget.
    mUI.translateX->setValue(0);
    mUI.translateY->setValue(0);
    mUI.scaleX->setValue(1.0f);
    mUI.scaleY->setValue(1.0f);
    mUI.rotation->setValue(0);
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
    auto* node = static_cast<game::SceneNodeClass*>(item->GetUserData());
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

        const auto visible_in_game = node->TestFlag(game::SceneNodeClass::Flags::VisibleInGame);
        const auto visible_in_editor = node->TestFlag(game::SceneNodeClass::Flags::VisibleInEditor);
        // reset the entity instance parameters to defaults since the
        // entity class has changed. only save the flags that are changed
        // through the this UI.
        node->ResetEntityParams();
        node->SetFlag(game::SceneNodeClass::Flags::VisibleInGame, visible_in_game);
        node->SetFlag(game::SceneNodeClass::Flags::VisibleInEditor, visible_in_editor);
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
        node->SetFlag(game::SceneNodeClass::Flags::VisibleInGame, GetValue(mUI.nodeIsVisible));
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

        DlgEntity dlg(this, *klass, *node);
        dlg.exec();
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
    const auto& tree = mState.scene.GetRenderTree();
    const auto* node = GetCurrentNode();
    mUI.actionNodeDuplicate->setEnabled(node != nullptr);
    mUI.actionNodeDelete->setEnabled(node != nullptr);
    mUI.actionNodeMoveDownLayer->setEnabled(node != nullptr);
    mUI.actionNodeMoveUpLayer->setEnabled(node != nullptr);
    mUI.actionNodeBreakLink->setEnabled(node != nullptr && tree.GetParent(node));
    mUI.actionNodeEdit->setEnabled(node != nullptr);
    mUI.actionEntityVarRef->setEnabled(node != nullptr);

    QMenu menu(this);
    menu.addAction(mUI.actionNodeMoveUpLayer);
    menu.addAction(mUI.actionNodeMoveDownLayer);
    menu.addAction(mUI.actionNodeDuplicate);
    menu.addAction(mUI.actionNodeBreakLink);
    menu.addSeparator();
    menu.addAction(mUI.actionNodeEdit);
    menu.addSeparator();
    menu.addAction(mUI.actionEntityVarRef);
    menu.addSeparator();
    menu.addAction(mUI.actionNodeDelete);
    menu.exec(QCursor::pos());
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
    auto& tree = mState.scene.GetRenderTree();
    auto* src_value = static_cast<game::SceneNodeClass*>(item->GetUserData());
    auto* dst_value = static_cast<game::SceneNodeClass*>(target->GetUserData());

    // check if we're trying to drag a parent onto its own child
    if (game::SearchChild(tree, dst_value, src_value))
        return;

    const bool retain_world_transform = true;
    mState.scene.ReparentChild(dst_value, src_value, retain_world_transform);

}
void SceneWidget::TreeClickEvent(TreeWidget::TreeItem* item)
{
    if (auto* node = GetCurrentNode())
    {
        const bool visibility = !node->TestFlag(game::SceneNodeClass::Flags::VisibleInEditor);
        node->SetFlag(game::SceneNodeClass::Flags::VisibleInEditor, visibility);
        item->SetIcon(visibility ? QIcon("icons:eye.png") : QIcon("icons:crossed_eye.png"));
        mUI.tree->Update();
    }
}

void SceneWidget::NewResourceAvailable(const app::Resource* resource)
{
    RebuildCombos();
    RebuildMenus();
    DisplaySceneProperties();
    DisplayCurrentNodeProperties();
}
void SceneWidget::ResourceToBeDeleted(const app::Resource* resource)
{
    UpdateResourceReferences();
    RebuildCombos();
    RebuildMenus();
    DisplayCurrentNodeProperties();
    DisplaySceneProperties();
}
void SceneWidget::ResourceUpdated(const app::Resource* resource)
{
    RebuildCombos();
    RebuildMenus();

    if (!resource->IsTilemap())
        return;

    if (!mState.scene.HasTilemap())
        return;

    const auto& mapId = mState.scene.GetTilemapId();
    if (mapId != resource->GetIdUtf8())
        return;

    // if the tilemap this scene refers to was just modified
    // then force a re-load by resetting the map object.
    mTilemap.reset();
}
void SceneWidget::InitScene(unsigned width, unsigned height)
{
    if (!mCameraWasLoaded)
    {
        // if the camera hasn't been loaded then compute now the
        // initial position for the camera.
        mState.camera_offset_x = mUI.widget->width() * 0.5;
        mState.camera_offset_y = mUI.widget->height() * 0.5;
    }
    DisplayCurrentCameraLocation();
}

void SceneWidget::PaintScene(gfx::Painter& painter, double /*secs*/)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto zoom   = (float)GetValue(mUI.zoom);
    const auto xs     = (float)GetValue(mUI.scaleX);
    const auto ys     = (float)GetValue(mUI.scaleY);
    const auto grid   = (GridDensity)GetValue(mUI.cmbGrid);
    const auto view_rotation_time = math::clamp(0.0, 1.0, mCurrentTime - mViewTransformStartTime);
    const auto view_rotation_angle = math::interpolate(mViewTransformRotation, (float)mUI.rotation->value(),
        view_rotation_time, math::Interpolation::Cosine);

    SetValue(mUI.widgetColor, mUI.widget->GetCurrentClearColor());

    // setup the view transform.
    gfx::Transform view;
    view.Scale(xs, ys);
    view.Scale(zoom, zoom);
    view.Rotate(qDegreesToRadians(view_rotation_angle));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    painter.SetViewport(0, 0, width, height);
    painter.SetPixelRatio(glm::vec2(xs*zoom, ys*zoom));
    painter.ResetViewMatrix();

    // render endless background grid.
    if (GetValue(mUI.chkShowGrid))
    {
        DrawCoordinateGrid(painter, view, grid, zoom, xs, ys, width, height);
    }

    // setup a viewport rect for culling draw packets against
    // draw packets which don't intersect with the viewrect are culled
    // for improved perf.
    const game::FRect viewport(0, 0, width, height);

    DrawHook hook(GetCurrentNode(), viewport);
    hook.SetIsPlaying(mPlayState == PlayState::Playing);
    hook.SetDrawVectors(true);
    hook.SetViewMatrix(view.GetAsMatrix());

    if (mState.scene.HasTilemap())
    {
        const auto& mapId = mState.scene.GetTilemapId();
        if (!mTilemap || mTilemap->GetClassId() != mapId)
        {
            auto klass = mState.workspace->GetTilemapClassById(mapId);
            mTilemap = game::CreateTilemap(klass);
            mTilemap->Load(*mState.workspace, 1024);
        }
    }

    painter.SetViewMatrix(view.GetAsMatrix());
    gfx::Transform scene;
    mState.renderer.BeginFrame();

    if (mTilemap)
    {
        mState.renderer.Draw(*mTilemap, viewport, painter, scene);
    }

    mState.renderer.Draw(mState.scene, painter, scene, &hook, &hook);
    mState.renderer.EndFrame();
    painter.ResetViewMatrix();

    if (mCurrentTool)
        mCurrentTool->Render(painter, view);

    // right arrow
    if (GetValue(mUI.chkShowOrigin))
    {
        DrawBasisVectors(painter, view);
    }

    if (GetValue(mUI.chkShowViewport))
    {
        const auto& settings = mState.workspace->GetProjectSettings();
        const float game_width = settings.viewport_width;
        const float game_height = settings.viewport_height;
        DrawViewport(painter, view, game_width, game_height, width, height);
    }

    PrintMousePos(view, painter, mUI.widget);
}

void SceneWidget::MouseMove(QMouseEvent* mickey)
{
    if (mCurrentTool)
    {
        gfx::Transform view;
        view.Scale(GetValue(mUI.scaleX) , GetValue(mUI.scaleY));
        view.Scale(GetValue(mUI.zoom) , GetValue(mUI.zoom));
        view.Rotate(qDegreesToRadians(mUI.rotation->value()));
        view.Translate(mState.camera_offset_x , mState.camera_offset_y);
        mCurrentTool->MouseMove(mickey , view);
        // update the properties that might have changed as the result of application
        // of the current tool.
        DisplayCurrentCameraLocation();
        DisplayCurrentNodeProperties();
    }
}
void SceneWidget::MousePress(QMouseEvent* mickey)
{
    gfx::Transform view;
    view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
    view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
    view.Rotate((float)qDegreesToRadians(mUI.rotation->value()));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

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
            const auto& entity = current->GetEntityClass();
            const auto& box    = entity->GetBoundingRect();
            view.Push(mState.scene.FindNodeTransform(current));

            const auto hotspot = TestToolHotspot(view, box, glm::vec4(click_point.x(), click_point.y(), 0.0f, 1.0));
            if (hotspot == ToolHotspot::Resize)
                mCurrentTool.reset(new ScaleRenderTreeNodeTool(mState.scene, current));
            else if (hotspot == ToolHotspot::Rotate)
                mCurrentTool.reset(new RotateRenderTreeNodeTool(mState.scene, current));
            view.Pop();
        }
        if (!mCurrentTool)
        {
            if (auto* selection = SelectNode(click_point))
            {
                mCurrentTool.reset(new MoveRenderTreeNodeTool(mState.scene, selection, snap, grid_size));
                mUI.tree->SelectItemById(app::FromUtf8(selection->GetId()));
            }
            else
            {
                mUI.tree->ClearSelection();
            }
        }
    }
    else if (!mCurrentTool && (mickey->button() == Qt::RightButton))
    {
        mCurrentTool.reset(new MoveCameraTool(mState));
    }

    if (mCurrentTool)
        mCurrentTool->MousePress(mickey, view);
}
void SceneWidget::MouseRelease(QMouseEvent* mickey)
{
    if (!mCurrentTool)
        return;

    gfx::Transform view;
    view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
    view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
    view.Rotate(qDegreesToRadians(mUI.rotation->value()));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    if (mCurrentTool->MouseRelease(mickey, view))
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

    DlgEntity dlg(this, *entity_klass, *scene_node);
    dlg.exec();
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
        case Qt::Key_W:
            TranslateCamera(0.0f, 20.0f);
            break;
        case Qt::Key_S:
            TranslateCamera(0.0f, -20.0f);
            break;
        case Qt::Key_A:
            TranslateCamera(20.0f, 0.0);
            break;
        case Qt::Key_D:
            TranslateCamera(-20.0f, 0.0f);
            break;
        case Qt::Key_Left:
            TranslateCurrentNode(-20.0f, 0.0f);
            break;
        case Qt::Key_Right:
            TranslateCurrentNode(20.0f, 0.0f);
            break;
        case Qt::Key_Up:
            TranslateCurrentNode(0.0f, -20.0f);
            break;
        case Qt::Key_Down:
            TranslateCurrentNode(0.0f, 20.0f);
            break;
        case Qt::Key_Escape:
            if (mCurrentTool)
            {
                mCurrentTool.reset();
                UncheckPlacementActions();
            }
            else mUI.tree->ClearSelection();
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
        SetValue(mUI.nodeIsVisible, node->TestFlag(game::SceneNodeClass::Flags::VisibleInGame));
        SetValue(mUI.nodeTranslateX, translate.x);
        SetValue(mUI.nodeTranslateY, translate.y);
        SetValue(mUI.nodeScaleX, scale.x);
        SetValue(mUI.nodeScaleY, scale.y);
        SetValue(mUI.nodeRotation, qRadiansToDegrees(node->GetRotation()));

        int link_index = -1;
        if (const auto* parent = mState.scene.GetRenderTree().GetParent(node))
        {
            const auto& klass  = parent->GetEntityClass();
            for (size_t i=0; i<klass->GetNumNodes(); ++i) {
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
    const auto vars = mState.scene.GetNumScriptVars();
    SetEnabled(mUI.btnEditScriptVar, vars > 0);
    SetEnabled(mUI.btnDeleteScriptVar, vars > 0);
    SetValue(mUI.name, mState.scene.GetName());
    SetValue(mUI.ID, mState.scene.GetId());
    SetValue(mUI.cmbScripts, ListItemId(mState.scene.GetScriptFileId()));
    SetValue(mUI.cmbTilemaps, ListItemId(mState.scene.GetTilemapId()));
    SetValue(mUI.cmbSpatialIndex, mState.scene.GetDynamicSpatialIndex());
    SetEnabled(mUI.btnEditScript, mState.scene.HasScriptFile());
    SetEnabled(mUI.btnEditMap, mState.scene.HasTilemap());

    const auto index = mState.scene.GetDynamicSpatialIndex();
    if (index == game::SceneClass::SpatialIndex::Disabled)
    {
        SetEnabled(mUI.spatialIndex, false);
    }
    else if (index == game::SceneClass::SpatialIndex::QuadTree)
    {
        SetEnabled(mUI.spatialIndex, true);
        SetEnabled(mUI.spQuadMaxLevels, true);
        SetEnabled(mUI.spQuadMaxItems,  true);
        SetEnabled(mUI.spDenseGridCols, false);
        SetEnabled(mUI.spDenseGridRows, false);
    }
    else if (index == game::SceneClass::SpatialIndex::DenseGrid)
    {
        SetEnabled(mUI.spatialIndex, true);
        SetEnabled(mUI.spQuadMaxLevels, false);
        SetEnabled(mUI.spQuadMaxItems,  false);
        SetEnabled(mUI.spDenseGridCols, true);
        SetEnabled(mUI.spDenseGridRows, true);
    }

    if (const auto* rect = mState.scene.GetDynamicSpatialRect())
    {
        SetValue(mUI.spRectX, rect->GetX());
        SetValue(mUI.spRectY, rect->GetY());
        SetValue(mUI.spRectW, rect->GetWidth());
        SetValue(mUI.spRectH, rect->GetHeight());
    }

    if (const auto* ptr = mState.scene.GetQuadTreeArgs())
    {
        SetValue(mUI.spQuadMaxLevels, ptr->max_levels);
        SetValue(mUI.spQuadMaxItems, ptr->max_items);
    }

    if (const auto* ptr = mState.scene.GetDenseGridArgs())
    {
        SetValue(mUI.spDenseGridRows, ptr->num_rows);
        SetValue(mUI.spDenseGridCols, ptr->num_cols);
    }

    SetValue(mUI.chkLeftBoundary,   false);
    SetValue(mUI.chkRightBoundary,  false);
    SetValue(mUI.chkTopBoundary,    false);
    SetValue(mUI.chkBottomBoundary, false);
    SetEnabled(mUI.spinLeftBoundary,   false);
    SetEnabled(mUI.spinRightBoundary,  false);
    SetEnabled(mUI.spinTopBoundary,    false);
    SetEnabled(mUI.spinBottomBoundary, false);

    if (const auto* ptr = mState.scene.GetLeftBoundary())
    {
        SetValue(mUI.spinLeftBoundary, *ptr);
        SetValue(mUI.chkLeftBoundary, true);
        SetEnabled(mUI.spinLeftBoundary, true);
    }
    if (const auto* ptr = mState.scene.GetRightBoundary())
    {
        SetValue(mUI.spinRightBoundary, *ptr);
        SetValue(mUI.chkRightBoundary, true);
        SetEnabled(mUI.spinRightBoundary, true);
    }
    if (const auto* ptr = mState.scene.GetTopBoundary())
    {
        SetValue(mUI.spinTopBoundary, *ptr);
        SetValue(mUI.chkTopBoundary, true);
        SetEnabled(mUI.spinTopBoundary, true);
    }
    if (const auto* ptr = mState.scene.GetBottomBoundary())
    {
        SetValue(mUI.spinBottomBoundary, *ptr);
        SetValue(mUI.chkBottomBoundary, true);
        SetEnabled(mUI.spinBottomBoundary, true);
    }
}

void SceneWidget::DisplayCurrentCameraLocation()
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto dist_x = mState.camera_offset_x - (width / 2.0f);
    const auto dist_y = mState.camera_offset_y - (height / 2.0f);
    SetValue(mUI.translateX, dist_x);
    SetValue(mUI.translateY, dist_y);
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
    for (size_t i=0; i<mState.workspace->GetNumResources(); ++i)
    {
        const auto& resource = mState.workspace->GetResource(i);
        const auto& name     = resource.GetName();
        const auto& id       = resource.GetId();
        if (resource.GetType() == app::Resource::Type::Entity)
        {
            QAction* action = mEntities->addAction(name);
            action->setData(id);
            connect(action, &QAction::triggered, this, &SceneWidget::PlaceNewEntity);
        }
    }
    SetEnabled(mEntities, !mEntities->isEmpty());
    SetEnabled(mUI.actionNodePlace, !mEntities->isEmpty());
}

void SceneWidget::RebuildCombos()
{
    SetList(mUI.nodeEntity, mState.workspace->ListUserDefinedEntities());
    SetList(mUI.cmbScripts, mState.workspace->ListUserDefinedScripts());
    SetList(mUI.cmbTilemaps, mState.workspace->ListUserDefinedMaps());
}

void SceneWidget::UpdateResourceReferences()
{
    for (size_t i=0; i<mState.scene.GetNumNodes(); ++i)
    {
        auto& node = mState.scene.GetNode(i);
        auto klass = mState.workspace->FindEntityClassById(node.GetEntityId());
        if (!klass)
        {
            WARN("Scene node '%1' refers to an entity ('%2') that is no longer available.",
                 node.GetName(), node.GetEntityId());
            node.ResetEntity();
            node.ResetEntityParams();
            continue;
        }
        // clear any script value that is no longer part of the entity class.
        node.ClearStaleScriptValues(*klass);
        // resolve the runtime entity klass object reference.
        node.SetEntity(klass);
    }
    if (mState.scene.HasScriptFile())
    {
        const auto& scriptId = mState.scene.GetScriptFileId();
        if (!mState.workspace->IsValidScript(scriptId))
        {
            WARN("Scene script is no longer available. [script='%1']", scriptId);
            mState.scene.ResetScriptFile();
            SetEnabled(mUI.btnEditScript, false);
        }
    }
    if (mState.scene.HasTilemap())
    {
        const auto& mapId = mState.scene.GetTilemapId();
        if (!mState.workspace->IsValidTilemap(mapId))
        {
            WARN("Scene tilemap is no longer available. [map='%1']", mapId);
            mState.scene.ResetTilemap();
            SetEnabled(mUI.btnEditMap, false);
        }
    }
}

void SceneWidget::SetSpatialIndexParams()
{
    mState.scene.SetDynamicSpatialIndex(GetValue(mUI.cmbSpatialIndex));

    if (const auto* ptr = mState.scene.GetDynamicSpatialRect())
    {
        auto rect = *ptr;
        rect.SetX(GetValue(mUI.spRectX));
        rect.SetY(GetValue(mUI.spRectY));
        rect.SetWidth(GetValue(mUI.spRectW));
        rect.SetHeight(GetValue(mUI.spRectH));
        mState.scene.SetDynamicSpatialRect(rect);
    }
    if (const auto* ptr = mState.scene.GetQuadTreeArgs())
    {
        auto args = *ptr;
        args.max_levels = GetValue(mUI.spQuadMaxLevels);
        args.max_items  = GetValue(mUI.spQuadMaxItems);
        mState.scene.SetDynamicSpatialIndexArgs(args);
    }
    if (const auto* ptr = mState.scene.GetDenseGridArgs())
    {
        auto args = *ptr;
        args.num_cols = GetValue(mUI.spDenseGridCols);
        args.num_rows = GetValue(mUI.spDenseGridRows);
        mState.scene.SetDynamicSpatialIndexArgs(args);
    }
}

void SceneWidget::SetSceneBoundary()
{
    mState.scene.ResetLeftBoundary();
    mState.scene.ResetRightBoundary();
    mState.scene.ResetTopBoundary();
    mState.scene.ResetBottomBoundary();

    if (GetValue(mUI.chkLeftBoundary))
        mState.scene.SetLeftBoundary(GetValue(mUI.spinLeftBoundary));
    if (GetValue(mUI.chkRightBoundary))
        mState.scene.SetRightBoundary(GetValue(mUI.spinRightBoundary));
    if (GetValue(mUI.chkTopBoundary))
        mState.scene.SetTopBoundary(GetValue(mUI.spinTopBoundary));
    if (GetValue(mUI.chkBottomBoundary))
        mState.scene.SetBottomBoundary(GetValue(mUI.spinBottomBoundary));
}

game::SceneNodeClass* SceneWidget::SelectNode(const QPoint& click_point)
{
    gfx::Transform view;
    view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
    view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
    view.Rotate(qDegreesToRadians(mUI.rotation->value()));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    const auto& view_to_scene     = glm::inverse(view.GetAsMatrix());
    const auto click_pos_in_view  = glm::vec4(click_point.x(), click_point.y(), 1.0f, 1.0f);
    const auto click_pos_in_scene = view_to_scene * click_pos_in_view;

    std::vector<game::SceneNodeClass*> hit_nodes;
    std::vector<glm::vec2> hit_positions;
    mState.scene.CoarseHitTest(click_pos_in_scene.x, click_pos_in_scene.y, &hit_nodes, &hit_positions);
    if (hit_nodes.empty())
        return nullptr;

    // per pixel selection based on the idea that we re-render the
    // objects returned by the coarse hit test with different colors
    // and then read back the color of the pixel under the click point
    // and see which object/node the color maps back to.
    class DrawHook : public engine::SceneClassDrawHook,
                     public engine::EntityClassDrawHook
    {
    public:
        DrawHook(const std::vector<game::SceneNodeClass*>& hits) : mHits(hits)
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
        virtual bool FilterEntity(const game::SceneNodeClass& node, gfx::Painter&, gfx::Transform&) override
        {
            // filter out nodes that are currently not visible.
            // probably don't want to select any of those.
            if (!node.TestFlag(game::SceneNodeClass::Flags::VisibleInEditor))
                return false;

            for (const auto* n : mHits)
                if (n == &node) return true;
            return false;
        }
        virtual void BeginDrawEntity(const game::SceneNodeClass& node, gfx::Painter&, gfx::Transform&) override
        {
            for (size_t i=0; i<mHits.size(); ++i)
            {
                if (mHits[i] == &node)
                {
                    mColorIndex = i;
                    return;
                }
            }
        }
        virtual bool InspectPacket(const game::EntityNodeClass* node, engine::DrawPacket& draw) override
        {
            ASSERT(mColorIndex < mColors.size());
            draw.material = gfx::CreateMaterialInstance(gfx::CreateMaterialClassFromColor(mColors[mColorIndex]));
            return true;
        }

    private:
        const std::vector<game::SceneNodeClass*>& mHits;
        std::vector<gfx::Color4f> mColors;
        std::size_t mColorIndex = 0;
    };

    auto& painter = mUI.widget->getPainter();
    auto& device = mUI.widget->getDevice();
    DrawHook hook(hit_nodes);
    mState.renderer.Draw(mState.scene, painter, view, &hook, &hook);

    {
        // for debugging.
        //auto bitmap = device.ReadColorBuffer(mUI.widget->width(), mUI.widget->height());
        //gfx::WritePNG(bitmap, "/tmp/click-test-debug.png");
    }
    const auto surface_width  = mUI.widget->width();
    const auto surface_height = mUI.widget->height();
    const auto& bitmap = device.ReadColorBuffer(click_point.x(), surface_height - click_point.y(), 1, 1);
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
        if (hit_nodes[i]->TestFlag(game::SceneNodeClass::Flags::VisibleInEditor) &&
            hit_nodes[i]->GetLayer() >= layer)
        {
            hit = hit_nodes[i];
            pos = hit_positions[i];
            layer = hit->GetLayer();
        }
    }
    if (!hit->TestFlag(game::SceneNodeClass::Flags::VisibleInEditor))
        return nullptr;
    return hit;
}

game::SceneNodeClass* SceneWidget::GetCurrentNode()
{
    TreeWidget::TreeItem* item = mUI.tree->GetSelectedItem();
    if (item == nullptr)
        return nullptr;
    else if (!item->GetUserData())
        return nullptr;
    return static_cast<game::SceneNodeClass*>(item->GetUserData());
}

const game::SceneNodeClass* SceneWidget::GetCurrentNode() const
{
    const TreeWidget::TreeItem* item = mUI.tree->GetSelectedItem();
    if (item == nullptr)
        return nullptr;
    else if (!item->GetUserData())
        return nullptr;
    return static_cast<const game::SceneNodeClass*>(item->GetUserData());
}

} // bui
