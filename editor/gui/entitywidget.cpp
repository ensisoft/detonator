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

#include "config.h"

#include "warnpush.h"
#  include <QPoint>
#  include <QMouseEvent>
#  include <QMessageBox>
#  include <QFile>
#  include <QFileInfo>
#  include <QFileDialog>
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
#include "editor/gui/entitywidget.h"
#include "editor/gui/animationtrackwidget.h"
#include "editor/gui/scriptwidget.h"
#include "editor/gui/utility.h"
#include "editor/gui/drawing.h"
#include "editor/gui/dlgscriptvar.h"
#include "editor/gui/dlgmaterial.h"
#include "editor/gui/dlgfont.h"
#include "editor/gui/dlgmaterialparams.h"
#include "editor/gui/dlgjoint.h"
#include "editor/gui/dlganimator.h"
#include "editor/gui/clipboard.h"
#include "editor/gui/playwindow.h"
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

class EntityWidget::JointModel : public QAbstractTableModel
{
public:
    JointModel(EntityWidget::State& state) : mState(state)
    {}
    virtual QVariant data(const QModelIndex& index, int role) const override
    {
        const auto& joint = mState.entity->GetJoint(index.row());
        const auto* src   = mState.entity->FindNodeById(joint.src_node_id);
        const auto* dst   = mState.entity->FindNodeById(joint.dst_node_id);
        if (role == Qt::DisplayRole)
        {
            switch (index.column()) {
                case 0: return app::toString(joint.type);
                case 1: return app::FromUtf8(joint.name);
                case 2: return app::FromUtf8(src->GetName());
                case 3: return app::FromUtf8(dst->GetName());
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
                case 2: return "Node";
                case 3: return "Node";
                default: BUG("Unknown script variable data index.");
            }
        }
        return QVariant();
    }
    virtual int rowCount(const QModelIndex&) const override
    {
        return static_cast<int>(mState.entity->GetNumJoints());
    }
    virtual int columnCount(const QModelIndex&) const override
    { return 4; }

    void AddJoint(game::EntityClass::PhysicsJoint&& joint)
    {
        const auto count = static_cast<int>(mState.entity->GetNumJoints());
        beginInsertRows(QModelIndex(), count, count);
        mState.entity->AddJoint(std::move(joint));
        endInsertRows();
    }
    void EditJoint(size_t row, game::EntityClass::PhysicsJoint&& joint)
    {
        mState.entity->SetJoint(row, std::move(joint));
        emit dataChanged(index(row, 0), index(row, 4));
    }
    void DeleteJoint(size_t row)
    {
        beginRemoveRows(QModelIndex(), row, row);
        mState.entity->DeleteJoint(row);
        endRemoveRows();
    }

    void Reset()
    {
        beginResetModel();
        endResetModel();
    }
private:
    EntityWidget::State& mState;
};

