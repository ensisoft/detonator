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

#define LOGTAG "scene"

#include "config.h"

#include "warnpush.h"
#  include <QPoint>
#  include <QMouseEvent>
#  include <QMessageBox>
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
#include "editor/gui/utility.h"
#include "editor/gui/drawing.h"
#include "base/assert.h"
#include "base/format.h"
#include "base/math.h"
#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/transform.h"
#include "graphics/drawing.h"
#include "graphics/types.h"

namespace {
    inline glm::vec4 ToVec4(const QPoint& point)
    { return glm::vec4(point.x(), point.y(), 1.0f, 1.0f); }
} // namespace

namespace gui
{

class SceneWidget::PlaceEntityTool : public MouseTool
{
public:
    PlaceEntityTool(SceneWidget::State& state, std::shared_ptr<const game::EntityClass> klass, bool snap, unsigned grid)
      : mState(state)
      , mClass(klass)
      , mSnapToGrid(snap)
      , mGridSize(grid)
    {}
    virtual void Render(gfx::Painter& painter, gfx::Transform& view) const override
    {
        view.Push();
            view.Translate(mWorldPos.x, mWorldPos.y);
            mState.renderer.Draw(*mClass, painter, view, nullptr);
        view.Pop();
    }
    virtual void MouseMove(QMouseEvent* mickey, gfx::Transform& view) override
    {
        const auto& view_to_scene   = glm::inverse(view.GetAsMatrix());
        const auto& mouse_pos_view  = ToVec4(mickey->pos());
        const auto& mouse_pos_scene = view_to_scene * mouse_pos_view;
        mWorldPos = mouse_pos_scene;
    }
    virtual void MousePress(QMouseEvent* mickey, gfx::Transform& view) override
    {
    }
    virtual bool MouseRelease(QMouseEvent* mickey, gfx::Transform& view) override
    {
        const auto button = mickey->button();
        if (button != Qt::LeftButton)
            return false;

        if (mSnapToGrid)
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
        auto* child = mState.scene.AddNode(std::move(node));
        mState.scene.LinkChild(nullptr, child);

        mState.view->Rebuild();
        mState.view->SelectItemById(app::FromUtf8(child->GetClassId()));
        DEBUG("Added new entity '%1'", name);
        return true;
    }
private:
    std::string CreateName() const
    {
        for (size_t i=0; i<10000; ++i)
        {
            std::string name = "Entity " + std::to_string(i);
            if (mState.scene.FindNodeByName(name) == nullptr)
                return name;
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
    unsigned mGridSize = 0;
};

SceneWidget::SceneWidget(app::Workspace* workspace)
{
    DEBUG("Create SceneWidget");

    mRenderTree.reset(new TreeModel(mState.scene));

    mUI.setupUi(this);
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
    mUI.widget->onKeyPress      = std::bind(&SceneWidget::KeyPress, this, std::placeholders::_1);
    mUI.widget->onPaintScene    = std::bind(&SceneWidget::PaintScene, this, std::placeholders::_1,
                                            std::placeholders::_2);
    mUI.widget->onInitScene     = std::bind(&SceneWidget::InitScene, this, std::placeholders::_1,
                                            std::placeholders::_2);
    // the menu for adding entities in the scene.
    mEntities = new QMenu(this);
    mEntities->menuAction()->setIcon(QIcon("icons:entity.png"));
    mEntities->menuAction()->setText("Entities");

    mState.workspace = workspace;
    mState.renderer.SetLoader(workspace);
    mState.view = mUI.tree;

    // connect tree widget signals
    connect(mUI.tree, &TreeWidget::currentRowChanged, this, &SceneWidget::TreeCurrentNodeChangedEvent);
    connect(mUI.tree, &TreeWidget::dragEvent,  this, &SceneWidget::TreeDragEvent);
    connect(mUI.tree, &TreeWidget::clickEvent, this, &SceneWidget::TreeClickEvent);
    // connect workspace signals for resource management
    connect(workspace, &app::Workspace::NewResourceAvailable, this, &SceneWidget::NewResourceAvailable);
    connect(workspace, &app::Workspace::ResourceToBeDeleted,  this, &SceneWidget::ResourceToBeDeleted);
    connect(workspace, &app::Workspace::ResourceUpdated,      this, &SceneWidget::ResourceUpdated);

    PopulateFromEnum<GridDensity>(mUI.cmbGrid);
    SetValue(mUI.cmbGrid, GridDensity::Grid50x50);
    SetValue(mUI.name, QString("My Scene"));
    SetValue(mUI.ID, mState.scene.GetId());
    setWindowTitle("My Scene");

    RebuildMenus();
    RebuildCombos();
}

SceneWidget::SceneWidget(app::Workspace* workspace, const app::Resource& resource)
  : SceneWidget(workspace)
{
    DEBUG("Editing scene '%1'", resource.GetName());
    const game::SceneClass* content = nullptr;
    resource.GetContent(&content);

    mState.scene = *content;
    mOriginalHash = mState.scene.GetHash();
    mCameraWasLoaded = true;

    SetValue(mUI.name, resource.GetName());
    SetValue(mUI.ID, content->GetId());
    GetUserProperty(resource, "zoom", mUI.zoom);
    GetUserProperty(resource, "grid", mUI.cmbGrid);
    GetUserProperty(resource, "snap", mUI.chkSnap);
    GetUserProperty(resource, "show_origin", mUI.chkShowOrigin);
    GetUserProperty(resource, "show_grid", mUI.chkShowGrid);
    GetUserProperty(resource, "widget", mUI.widget);
    GetUserProperty(resource, "camera_scale_x", mUI.scaleX);
    GetUserProperty(resource, "camera_scale_y", mUI.scaleY);
    GetUserProperty(resource, "camera_rotation", mUI.rotation);
    GetUserProperty(resource, "camera_offset_x", &mState.camera_offset_x);
    GetUserProperty(resource, "camera_offset_y", &mState.camera_offset_y);
    setWindowTitle(resource.GetName());

    UpdateResourceReferences();

    mRenderTree.reset(new TreeModel(mState.scene));
    mUI.tree->SetModel(mRenderTree.get());
    mUI.tree->Rebuild();
}

SceneWidget::~SceneWidget()
{
    DEBUG("Destroy SceneWidget");
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
    menu.addAction(mEntities->menuAction());
}

bool SceneWidget::SaveState(Settings& settings) const
{
    settings.saveWidget("Scene", mUI.name);
    settings.saveWidget("Scene", mUI.ID);
    settings.saveWidget("Scene", mUI.scaleX);
    settings.saveWidget("Scene", mUI.scaleY);
    settings.saveWidget("Scene", mUI.rotation);
    settings.saveWidget("Scene", mUI.chkShowOrigin);
    settings.saveWidget("Scene", mUI.chkShowGrid);
    settings.saveWidget("Scene", mUI.chkSnap);
    settings.saveWidget("Scene", mUI.cmbGrid);
    settings.saveWidget("Scene", mUI.zoom);
    settings.saveWidget("Scene", mUI.widget);
    settings.setValue("Scene", "camera_offset_x", mState.camera_offset_x);
    settings.setValue("Scene", "camera_offset_y", mState.camera_offset_y);
    // the scene can already serialize into JSON.
    // so let's use the JSON serialization in the scene
    // and then convert that into base64 string which we can
    // stick in the settings data stream.
    const auto& json = mState.scene.ToJson();
    const auto& base64 = base64::Encode(json.dump(2));
    settings.setValue("Scene", "content", base64);
    return true;
}
bool SceneWidget::LoadState(const Settings& settings)
{
    settings.loadWidget("Scene", mUI.name);
    settings.loadWidget("Scene", mUI.ID);
    settings.loadWidget("Scene", mUI.scaleX);
    settings.loadWidget("Scene", mUI.scaleY);
    settings.loadWidget("Scene", mUI.rotation);
    settings.loadWidget("Scene", mUI.chkShowOrigin);
    settings.loadWidget("Scene", mUI.chkShowGrid);
    settings.loadWidget("Scene", mUI.chkSnap);
    settings.loadWidget("Scene", mUI.cmbGrid);
    settings.loadWidget("Scene", mUI.zoom);
    settings.loadWidget("Scene", mUI.widget);
    setWindowTitle(mUI.name->text());
    mState.camera_offset_x = settings.getValue("Scene", "camera_offset_x", mState.camera_offset_x);
    mState.camera_offset_y = settings.getValue("Scene", "camera_offset_y", mState.camera_offset_y);
    // set a flag to *not* adjust the camera on gfx widget init to the middle the of widget.
    mCameraWasLoaded = true;

    const std::string& base64 = settings.getValue("Scene", "content", std::string(""));
    if (base64.empty())
        return true;

    const auto& json = nlohmann::json::parse(base64::Decode(base64));
    auto ret  = game::SceneClass::FromJson(json);
    if (!ret.has_value())
    {
        ERROR("Failed to load scene widget state.");
        return false;
    }
    mState.scene  = std::move(ret.value());
    mOriginalHash = mState.scene.GetHash();

    UpdateResourceReferences();

    mRenderTree.reset(new TreeModel(mState.scene));
    mUI.tree->SetModel(mRenderTree.get());
    mUI.tree->Rebuild();
    return true;
}
bool SceneWidget::CanZoomIn() const
{
    const auto max = mUI.zoom->maximum();
    const auto val = mUI.zoom->value();
    return val < max;
}
bool SceneWidget::CanZoomOut() const
{
    const auto min = mUI.zoom->minimum();
    const auto val = mUI.zoom->value();
    return val > min;
}
void SceneWidget::ZoomIn()
{
    const auto value = mUI.zoom->value();
    mUI.zoom->setValue(value + 0.1);
}
void SceneWidget::ZoomOut()
{
    const auto value = mUI.zoom->value();
    mUI.zoom->setValue(value - 0.1);
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
        mUI.time->setText(QString::number(mSceneTime));
    }
    mCurrentTime += secs;
}
void SceneWidget::Render()
{
    mUI.widget->triggerPaint();
}
bool SceneWidget::ConfirmClose()
{
    const auto hash = mState.scene.GetHash();
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
void SceneWidget::Refresh()
{

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
    mUI.time->clear();
}
void SceneWidget::on_actionSave_triggered()
{
    if (!MustHaveInput(mUI.name))
        return;
    const QString& name = GetValue(mUI.name);
    app::SceneResource resource(mState.scene, name);
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
    SetUserProperty(resource, "widget", mUI.widget);

    mState.workspace->SaveResource(resource);
    mOriginalHash = mState.scene.GetHash();

    INFO("Saved scene '%1'", name);
    NOTE("Saved scene '%1'", name);
    setWindowTitle(name);
}

void SceneWidget::on_actionNodeDelete_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        mState.scene.DeleteNode(node);

        mUI.tree->Rebuild();
    }
}

void SceneWidget::on_actionNodeDuplicate_triggered()
{
    if (const auto* node = GetCurrentNode())
    {
        auto* dupe = mState.scene.DuplicateNode(node);
        // update the the translation for the parent of the new hierarchy
        // so that it's possible to tell it apart from the source of the copy.
        dupe->SetTranslation(node->GetTranslation() * 1.2f);

        mState.view->Rebuild();
        mState.view->SelectItemById(app::FromUtf8(dupe->GetClassId()));
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

void SceneWidget::on_plus90_clicked()
{
    const float value = GetValue(mUI.rotation);
    mUI.rotation->setValue(math::clamp(-180.0f, 180.0f, value + 90.0f));
    mViewTransformRotation  = value;
    mViewTransformStartTime = mCurrentTime;
}

void SceneWidget::on_minus90_clicked()
{
    const float value = GetValue(mUI.rotation);
    mUI.rotation->setValue(math::clamp(-180.0f, 180.0f, value - 90.0f));
    mViewTransformRotation  = value;
    mViewTransformStartTime = mCurrentTime;
}

void SceneWidget::on_resetTransform_clicked()
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
        auto klass = mState.workspace->GetEntityClassByName(name);
        node->SetEntity(klass);
    }
}

void SceneWidget::on_nodeLayer_valueChanged(int layer)
{
    if (auto* node = GetCurrentNode())
    {
        node->SetLayer(layer);
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

void SceneWidget::on_nodePlus90_clicked()
{
    if (auto* node = GetCurrentNode())
    {
        const float value = GetValue(mUI.nodeRotation);
        // careful, triggers value changed event.
        mUI.nodeRotation->setValue(math::clamp(-180.0f, 180.0f, value + 90.0f));
    }
}
void SceneWidget::on_nodeMinus90_clicked()
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
    const auto* node = GetCurrentNode();
    mUI.actionNodeDuplicate->setEnabled(node != nullptr);
    mUI.actionNodeDelete->setEnabled(node != nullptr);
    mUI.actionNodeMoveDownLayer->setEnabled(node != nullptr);
    mUI.actionNodeMoveUpLayer->setEnabled(node != nullptr);

    QMenu menu(this);
    menu.addAction(mUI.actionNodeMoveUpLayer);
    menu.addAction(mUI.actionNodeMoveDownLayer);
    menu.addAction(mUI.actionNodeDuplicate);
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
    if (const auto* node = GetCurrentNode())
    {
        SetEnabled(mUI.nodeProperties, true);
        SetEnabled(mUI.nodeTransform, true);
    }
    else
    {
        SetEnabled(mUI.nodeProperties, false);
        SetEnabled(mUI.nodeTransform, false);
    }
    DisplayCurrentNodeProperties();
}
void SceneWidget::TreeDragEvent(TreeWidget::TreeItem* item, TreeWidget::TreeItem* target)
{
    auto& tree = mState.scene.GetRenderTree();
    auto* src_value = static_cast<game::SceneNodeClass*>(item->GetUserData());
    auto* dst_value = static_cast<game::SceneNodeClass*>(target->GetUserData());

    // find the game graph node that contains this AnimationNode.
    auto* src_node   = tree.FindNodeByValue(src_value);
    auto* src_parent = tree.FindParent(src_node);

    // check if we're trying to drag a parent onto its own child
    if (src_node->FindNodeByValue(dst_value))
        return;

    game::SceneClass::RenderTreeNode branch = *src_node;
    src_parent->DeleteChild(src_node);

    auto* dst_node  = tree.FindNodeByValue(dst_value);
    dst_node->AppendChild(std::move(branch));

}
void SceneWidget::TreeClickEvent(TreeWidget::TreeItem* item)
{
    //DEBUG("Tree click event: %1", item->GetId());
    auto* node = static_cast<game::EntityNodeClass*>(item->GetUserData());
    if (node == nullptr)
        return;

    const bool visibility = node->TestFlag(game::EntityNodeClass::Flags::VisibleInEditor);
    node->SetFlag(game::EntityNodeClass::Flags::VisibleInEditor, !visibility);
    item->SetIconMode(visibility ? QIcon::Disabled : QIcon::Normal);
}

void SceneWidget::NewResourceAvailable(const app::Resource* resource)
{
    RebuildCombos();
    RebuildMenus();
}
void SceneWidget::ResourceToBeDeleted(const app::Resource* resource)
{
    UpdateResourceReferences();
    RebuildCombos();
    RebuildMenus();
    DisplayCurrentNodeProperties();
}
void SceneWidget::ResourceUpdated(const app::Resource* resource)
{
    RebuildCombos();
    RebuildMenus();
}

void SceneWidget::InitScene(unsigned width, unsigned height)
{
    if (!mCameraWasLoaded)
    {
        // if the camera hasn't been loaded then compute now the
        // initial position for the camera.
        mState.camera_offset_x = width * 0.5;
        mState.camera_offset_y = height * 0.5;
    }
    DisplayCurrentCameraLocation();
}

void SceneWidget::PaintScene(gfx::Painter& painter, double /*secs*/)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    const auto view_rotation_time = math::clamp(0.0, 1.0, mCurrentTime - mViewTransformStartTime);
    const auto view_rotation_angle = math::interpolate(mViewTransformRotation, (float)mUI.rotation->value(),
        view_rotation_time, math::Interpolation::Cosine);

    // apply the view transformation. The view transformation is not part of the
    // animation per-se but it's the transformation that transforms the animation
    // and its components from the space of the animation to the global space.
    gfx::Transform view;
    view.Push();
    view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
    view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
    view.Rotate(qDegreesToRadians(view_rotation_angle));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    // render endless background grid.
    if (GetValue(mUI.chkShowGrid))
    {
        const float zoom = GetValue(mUI.zoom);
        const float xs = GetValue(mUI.scaleX);
        const float ys = GetValue(mUI.scaleY);
        const GridDensity grid = GetValue(mUI.cmbGrid);
        DrawCoordinateGrid(painter, view, grid, zoom, xs, ys, width, height);
    }

    DrawHook hook(GetCurrentNode(), mPlayState == PlayState::Playing);

    // begin the animation transformation space
    view.Push();
        mState.renderer.BeginFrame();
        mState.renderer.Draw(mState.scene, painter, view, &hook, &hook);
        mState.renderer.EndFrame();
    view.Pop();

    if (mCurrentTool)
        mCurrentTool->Render(painter, view);

    // right arrow
    if (GetValue(mUI.chkShowOrigin))
    {
        DrawBasisVectors(painter, view);
    }

    // pop view transformation
    view.Pop();
}

void SceneWidget::MouseMove(QMouseEvent* mickey)
{
    if (mCurrentTool)
    {
        gfx::Transform view;
        view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
        view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
        view.Rotate(qDegreesToRadians(mUI.rotation->value()));
        view.Translate(mState.camera_offset_x, mState.camera_offset_y);
        mCurrentTool->MouseMove(mickey, view);
    }
    // update the properties that might have changed as the result of application
    // of the current tool.
    DisplayCurrentCameraLocation();
    DisplayCurrentNodeProperties();
}
void SceneWidget::MousePress(QMouseEvent* mickey)
{
    gfx::Transform view;
    view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
    view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
    view.Rotate((float)qDegreesToRadians(mUI.rotation->value()));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    if (!mCurrentTool)
    {
        // On a mouse press start we want to select the tool based on where the pointer
        // is and which object it interacts with in the game when mouse press starts.
        //
        // If the mouse pointer doesn't intersect with an object we create a new
        // camera tool for moving the viewport and object selection gets cleared.
        //
        // If the mouse pointer intersects with an object that is the same object
        // that was already selected:
        // Check if the pointer intersects with one of the resizing boxes inside
        // the object's selection box. If it does then we create a new resizing tool
        // otherwise we create a new move tool instance for moving the object.
        //
        // If the mouse pointer intersects with an object that is not the same object
        // that was previously selected:
        // Select the object.
        //
        // take the widget space mouse coordinate and transform into view/camera space.
        const auto& widget_to_view = glm::inverse(view.GetAsMatrix());
        const auto& mouse_view_position = widget_to_view * glm::vec4(mickey->pos().x(), mickey->pos().y(),1.0f, 1.0f);

        std::vector<game::SceneNodeClass *> nodes_hit;
        std::vector<glm::vec2> hitbox_coords;
        mState.scene.CoarseHitTest(mouse_view_position.x, mouse_view_position.y, &nodes_hit, &hitbox_coords);
        if (nodes_hit.empty())
        {
            mUI.tree->ClearSelection();
            mCurrentTool.reset(new MoveCameraTool(mState));
        }
        else
        {
            auto* current = GetCurrentNode();
            // if the currently selected node is among the ones being hit
            // then retain that selection.
            // otherwise select the last one of the list. (the rightmost child)
            game::SceneNodeClass* hit = nodes_hit.back();
            glm::vec2 hitpos = hitbox_coords.back();
            for (size_t i = 0; i < nodes_hit.size(); ++i)
            {
                if (nodes_hit[i] == current)
                {
                    hit = nodes_hit[i];
                    hitpos = hitbox_coords[i];
                    break;
                }
            }
            // todo: figure out how to rotate or scale a node
            const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
            const auto snap = (bool)GetValue(mUI.chkSnap);
            const unsigned grid_size = static_cast<unsigned>(grid);

            mCurrentTool.reset(new MoveRenderTreeNodeTool(mState.scene, hit, snap, grid_size));
            mUI.tree->SelectItemById(app::FromUtf8(hit->GetClassId()));
        }
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

    mCurrentTool->MouseRelease(mickey, view);
    mCurrentTool.release();

    UncheckPlacementActions();
}

bool SceneWidget::KeyPress(QKeyEvent* key)
{
    switch (key->key()) {
        case Qt::Key_Delete:
            on_actionNodeDelete_triggered();
            break;
        case Qt::Key_W:
            mState.camera_offset_y += 20.0f;
            break;
        case Qt::Key_S:
            mState.camera_offset_y -= 20.0f;
            break;
        case Qt::Key_A:
            mState.camera_offset_x += 20.0f;
            break;
        case Qt::Key_D:
            mState.camera_offset_x -= 20.0f;
            break;
        case Qt::Key_Left:
            UpdateCurrentNodePosition(-20.0f, 0.0f);
            break;
        case Qt::Key_Right:
            UpdateCurrentNodePosition(20.0f, 0.0f);
            break;
        case Qt::Key_Up:
            UpdateCurrentNodePosition(0.0f, -20.0f);
            break;
        case Qt::Key_Down:
            UpdateCurrentNodePosition(0.0f, 20.0f);
            break;
        case Qt::Key_Escape:
            mUI.tree->ClearSelection();
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

    if (const auto* node = GetCurrentNode())
    {
        const auto& translate = node->GetTranslation();
        const auto& scale = node->GetScale();
        SetValue(mUI.nodeID, node->GetClassId());
        SetValue(mUI.nodeName, node->GetName());
        SetValue(mUI.nodeEntity, mState.workspace->MapEntityIdToName(node->GetEntityId()));
        SetValue(mUI.nodeLayer, node->GetLayer());
        SetValue(mUI.nodeIsVisible, node->TestFlag(game::SceneNodeClass::Flags::VisibleInGame));
        SetValue(mUI.nodeTranslateX, translate.x);
        SetValue(mUI.nodeTranslateY, translate.y);
        SetValue(mUI.nodeScaleX, scale.x);
        SetValue(mUI.nodeScaleY, scale.y);
        SetValue(mUI.nodeRotation, qRadiansToDegrees(node->GetRotation()));
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

}

void SceneWidget::UpdateCurrentNodePosition(float dx, float dy)
{
    if (auto* node = GetCurrentNode())
    {
        auto pos = node->GetTranslation();
        pos.x += dx;
        pos.y += dy;
        node->SetTranslation(pos);
    }
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
}

void SceneWidget::RebuildCombos()
{
    SetList(mUI.nodeEntity, mState.workspace->ListUserDefinedEntities());
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
            continue;
        }
        node.SetEntity(klass);
    }
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

} // bui