class EntityWidget::ScriptVarModel : public QAbstractTableModel
{
public:
    ScriptVarModel(EntityWidget::State& state) : mState(state)
    {}
    virtual QVariant data(const QModelIndex& index, int role) const override
    {
        const auto& var = mState.entity->GetScriptVar(index.row());
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
        return static_cast<int>(mState.entity->GetNumScriptVars());
    }
    virtual int columnCount(const QModelIndex&) const override
    {
        //return 3;
        return 2;
    }
    void AddVariable(game::ScriptVar&& var)
    {
        const auto count = static_cast<int>(mState.entity->GetNumScriptVars());
        beginInsertRows(QModelIndex(), count, count);
        mState.entity->AddScriptVar(std::move(var));
        endInsertRows();
    }
    void EditVariable(size_t row, game::ScriptVar&& var)
    {
        mState.entity->SetScriptVar(row, std::move(var));
        emit dataChanged(index(row, 0), index(row, 3));
    }
    void DeleteVariable(size_t row)
    {
        beginRemoveRows(QModelIndex(), row, row);
        mState.entity->DeleteScriptVar(row);
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
            case game::ScriptVar::Type::EntityNodeReference:
                if (!var.IsArray()) {
                    const auto& val = var.GetValue<game::ScriptVar::EntityNodeReference>();
                    if (const auto* node = mState.entity->FindNodeById(val.id))
                        return app::FromUtf8(node->GetName());
                    return "Nil";
                } else {
                    const auto& val = var.GetArray<game::ScriptVar::EntityNodeReference>()[0];
                    if (const auto* node = mState.entity->FindNodeById(val.id))
                        return QString("[0]=%1 ...").arg(app::FromUtf8(node->GetName()));
                    return "[0]=Nil ...";
                }
                break;
            case game::ScriptVar::Type::EntityReference:
                if (!var.IsArray()) {
                    return "Nil";
                } else {
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
    EntityWidget::State& mState;
};

class EntityWidget::PlaceShapeTool : public MouseTool
{
public:
    PlaceShapeTool(EntityWidget::State& state, const QString& material, const QString& drawable)
            : mState(state)
            , mMaterialId(material)
            , mDrawableId(drawable)
    {
        mDrawableClass = mState.workspace->GetDrawableClassById(mDrawableId);
        mMaterialClass = mState.workspace->GetMaterialClassById(mMaterialId);
        mMaterial = gfx::CreateMaterialInstance(mMaterialClass);
        mDrawable = gfx::CreateDrawableInstance(mDrawableClass);
    }
    virtual void Render(gfx::Painter& painter, gfx::Transform& view) const override
    {
        if (!mEngaged)
        {
            ShowMessage("Click + hold to draw!",
                        gfx::FRect(40 + mMousePos.x(),
                                   40 + mMousePos.y(), 200, 20), painter);
            return;
        }

        const auto& diff = mCurrent - mStart;
        if (diff.x <= 0.0f || diff.y <= 0.0f)
            return;

        const float xpos = mStart.x;
        const float ypos = mStart.y;
        const float hypotenuse = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        const float width  = mAlwaysSquare ? hypotenuse : diff.x;
        const float height = mAlwaysSquare ? hypotenuse : diff.y;

        view.Push();
        view.Scale(width, height);
        view.Translate(xpos, ypos);
        painter.Draw(*mDrawable, view, *mMaterial);

        // draw a selection rect around it.
        painter.Draw(gfx::Rectangle(gfx::Drawable::Style::Outline), view,
                     gfx::CreateMaterialFromColor(gfx::Color::Green));
        view.Pop();
    }
    virtual void MouseMove(QMouseEvent* mickey, gfx::Transform& view) override
    {
        mMousePos =  mickey->pos();

        if (!mEngaged)
            return;

        const auto& view_to_model = glm::inverse(view.GetAsMatrix());
        mCurrent = view_to_model * ToVec4(mMousePos);
        mAlwaysSquare = mickey->modifiers() & Qt::ControlModifier;
    }
    virtual void MousePress(QMouseEvent* mickey, gfx::Transform& view) override
    {
        const auto button = mickey->button();
        if (button == Qt::LeftButton)
        {
            const auto& view_to_model = glm::inverse(view.GetAsMatrix());
            const auto& p = mickey->pos();
            mStart = view_to_model * glm::vec4(p.x(), p.y(), 1.0f, 1.0f);
            mCurrent = mStart;
            mEngaged = true;
        }
    }
    virtual bool MouseRelease(QMouseEvent* mickey, gfx::Transform& view) override
    {
        const auto button = mickey->button();
        if (button != Qt::LeftButton)
            return false;

        ASSERT(mEngaged);

        mEngaged = false;
        const auto& diff = mCurrent - mStart;
        if (diff.x <= 0.0f || diff.y <= 0.0f)
            return true;

        std::string name;
        for (size_t i=0; i<666666; ++i)
        {
            name = "Node " + std::to_string(i);
            if (CheckNameAvailability(name))
                break;
        }

        const float xpos = mStart.x;
        const float ypos = mStart.y;
        const float hypotenuse = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        const float width = mAlwaysSquare ? hypotenuse : diff.x;
        const float height = mAlwaysSquare ? hypotenuse : diff.y;

        game::DrawableItemClass item;
        item.SetMaterialId(mMaterialClass->GetId());
        item.SetDrawableId(mDrawableClass->GetId());
        game::EntityNodeClass node;
        node.SetDrawable(item);
        node.SetName(name);
        node.SetTranslation(glm::vec2(xpos + 0.5*width, ypos + 0.5*height));
        node.SetSize(glm::vec2(width, height));
        node.SetScale(glm::vec2(1.0f, 1.0f));

        // by default we're appending to the root item.
        auto* child = mState.entity->AddNode(std::move(node));
        mState.entity->LinkChild(nullptr, child);
        mState.view->Rebuild();
        mState.view->SelectItemById(child->GetId());
        DEBUG("Added new shape '%1'", name);
        return true;
    }
    bool CheckNameAvailability(const std::string& name) const
    {
        for (size_t i=0; i<mState.entity->GetNumNodes(); ++i)
        {
            const auto& node = mState.entity->GetNode(i);
            if (node.GetName() == name)
                return false;
        }
        return true;
    }
private:
    EntityWidget::State& mState;
    // the starting object position in model coordinates of the placement
    // based on the mouse position at the time.
    glm::vec4 mStart;
    // the current object ending position in model coordinates.
    // the object occupies the rectangular space between the start
    // and current positions on the X and Y axis.
    glm::vec4 mCurrent;
    bool mEngaged = false;
    bool mAlwaysSquare = false;
    QPoint mMousePos;
private:
    QString mMaterialId;
    QString mDrawableId;
    std::shared_ptr<const gfx::DrawableClass> mDrawableClass;
    std::shared_ptr<const gfx::MaterialClass> mMaterialClass;
    std::unique_ptr<gfx::Material> mMaterial;
    std::unique_ptr<gfx::Drawable> mDrawable;
};


EntityWidget::EntityWidget(app::Workspace* workspace) : mUndoStack(3)
{
    DEBUG("Create EntityWidget");

    mState.entity = std::make_shared<game::EntityClass>();
    mState.entity->SetName("My Entity");
    mOriginalHash = ComputeHash();

    mRenderTree.reset(new TreeModel(*mState.entity));
    mScriptVarModel.reset(new ScriptVarModel(mState));
    mJointModel.reset(new JointModel(mState));

    mUI.setupUi(this);
    mUI.scriptVarList->setModel(mScriptVarModel.get());
    mUI.jointList->setModel(mJointModel.get());
    QHeaderView* verticalHeader = mUI.scriptVarList->verticalHeader();
    verticalHeader->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader->setDefaultSectionSize(16);
    mUI.tree->SetModel(mRenderTree.get());
    mUI.tree->Rebuild();
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(false);
    mUI.widget->onZoomIn       = [this]() { MouseZoom(std::bind(&EntityWidget::ZoomIn, this)); };
    mUI.widget->onZoomOut      = [this]() { MouseZoom(std::bind(&EntityWidget::ZoomOut, this)); };
    mUI.widget->onMouseMove    = std::bind(&EntityWidget::MouseMove, this, std::placeholders::_1);
    mUI.widget->onMousePress   = std::bind(&EntityWidget::MousePress, this, std::placeholders::_1);
    mUI.widget->onMouseRelease = std::bind(&EntityWidget::MouseRelease, this, std::placeholders::_1);
    mUI.widget->onKeyPress     = std::bind(&EntityWidget::KeyPress, this, std::placeholders::_1);
    mUI.widget->onMouseDoubleClick = std::bind(&EntityWidget::MouseDoubleClick, this, std::placeholders::_1);
    mUI.widget->onPaintScene   = std::bind(&EntityWidget::PaintScene, this, std::placeholders::_1,
                                           std::placeholders::_2);
    mUI.widget->onInitScene    = std::bind(&EntityWidget::InitScene, this, std::placeholders::_1,
                                           std::placeholders::_2);

    // create the menu for creating instances of user defined drawables
    // since there doesn't seem to be a way to do this in the designer.
    mParticleSystems = new QMenu(this);
    mParticleSystems->menuAction()->setIcon(QIcon("icons:particle.png"));
    mParticleSystems->menuAction()->setText("Particles");
    mParticleSystems->menuAction()->setCheckable(true);
    mCustomShapes = new QMenu(this);
    mCustomShapes->menuAction()->setIcon(QIcon("icons:polygon.png"));
    mCustomShapes->menuAction()->setText("Shapes");
    mCustomShapes->menuAction()->setCheckable(true);

    mState.workspace = workspace;
    mState.renderer.SetClassLibrary(workspace);
    mState.renderer.SetEditingMode(true);
    mState.view = mUI.tree;

    // connect tree widget signals
    connect(mUI.tree, &TreeWidget::currentRowChanged, this, &EntityWidget::TreeCurrentNodeChangedEvent);
    connect(mUI.tree, &TreeWidget::dragEvent,  this, &EntityWidget::TreeDragEvent);
    connect(mUI.tree, &TreeWidget::clickEvent, this, &EntityWidget::TreeClickEvent);
    // connect workspace signals for resource management
    connect(workspace, &app::Workspace::ResourceAdded,   this, &EntityWidget::ResourceAdded);
    connect(workspace, &app::Workspace::ResourceRemoved, this, &EntityWidget::ResourceRemoved);
    connect(workspace, &app::Workspace::ResourceUpdated, this, &EntityWidget::ResourceUpdated);

    PopulateFromEnum<GridDensity>(mUI.cmbGrid);
    PopulateFromEnum<game::DrawableItemClass::RenderPass>(mUI.dsRenderPass);
    PopulateFromEnum<game::DrawableItemClass::RenderStyle>(mUI.dsRenderStyle);
    PopulateFromEnum<game::RigidBodyItemClass::Simulation>(mUI.rbSimulation);
    PopulateFromEnum<game::RigidBodyItemClass::CollisionShape>(mUI.rbShape);
    PopulateFromEnum<game::TextItemClass::VerticalTextAlign>(mUI.tiVAlign);
    PopulateFromEnum<game::TextItemClass::HorizontalTextAlign>(mUI.tiHAlign);
    PopulateFromEnum<game::SpatialNodeClass::Shape>(mUI.spnShape);
    PopulateFromEnum<game::FixtureClass::CollisionShape>(mUI.fxShape);
    PopulateFontNames(mUI.tiFontName);
    PopulateFontSizes(mUI.tiFontSize);
    SetValue(mUI.cmbGrid, GridDensity::Grid50x50);
    SetValue(mUI.zoom, 1.0f);

    RebuildMenus();
    RebuildCombos();

    RegisterEntityWidget(this);
    DisplayEntityProperties();
    DisplayCurrentNodeProperties();
    DisplayCurrentCameraLocation();
    setWindowTitle("My Entity");
}

EntityWidget::EntityWidget(app::Workspace* workspace, const app::Resource& resource)
  : EntityWidget(workspace)
{
    DEBUG("Editing entity '%1'", resource.GetName());
    const game::EntityClass* content = nullptr;
    resource.GetContent(&content);
    GetUserProperty(resource, "zoom", mUI.zoom);
    GetUserProperty(resource, "grid", mUI.cmbGrid);
    GetUserProperty(resource, "snap", mUI.chkSnap);
    GetUserProperty(resource, "show_origin", mUI.chkShowOrigin);
    GetUserProperty(resource, "show_grid", mUI.chkShowGrid);
    GetUserProperty(resource, "show_viewport", mUI.chkShowViewport);
    GetUserProperty(resource, "show_comments", mUI.chkShowComments);
    GetUserProperty(resource, "widget", mUI.widget);
    GetUserProperty(resource, "camera_scale_x", mUI.scaleX);
    GetUserProperty(resource, "camera_scale_y", mUI.scaleY);
    GetUserProperty(resource, "camera_rotation", mUI.rotation);
    GetUserProperty(resource, "callbacks_group", mUI.callbacks);
    GetUserProperty(resource, "variables_group", mUI.variables);
    GetUserProperty(resource, "animations_group", mUI.animations);
    GetUserProperty(resource, "joints_group", mUI.joints);
    GetUserProperty(resource, "camera_offset_x", &mState.camera_offset_x);
    GetUserProperty(resource, "camera_offset_y", &mState.camera_offset_y);
    mCameraWasLoaded = true;

    mState.entity = std::make_shared<game::EntityClass>(*content);

    // load per track resource properties.
    for (size_t i=0; i< mState.entity->GetNumAnimations(); ++i)
    {
        const auto& track = mState.entity->GetAnimation(i);
        const auto& Id = track.GetId();
        QVariantMap properties;
        GetProperty(resource, "track_" + Id, &properties);
        mTrackProperties[Id] = properties;
    }
    // load per node comments
    for (size_t i=0; i<mState.entity->GetNumNodes(); ++i)
    {
        const auto& node = mState.entity->GetNode(i);
        const auto& id = node.GetId();
        QString comment;
        GetProperty(resource, "comment_" + id, &comment);
        mComments[id] = comment;
    }

    // load animator properties
    for (size_t i=0; i<mState.entity->GetNumAnimators(); ++i)
    {
        const auto& anim = mState.entity->GetAnimator(i);
        const auto& Id = anim.GetId();
        QVariantMap properties;
        GetProperty(resource, "animator_" + Id, &properties);
        mAnimatorProperties[Id] = properties;
    }

    mOriginalHash = ComputeHash();

    UpdateDeletedResourceReferences();
    RebuildCombosInternal();
    DisplayEntityProperties();
    DisplayCurrentNodeProperties();
    DisplayCurrentCameraLocation();

    mScriptVarModel->Reset();
    mJointModel->Reset();

    mRenderTree.reset(new TreeModel(*mState.entity));
    mUI.tree->SetModel(mRenderTree.get());
    mUI.tree->Rebuild();
}

EntityWidget::~EntityWidget()
{
    DEBUG("Destroy EntityWidget");

    DeleteEntityWidget(this);
}

QString EntityWidget::GetId() const
{
    return GetValue(mUI.entityID);
}

void EntityWidget::InitializeSettings(const UISettings& settings)
{
    SetValue(mUI.chkSnap,         settings.snap_to_grid);
    SetValue(mUI.chkShowViewport, settings.show_viewport);
    SetValue(mUI.chkShowOrigin,   settings.show_origin);
    SetValue(mUI.chkShowGrid,     settings.show_grid);
    SetValue(mUI.cmbGrid,         settings.grid);
    SetValue(mUI.zoom,            settings.zoom);
}

void EntityWidget::SetViewerMode()
{
    SetVisible(mUI.baseProperties, false);
    SetVisible(mUI.animator,       false);
    SetVisible(mUI.entity,         false);
    SetVisible(mUI.scrollArea,     false);
    SetVisible(mUI.transform,      false);
    SetVisible(mUI.lblHelp,        false);
    SetVisible(mUI.renderTree,     false);
    SetVisible(mUI.nodeProperties, false);
    SetVisible(mUI.nodeTransform,  false);
    SetVisible(mUI.nodeItems,      false);
    SetValue(mUI.chkShowGrid,      false);
    SetValue(mUI.chkShowOrigin,    false);
    SetValue(mUI.chkShowViewport,  false);
    SetValue(mUI.chkShowOrigin,    false);
    SetValue(mUI.chkSnap,          false);
    SetValue(mUI.chkShowComments,  false);
    SetVisible(mUI.chkShowComments, false);
    SetVisible(mUI.chkShowViewport, false);
    SetVisible(mUI.chkSnap,        false);
    SetVisible(mUI.chkShowOrigin,  false);

    QTimer::singleShot(10, this, &EntityWidget::on_btnViewReset_clicked);
    on_actionPlay_triggered();
}

void EntityWidget::AddActions(QToolBar& bar)
{
    bar.addAction(mUI.actionPlay);
    bar.addAction(mUI.actionPause);
    bar.addAction(mUI.actionStop);
    bar.addSeparator();
    bar.addAction(mUI.actionSave);
    bar.addSeparator();
    bar.addAction(mUI.actionPreview);
    bar.addSeparator();
    bar.addAction(mUI.actionNewRect);
    bar.addAction(mUI.actionNewRoundRect);
    bar.addAction(mUI.actionNewCircle);
    bar.addAction(mUI.actionNewSemiCircle);
    bar.addAction(mUI.actionNewIsoscelesTriangle);
    bar.addAction(mUI.actionNewRightTriangle);
    bar.addAction(mUI.actionNewTrapezoid);
    bar.addAction(mUI.actionNewParallelogram);
    bar.addAction(mUI.actionNewCapsule);
    bar.addSeparator();
    bar.addAction(mCustomShapes->menuAction());
    bar.addSeparator();
    bar.addAction(mParticleSystems->menuAction());
}
void EntityWidget::AddActions(QMenu& menu)
{
    menu.addAction(mUI.actionPlay);
    menu.addAction(mUI.actionPause);
    menu.addAction(mUI.actionStop);
    menu.addSeparator();
    menu.addAction(mUI.actionSave);
    menu.addSeparator();
    menu.addAction(mUI.actionPreview);
    menu.addSeparator();
    menu.addAction(mUI.actionNewRect);
    menu.addAction(mUI.actionNewRoundRect);
    menu.addAction(mUI.actionNewCircle);
    menu.addAction(mUI.actionNewSemiCircle);
    menu.addAction(mUI.actionNewIsoscelesTriangle);
    menu.addAction(mUI.actionNewRightTriangle);
    menu.addAction(mUI.actionNewTrapezoid);
    menu.addAction(mUI.actionNewParallelogram);
    menu.addAction(mUI.actionNewCapsule);
    menu.addSeparator();
    menu.addAction(mCustomShapes->menuAction());
    menu.addSeparator();
    menu.addAction(mParticleSystems->menuAction());
}

bool EntityWidget::SaveState(Settings& settings) const
{
    data::JsonObject json;
    mState.entity->IntoJson(json);
    settings.SetValue("Entity", "content", json);
    settings.SetValue("Entity", "hash", mOriginalHash);
    settings.SetValue("Entity", "camera_offset_x", mState.camera_offset_x);
    settings.SetValue("Entity", "camera_offset_y", mState.camera_offset_y);

    for (const auto& [id, props] : mTrackProperties)
    {
        settings.SetValue("Entity", "track_" + id, props);
    }

    for (const auto& [id, comment] : mComments)
    {
        settings.SetValue("Entity", "comment_" + id, comment);
    }

    for (const auto& [id, props] : mAnimatorProperties)
    {
        settings.SetValue("Entity", "animator_" + id, props);
    }

    settings.SaveWidget("Entity", mUI.scaleX);
    settings.SaveWidget("Entity", mUI.scaleY);
    settings.SaveWidget("Entity", mUI.rotation);
    settings.SaveWidget("Entity", mUI.chkShowOrigin);
    settings.SaveWidget("Entity", mUI.chkShowGrid);
    settings.SaveWidget("Entity", mUI.chkShowViewport);
    settings.SaveWidget("Entity", mUI.chkShowComments);
    settings.SaveWidget("Entity", mUI.chkSnap);
    settings.SaveWidget("Entity", mUI.cmbGrid);
    settings.SaveWidget("Entity", mUI.zoom);
    settings.SaveWidget("Entity", mUI.widget);
    settings.SaveWidget("Entity", mUI.callbacks);
    settings.SaveWidget("Entity", mUI.variables);
    settings.SaveWidget("Entity", mUI.animations);
    settings.SaveWidget("Entity", mUI.joints);
    return true;
}
bool EntityWidget::LoadState(const Settings& settings)
{
    data::JsonObject json;
    settings.GetValue("Entity", "content", &json);
    settings.GetValue("Entity", "hash", &mOriginalHash);
    settings.GetValue("Entity", "camera_offset_x", &mState.camera_offset_x);
    settings.GetValue("Entity", "camera_offset_y", &mState.camera_offset_y);
    mCameraWasLoaded = true;

    settings.LoadWidget("Entity", mUI.scaleX);
    settings.LoadWidget("Entity", mUI.scaleY);
    settings.LoadWidget("Entity", mUI.rotation);
    settings.LoadWidget("Entity", mUI.chkShowOrigin);
    settings.LoadWidget("Entity", mUI.chkShowGrid);
    settings.LoadWidget("Entity", mUI.chkShowViewport);
    settings.LoadWidget("Entity", mUI.chkShowComments);
    settings.LoadWidget("Entity", mUI.chkSnap);
    settings.LoadWidget("Entity", mUI.cmbGrid);
    settings.LoadWidget("Entity", mUI.zoom);
    settings.LoadWidget("Entity", mUI.widget);
    settings.LoadWidget("Entity", mUI.callbacks);
    settings.LoadWidget("Entity", mUI.variables);
    settings.LoadWidget("Entity", mUI.animations);
    settings.LoadWidget("Entity", mUI.joints);

    game::EntityClass klass;
    if (!klass.FromJson(json))
        WARN("Failed to restore entity state.");

    auto hash  = klass.GetHash();
    mState.entity = FindSharedEntity(hash);
    if (!mState.entity)
    {
        mState.entity = std::make_shared<game::EntityClass>(std::move(klass));
        ShareEntity(mState.entity);
    }

    for (size_t i=0; i< mState.entity->GetNumAnimations(); ++i)
    {
        const auto& track = mState.entity->GetAnimation(i);
        const auto& id = track.GetId();

        QVariantMap properties;
        settings.GetValue("Entity", "track_" + id, &properties);
        mTrackProperties[id] = properties;
    }

    for (size_t i=0; i<mState.entity->GetNumNodes(); ++i)
    {
        const auto& node = mState.entity->GetNode(i);
        const auto& id = node.GetId();
        QString comment;
        settings.GetValue("Entity", "comment_" + id, &comment);
        mComments[id] = comment;
    }

    // load animator properties
    for (size_t i=0; i<mState.entity->GetNumAnimators(); ++i)
    {
        const auto& anim = mState.entity->GetAnimator(i);
        const auto& Id = anim.GetId();
        QVariantMap properties;
        settings.GetValue("Entity", "animator_" + Id, &properties);
        mAnimatorProperties[Id] = properties;
    }


    UpdateDeletedResourceReferences();
    RebuildCombosInternal();
    DisplayEntityProperties();
    DisplayCurrentNodeProperties();
    DisplayCurrentCameraLocation();

    mScriptVarModel->Reset();
    mJointModel->Reset();
    mRenderTree.reset(new TreeModel(*mState.entity));
    mUI.tree->SetModel(mRenderTree.get());
    mUI.tree->Rebuild();
    return true;
}

bool EntityWidget::CanTakeAction(Actions action, const Clipboard* clipboard) const
{
    switch (action)
    {
        case Actions::CanPaste:
            if (clipboard->GetType() == "application/json/entity")
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
            return true;
    }
    return false;
}

void EntityWidget::Cut(Clipboard& clipboard)
{
    if (auto* node = GetCurrentNode())
    {
        data::JsonObject json;
        const auto& tree = mState.entity->GetRenderTree();
        game::RenderTreeIntoJson(tree, [this, &clipboard](data::Writer& data, const auto* node) {
            node->IntoJson(data);
            if (const auto* comment = base::SafeFind(mComments, node->GetId()))
                clipboard.SetProperty("comment_" + node->GetId(), *comment);
            mComments.erase(node->GetId());
        }, json, node);

        clipboard.Clear();
        clipboard.SetType("application/json/entity/node");
        clipboard.SetText(json.ToString());
        NOTE("Copied JSON to application clipboard.");

        mState.entity->DeleteNode(node);
        mUI.tree->Rebuild();
        mUI.tree->ClearSelection();
        RealizeEntityChange(mState.entity);
    }
}
void EntityWidget::Copy(Clipboard& clipboard) const
{
    if (const auto* node = GetCurrentNode())
    {
        data::JsonObject json;
        const auto& tree = mState.entity->GetRenderTree();
        game::RenderTreeIntoJson(tree, [this, &clipboard](data::Writer& data, const auto* node) {
            node->IntoJson(data);
            if (const auto* comment = base::SafeFind(mComments, node->GetId()))
                clipboard.SetProperty("comment_" + node->GetId(), *comment);
        }, json, node);

        clipboard.Clear();
        clipboard.SetType("application/json/entity");
        clipboard.SetText(json.ToString());
        NOTE("Copied JSON to application clipboard.");
    }
}
void EntityWidget::Paste(const Clipboard& clipboard)
{
    if (clipboard.IsEmpty())
    {
        NOTE("Clipboard is empty.");
        return;
    }
    else if (clipboard.GetType() != "application/json/entity")
    {
        NOTE("No entity JSON data found in clipboard.");
        return;
    }

    mUI.widget->setFocus();

    data::JsonObject json;
    auto [success, _] = json.ParseString(clipboard.GetText());
    if (!success)
    {
        NOTE("Clipboard JSON parse failed.");
        return;
    }

    // use a temporary vector in case there's a problem
    std::vector<std::unique_ptr<game::EntityNodeClass>> nodes;
    std::unordered_map<std::string, QString> comments;

    bool error = false;
    game::EntityClass::RenderTree tree;
    game::RenderTreeFromJson(tree, [&nodes, &error, &comments, &clipboard](const data::Reader& data) {

        game::EntityNodeClass ret;
        if (ret.FromJson(data))
        {
            auto node = std::make_unique<game::EntityNodeClass>(ret.Clone());
            node->SetName(base::FormatString("Copy of %1", ret.GetName()));

            QString comment;
            if (clipboard.GetProperty("comment_" + ret.GetId(), &comment))
                comments[node->GetId()] = comment;

            nodes.push_back(std::move(node));
            return nodes.back().get();
        }
        error = true;
        return (game::EntityNodeClass*)nullptr;
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
    view.RotateAroundZ(qDegreesToRadians((float) GetValue(mUI.rotation)));
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
        mState.entity->AddNode(std::move(node));
    }
    nodes.clear();
    // walk the tree and link the nodes into the scene.
    tree.PreOrderTraverseForEach([this, &tree](game::EntityNodeClass* node) {
        if (node == nullptr)
            return;
        auto* parent = tree.GetParent(node);
        mState.entity->LinkChild(parent, node);
    });

    mComments.merge(comments);

    mUI.tree->Rebuild();
    mUI.tree->SelectItemById(app::FromUtf8(paste_root->GetId()));
    RealizeEntityChange(mState.entity);
}

void EntityWidget::Save()
{
    on_actionSave_triggered();
}

void EntityWidget::Undo()
{
    if (mUndoStack.size() <= 1)
    {
        NOTE("No undo available.");
        return;
    }

    // if the timer has run the top of the undo stack
    // is the same copy as the actual scene object.
    if (mUndoStack.back().GetHash() == mState.entity->GetHash())
        mUndoStack.pop_back();

    // todo: how to deal with entity being changed when the
    // animation track widget is open?

    *mState.entity = mUndoStack.back();
    mState.view->Rebuild();
    mUndoStack.pop_back();
    mScriptVarModel->Reset();
    mJointModel->Reset();
    DisplayCurrentNodeProperties();
    NOTE("Undo!");
}

void EntityWidget::ZoomIn()
{
    const float value = GetValue(mUI.zoom);
    SetValue(mUI.zoom, value + 0.1f);
}
void EntityWidget::ZoomOut()
{
    const float value = GetValue(mUI.zoom);
    SetValue(mUI.zoom, value - 0.1f);
}
void EntityWidget::ReloadShaders()
{
    mUI.widget->reloadShaders();
}
void EntityWidget::ReloadTextures()
{
    mUI.widget->reloadTextures();
}
void EntityWidget::Shutdown()
{
    if (mPreview)
    {
        mPreview->Shutdown();
        mPreview->close();
        mPreview.reset();
    }

    mUI.widget->dispose();
}
void EntityWidget::Update(double secs)
{
    if (mPlayState == PlayState::Playing)
    {
        mState.renderer.Update(*mState.entity, mEntityTime, secs);
        mEntityTime += secs;
    }
    mCurrentTime += secs;
}
void EntityWidget::Render()
{
    // call for the widget to paint, it will set its own OpenGL context on this thread
    // and everything should be fine.
    mUI.widget->triggerPaint();
}

void EntityWidget::RunGameLoopOnce()
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

bool EntityWidget::HasUnsavedChanges() const
{
    if (mOriginalHash == ComputeHash())
        return false;
    return true;
}

void EntityWidget::Refresh()
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
    else if (mUI.nodeComment->hasFocus())
        return;
    else if (mUI.nodeTag->hasFocus())
        return;

    // don't take undo snapshot while continuous edits to text props
    if (mUI.tiTextColor->isDialogOpen() || mUI.tiText->hasFocus())
        return;

    if (mUndoStack.empty())
    {
        mUndoStack.push_back(*mState.entity);
    }

    const auto curr_hash = mState.entity->GetHash();
    const auto undo_hash = mUndoStack.back().GetHash();
    if (curr_hash != undo_hash)
    {
        mUndoStack.push_back(*mState.entity);
        DEBUG("Created undo copy. stack size: %1", mUndoStack.size());
    }
}

bool EntityWidget::GetStats(Stats* stats) const
{
    stats->time  = mEntityTime;
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

bool EntityWidget::OnEscape()
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

bool EntityWidget::LaunchScript(const QString& id)
{
    const auto& entity_script_id = (QString)GetItemId(mUI.scriptFile);
    const auto& animator_script_id = (QString)GetItemId(mUI.animatorScript);
    if (entity_script_id ==  id || animator_script_id == id)
    {
        on_actionPreview_triggered();
        return true;
    }
    return false;
}

void EntityWidget::SaveAnimation(const game::AnimationClass& track, const QVariantMap& properties)
{
    // keep track of the associated track properties 
    // separately. these only pertain to the UI and are not
    // used by the track/animation system itself.
    mTrackProperties[track.GetId()] = properties;

    for (size_t i=0; i< mState.entity->GetNumAnimations(); ++i)
    {
        auto& other = mState.entity->GetAnimation(i);
        if (other.GetId() != track.GetId())
            continue;

        // copy it over.
        other = track;
        INFO("Saved animation track '%1'", track.GetName());
        NOTE("Saved animation track '%1'", track.GetName());
        DisplayEntityProperties();
        return;
    }
    // add a copy
    mState.entity->AddAnimation(track);
    INFO("Saved animation track '%1'", track.GetName());
    NOTE("Saved animation track '%1'", track.GetName());
    DisplayEntityProperties();
}

void EntityWidget::SaveAnimator(const game::AnimatorClass& animator, const QVariantMap& properties)
{
    mAnimatorProperties[animator.GetId()] = properties;

    for (size_t i=0; i<mState.entity->GetNumAnimators(); ++i)
    {
        auto& other = mState.entity->GetAnimator(i);
        if (other.GetId() != animator.GetId())
            continue;

        // copy it over.
        other = animator;
        INFO("Saved animator '%1'", animator.GetName());
        NOTE("Saved animator '%1'", animator.GetName());
        DisplayEntityProperties();
        return;
    }
    // add a copy
    mState.entity->AddAnimator(animator);
    INFO("Saved animator '%1'", animator.GetName());
    NOTE("Saved animator '%1'", animator.GetName());
    DisplayEntityProperties();
}

void EntityWidget::on_widgetColor_colorChanged(const QColor& color)
{
    mUI.widget->SetClearColor(color);
}

void EntityWidget::on_actionPlay_triggered()
{
    // quick restart.
    if (mPlayState == PlayState::Playing)
    {
        mState.renderer.ClearPaintState();
        mEntityTime = 0.0f;
        NOTE("Restarted entity '%1' play.", mState.entity->GetName());
    }
    mPlayState = PlayState::Playing;
    mUI.actionPause->setEnabled(true);
    mUI.actionStop->setEnabled(true);
}
void EntityWidget::on_actionPause_triggered()
{
    mPlayState = PlayState::Paused;
    mUI.actionPause->setEnabled(false);
}
void EntityWidget::on_actionStop_triggered()
{
    mPlayState = PlayState::Stopped;
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(false);
    mEntityTime = 0.0f;
    mState.renderer.ClearPaintState();
}
void EntityWidget::on_actionSave_triggered()
{
    if (!MustHaveInput(mUI.entityName))
        return;

    app::EntityResource resource(*mState.entity, GetValue(mUI.entityName));
    SetUserProperty(resource, "camera_offset_x", mState.camera_offset_x);
    SetUserProperty(resource, "camera_offset_y", mState.camera_offset_y);
    SetUserProperty(resource, "camera_scale_x", mUI.scaleX);
    SetUserProperty(resource, "camera_scale_y", mUI.scaleY);
    SetUserProperty(resource, "camera_rotation", mUI.rotation);
    SetUserProperty(resource, "zoom", mUI.zoom);
    SetUserProperty(resource, "grid", mUI.cmbGrid);
    SetUserProperty(resource, "snap", mUI.chkSnap);
    SetUserProperty(resource, "show_origin", mUI.chkShowOrigin);
    SetUserProperty(resource, "show_comments", mUI.chkShowComments);
    SetUserProperty(resource, "show_grid", mUI.chkShowGrid);
    SetUserProperty(resource, "widget", mUI.widget);
    SetUserProperty(resource, "show_viewport", mUI.chkShowViewport);
    SetUserProperty(resource, "callbacks_group", mUI.callbacks);
    SetUserProperty(resource, "variables_group", mUI.variables);
    SetUserProperty(resource, "animations_group", mUI.animations);
    SetUserProperty(resource, "joints_group", mUI.joints);

    // save the track properties.
    for (const auto& [id, props] : mTrackProperties)
    {
        SetProperty(resource, "track_" + id, props);
    }
    // save the node comments
    for (const auto& [id, comment] : mComments)
    {
        SetProperty(resource, "comment_"  + id, comment);
    }
    // save the animator properties
    for (const auto& [id, props] : mAnimatorProperties)
    {
        SetProperty(resource, "animator_" + id, props);
    }

    mState.workspace->SaveResource(resource);
    mOriginalHash = ComputeHash();
}

void EntityWidget::on_actionPreview_triggered()
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
        preview->LoadPreview(mState.entity);
        mPreview = std::move(preview);
    }
}

void EntityWidget::on_actionNewRect_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "_checkerboard", "_rect"));

    UncheckPlacementActions();
    mUI.actionNewRect->setChecked(true);
}
void EntityWidget::on_actionNewCircle_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "_checkerboard", "_circle"));

    UncheckPlacementActions();
    mUI.actionNewCircle->setChecked(true);
}

void EntityWidget::on_actionNewSemiCircle_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "_checkerboard", "_semi_circle"));

    UncheckPlacementActions();
    mUI.actionNewSemiCircle->setChecked(true);
}

void EntityWidget::on_actionNewIsoscelesTriangle_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "_checkerboard", "_isosceles_triangle"));

    UncheckPlacementActions();
    mUI.actionNewIsoscelesTriangle->setChecked(true);
}
void EntityWidget::on_actionNewRightTriangle_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "_checkerboard", "_right_triangle"));

    UncheckPlacementActions();
    mUI.actionNewRightTriangle->setChecked(true);
}
void EntityWidget::on_actionNewRoundRect_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "_checkerboard", "_round_rect"));

    UncheckPlacementActions();
    mUI.actionNewRoundRect->setChecked(true);
}
void EntityWidget::on_actionNewTrapezoid_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "_checkerboard", "_trapezoid"));

    UncheckPlacementActions();
    mUI.actionNewTrapezoid->setChecked(true);
}
void EntityWidget::on_actionNewCapsule_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "_checkerboard", "_capsule"));

    UncheckPlacementActions();
    mUI.actionNewCapsule->setChecked(true);
}
void EntityWidget::on_actionNewParallelogram_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "_checkerboard", "_parallelogram"));

    UncheckPlacementActions();
    mUI.actionNewParallelogram->setChecked(true);
}

void EntityWidget::on_actionNodeDelete_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        const auto& tree = mState.entity->GetRenderTree();
        tree.ForEachChild([this](const auto* node) {
            mComments.erase(node->GetId());
        }, node);

        mState.entity->DeleteNode(node);

        mUI.tree->Rebuild();
        mUI.tree->ClearSelection();
        RealizeEntityChange(mState.entity);
    }
}
void EntityWidget::on_actionNodeVarRef_triggered()
{
    if (const auto* node = GetCurrentNode())
    {
        std::vector<ResourceListItem> entities;
        std::vector<ResourceListItem> nodes;
        for (size_t i = 0; i < mState.entity->GetNumNodes(); ++i)
        {
            const auto& node = mState.entity->GetNode(i);
            ResourceListItem item;
            item.name = node.GetName();
            item.id   = node.GetId();
            nodes.push_back(std::move(item));
        }
        QString name = app::FromUtf8(node->GetName());
        name = name.replace(' ', '_');
        name = name.toLower();
        game::ScriptVar::EntityNodeReference  ref;
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

void EntityWidget::on_actionNodeMoveUpLayer_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* item = node->GetDrawable())
        {
            const int layer = item->GetLayer();
            item->SetLayer(layer + 1);
            DisplayCurrentNodeProperties();
            RealizeEntityChange(mState.entity);
        }
    }
}

void EntityWidget::on_actionNodeMoveDownLayer_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* item = node->GetDrawable())
        {
            const int layer = item->GetLayer();
            item->SetLayer(layer - 1);
            DisplayCurrentNodeProperties();
            RealizeEntityChange(mState.entity);
        }
    }
}

void EntityWidget::on_actionNodeDuplicate_triggered()
{
    if (const auto* node = GetCurrentNode())
    {
        auto* dupe = mState.entity->DuplicateNode(node);
        // update the translation for the parent of the new hierarchy
        // so that it's possible to tell it apart from the source of the copy.
        dupe->SetTranslation(node->GetTranslation() * 1.2f);

        mState.view->Rebuild();
        mState.view->SelectItemById(app::FromUtf8(dupe->GetId()));
        RealizeEntityChange(mState.entity);
    }
}

void EntityWidget::on_actionNodeComment_triggered()
{
    if (const auto* node = GetCurrentNode())
    {
        QString comment;
        if (const auto* ptr = base::SafeFind(mComments, node->GetId()))
            comment = *ptr;
        bool accepted = false;
        comment = QInputDialog::getText(this,
            tr("Edit Comment"),
            tr("Comment: "), QLineEdit::Normal, comment, &accepted);
        if (!accepted)
            return;
        mComments[node->GetId()] = comment;
        SetValue(mUI.nodeComment, comment);
    }
}

void EntityWidget::on_actionNodeRename_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        QString name = app::FromUtf8(node->GetName());
        bool accepted = false;
        name = QInputDialog::getText(this,
            tr("Rename Node"),
            tr("Name: "), QLineEdit::Normal, name, &accepted);
        if (!accepted)
            return;
        node->SetName(app::ToUtf8(name));
        SetValue(mUI.nodeName, name);
        mUI.tree->Rebuild();
    }
}

void EntityWidget::on_actionNodeRenameAll_triggered()
{
    bool accepted = false;
    QString name = "Node %i";
    name = QInputDialog::getText(this,
        tr("Rename Node"),
        tr("Name: "), QLineEdit::Normal, name, &accepted);
    if (!accepted)
        return;
    for (unsigned i=0; i<mState.entity->GetNumNodes(); ++i)
    {
        auto& node = mState.entity->GetNode(i);
        QString node_name = name;
        node_name.replace("%i", QString::number(i));
        node.SetName(app::ToUtf8(node_name));
    }
    mUI.tree->Rebuild();
    DisplayCurrentNodeProperties();
}

void EntityWidget::on_actionScriptVarAdd_triggered()
{
    on_btnNewScriptVar_clicked();
}
void EntityWidget::on_actionScriptVarDel_triggered()
{
    on_btnDeleteScriptVar_clicked();
}
void EntityWidget::on_actionScriptVarEdit_triggered()
{
    on_btnEditScriptVar_clicked();
}

void EntityWidget::on_actionJointAdd_triggered()
{
    on_btnNewJoint_clicked();
}
void EntityWidget::on_actionJointDel_triggered()
{
    on_btnDeleteJoint_clicked();
}
void EntityWidget::on_actionJointEdit_triggered()
{
    on_btnEditJoint_clicked();
}

void EntityWidget::on_actionAnimationAdd_triggered()
{
    on_btnNewTrack_clicked();
}
void EntityWidget::on_actionAnimationDel_triggered()
{
    on_btnDeleteTrack_clicked();
}
void EntityWidget::on_actionAnimationEdit_triggered()
{
    on_btnEditTrack_clicked();
}

void EntityWidget::on_entityName_textChanged(const QString& text)
{
    mState.entity->SetName(GetValue(mUI.entityName));
}

void EntityWidget::on_entityTag_textChanged(const QString& text)
{
    mState.entity->SetTag(GetValue(mUI.entityTag));
}

void EntityWidget::on_entityLifetime_valueChanged(double value)
{
    const bool limit_lifetime = value > 0.0;
    mState.entity->SetLifetime(GetValue(mUI.entityLifetime));
    mState.entity->SetFlag(game::EntityClass::Flags::LimitLifetime, limit_lifetime);
}

void EntityWidget::on_chkKillAtLifetime_stateChanged(int)
{
    mState.entity->SetFlag(game::EntityClass::Flags::KillAtLifetime, GetValue(mUI.chkKillAtLifetime));
}
void EntityWidget::on_chkKillAtBoundary_stateChanged(int)
{
    mState.entity->SetFlag(game::EntityClass::Flags::KillAtBoundary, GetValue(mUI.chkKillAtBoundary));
}

void EntityWidget::on_chkTickEntity_stateChanged(int)
{
    mState.entity->SetFlag(game::EntityClass::Flags::TickEntity, GetValue(mUI.chkTickEntity));
}
void EntityWidget::on_chkUpdateEntity_stateChanged(int)
{
    mState.entity->SetFlag(game::EntityClass::Flags::UpdateEntity, GetValue(mUI.chkUpdateEntity));
}
void EntityWidget::on_chkKeyEvents_stateChanged(int)
{
    mState.entity->SetFlag(game::EntityClass::Flags::WantsKeyEvents, GetValue(mUI.chkKeyEvents));
}
void EntityWidget::on_chkMouseEvents_stateChanged(int)
{
    mState.entity->SetFlag(game::EntityClass::Flags::WantsMouseEvents, GetValue(mUI.chkMouseEvents));
}

void EntityWidget::on_btnAddIdleTrack_clicked()
{
    // todo:
}

void EntityWidget::on_btnResetIdleTrack_clicked()
{
    mState.entity->ResetIdleTrack();
    SetValue(mUI.idleTrack, -1);
}

void EntityWidget::on_btnAddScript_clicked()
{
    app::Script script;
    // use the script ID as the file name so that we can
    // avoid naming clashes and always find the correct lua
    // file even if the entity is later renamed.
    //const auto& filename = app::FromUtf8(script.GetId());
    const auto& uri  = app::toString("ws://lua/%1.lua", script.GetId());
    const auto& file = mState.workspace->MapFileToFilesystem(uri);
    const auto& name = GetValue(mUI.entityName);
    if (app::FileExists(file))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Question);
        msg.setWindowTitle(tr("File Exists"));
        msg.setText(tr("Overwrite existing script file?\n%1").arg(file));
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
        if (msg.exec() == QMessageBox::Cancel)
            return;
    }

    QString source = GenerateEntityScriptSource(GetValue(mUI.entityName));

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
    app::ScriptResource resource(script, GetValue(mUI.entityName));
    mState.workspace->SaveResource(resource);
    mState.entity->SetSriptFileId(script.GetId());

    auto* widget = new ScriptWidget(mState.workspace, resource);
    emit OpenNewWidget(widget);

    SetValue(mUI.scriptFile, ListItemId(script.GetId()));
    SetEnabled(mUI.btnEditScript, true);
}
void EntityWidget::on_btnEditScript_clicked()
{
    const auto& id = (QString)GetItemId(mUI.scriptFile);
    if (id.isEmpty())
        return;
    emit OpenResource(id);
}

void EntityWidget::on_btnResetScript_clicked()
{
    mState.entity->ResetScriptFile();
    SetValue(mUI.scriptFile, -1);
    SetEnabled(mUI.btnEditScript, false);
}

void EntityWidget::on_btnEditAnimator_clicked()
{
    if (mState.entity->GetNumAnimators() == 0)
        return;
    const auto& animator = mState.entity->GetAnimator(0);

    QVariantMap props;
    if (const auto* ptr = base::SafeFind(mAnimatorProperties, animator.GetId()))
        props = *ptr;
    DlgAnimator dlg(this, *mState.entity, animator, props);
    dlg.SetEntityWidget(this);
    dlg.exec();
}

void EntityWidget::on_btnViewPlus90_clicked()
{
    const float value = GetValue(mUI.rotation);
    mUI.rotation->setValue(math::clamp(-180.0f, 180.0f, value + 90.0f));
    mViewTransformRotation  = value;
    mViewRotationStartTime = mCurrentTime;
}

void EntityWidget::on_btnViewMinus90_clicked()
{
    const float value = GetValue(mUI.rotation);
    mUI.rotation->setValue(math::clamp(-180.0f, 180.0f, value - 90.0f));
    mViewTransformRotation  = value;
    mViewRotationStartTime = mCurrentTime;
}

void EntityWidget::on_btnViewReset_clicked()
{
    const auto width = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto rotation = mUI.rotation->value();
    mViewRotationStartTime    = mCurrentTime;
    mViewTranslationStartTime = mCurrentTime;
    mViewTranslationStart     = glm::vec2(mState.camera_offset_x, mState.camera_offset_y);
    mViewTransformRotation    = rotation;
    mState.camera_offset_x    = width * 0.5f;
    mState.camera_offset_y    = height * 0.5f;

    // set new camera offset to the center of the widget.
    SetValue(mUI.translateX, 0.0f);
    SetValue(mUI.translateY, 0.0f);
    SetValue(mUI.scaleX,     1.0f);
    SetValue(mUI.scaleY,     1.0f);
    SetValue(mUI.rotation,   0.0f);
}

void EntityWidget::on_btnNewTrack_clicked()
{
    // sharing the animation class object with the new animation
    // track widget.
    auto* widget = new AnimationTrackWidget(mState.workspace, mState.entity);
    widget->SetZoom(GetValue(mUI.zoom));
    widget->SetShowGrid(GetValue(mUI.chkShowGrid));
    widget->SetShowOrigin(GetValue(mUI.chkShowOrigin));
    widget->SetShowViewport(GetValue(mUI.chkShowViewport));
    widget->SetSnapGrid(GetValue(mUI.chkSnap));
    widget->SetGrid(GetValue(mUI.cmbGrid));
    emit OpenNewWidget(widget);
}
void EntityWidget::on_btnEditTrack_clicked()
{
    auto items = mUI.trackList->selectedItems();
    if (items.isEmpty())
        return;
    QListWidgetItem* item = items[0];
    QString id = item->data(Qt::UserRole).toString();

    for (size_t i=0; i< mState.entity->GetNumAnimations(); ++i)
    {
        const auto& klass = mState.entity->GetAnimation(i);
        if (klass.GetId() != app::ToUtf8(id))
            continue;
        auto it = mTrackProperties.find(klass.GetId());
        ASSERT(it != mTrackProperties.end());
        const auto& properties = (*it).second;
        auto* widget = new AnimationTrackWidget(mState.workspace, mState.entity, klass, properties);
        widget->SetZoom(GetValue(mUI.zoom));
        widget->SetShowGrid(GetValue(mUI.chkShowGrid));
        widget->SetShowOrigin(GetValue(mUI.chkShowOrigin));
        widget->SetSnapGrid(GetValue(mUI.chkSnap));
        widget->SetGrid(GetValue(mUI.cmbGrid));
        emit OpenNewWidget(widget);
    }
}
void EntityWidget::on_btnDeleteTrack_clicked()
{
    const auto& items = mUI.trackList->selectedItems();
    if (items.isEmpty())
        return;
    const auto* item = items[0];
    const auto& trackId = (std::string)GetItemId(item);

    if (mState.entity->HasIdleTrack())
    {
        if (mState.entity->GetIdleTrackId() == trackId)
        {
            QMessageBox msg(this);
            msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msg.setIcon(QMessageBox::Question);
            msg.setText(tr("The selected track is the current entity idle track.\n"
                           "Are you sure you want to delete it?"));
            if (msg.exec() == QMessageBox::No)
                return;
            mState.entity->ResetIdleTrack();
            SetValue(mUI.idleTrack, -1);
        }
    }
    mState.entity->DeleteAnimationById(trackId);
    // this will remove it from the widget.
    delete item;
    // delete the associated properties.
    auto it = mTrackProperties.find(trackId);
    ASSERT(it != mTrackProperties.end());
    mTrackProperties.erase(it);
}

void EntityWidget::on_btnNewScriptVar_clicked()
{
    std::vector<ResourceListItem> entities;
    std::vector<ResourceListItem> nodes;
    for (size_t i=0; i<mState.entity->GetNumNodes(); ++i)
    {
        const auto& node = mState.entity->GetNode(i);
        ResourceListItem item;
        item.name = node.GetName();
        item.id   = node.GetId();
        nodes.push_back(std::move(item));
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
void EntityWidget::on_btnEditScriptVar_clicked()
{
    auto items = mUI.scriptVarList->selectionModel()->selectedRows();
    if (items.isEmpty())
        return;

    std::vector<ResourceListItem> entities;
    std::vector<ResourceListItem> nodes;
    for (size_t i=0; i<mState.entity->GetNumNodes(); ++i)
    {
        const auto& node = mState.entity->GetNode(i);
        ResourceListItem item;
        item.name = node.GetName();
        item.id   = node.GetId();
        nodes.push_back(std::move(item));
    }

    // single selection for now.
    const auto index = items[0];
    game::ScriptVar var = mState.entity->GetScriptVar(index.row());
    DlgScriptVar dlg(nodes, entities, mState.workspace->ListAllMaterials(),
                     this, var);
    if (dlg.exec() == QDialog::Rejected)
        return;

    mScriptVarModel->EditVariable(index.row(), std::move(var));
}

void EntityWidget::on_btnDeleteScriptVar_clicked()
{
    auto items = mUI.scriptVarList->selectionModel()->selectedRows();
    if (items.isEmpty())
        return;

    // single selection for now.
    const auto index = items[0];
    mScriptVarModel->DeleteVariable(index.row());
    const auto vars = mState.entity->GetNumScriptVars();
    SetEnabled(mUI.btnEditScriptVar, vars > 0);
    SetEnabled(mUI.btnDeleteScriptVar, vars > 0);
}
void EntityWidget::on_btnResetLifetime_clicked()
{
    mState.entity->SetFlag(game::EntityClass::Flags::LimitLifetime, false);
    mState.entity->SetLifetime(0.0f);
    SetValue(mUI.entityLifetime, 0.0f);
}

void EntityWidget::on_btnNewJoint_clicked()
{
    game::EntityClass::PhysicsJoint joint;
    joint.id = base::RandomString(10);
    DlgJoint dlg(this, *mState.entity, joint);
    if (dlg.exec() == QDialog::Rejected)
        return;

    mJointModel->AddJoint(std::move(joint));
    SetEnabled(mUI.btnEditJoint, true);
    SetEnabled(mUI.btnDeleteJoint, true);
}
void EntityWidget::on_btnEditJoint_clicked()
{
    auto items = mUI.jointList->selectionModel()->selectedRows();
    if (items.isEmpty())
        return;

    // single selection for now.
    const auto index = items[0];
    auto joint = mState.entity->GetJoint(index.row());
    DlgJoint dlg(this, *mState.entity, joint);
    if (dlg.exec() == QDialog::Rejected)
        return;

    mJointModel->EditJoint(index.row(), std::move(joint));

}
void EntityWidget::on_btnDeleteJoint_clicked()
{
    auto items = mUI.jointList->selectionModel()->selectedRows();
    if (items.isEmpty())
        return;

    // single selection for now.
    const auto index = items[0];
    mJointModel->DeleteJoint(index.row());
    const auto joints = mState.entity->GetNumJoints();
    SetEnabled(mUI.btnEditJoint, joints > 0);
    SetEnabled(mUI.btnDeleteJoint, joints > 0);
}

void EntityWidget::on_btnSelectMaterial_clicked()
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* drawable = node->GetDrawable())
        {
            DlgMaterial dlg(this, mState.workspace, drawable->GetMaterialId());
            if (dlg.exec() == QDialog::Rejected)
                return;
            const auto& material = app::ToUtf8(dlg.GetSelectedMaterialId());
            if (drawable->GetMaterialId() == material)
                return;
            drawable->ResetMaterial();
            drawable->SetMaterialId(material);
            DisplayCurrentNodeProperties();
            RealizeEntityChange(mState.entity);
        }
    }
}

void EntityWidget::on_btnSetMaterialParams_clicked()
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* drawable = node->GetDrawable())
        {
            const auto& material = mState.workspace->GetMaterialClassById(drawable->GetMaterialId());
            DlgMaterialParams dlg(this, drawable);
            dlg.AdaptInterface(mState.workspace, material.get());
            dlg.exec();
        }
    }
}

void EntityWidget::on_btnEditDrawable_clicked()
{
    if (auto* node = GetCurrentNode())
    {
        const auto& id = (QString)GetItemId(mUI.dsDrawable);
        if (id.isEmpty())
            return;
        auto& resource = mState.workspace->GetResourceById(id);
        if (resource.IsPrimitive())
            return;
        emit OpenResource(id);
    }
}

void EntityWidget::on_btnEditMaterial_clicked()
{
    if (auto* node = GetCurrentNode())
    {
        const auto& id = (QString)GetItemId(mUI.dsMaterial);
        if (id.isEmpty())
            return;
        auto& resource = mState.workspace->GetResourceById(id);
        if (resource.IsPrimitive())
            return;
        emit OpenResource(id);
    }
}

void EntityWidget::on_btnEditAnimatorScript_clicked()
{
    const auto& id = (QString)GetItemId(mUI.animatorScript);
    if (id.isEmpty())
        return;

    emit OpenResource(id);
}

void EntityWidget::on_btnAddAnimatorScript_clicked()
{
    if (mState.entity->GetNumAnimators() == 0)
        return;

    auto& animator = mState.entity->GetAnimator(0);

    app::Script script;
    const auto& uri = app::toString("ws://lua/%1.lua", script.GetId());
    const auto& file = mState.workspace->MapFileToFilesystem(uri);
    if (app::FileExists(file))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Question);
        msg.setWindowTitle("File Exists");
        msg.setText(tr("Overwrite existing script file?\n%1").arg(file));
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
        if (msg.exec() == QMessageBox::Cancel)
            return;
    }
    QString source = GenerateAnimatorScriptSource();
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
    script.SetName(base::FormatString("%1 / Animator", mState.entity->GetName()));
    app::ScriptResource resource(script, app::toString("%1 / Animator", mState.entity->GetName()));
    mState.workspace->SaveResource(resource);

    animator.SetScriptId(script.GetId());

    auto* widget = new ScriptWidget(mState.workspace, resource);
    emit OpenNewWidget(widget);

    SetValue(mUI.animatorScript, ListItemId(script.GetId()));
    SetEnabled(mUI.btnEditAnimatorScript, true);
    SetEnabled(mUI.btnResetAnimatorScript, true);
}
void EntityWidget::on_btnResetAnimatorScript_clicked()
{
    if (mState.entity->GetNumAnimators() == 0)
        return;

    auto& animator = mState.entity->GetAnimator(0);
    animator.SetScriptId("");
    SetValue(mUI.animatorScript, -1);
    SetEnabled(mUI.btnEditAnimatorScript, false);
    SetEnabled(mUI.btnResetAnimatorScript, false);
}

void EntityWidget::on_trackList_itemSelectionChanged()
{
    auto list = mUI.trackList->selectedItems();
    mUI.btnEditTrack->setEnabled(!list.isEmpty());
    mUI.btnDeleteTrack->setEnabled(!list.isEmpty());
}

void EntityWidget::on_idleTrack_currentIndexChanged(int index)
{
    if (index == -1)
    {
        mState.entity->ResetIdleTrack();
        return;
    }
    mState.entity->SetIdleTrackId(GetItemId(mUI.idleTrack));
}

void EntityWidget::on_scriptFile_currentIndexChanged(int index)
{
    if (index == -1)
    {
        mState.entity->ResetScriptFile();
        SetEnabled(mUI.btnEditScript, false);
        SetEnabled(mUI.btnResetScript, false);
        return;
    }
    mState.entity->SetSriptFileId(GetItemId(mUI.scriptFile));
    SetEnabled(mUI.btnEditScript, true);
}

void EntityWidget::on_animatorScript_currentIndexChanged(int index)
{
    if (index == -1)
    {
        for (size_t i=0; i<mState.entity->GetNumAnimators(); ++i)
        {
            auto& anim = mState.entity->GetAnimator(i);
            anim.SetScriptId("");
        }
        SetEnabled(mUI.btnEditAnimatorScript, false);
        SetEnabled(mUI.btnResetAnimatorScript, false);
    }
    else
    {
        for (size_t i=0; i<mState.entity->GetNumAnimators(); ++i)
        {
            auto& anim = mState.entity->GetAnimator(i);
            anim.SetScriptId(GetItemId(mUI.animatorScript));
        }
        SetEnabled(mUI.btnEditAnimatorScript, true);
        SetEnabled(mUI.btnResetAnimatorScript, true);
    }
}

void EntityWidget::on_nodeName_textChanged(const QString& text)
{
    TreeWidget::TreeItem* item = mUI.tree->GetSelectedItem();
    if (item == nullptr)
        return;
    else if (!item->GetUserData())
        return;
    auto* node = static_cast<game::EntityNodeClass*>(item->GetUserData());
    node->SetName(app::ToUtf8(text));
    item->SetText(text);
    mUI.tree->Update();
    RebuildCombosInternal();
    RealizeEntityChange(mState.entity);
}
void EntityWidget::on_nodeComment_textChanged(const QString& text)
{
    if (const auto* node = GetCurrentNode())
    {
        if (text.isEmpty())
            mComments.erase(node->GetId());
        else mComments[node->GetId()] = text;
    }
}

void EntityWidget::on_nodeTag_textChanged(const QString& text)
{
    if (auto* node = GetCurrentNode())
    {
        node->SetTag(GetValue(mUI.nodeTag));
    }
}

void EntityWidget::on_nodeIndex_valueChanged(int)
{
    if (const auto* node = GetCurrentNode())
    {
        const size_t src_index = mState.entity->FindNodeIndex(node);
        ASSERT(src_index < mState.entity->GetNumNodes());

        const size_t dst_index = GetValue(mUI.nodeIndex);
        if (dst_index >= mState.entity->GetNumNodes())
        {
            SetValue(mUI.nodeIndex, src_index);
            return;
        }
        mState.entity->MoveNode(src_index, dst_index);
    }
}

void EntityWidget::on_nodeSizeX_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_nodeSizeY_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_nodeTranslateX_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_nodeTranslateY_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_nodeScaleX_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_nodeScaleY_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_nodeRotation_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_nodePlus90_clicked()
{
    const float value = GetValue(mUI.nodeRotation);
    SetValue(mUI.nodeRotation, math::clamp(-180.0f, 180.0f, value + 90.0f));
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_nodeMinus90_clicked()
{
    const float value = GetValue(mUI.nodeRotation);
    SetValue(mUI.nodeRotation,math::clamp(-180.0f, 180.0f, value - 90.0f));
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsDrawable_currentIndexChanged(const QString& name)
{
    UpdateCurrentNodeProperties();
    DisplayCurrentNodeProperties();
}
void EntityWidget::on_dsMaterial_currentIndexChanged(const QString& name)
{
    if (auto* node = GetCurrentNode())
    {
        auto* drawable = node->GetDrawable();
        if (drawable)
            drawable->ClearMaterialParams();
    }

    UpdateCurrentNodeProperties();
    DisplayCurrentNodeProperties();
}
void EntityWidget::on_dsRenderPass_currentIndexChanged(const QString&)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_dsRenderStyle_currentIndexChanged(const QString&)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsLayer_valueChanged(int value)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsLineWidth_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsTimeScale_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsVisible_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsUpdateDrawable_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_dsUpdateMaterial_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_dsRestartDrawable_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_dsFlipHorizontally_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_dsFlipVertically_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsBloom_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_rbSimulation_currentIndexChanged(const QString&)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_rbShape_currentIndexChanged(const QString&)
{
    UpdateCurrentNodeProperties();
    DisplayCurrentNodeProperties();
}

void EntityWidget::on_rbPolygon_currentIndexChanged(const QString&)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_rbFriction_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbRestitution_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbAngularDamping_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbLinearDamping_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbDensity_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbIsBullet_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbIsSensor_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbIsEnabled_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_rbCanSleep_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbDiscardRotation_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiFontName_currentIndexChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiFontSize_currentIndexChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiVAlign_currentIndexChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiHAlign_currentIndexChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiTextColor_colorChanged(QColor color)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiLineHeight_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiLayer_valueChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiRasterWidth_valueChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiRasterHeight_valueChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiText_textChanged()
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiVisible_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiUnderline_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiBlink_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_tiStatic_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_tiBloom_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_spnShape_currentIndexChanged(const QString&)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_spnEnabled_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_fxShape_currentIndexChanged(const QString&)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_fxBody_currentIndexChanged(const QString&)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_fxPolygon_currentIndexChanged(const QString&)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_fxFriction_valueChanged(double)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_fxDensity_valueChanged(double)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_fxBounciness_valueChanged(double)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_fxIsSensor_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_btnResetFxFriction_clicked()
{
    SetValue(mUI.fxFriction,  mUI.fxFriction->minimum());
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_btnResetFxDensity_clicked()
{
    SetValue(mUI.fxDensity, mUI.fxDensity->minimum());
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_btnResetFxBounciness_clicked()
{
    SetValue(mUI.fxBounciness, mUI.fxBounciness->minimum());
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_btnSelectFont_clicked()
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* text = node->GetTextItem())
        {
            DlgFont::DisplaySettings disp;
            disp.font_size  = text->GetFontSize();
            disp.text_color = FromGfx(text->GetTextColor());
            disp.underline  = text->TestFlag(game::TextItemClass::Flags::UnderlineText);
            disp.blinking   = text->TestFlag(game::TextItemClass::Flags::BlinkText);
            DlgFont dlg(this, mState.workspace, text->GetFontName(), disp);
            if (dlg.exec() == QDialog::Rejected)
                return;
            SetValue(mUI.tiFontName, dlg.GetSelectedFontURI());
            text->SetFontName(dlg.GetSelectedFontURI());
            RealizeEntityChange(mState.entity);
        }
    }
}

void EntityWidget::on_btnSelectFontFile_clicked()
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* text = node->GetTextItem())
        {
            const auto& name = QFileDialog::getOpenFileName(this ,
                tr("Select Font File") , "" ,
                tr("Font (*.ttf *.otf *.json)"));
            if (name.isEmpty())
                return;
            const auto& file = mState.workspace->MapFileToWorkspace(name);
            SetValue(mUI.tiFontName , file);
            text->SetFontName(file);
            RealizeEntityChange(mState.entity);
        }
    }
}

void EntityWidget::on_btnResetTextRasterWidth_clicked()
{
    SetValue(mUI.tiRasterWidth, 0);
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_btnResetTextRasterHeight_clicked()
{
    SetValue(mUI.tiRasterHeight, 0);
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_drawableItem_toggled(bool on)
{
    if (auto* node = GetCurrentNode())
    {
        if (on)
        {
            if (!node->HasDrawable())
            {
                game::DrawableItemClass draw;
                draw.SetMaterialId(mState.workspace->GetMaterialClassByName("Checkerboard")->GetId());
                draw.SetDrawableId(mState.workspace->GetDrawableClassByName("Rectangle")->GetId());
                node->SetDrawable(draw);
                DEBUG("Added drawable item to '%1'", node->GetName());
            }
        }
        else
        {
            node->RemoveDrawable();
            DEBUG("Removed drawable item from '%1'", node->GetName());
        }
        DisplayCurrentNodeProperties();
        RealizeEntityChange(mState.entity);
    }
}
void EntityWidget::on_rigidBodyItem_toggled(bool on)
{
    auto* node = GetCurrentNode();
    if (!node)
        return;
    if (!on)
    {
        node->RemoveRigidBody();
        DEBUG("Removed rigid body from '%1'" , node->GetName());
    }
    else if (!node->HasRigidBody())
    {
        game::RigidBodyItemClass body;
        // try to see if we can figure out the right collision
        // box for this rigid body based on the drawable.
        if (auto* item = node->GetDrawable())
        {
            const auto& drawableId = item->GetDrawableId();
            if (drawableId == "_circle")
                body.SetCollisionShape(game::RigidBodyItemClass::CollisionShape::Circle);
            else if (drawableId == "_parallelogram")
                body.SetCollisionShape(game::RigidBodyItemClass::CollisionShape::Parallelogram);
            else if (drawableId == "_rect" || drawableId == "_round_rect")
                body.SetCollisionShape(game::RigidBodyItemClass::CollisionShape::Box);
            else if (drawableId == "_isosceles_triangle")
                body.SetCollisionShape(game::RigidBodyItemClass::CollisionShape::IsoscelesTriangle);
            else if (drawableId == "_right_triangle")
                body.SetCollisionShape(game::RigidBodyItemClass::CollisionShape::RightTriangle);
            else if (drawableId == "_trapezoid")
                body.SetCollisionShape(game::RigidBodyItemClass::CollisionShape::Trapezoid);
            else if (drawableId == "_semi_circle")
                body.SetCollisionShape(game::RigidBodyItemClass::CollisionShape::SemiCircle);
            else if (auto klass = mState.workspace->FindDrawableClassById(drawableId)) {
                if (klass->GetType() == gfx::DrawableClass::Type::Polygon) {
                    body.SetPolygonShapeId(drawableId);
                    body.SetCollisionShape(game::RigidBodyItemClass::CollisionShape::Polygon);
                }
            }
        }
        node->SetRigidBody(body);
        DEBUG("Added rigid body to '%1'", node->GetName());
    }

    mState.entity->DeleteInvalidJoints();
    mState.entity->DeleteInvalidFixtures();
    mJointModel->Reset();
    DisplayEntityProperties();
    DisplayCurrentNodeProperties();
    RebuildCombosInternal();
    RealizeEntityChange(mState.entity);
}

void EntityWidget::on_textItem_toggled(bool on)
{
    if (auto* node = GetCurrentNode())
    {
        if (on)
        {
            if (!node->HasTextItem())
            {
                // Select some font as a default. Without this the font is an
                // empty string which will not render any text (but rather print
                // a cascade of crap in the debug/error logs)
                SetValue(mUI.tiFontName, 0);

                game::TextItemClass text;
                text.SetFontSize(GetValue(mUI.tiFontSize));
                text.SetFontName(GetValue(mUI.tiFontName));
                node->SetTextItem(text);
                DEBUG("Added text item to '%1'", node->GetName());
            }
        }
        else
        {
            node->RemoveTextItem();
            DEBUG("Removed text item from '%1'" , node->GetName());
        }
        RealizeEntityChange(mState.entity);
        DisplayCurrentNodeProperties();
    }
}
void EntityWidget::on_spatialNode_toggled(bool on)
{
    if (auto* node = GetCurrentNode())
    {
        if (on)
        {
            if (!node->HasSpatialNode())
            {
                game::SpatialNodeClass sp;
                sp.SetShape(GetValue(mUI.spnShape));
                sp.SetFlag(game::SpatialNodeClass::Flags::Enabled, GetValue(mUI.spnEnabled));
                node->SetSpatialNode(sp);
                DEBUG("Added spatial node to '%1'.", node->GetName());
            }
        }
        else
        {
            node->RemoveSpatialNode();
            DEBUG("Removed spatial node from '%1'.", node->GetName());
        }
    }
    DisplayCurrentNodeProperties();
}

void EntityWidget::on_fixture_toggled(bool on)
{
    if (auto* node = GetCurrentNode())
    {
        if (on && !node->HasFixture())
        {
            game::FixtureClass fixture;
            // try to see if we can figure out the right collision
            // box for this rigid body based on the drawable.
            if (auto* item = node->GetDrawable())
            {
                const auto& drawableId = item->GetDrawableId();
                if (drawableId == "_circle")
                    fixture.SetCollisionShape(game::FixtureClass::CollisionShape::Circle);
                else if (drawableId == "_parallelogram")
                    fixture.SetCollisionShape(game::FixtureClass::CollisionShape::Parallelogram);
                else if (drawableId == "_rect" || drawableId == "_round_rect")
                    fixture.SetCollisionShape(game::FixtureClass::CollisionShape::Box);
                else if (drawableId == "_isosceles_triangle")
                    fixture.SetCollisionShape(game::FixtureClass::CollisionShape::IsoscelesTriangle);
                else if (drawableId == "_right_triangle")
                    fixture.SetCollisionShape(game::FixtureClass::CollisionShape::RightTriangle);
                else if (drawableId == "_trapezoid")
                    fixture.SetCollisionShape(game::FixtureClass::CollisionShape::Trapezoid);
                else if (drawableId == "_semi_circle")
                    fixture.SetCollisionShape(game::FixtureClass::CollisionShape::SemiCircle);
                else if (auto klass = mState.workspace->FindDrawableClassById(drawableId)) {
                    if (klass->GetType() == gfx::DrawableClass::Type::Polygon)
                    {
                        fixture.SetPolygonShapeId(drawableId);
                        fixture.SetCollisionShape(game::FixtureClass::CollisionShape::Polygon);
                    }
                }
            }


            node->SetFixture(fixture);
            DEBUG("Added fixture to '%1'.", node->GetName());
        }
        else if (!on && node->HasFixture())
        {
            node->RemoveFixture();
            DEBUG("Removed fixture from '%1'.", node->GetName());
        }
    }
    DisplayCurrentNodeProperties();
}

void EntityWidget::on_animator_toggled(bool on)
{
    if (on)
    {
        game::AnimatorClass animator;
        animator.SetName("My Animator");
        mState.entity->AddAnimator(std::move(animator));
    }
    else
    {
        ASSERT(mState.entity->GetNumAnimators() == 1);
        mState.entity->DeleteAnimator(0);
    }
}

void EntityWidget::on_tree_customContextMenuRequested(QPoint)
{
    const auto* node = GetCurrentNode();
    const auto* item = node ? node->GetDrawable() : nullptr;

    mUI.actionNodeMoveDownLayer->setEnabled(item != nullptr);
    mUI.actionNodeMoveUpLayer->setEnabled(item != nullptr);
    mUI.actionNodeDelete->setEnabled(node != nullptr);
    mUI.actionNodeDuplicate->setEnabled(node != nullptr);
    mUI.actionNodeVarRef->setEnabled(node != nullptr);
    mUI.actionNodeComment->setEnabled(node != nullptr);
    mUI.actionNodeRename->setEnabled(node != nullptr);

    QMenu menu(this);
    menu.addAction(mUI.actionNodeMoveUpLayer);
    menu.addAction(mUI.actionNodeMoveDownLayer);
    menu.addSeparator();
    menu.addAction(mUI.actionNodeDuplicate);
    menu.addAction(mUI.actionNodeRename);
    menu.addAction(mUI.actionNodeRenameAll);
    menu.addSeparator();
    menu.addAction(mUI.actionNodeVarRef);
    menu.addAction(mUI.actionNodeComment);
    menu.addSeparator();
    menu.addAction(mUI.actionNodeDelete);
    menu.exec(QCursor::pos());
}

void EntityWidget::on_scriptVarList_customContextMenuRequested(QPoint)
{
    QMenu menu(this);
    menu.addAction(mUI.actionScriptVarAdd);
    menu.addAction(mUI.actionScriptVarEdit);
    menu.addAction(mUI.actionScriptVarDel);
    menu.exec(QCursor::pos());
}

void EntityWidget::on_jointList_customContextMenuRequested(QPoint)
{
    QMenu menu(this);
    menu.addAction(mUI.actionJointAdd);
    menu.addAction(mUI.actionJointEdit);
    menu.addAction(mUI.actionJointDel);
    menu.exec(QCursor::pos());
}

void EntityWidget::on_trackList_customContextMenuRequested(QPoint)
{
    QMenu menu(this);
    menu.addAction(mUI.actionAnimationAdd);
    menu.addAction(mUI.actionAnimationEdit);
    menu.addAction(mUI.actionAnimationDel);
    menu.exec(QCursor::pos());
}

void EntityWidget::TreeCurrentNodeChangedEvent()
{
    DisplayCurrentNodeProperties();
}
void EntityWidget::TreeDragEvent(TreeWidget::TreeItem* item, TreeWidget::TreeItem* target)
{
    auto& tree = mState.entity->GetRenderTree();
    auto* src_value = static_cast<game::EntityNodeClass*>(item->GetUserData());
    auto* dst_value = static_cast<game::EntityNodeClass*>(target->GetUserData());

    // check if we're trying to drag a parent onto its own child
    if (game::SearchChild(tree, dst_value, src_value))
        return;
    const bool retain_world_transform = true;
    mState.entity->ReparentChild(dst_value, src_value, retain_world_transform);
}
void EntityWidget::TreeClickEvent(TreeWidget::TreeItem* item)
{
    //DEBUG("Tree click event: %1", item->GetId());
    auto* node = static_cast<game::EntityNodeClass*>(item->GetUserData());
    if (node == nullptr)
        return;

    const bool visibility = !node->TestFlag(game::EntityNodeClass::Flags::VisibleInEditor);
    node->SetFlag(game::EntityNodeClass::Flags::VisibleInEditor, visibility);
    item->SetIcon(visibility ? QIcon("icons:eye.png") : QIcon("icons:crossed_eye.png"));
    mUI.tree->Update();
}

void EntityWidget::ResourceAdded(const app::Resource* resource)
{
    RebuildCombos();
    RebuildMenus();
    DisplayEntityProperties();
    DisplayCurrentNodeProperties();
}
void EntityWidget::ResourceRemoved(const app::Resource* resource)
{
    UpdateDeletedResourceReferences();
    RebuildCombos();
    RebuildMenus();
    DisplayEntityProperties();
    DisplayCurrentNodeProperties();
}
void EntityWidget::ResourceUpdated(const app::Resource* resource)
{
    RebuildCombos();
    RebuildMenus();
    RealizeEntityChange(mState.entity);
    mState.renderer.ClearPaintState();
}

void EntityWidget::PlaceNewParticleSystem()
{
    const auto* action = qobject_cast<QAction*>(sender());
    // using the data in the action as the class id of the drawable.
    const auto drawable = action->data().toString();
    // check the resource in order to get the default material name set in the
    // particle editor.
    const auto& resource = mState.workspace->GetResourceById(drawable);
    QString material = resource.GetProperty("material",  QString("_checkerboard"));
    if (!mState.workspace->IsValidMaterial(material))
        material = "_checkerboard";
    mCurrentTool.reset(new PlaceShapeTool(mState, material, drawable));
    mParticleSystems->menuAction()->setChecked(true);
}
void EntityWidget::PlaceNewCustomShape()
{
    const auto* action = qobject_cast<QAction*>(sender());
    // using the data in the action as the name of the drawable.
    const auto drawable = action->data().toString();
    // check the resource in order to get the default material name set in the
    // shape editor.
    const auto& resource = mState.workspace->GetResourceById(drawable);
    QString material = resource.GetProperty("material",  QString("_checkerboard"));
    if (!mState.workspace->IsValidMaterial(material))
        material = "_checkerboard";
    mCurrentTool.reset(new PlaceShapeTool(mState, material, drawable));
    mCustomShapes->menuAction()->setChecked(true);
}

void EntityWidget::InitScene(unsigned width, unsigned height)
{
    if (!mCameraWasLoaded)
    {
        // if the camera hasn't been loaded then compute now the
        // initial position for the camera.
        mState.camera_offset_x = mUI.widget->width()  * 0.5;
        mState.camera_offset_y = mUI.widget->height() * 0.5;
    }
    DisplayCurrentCameraLocation();
}

void EntityWidget::PaintScene(gfx::Painter& painter, double /*secs*/)
{
    // WARNING, if you use the preview window here to draw the underlying
    // OpenGL Context will change unexpectedly and then drawing below
    // will trigger OpenGL errors.

    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto zoom   = (float)GetValue(mUI.zoom);
    const auto xs     = (float)GetValue(mUI.scaleX);
    const auto ys     = (float)GetValue(mUI.scaleY);
    const auto grid   = (GridDensity)GetValue(mUI.cmbGrid);
    const auto view_rotation_time = math::clamp(0.0, 1.0, mCurrentTime - mViewRotationStartTime);
    const auto view_rotation_angle = math::interpolate(mViewTransformRotation, (float)mUI.rotation->value(),
                                                       view_rotation_time, math::Interpolation::Cosine);
    const auto view_translation_time = math::clamp(0.0, 1.0, mCurrentTime - mViewTranslationStartTime);
    const auto view_translation = math::interpolate(mViewTranslationStart, glm::vec2(mState.camera_offset_x, mState.camera_offset_y),
                                                    view_translation_time, math::Interpolation::Cosine);

    SetValue(mUI.widgetColor, mUI.widget->GetCurrentClearColor());

    gfx::Transform view;
    view.Scale(xs, ys);
    view.Scale(zoom, zoom);
    view.RotateAroundZ(qDegreesToRadians(view_rotation_angle));
    view.Translate(view_translation);

    painter.SetViewport(0, 0, width, height);
    painter.SetPixelRatio(glm::vec2(xs*zoom, ys*zoom));
    painter.ResetViewMatrix();

    // render endless background grid.
    if (GetValue(mUI.chkShowGrid))
    {
        DrawCoordinateGrid(painter, view, grid, zoom, xs, ys, width, height);
    }

    DrawHook hook(GetCurrentNode());
    hook.SetDrawVectors(true);
    hook.SetIsPlaying(mPlayState == PlayState::Playing);

    // Draw entity
    painter.SetViewMatrix(view.GetAsMatrix());

    gfx::Transform entity;
    mState.renderer.BeginFrame();
    mState.renderer.Draw(*mState.entity, painter, entity, &hook);
    mState.renderer.EndFrame();
    // Draw joints, drawn in the entity space.
    for (size_t i=0; i<mState.entity->GetNumJoints(); ++i)
    {
        const auto& joint = mState.entity->GetJoint(i);
        if (joint.type == game::EntityClass::PhysicsJointType::Distance)
        {
            const auto* src_node = mState.entity->FindNodeById(joint.src_node_id);
            const auto* dst_node = mState.entity->FindNodeById(joint.dst_node_id);
            const auto& src_anchor_point = src_node->GetSize() * 0.5f + joint.src_node_anchor_point;
            const auto& dst_anchor_point = dst_node->GetSize() * 0.5f + joint.dst_node_anchor_point;
            const auto& src_point = mState.entity->MapCoordsFromNodeBox(src_anchor_point, src_node);
            const auto& dst_point = mState.entity->MapCoordsFromNodeBox(dst_anchor_point, dst_node);
            DrawLine(entity, src_point, dst_point, painter);
        }
    }
    // Draw comments if any
    if (GetValue(mUI.chkShowComments))
    {
        for (const auto&[id, comment]: mComments)
        {
            if (comment.isEmpty())
                continue;
            if (const auto* node = mState.entity->FindNodeById(id))
            {
                const auto& size = node->GetSize();
                const auto& pos = mState.entity->MapCoordsFromNodeBox(size, node);
                ShowMessage(app::ToUtf8(comment), gfx::FPoint(pos.x + 10, pos.y + 10), painter);
            }
        }
    }

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

void EntityWidget::MouseZoom(std::function<void(void)> zoom_function)
{
    // where's the mouse in the widget
    const auto& mickey = mUI.widget->mapFromGlobal(QCursor::pos());
    // can't use underMouse here because of the way the gfx widget
    // is constructed i.e QWindow and Widget as container
    if (mickey.x() < 0 || mickey.y() < 0 ||
        mickey.x() > mUI.widget->width() ||
        mickey.y() > mUI.widget->height())
        return;

    glm::vec4 mickey_pos_in_entity;
    glm::vec4 mickey_pos_in_widget;

    {
        gfx::Transform view;
        view.Scale(GetValue(mUI.scaleX) , GetValue(mUI.scaleY));
        view.Scale(GetValue(mUI.zoom) , GetValue(mUI.zoom));
        view.RotateAroundZ(qDegreesToRadians(mUI.rotation->value()));
        view.Translate(mState.camera_offset_x , mState.camera_offset_y);
        const auto& mat = glm::inverse(view.GetAsMatrix());
        mickey_pos_in_entity = mat * glm::vec4(mickey.x() , mickey.y() , 1.0f , 1.0f);
    }

    zoom_function();

    {
        gfx::Transform view;
        view.Scale(GetValue(mUI.scaleX) , GetValue(mUI.scaleY));
        view.Scale(GetValue(mUI.zoom) , GetValue(mUI.zoom));
        view.RotateAroundZ(qDegreesToRadians(mUI.rotation->value()));
        view.Translate(mState.camera_offset_x , mState.camera_offset_y);
        const auto& mat = view.GetAsMatrix();
        mickey_pos_in_widget = mat * mickey_pos_in_entity;
    }
    mState.camera_offset_x += (mickey.x() - mickey_pos_in_widget.x);
    mState.camera_offset_y += (mickey.y() - mickey_pos_in_widget.y);
    DisplayCurrentCameraLocation();
}

void EntityWidget::MouseMove(QMouseEvent* mickey)
{
    if (mCurrentTool)
    {
        gfx::Transform view;
        view.Scale(GetValue(mUI.scaleX) , GetValue(mUI.scaleY));
        view.Scale(GetValue(mUI.zoom) , GetValue(mUI.zoom));
        view.RotateAroundZ(qDegreesToRadians(mUI.rotation->value()));
        view.Translate(mState.camera_offset_x , mState.camera_offset_y);
        mCurrentTool->MouseMove(mickey , view);

        // update the properties that might have changed as the result of application
        // of the current tool.
        DisplayCurrentCameraLocation();
        DisplayCurrentNodeProperties();
    }
}
void EntityWidget::MousePress(QMouseEvent* mickey)
{
    const auto snap = (bool)GetValue(mUI.chkSnap);
    const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
    const auto grid_size = (unsigned)grid;

    gfx::Transform view;
    view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
    view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
    view.RotateAroundZ((float) qDegreesToRadians(mUI.rotation->value()));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    if (!mCurrentTool && (mickey->button() == Qt::LeftButton))
    {
        auto [hitnode, hitpos] = SelectNode(mickey->pos(), view, *mState.entity, GetCurrentNode());
        if (hitnode)
        {
            view.Push(mState.entity->FindNodeTransform(hitnode));
                const auto mat = view.GetAsMatrix();
                glm::vec3 scale;
                glm::vec3 translation;
                glm::vec3 skew;
                glm::vec4 perspective;
                glm::quat orientation;
                glm::decompose(mat, scale, orientation, translation, skew,  perspective);
            view.Pop();
            
            const auto& size = hitnode->GetSize();
            const auto& box_size = glm::vec2(10.0f/scale.x, 10.0f/scale.y);
            // check if any particular special area of interest is being hit
            const bool bottom_right_hitbox_hit = hitpos.x >= size.x - box_size.x &&
                                                 hitpos.y >= size.y - box_size.y;
            const bool top_left_hitbox_hit = hitpos.x >= 0 && hitpos.x <= box_size.x &&
                                             hitpos.y >= 0 && hitpos.y <= box_size.y;
            if (bottom_right_hitbox_hit)
                mCurrentTool.reset(new ResizeRenderTreeNodeTool(*mState.entity, hitnode, snap, grid_size));
            else if (top_left_hitbox_hit)
                mCurrentTool.reset(new RotateRenderTreeNodeTool(*mState.entity, hitnode));
            else mCurrentTool.reset(new MoveRenderTreeNodeTool(*mState.entity, hitnode, snap, grid_size));

            mUI.tree->SelectItemById(app::FromUtf8(hitnode->GetId()));
        }
        else
        {
            mUI.tree->ClearSelection();
        }
    }
    else if (!mCurrentTool && (mickey->button() == Qt::RightButton))
    {
        mCurrentTool.reset(new MoveCameraTool(mState));
    }
    if (mCurrentTool)
        mCurrentTool->MousePress(mickey, view);
}
void EntityWidget::MouseRelease(QMouseEvent* mickey)
{
    if (!mCurrentTool)
        return;

    gfx::Transform view;
    view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
    view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
    view.RotateAroundZ(qDegreesToRadians(mUI.rotation->value()));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    if (mCurrentTool->MouseRelease(mickey, view))
    {
        mCurrentTool.reset();
        UncheckPlacementActions();
        DisplayCurrentNodeProperties();
        RealizeEntityChange(mState.entity);
    }
}

void EntityWidget::MouseDoubleClick(QMouseEvent* mickey)
{
    // double click is preceded by a regular click event and quick
    // googling suggests that there's really no way to filter out
    // single click when trying to react only to double click other
    // than to set a timer (which adds latency).
    // Going to simply discard any tool selection here on double click.
    mCurrentTool.reset();

    gfx::Transform view;
    view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
    view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
    view.RotateAroundZ((float) qDegreesToRadians(mUI.rotation->value()));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    auto [hitnode, hitpos] = SelectNode(mickey->pos(), view, *mState.entity, GetCurrentNode());
    if (!hitnode  || !hitnode->GetDrawable())
        return;

    auto* drawable = hitnode->GetDrawable();
    DlgMaterial dlg(this, mState.workspace, drawable->GetMaterialId());
    if (dlg.exec() == QDialog::Rejected)
        return;
    const auto& materialId = app::ToUtf8(dlg.GetSelectedMaterialId());
    if (drawable->GetMaterialId() == materialId)
        return;
    drawable->ResetMaterial();
    drawable->SetMaterialId(materialId);
    DisplayCurrentNodeProperties();
    RealizeEntityChange(mState.entity);
}

bool EntityWidget::KeyPress(QKeyEvent* key)
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
            TranslateCamera(20.0f, 0.0f);
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
            OnEscape();
            break;
        default:
            return false;
    }
    return true;
}

void EntityWidget::DisplayEntityProperties()
{
    std::vector<ResourceListItem> tracks;
    for (size_t i=0; i< mState.entity->GetNumAnimations(); ++i)
    {
        const auto& track = mState.entity->GetAnimation(i);
        ResourceListItem item;
        item.name = track.GetName();
        item.id   = track.GetId();
        item.icon = QIcon("icons:animation_track.png");
        tracks.push_back(std::move(item));
    }
    SetList(mUI.trackList, tracks);
    SetList(mUI.idleTrack, tracks);

    const auto animators = mState.entity->GetNumAnimators();
    const auto vars   = mState.entity->GetNumScriptVars();
    const auto joints = mState.entity->GetNumJoints();
    SetEnabled(mUI.btnEditScriptVar, vars > 0);
    SetEnabled(mUI.btnDeleteScriptVar, vars > 0);
    SetEnabled(mUI.btnDeleteTrack, false);
    SetEnabled(mUI.btnEditTrack, false);
    SetEnabled(mUI.btnEditJoint, joints > 0);
    SetEnabled(mUI.btnDeleteJoint, joints > 0);
    SetEnabled(mUI.btnEditScript, mState.entity->HasScriptFile());

    SetValue(mUI.animator, animators > 0);
    SetValue(mUI.entityName, mState.entity->GetName());
    SetValue(mUI.entityTag, mState.entity->GetTag());
    SetValue(mUI.entityID, mState.entity->GetId());
    SetValue(mUI.idleTrack, ListItemId(mState.entity->GetIdleTrackId()));
    SetValue(mUI.scriptFile, ListItemId(mState.entity->GetScriptFileId()));
    SetValue(mUI.entityLifetime, mState.entity->TestFlag(game::EntityClass::Flags::LimitLifetime)
                                 ? mState.entity->GetLifetime() : 0.0f);
    SetValue(mUI.chkKillAtLifetime, mState.entity->TestFlag(game::EntityClass::Flags::KillAtLifetime));
    SetValue(mUI.chkKillAtBoundary, mState.entity->TestFlag(game::EntityClass::Flags::KillAtBoundary));
    SetValue(mUI.chkTickEntity, mState.entity->TestFlag(game::EntityClass::Flags::TickEntity));
    SetValue(mUI.chkUpdateEntity, mState.entity->TestFlag(game::EntityClass::Flags::UpdateEntity));
    SetValue(mUI.chkKeyEvents, mState.entity->TestFlag(game::EntityClass::Flags::WantsKeyEvents));
    SetValue(mUI.chkMouseEvents, mState.entity->TestFlag(game::EntityClass::Flags::WantsMouseEvents));

    if (animators > 0)
    {
        const auto& animator = mState.entity->GetAnimator(0);
        if (animator.HasScriptId())
        {
            SetValue(mUI.animatorScript, ListItemId(animator.GetScriptId()));
            SetEnabled(mUI.btnResetAnimatorScript, true);
            SetEnabled(mUI.btnEditAnimatorScript, true);
        }
        else
        {
            SetValue(mUI.animatorScript, -1);
            SetEnabled(mUI.btnResetAnimatorScript, false);
            SetEnabled(mUI.btnEditAnimatorScript, false);
        }
    }
    else
    {
        SetValue(mUI.animatorScript, -1);
    }

    if (!mUI.trackList->selectedItems().isEmpty())
    {
        SetEnabled(mUI.btnDeleteTrack, true);
        SetEnabled(mUI.btnEditTrack, true);
    }
}

void EntityWidget::DisplayCurrentNodeProperties()
{
    SetValue(mUI.nodeID, QString(""));
    SetValue(mUI.nodeName, QString(""));
    SetValue(mUI.nodeTag, QString(""));
    SetValue(mUI.nodeComment, QString(""));
    SetValue(mUI.nodeIndex, 0);
    SetValue(mUI.nodeTranslateX, 0.0f);
    SetValue(mUI.nodeTranslateY, 0.0f);
    SetValue(mUI.nodeSizeX, 0.0f);
    SetValue(mUI.nodeSizeY, 0.0f);
    SetValue(mUI.nodeScaleX, 1.0f);
    SetValue(mUI.nodeScaleY, 1.0f);
    SetValue(mUI.nodeRotation, 0.0f);
    SetValue(mUI.drawableItem, false);
    SetValue(mUI.rigidBodyItem, false);
    SetValue(mUI.textItem, false);
    SetValue(mUI.spatialNode, false);
    SetValue(mUI.dsMaterial, QString(""));
    SetValue(mUI.dsDrawable, QString(""));
    SetValue(mUI.dsLayer, 0);
    SetValue(mUI.dsRenderPass, -1);
    SetValue(mUI.dsRenderStyle, -1);
    SetValue(mUI.dsLineWidth, 1.0f);
    SetValue(mUI.dsTimeScale, 1.0f);
    SetValue(mUI.rbShape, -1);
    SetValue(mUI.rbFriction, 0.0f);
    SetValue(mUI.rbRestitution, 0.0f);
    SetValue(mUI.rbAngularDamping, 0.0f);
    SetValue(mUI.rbLinearDamping, 0.0f);
    SetValue(mUI.rbDensity, 0.0f);
    SetValue(mUI.rbIsBullet, false);
    SetValue(mUI.rbIsSensor, false);
    SetValue(mUI.rbIsEnabled, false);
    SetValue(mUI.rbCanSleep, false);
    SetValue(mUI.rbDiscardRotation, false);
    SetValue(mUI.tiFontName, QString(""));
    SetValue(mUI.tiFontSize, 16);
    SetValue(mUI.tiVAlign, -1);
    SetValue(mUI.tiHAlign, -1);
    SetValue(mUI.tiTextColor, Qt::white);
    SetValue(mUI.tiLineHeight, 1.0f);
    SetValue(mUI.tiLayer, 0);
    SetValue(mUI.tiRasterWidth, 0);
    SetValue(mUI.tiRasterHeight, 0);
    SetValue(mUI.tiText, QString(""));
    SetValue(mUI.tiVisible, true);
    SetValue(mUI.tiUnderline, false);
    SetValue(mUI.tiBlink, false);
    SetValue(mUI.tiStatic, false);
    SetValue(mUI.tiBloom, false);
    SetValue(mUI.spnShape, -1);
    SetValue(mUI.spnEnabled, true);
    SetValue(mUI.fixture, false);
    SetValue(mUI.fxBody, QString(""));
    SetValue(mUI.fxShape, -1);
    SetValue(mUI.fxPolygon, QString(""));
    SetValue(mUI.fxFriction, mUI.fxFriction->minimum());
    SetValue(mUI.fxBounciness, mUI.fxBounciness->minimum());
    SetValue(mUI.fxDensity, mUI.fxDensity->minimum());
    SetValue(mUI.fxIsSensor, false);
    SetEnabled(mUI.nodeProperties, false);
    SetEnabled(mUI.nodeTransform, false);
    SetEnabled(mUI.nodeItems, false);

    if (const auto* node = GetCurrentNode())
    {
        SetEnabled(mUI.nodeProperties, true);
        SetEnabled(mUI.nodeTransform, true);
        SetEnabled(mUI.nodeItems, true);

        const auto& translate = node->GetTranslation();
        const auto& size = node->GetSize();
        const auto& scale = node->GetScale();
        SetValue(mUI.nodeID, node->GetId());
        SetValue(mUI.nodeName, node->GetName());
        SetValue(mUI.nodeTag, node->GetTag());
        SetValue(mUI.nodeIndex, mState.entity->FindNodeIndex(node));
        SetValue(mUI.nodeTranslateX, translate.x);
        SetValue(mUI.nodeTranslateY, translate.y);
        SetValue(mUI.nodeSizeX, size.x);
        SetValue(mUI.nodeSizeY, size.y);
        SetValue(mUI.nodeScaleX, scale.x);
        SetValue(mUI.nodeScaleY, scale.y);
        SetValue(mUI.nodeRotation, qRadiansToDegrees(node->GetRotation()));
        if (const auto* ptr = base::SafeFind(mComments, node->GetId()))
            SetValue(mUI.nodeComment, *ptr);
        if (const auto* item = node->GetDrawable())
        {
            SetValue(mUI.drawableItem, true);
            SetValue(mUI.dsMaterial, ListItemId(item->GetMaterialId()));
            SetValue(mUI.dsDrawable, ListItemId(item->GetDrawableId()));
            SetValue(mUI.dsRenderPass, item->GetRenderPass());
            SetValue(mUI.dsRenderStyle, item->GetRenderStyle());
            SetValue(mUI.dsLayer, item->GetLayer());
            SetValue(mUI.dsLineWidth, item->GetLineWidth());
            SetValue(mUI.dsTimeScale, item->GetTimeScale());
            SetValue(mUI.dsVisible, item->TestFlag(game::DrawableItemClass::Flags::VisibleInGame));
            SetValue(mUI.dsUpdateDrawable, item->TestFlag(game::DrawableItemClass::Flags::UpdateDrawable));
            SetValue(mUI.dsUpdateMaterial, item->TestFlag(game::DrawableItemClass::Flags::UpdateMaterial));
            SetValue(mUI.dsRestartDrawable, item->TestFlag(game::DrawableItemClass::Flags::RestartDrawable));
            SetValue(mUI.dsFlipHorizontally, item->TestFlag(game::DrawableItemClass::Flags::FlipHorizontally));
            SetValue(mUI.dsFlipVertically, item->TestFlag(game::DrawableItemClass::Flags::FlipVertically));
            SetValue(mUI.dsBloom, item->TestFlag(game::DrawableItemClass::Flags::PP_EnableBloom));

            const auto& material = mState.workspace->GetResourceById(GetItemId(mUI.dsMaterial));
            const auto& drawable = mState.workspace->GetResourceById(GetItemId(mUI.dsDrawable));
            SetEnabled(mUI.btnEditMaterial, !material.IsPrimitive());
            SetEnabled(mUI.btnEditDrawable, !drawable.IsPrimitive());
            if (drawable.GetType() == app::Resource::Type::Shape)
                mUI.btnEditDrawable->setIcon(QIcon("icons:polygon.png"));
            else if (drawable.GetType() == app::Resource::Type::ParticleSystem)
                mUI.btnEditDrawable->setIcon(QIcon("icons:particle.png"));
        }
        if (const auto* body = node->GetRigidBody())
        {
            SetValue(mUI.rigidBodyItem, true);
            SetValue(mUI.rbSimulation, body->GetSimulation());
            SetValue(mUI.rbShape, body->GetCollisionShape());
            SetValue(mUI.rbFriction, body->GetFriction());
            SetValue(mUI.rbRestitution, body->GetRestitution());
            SetValue(mUI.rbAngularDamping, body->GetAngularDamping());
            SetValue(mUI.rbLinearDamping, body->GetLinearDamping());
            SetValue(mUI.rbDensity, body->GetDensity());
            SetValue(mUI.rbIsBullet, body->TestFlag(game::RigidBodyItemClass::Flags::Bullet));
            SetValue(mUI.rbIsSensor, body->TestFlag(game::RigidBodyItemClass::Flags::Sensor));
            SetValue(mUI.rbIsEnabled, body->TestFlag(game::RigidBodyItemClass::Flags::Enabled));
            SetValue(mUI.rbCanSleep, body->TestFlag(game::RigidBodyItemClass::Flags::CanSleep));
            SetValue(mUI.rbDiscardRotation, body->TestFlag(game::RigidBodyItemClass::Flags::DiscardRotation));
            if (body->GetCollisionShape() == game::RigidBodyItemClass::CollisionShape::Polygon)
            {
                SetEnabled(mUI.rbPolygon, true);
                SetValue(mUI.rbPolygon, ListItemId(body->GetPolygonShapeId()));
            }
            else
            {
                SetEnabled(mUI.rbPolygon, false);
                SetValue(mUI.rbPolygon, QString(""));
            }
        }
        if (const auto* text = node->GetTextItem())
        {
            SetValue(mUI.textItem, true);
            SetValue(mUI.tiFontName, text->GetFontName());
            SetValue(mUI.tiFontSize, text->GetFontSize());
            SetValue(mUI.tiVAlign, text->GetVAlign());
            SetValue(mUI.tiHAlign, text->GetHAlign());
            SetValue(mUI.tiTextColor, text->GetTextColor());
            SetValue(mUI.tiLineHeight, text->GetLineHeight());
            SetValue(mUI.tiLayer, text->GetLayer());
            SetValue(mUI.tiRasterWidth, text->GetRasterWidth());
            SetValue(mUI.tiRasterHeight, text->GetRasterHeight());
            SetValue(mUI.tiText, text->GetText());
            SetValue(mUI.tiVisible, text->TestFlag(game::TextItemClass::Flags::VisibleInGame));
            SetValue(mUI.tiUnderline, text->TestFlag(game::TextItemClass::Flags::UnderlineText));
            SetValue(mUI.tiBlink, text->TestFlag(game::TextItemClass::Flags::BlinkText));
            SetValue(mUI.tiStatic, text->TestFlag(game::TextItemClass::Flags::StaticContent));
            SetValue(mUI.tiBloom, text->TestFlag(game::TextItemClass::Flags::PP_EnableBloom));
        }
        if (const auto* sp = node->GetSpatialNode())
        {
            SetValue(mUI.spatialNode, true);
            SetValue(mUI.spnShape, sp->GetShape());
            SetValue(mUI.spnEnabled, sp->TestFlag(game::SpatialNodeClass::Flags::Enabled));
        }
        if (const auto* fixture = node->GetFixture())
        {
            SetValue(mUI.fixture, true);
            SetValue(mUI.fxBody, ListItemId(fixture->GetRigidBodyNodeId()));
            SetValue(mUI.fxShape, fixture->GetCollisionShape());
            if (fixture->GetCollisionShape() == game::FixtureClass::CollisionShape::Polygon)
            {
                SetEnabled(mUI.fxPolygon, true);
                SetValue(mUI.fxPolygon, ListItemId(fixture->GetPolygonShapeId()));
            }
            else
            {
                SetEnabled(mUI.fxPolygon, false);
                SetValue(mUI.fxPolygon, -1);
            }
            if (const auto* val = fixture->GetFriction())
                SetValue(mUI.fxFriction, *val);
            if (const auto* val = fixture->GetRestitution())
                SetValue(mUI.fxBounciness, *val);
            if (const auto* val = fixture->GetDensity())
                SetValue(mUI.fxDensity, *val);

            SetValue(mUI.fxIsSensor, fixture->TestFlag(game::FixtureClass::Flags::Sensor));
        }
    }
}

void EntityWidget::DisplayCurrentCameraLocation()
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto dist_x = mState.camera_offset_x - (width / 2.0f);
    const auto dist_y = mState.camera_offset_y - (height / 2.0f);
    SetValue(mUI.translateX, dist_x);
    SetValue(mUI.translateY, dist_y);
}

void EntityWidget::UncheckPlacementActions()
{
    mUI.actionNewRect->setChecked(false);
    mUI.actionNewCircle->setChecked(false);
    mUI.actionNewIsoscelesTriangle->setChecked(false);
    mUI.actionNewRightTriangle->setChecked(false);
    mUI.actionNewRoundRect->setChecked(false);
    mUI.actionNewTrapezoid->setChecked(false);
    mUI.actionNewParallelogram->setChecked(false);
    mUI.actionNewCapsule->setChecked(false);
    mUI.actionNewSemiCircle->setChecked(false);
    mParticleSystems->menuAction()->setChecked(false);
    mCustomShapes->menuAction()->setChecked(false);
}

void EntityWidget::TranslateCamera(float dx, float dy)
{
    mState.camera_offset_x += dx;
    mState.camera_offset_y += dy;
    DisplayCurrentCameraLocation();
}

void EntityWidget::TranslateCurrentNode(float dx, float dy)
{
    if (auto* node = GetCurrentNode())
    {
        auto pos = node->GetTranslation();
        pos.x += dx;
        pos.y += dy;
        node->SetTranslation(pos);
        SetValue(mUI.nodeTranslateX, pos.x);
        SetValue(mUI.nodeTranslateY, pos.y);
        RealizeEntityChange(mState.entity);
    }
}

void EntityWidget::UpdateCurrentNodeProperties()
{
    auto* node = GetCurrentNode();
    if (node == nullptr)
        return;

    glm::vec2 size, scale, translation;
    size.x = GetValue(mUI.nodeSizeX);
    size.y = GetValue(mUI.nodeSizeY);
    scale.x = GetValue(mUI.nodeScaleX);
    scale.y = GetValue(mUI.nodeScaleY);
    translation.x = GetValue(mUI.nodeTranslateX);
    translation.y = GetValue(mUI.nodeTranslateY);
    node->SetSize(size);
    node->SetScale(scale);
    node->SetTranslation(translation);
    node->SetRotation(qDegreesToRadians((float)GetValue(mUI.nodeRotation)));

    if (auto* item = node->GetDrawable())
    {
        item->SetDrawableId(GetItemId(mUI.dsDrawable));
        item->SetMaterialId(GetItemId(mUI.dsMaterial));
        item->SetTimeScale(GetValue(mUI.dsTimeScale));
        item->SetLineWidth(GetValue(mUI.dsLineWidth));
        item->SetLayer(GetValue(mUI.dsLayer));
        item->SetRenderStyle(GetValue(mUI.dsRenderStyle));
        item->SetRenderPass(GetValue(mUI.dsRenderPass));

        item->SetFlag(game::DrawableItemClass::Flags::VisibleInGame, GetValue(mUI.dsVisible));
        item->SetFlag(game::DrawableItemClass::Flags::UpdateDrawable, GetValue(mUI.dsUpdateDrawable));
        item->SetFlag(game::DrawableItemClass::Flags::UpdateMaterial, GetValue(mUI.dsUpdateMaterial));
        item->SetFlag(game::DrawableItemClass::Flags::RestartDrawable, GetValue(mUI.dsRestartDrawable));
        item->SetFlag(game::DrawableItemClass::Flags::FlipHorizontally, GetValue(mUI.dsFlipHorizontally));
        item->SetFlag(game::DrawableItemClass::Flags::FlipVertically, GetValue(mUI.dsFlipVertically));
        item->SetFlag(game::DrawableItemClass::Flags::PP_EnableBloom, GetValue(mUI.dsBloom));
    }

    if (auto* body = node->GetRigidBody())
    {
        body->SetPolygonShapeId(GetItemId(mUI.rbPolygon));
        body->SetSimulation(GetValue(mUI.rbSimulation));
        body->SetCollisionShape(GetValue(mUI.rbShape));
        body->SetFriction(GetValue(mUI.rbFriction));
        body->SetRestitution(GetValue(mUI.rbRestitution));
        body->SetAngularDamping(GetValue(mUI.rbAngularDamping));
        body->SetLinearDamping(GetValue(mUI.rbLinearDamping));
        body->SetDensity(GetValue(mUI.rbDensity));

        // flags
        body->SetFlag(game::RigidBodyItemClass::Flags::Bullet, GetValue(mUI.rbIsBullet));
        body->SetFlag(game::RigidBodyItemClass::Flags::Sensor, GetValue(mUI.rbIsSensor));
        body->SetFlag(game::RigidBodyItemClass::Flags::Enabled, GetValue(mUI.rbIsEnabled));
        body->SetFlag(game::RigidBodyItemClass::Flags::CanSleep, GetValue(mUI.rbCanSleep));
        body->SetFlag(game::RigidBodyItemClass::Flags::DiscardRotation, GetValue(mUI.rbDiscardRotation));
    }

    if (auto* text = node->GetTextItem())
    {
        text->SetFontName(GetValue(mUI.tiFontName));
        text->SetFontSize(GetValue(mUI.tiFontSize));
        text->SetAlign((game::TextItemClass::VerticalTextAlign)GetValue(mUI.tiVAlign));
        text->SetAlign((game::TextItemClass::HorizontalTextAlign)GetValue(mUI.tiHAlign));
        text->SetTextColor(GetValue(mUI.tiTextColor));
        text->SetLineHeight(GetValue(mUI.tiLineHeight));
        text->SetText(GetValue(mUI.tiText));
        text->SetLayer(GetValue(mUI.tiLayer));
        text->SetRasterWidth(GetValue(mUI.tiRasterWidth));
        text->SetRasterHeight(GetValue(mUI.tiRasterHeight));
        // flags
        text->SetFlag(game::TextItemClass::Flags::VisibleInGame, GetValue(mUI.tiVisible));
        text->SetFlag(game::TextItemClass::Flags::UnderlineText, GetValue(mUI.tiUnderline));
        text->SetFlag(game::TextItemClass::Flags::BlinkText, GetValue(mUI.tiBlink));
        text->SetFlag(game::TextItemClass::Flags::StaticContent, GetValue(mUI.tiStatic));
        text->SetFlag(game::TextItemClass::Flags::PP_EnableBloom, GetValue(mUI.tiBloom));
    }
    if (auto* fixture = node->GetFixture())
    {
        fixture->SetRigidBodyNodeId(GetItemId(mUI.fxBody));
        fixture->SetPolygonShapeId(GetItemId(mUI.fxPolygon));
        fixture->SetCollisionShape(GetValue(mUI.fxShape));
        const float friction   = GetValue(mUI.fxFriction);
        const float density    = GetValue(mUI.fxDensity);
        const float bounciness = GetValue(mUI.fxBounciness);
        if (friction >= 0.0f)
            fixture->SetFriction(friction);
        else fixture->ResetFriction();

        if (density >= 0.0f)
            fixture->SetDensity(density);
        else fixture->ResetDensity();

        if (bounciness >= 0.0f)
            fixture->SetRestitution(bounciness);
        else fixture->ResetRestitution();

        fixture->SetFlag(game::FixtureClass::Flags::Sensor, GetValue(mUI.fxIsSensor));
    }

    if (auto* sp = node->GetSpatialNode())
    {
        sp->SetShape(GetValue(mUI.spnShape));
        sp->SetFlag(game::SpatialNodeClass::Flags::Enabled, GetValue(mUI.spnEnabled));
    }

    RealizeEntityChange(mState.entity);
}

void EntityWidget::RebuildMenus()
{
    // rebuild the drawable menus for custom shapes
    // and particle systems.
    mParticleSystems->clear();
    mCustomShapes->clear();
    for (size_t i = 0; i < mState.workspace->GetNumResources(); ++i)
    {
        const auto& resource = mState.workspace->GetResource(i);
        const auto& name = resource.GetName();
        const auto& id   = resource.GetId();
        if (resource.GetType() == app::Resource::Type::ParticleSystem)
        {
            QAction* action = mParticleSystems->addAction(name);
            action->setData(id);
            connect(action, &QAction::triggered, this, &EntityWidget::PlaceNewParticleSystem);
        }
        else if (resource.GetType() == app::Resource::Type::Shape)
        {
            QAction* action = mCustomShapes->addAction(name);
            action->setData(id);
            connect(action, &QAction::triggered,this, &EntityWidget::PlaceNewCustomShape);
        }
    }
}

void EntityWidget::RebuildCombos()
{
    SetList(mUI.dsMaterial, mState.workspace->ListAllMaterials());
    SetList(mUI.dsDrawable, mState.workspace->ListAllDrawables());

    std::vector<ResourceListItem> polygons;
    std::vector<ResourceListItem> scripts;
    // for the rigid body we need to list the polygonal (custom) shape
    // objects. (note that it's actually possible that these would be concave
    // but this case isn't currently supported)
    for (size_t i=0; i<mState.workspace->GetNumUserDefinedResources(); ++i)
    {
        const auto& res = mState.workspace->GetUserDefinedResource(i);
        ResourceListItem pair;
        pair.name = res.GetName();
        pair.id   = res.GetId();
        if (res.GetType() == app::Resource::Type::Shape) {
            polygons.push_back(pair);
        } else if (res.GetType() == app::Resource::Type::Script) {
            scripts.push_back(pair);
        }
    }
    SetList(mUI.rbPolygon, polygons);
    SetList(mUI.fxPolygon, polygons);
    SetList(mUI.scriptFile, scripts);
    SetList(mUI.animatorScript, scripts);
}

void EntityWidget::RebuildCombosInternal()
{
    std::vector<ResourceListItem> bodies;

    for (size_t i=0; i<mState.entity->GetNumNodes(); ++i)
    {
        auto& node = mState.entity->GetNode(i);
        if (auto* body = node.GetRigidBody())
        {
            ResourceListItem pair;
            pair.name = node.GetName();
            pair.id   = node.GetId();
            bodies.push_back(pair);
        }
    }
    SetList(mUI.fxBody, bodies);
}

void EntityWidget::UpdateDeletedResourceReferences()
{
    for (size_t i=0; i<mState.entity->GetNumNodes(); ++i)
    {
        auto& node = mState.entity->GetNode(i);
        if (auto* draw = node.GetDrawable())
        {
            const auto drawable = draw->GetDrawableId();
            const auto material = draw->GetMaterialId();
            if (!mState.workspace->IsValidMaterial(material))
            {
                WARN("Entity node '%1' uses material which is no longer available." , node.GetName());
                draw->ResetMaterial();
                draw->SetMaterialId("_checkerboard");
            }
            if (!mState.workspace->IsValidDrawable(drawable))
            {
                WARN("Entity node '%1' uses drawable which is no longer available." , node.GetName());
                draw->ResetDrawable();
                draw->SetDrawableId("_rect");
            }
        }
        if (auto* body = node.GetRigidBody())
        {
            if (body->GetCollisionShape() == game::RigidBodyItemClass::CollisionShape::Polygon)
            {
                if (!mState.workspace->IsValidDrawable(body->GetPolygonShapeId()))
                {
                    WARN("Entity node '%1' uses rigid body shape which is no longer available.", node.GetName());
                    body->ResetPolygonShapeId();
                }
            }
        }
        if (auto* fixture = node.GetFixture())
        {
            if (fixture->GetCollisionShape() == game::FixtureClass::CollisionShape::Polygon)
            {
                if (!mState.workspace->IsValidDrawable(fixture->GetPolygonShapeId()))
                {
                    WARN("Entity node '%1' fixture uses rigid body shape which is no longer available.", node.GetName());
                    fixture->ResetPolygonShapeId();
                }
            }
        }
    }

    if (mState.entity->HasScriptFile())
    {
        const auto& scriptId = mState.entity->GetScriptFileId();
        if (!mState.workspace->IsValidScript(scriptId))
        {
            WARN("Entity '%1' script is no longer available.", mState.entity->GetName());
            mState.entity->ResetScriptFile();
            SetEnabled(mUI.btnEditScript, false);
        }
    }
    RealizeEntityChange(mState.entity);
}

game::EntityNodeClass* EntityWidget::GetCurrentNode()
{
    TreeWidget::TreeItem* item = mUI.tree->GetSelectedItem();
    if (item == nullptr)
        return nullptr;
    else if (!item->GetUserData())
        return nullptr;
    return static_cast<game::EntityNodeClass*>(item->GetUserData());
}

const game::EntityNodeClass* EntityWidget::GetCurrentNode() const
{
    TreeWidget::TreeItem* item = mUI.tree->GetSelectedItem();
    if (item == nullptr)
        return nullptr;
    else if (!item->GetUserData())
        return nullptr;
    return static_cast<game::EntityNodeClass*>(item->GetUserData());
}

size_t EntityWidget::ComputeHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mState.entity->GetHash());
    // include the track properties.
    for (const auto& [key, props] : mTrackProperties)
    {
        hash = base::hash_combine(hash, app::FromUtf8(key));
        for (const auto& value : props)
        {
            hash = app::VariantHash(value);
        }
    }
    // include the node specific comments
    for (const auto& [node, comment] : mComments)
    {
        hash = base::hash_combine(hash, node);
        hash = base::hash_combine(hash, app::ToUtf8(comment));
    }
    return hash;
}

QString GenerateEntityScriptSource(QString entity)
{
    entity = app::GenerateScriptVarName(entity);

QString source(R"(
--
-- Entity '%1' script.
-- This script will be called for every instance of '%1' in the scene during gameplay.
-- You're free to delete functions you don't need.
--

-- Called once when the game play begins for the entity in the scene.
function BeginPlay(%1, scene)
end

-- Called once when the game play ends for the entity in the scene.
function EndPlay(%1, scene)
end

-- Called on every low frequency game tick. The tick frequency is
-- determined in the project settings. If you want to perform animation
-- such as move your game objects more smoothly then Update is the place
-- to do it. This function can be used to do thing such as evaluate AI or
-- path finding etc.
function Tick(%1, game_time, dt)
end

-- Called on every iteration of the game loop. game_time is the current
-- game time so far in seconds not including the next time step dt.
function Update(%1, game_time, dt)
end

-- Called on every iteration of the game loop game after *all* entities
-- in the scene have been updated. This means that all objects are in their
-- final places and it's possible to do things such as query scene spatial
-- nodes for finding interesting objects in any particular location.
function PostUpdate(%1, game_time)
end

-- Called on collision events with other objects based on the information
-- from the physics engine. You can only get these events  when your entity
-- node(s) have rigid bodies and are colliding with other rigid bodies. The
-- contact can exist over multiple time steps depending on the type of bodies etc.
-- Node is this entity's entity node with rigid body that collided with the
-- other entity's other_node's rigid body.
function OnBeginContact(%1, node, other_entity, other_node)
end

-- Similar to OnBeginContact except this happens when the contact ends.
function OnEndContact(%1, node, other_entity, other_node)
end

-- Called on key down events. This is only called when the entity has enabled
-- the keyboard input processing to take place. You can find this setting under
-- 'Script callbacks' in the entity editor. Symbol is one of the virtual key
-- symbols rom the wdk.Keys table and modifier bits is the bitwise combination
-- of control keys (Ctrl, Shift, etc) at the time of the key event.
-- The modifier_bits are expressed as an object of wdk.KeyBitSet.
--
-- Note that because some platforms post repeated events when a key is
-- continuously held you can get this event multiple times without getting
-- the corresponding key up!
function OnKeyDown(%1, symbol, modifier_bits)
end

-- Called on key up events. See OnKeyDown for more details.
function OnKeyUp(%1, symbol, modifier_bits)
end

-- Called on mouse button press events. This is only called when the entity
-- has enabled the mouse input processing to take place. You can find this
-- setting under 'Script callbacks' in the entity editor.
-- Mouse argument is of type game.MouseEvent and provides an aggregate of
-- information about the event. You can find more details about this type in
-- the Lua API doc.
function OnMousePress(%1, mouse)
end

-- Called on mouse button release events. See OnMousePress for more details.
function OnMouseRelease(%1, mouse)
end

-- Called on mouse move events. See OnMousePress for more details.
function OnMouseMove(%1, mouse)
end

-- Called on game events. Game events are broad-casted to all entities in
-- the scene.  GameEvents are useful when there's an unknown number of
-- entities possibly interested in some game event. Use Game:PostEvent to
-- post a new game event. Each entity will then receive the same event object
-- in this callback and can proceed to process the information.
function OnGameEvent(%1, event)
end

-- Called on animation finished events, i.e. when this entity has finished
-- playing the animation in question.
function OnAnimationFinished(%1, animation)
end

-- Called on timer events. Timers are set on an Entity by calling SetTimer.
-- When the timer expires this callback is then invoked. Timer is then the
-- name of the timer (same as in SetTimer) that fired and jitter defines
-- the difference to ideal time when the timer should have fired. In general
-- entity timers are limited in their resolution to game update resolution.
-- In other words if the game updates at 60 Hz the timer frequency is then
-- 1/60 seconds. If jitter is positive it means the timer is firing early
-- and a negative value indicates the timer fired late.
function OnTimer(%1, timer, jitter)
end

-- Called on posted entity events. Events can be posted on particular entities
-- by calling entity:PostEvent. Unlike game.GameEvents game.EntityEvent are
-- entity specific and only ever delivered to a single entity (the receiver).
function OnEvent(%1, event)
end
    )");
    return source.replace("%1", entity);
}

QString GenerateAnimatorScriptSource()
{
    static const QString source = R"(
--
-- Entity animator script.
-- This script will be called for every entity animator instance that
-- has this particular script assigned.
-- This script allows you to write the logic for performing some
-- particular actions when entering/leaving animation states and
-- when transitioning from one state to another.
--
-- You're free to delete functions you don't need.
--

-- Called once when the animator is first created.
-- This is the place where you can set the initial entity and animator
-- state to a known/desired first state.
function Init(animator, entity)

end


-- Called once when the entity enters a new animation state at the end
-- of a transition.
function EnterState(animator, state, entity)

end

-- Called once when the entity is leaving an animation state at the start
-- of a transition.
function LeaveState(animator, state, entity)

end

-- Called continuously on the current state. This is the place where you can
-- realize changes to the current input when in some particular state.
function UpdateState(animator, state, time, dt, entity)

end

-- Evaluate the condition to trigger a transition from one animation state
-- to another. Return true to take the transition or false to reject it.
function EvalTransition(animator, from, to, entity)
    return false
end

-- Called once when a transition is started from one state to another.
function StartTransition(animator, from, to, duration, entity)

end

-- Called once when the transition from one state to another is finished.
function FinishTransition(animator, from, to, entity)

end

-- Called continuously on a transition while it's in progress.
function UpdateTransition(animator, from, to, duration, time, dt, entity)

end
)";
    return source;
}

} // bui
