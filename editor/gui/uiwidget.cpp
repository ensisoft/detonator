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
#  include <QMessageBox>
#  include <QFileDialog>
#  include <QFileInfo>
#  include <QPixmap>
#  include <QTextStream>
#  include <QStringList>
#  include <base64/base64.h>
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <cmath>

#include "base/assert.h"
#include "base/utility.h"
#include "base/format.h"
#include "data/json.h"
#include "uikit/widget.h"
#include "uikit/window.h"
#include "uikit/state.h"
#include "uikit/op.h"
#include "graphics/drawing.h"
#include "editor/app/utility.h"
#include "editor/app/eventlog.h"
#include "editor/app/workspace.h"
#include "editor/gui/uiwidget.h"
#include "editor/gui/clipboard.h"
#include "editor/gui/drawing.h"
#include "editor/gui/utility.h"
#include "editor/gui/tool.h"
#include "editor/gui/settings.h"
#include "editor/gui/dlgmaterial.h"

namespace gui
{
class UIWidget::TreeModel : public TreeWidget::TreeModel
{
public:
    TreeModel(State& state) : mState(state)
    {}
    virtual void Flatten(std::vector<TreeWidget::TreeItem>& list) override
    {
        // visit the window and it's widgets hierarchically
        // and produce the data for the tree widget.
        class Visitor : public uik::Window::Visitor
        {
        public:
            Visitor(std::vector<TreeWidget::TreeItem>& list) : mList(list)
            {}
            virtual bool EnterWidget(uik::Widget* widget) override
            {
                const bool visible = widget->TestFlag(uik::Widget::Flags::VisibleInEditor);
                TreeWidget::TreeItem item;
                item.SetId(app::FromUtf8(widget->GetId()));
                item.SetText(app::FromUtf8(widget->GetName()));
                item.SetUserData(widget);
                item.SetLevel(mLevel);
                item.SetIcon(QIcon("icons:eye.png"));
                item.SetIconMode(visible ? QIcon::Normal : QIcon::Disabled);
                mList.push_back(std::move(item));
                mLevel++;
                return false;
            }
            virtual bool LeaveWidget(uik::Widget* widget) override
            {
                mLevel--;
                return false;
            }
        private:
            unsigned mLevel = 0;
            std::vector<TreeWidget::TreeItem>& mList;
        };
        Visitor visitor(list);
        mState.window.Visit(visitor);
    }
private:
    State& mState;
};

class UIWidget::PlaceWidgetTool : public MouseTool
{
public:
    PlaceWidgetTool(State& state, std::unique_ptr<uik::Widget> widget, bool snap = false, unsigned grid = 0)
      : mState(state)
      , mWidget(std::move(widget))
      , mSnapGrid(snap)
      , mGridSize(grid)
    {}
    virtual void Render(gfx::Painter& painter, gfx::Transform& view) const override
    {
        uik::State state;
        uik::Widget::PaintEvent paint;
        paint.focused = false;
        paint.moused  = false;
        paint.rect    = mWidget->GetRect();
        paint.rect.Translate(mWidgetPos.x, mWidgetPos.y);
        mWidget->Paint(paint, state, *mState.painter);
    }
    virtual void MouseMove(QMouseEvent* mickey, gfx::Transform& view) override
    {
        const auto& view_to_scene   = glm::inverse(view.GetAsMatrix());
        const auto& mouse_pos_view  = ToVec4(mickey->pos());
        const auto& mouse_pos_scene = view_to_scene * mouse_pos_view;
        mWidgetPos = mouse_pos_scene;
    }

    virtual void MousePress(QMouseEvent* mickey, gfx::Transform& view) override
    {
        // intentionally empty
    }

    virtual bool MouseRelease(QMouseEvent* mickey, gfx::Transform& view) override
    {
        const auto button = mickey->button();
        if (button != Qt::LeftButton)
            return false;

        if (mickey->modifiers() & Qt::ControlModifier)
            mSnapGrid = !mSnapGrid;

        if (mSnapGrid)
        {
            mWidgetPos.x = std::round(mWidgetPos.x / mGridSize) * mGridSize;
            mWidgetPos.y = std::round(mWidgetPos.y / mGridSize) * mGridSize;
        }
        uik::Widget* child = nullptr;
        uik::FPoint hit_point;
        auto* parent =  mState.window.HitTest(uik::FPoint(mWidgetPos.x, mWidgetPos.y), &hit_point);
        if (parent && parent->IsContainer())
        {
            mWidget->SetName(CreateName());
            mWidget->SetPosition(hit_point);
            child = parent->AddChild(std::move(mWidget));
        }
        else
        {
            mWidget->SetName(CreateName());
            mWidget->SetPosition(mWidgetPos.x , mWidgetPos.y);
            child = mState.window.AddWidget(std::move(mWidget));
        }
        mState.tree->Rebuild();
        mState.tree->SelectItemById(app::FromUtf8(child->GetId()));
        return true;
    }
private:
    std::string CreateName() const
    {
        unsigned count = 1;
        mState.window.ForEachWidget([&count, this](const uik::Widget* widget) {
            if (widget->GetType() == mWidget->GetType())
                ++count;
        });
        return base::FormatString("%1_%2", mWidget->GetType(), count);
    }
private:
    UIWidget::State& mState;
    std::unique_ptr<uik::Widget> mWidget;
    glm::vec4 mWidgetPos;
    bool mSnapGrid = false;
    unsigned mGridSize = 0;
};

class UIWidget::MoveWidgetTool : public MouseTool
{
public:
    MoveWidgetTool(UIWidget::State& state, uik::Widget* widget, bool snap = false, unsigned grid = 0)
      : mState(state)
      , mWidget(widget)
      , mSnapGrid(snap)
      , mGridSize(grid)
    {}
    virtual void MouseMove(QMouseEvent* mickey, gfx::Transform& view) override
    {
        const auto& mouse_pos_in_window = ToVec4(mickey->pos());
        const auto& window_to_scene = glm::inverse(view.GetAsMatrix());
        const auto& mouse_pos_in_scene = window_to_scene * mouse_pos_in_window;
        const auto& move_delta = mouse_pos_in_scene - mPrevMousePos;
        const auto& pos = mWidget->GetPosition();
        mWidget->Translate(move_delta.x , move_delta.y);
        mPrevMousePos = mouse_pos_in_scene;
        // set a flag to indicate that there was actual mouse move action.
        // otherwise simply selecting a node (and creating a new move tool)
        // will then snap the widget into a new place if "snap to grid" was on.
        mWasMoved = true;
    }
    virtual void MousePress(QMouseEvent* mickey, gfx::Transform& view) override
    {
        const auto& mouse_pos_in_window = ToVec4(mickey->pos());
        const auto& window_to_scene = glm::inverse(view.GetAsMatrix());
        const auto& mouse_pos_in_scene = window_to_scene * mouse_pos_in_window;
        mPrevMousePos = mouse_pos_in_scene;
    }
    virtual bool MouseRelease(QMouseEvent* mickey, gfx::Transform& view) override
    {
        if (!mWasMoved)
            return true;

        if (mickey->modifiers() & Qt::ControlModifier)
            mSnapGrid = !mSnapGrid;
        if (mSnapGrid)
        {
            auto pos = mWidget->GetPosition();
            auto x = pos.GetX();
            auto y = pos.GetY();
            x = std::round(x / mGridSize) * mGridSize;
            y = std::round(y / mGridSize) * mGridSize;
            mWidget->SetPosition(x , y);
        }
        return true;
    }
private:
    UIWidget::State& mState;
    uik::Widget* mWidget = nullptr;
    glm::vec4 mPrevMousePos;
    bool mWasMoved = false;
    bool mSnapGrid = false;
    unsigned mGridSize = 0;
};

class UIWidget::ResizeWidgetTool : public MouseTool
{
public:
    ResizeWidgetTool(State& state, uik::Widget* widget)
      : mState(state)
      , mWidget(widget)
    {}
    virtual void MouseMove(QMouseEvent* mickey, gfx::Transform& view) override
    {
        const auto& view_to_scene = glm::inverse(view.GetAsMatrix());
        const auto& mouse_pos_view = ToVec4(mickey->pos());
        const auto& mouse_pos_scene = view_to_scene * mouse_pos_view;
        const auto& mouse_delta = mouse_pos_scene - mMousePos;
        mWidget->Grow(mouse_delta.x, mouse_delta.y);
        mMousePos = mouse_pos_scene;
    }
    virtual void MousePress(QMouseEvent* mickey, gfx::Transform& view) override
    {
        const auto& view_to_scene = glm::inverse(view.GetAsMatrix());
        const auto& mouse_pos_view = ToVec4(mickey->pos());
        mMousePos = view_to_scene * mouse_pos_view;
    }
    virtual bool MouseRelease(QMouseEvent* mickey, gfx::Transform& view) override
    {
        return true;
    }
private:
    UIWidget::State& mState;
    uik::Widget* mWidget = nullptr;
    glm::vec4 mMousePos;
};

UIWidget::UIWidget(app::Workspace* workspace) : mUndoStack(3)
{
    DEBUG("Create UIWidget");

    mWidgetTree.reset(new TreeModel(mState));

    mUI.setupUi(this);
    mUI.tree->SetModel(mWidgetTree.get());
    mUI.tree->Rebuild();

    mUI.widget->onMouseMove    = std::bind(&UIWidget::MouseMove, this, std::placeholders::_1);
    mUI.widget->onMousePress   = std::bind(&UIWidget::MousePress, this, std::placeholders::_1);
    mUI.widget->onMouseRelease = std::bind(&UIWidget::MouseRelease, this, std::placeholders::_1);
    mUI.widget->onKeyPress     = std::bind(&UIWidget::KeyPress, this, std::placeholders::_1);
    mUI.widget->onZoomIn       = std::bind(&UIWidget::ZoomIn, this);
    mUI.widget->onZoomOut      = std::bind(&UIWidget::ZoomOut, this);
    mUI.widget->onPaintScene   = std::bind(&UIWidget::PaintScene, this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onInitScene    = [this](unsigned width, unsigned height) {
        if (!mCameraWasLoaded) {
            on_btnResetTransform_clicked();
        }
    };
    // connect tree widget signals
    connect(mUI.tree, &TreeWidget::currentRowChanged, this, &UIWidget::TreeCurrentWidgetChangedEvent);
    connect(mUI.tree, &TreeWidget::dragEvent, this, &UIWidget::TreeDragEvent);
    connect(mUI.tree, &TreeWidget::clickEvent, this, &UIWidget::TreeClickEvent);
    // connect workspace signals for resource management
    connect(workspace, &app::Workspace::NewResourceAvailable, this, &UIWidget::NewResourceAvailable);
    connect(workspace, &app::Workspace::ResourceToBeDeleted,  this, &UIWidget::ResourceToBeDeleted);
    connect(workspace, &app::Workspace::ResourceUpdated,      this, &UIWidget::ResourceUpdated);

    mState.tree = mUI.tree;
    mState.workspace = workspace;
    mState.window.Resize(1024, 768);
    mState.window.SetName("My UI");
    mState.window.SetStyleName("app://ui/default.json");
    mState.painter.reset(new game::UIPainter);

    mUI.widgetNormal->SetPropertySelector("");
    mUI.widgetDisabled->SetPropertySelector("/disabled");
    mUI.widgetFocused->SetPropertySelector("/focused");
    mUI.widgetMoused->SetPropertySelector("/mouse-over");
    mUI.widgetPressed->SetPropertySelector("/pressed");

    PopulateFromEnum<GridDensity>(mUI.cmbGrid);
    SetValue(mUI.windowID, mState.window.GetId());
    SetValue(mUI.windowName, mState.window.GetName());
    SetValue(mUI.cmbGrid, GridDensity::Grid50x50);
    SetEnabled(mUI.actionPause, false);
    SetEnabled(mUI.actionStop, false);

    PopulateUIStyles(mUI.baseStyle);

    LoadStyle("app://ui/default.json");
    RebuildCombos();
    setWindowTitle("My UI");
    DisplayCurrentWidgetProperties();
}

UIWidget::UIWidget(app::Workspace* workspace, const app::Resource& resource) : UIWidget(workspace)
{
    DEBUG("Editing UI: '%1'", resource.GetName());

    const uik::Window* window = nullptr;
    resource.GetContent(&window);

    mState.window = *window;
    mOriginalHash = mState.window.GetHash();

    SetValue(mUI.windowName, window->GetName());
    SetValue(mUI.windowID, window->GetId());
    SetValue(mUI.baseStyle, window->GetStyleName());
    GetUserProperty(resource, "zoom", mUI.zoom);
    GetUserProperty(resource, "grid", mUI.cmbGrid);
    GetUserProperty(resource, "snap", mUI.chkSnap);
    GetUserProperty(resource, "show_origin", mUI.chkShowOrigin);
    GetUserProperty(resource, "show_grid", mUI.chkShowGrid);
    GetUserProperty(resource, "widget", mUI.widget);
    GetUserProperty(resource, "camera_scale_x", mUI.scaleX);
    GetUserProperty(resource, "camera_scale_y", mUI.scaleY);
    GetUserProperty(resource, "camera_rotation", mUI.rotation);
    mCameraWasLoaded = GetUserProperty(resource, "camera_offset_x", &mState.camera_offset_x) &&
                       GetUserProperty(resource, "camera_offset_y", &mState.camera_offset_y);

    setWindowTitle(resource.GetName());
    LoadStyle(app::FromUtf8(mState.window.GetStyleName()));
    UpdateDeletedResourceReferences();
    DisplayCurrentCameraLocation();
    DisplayCurrentWidgetProperties();

    mUI.tree->Rebuild();
}

UIWidget::~UIWidget()
{
    DEBUG("Destroy UIWidget");
}

void UIWidget::AddActions(QToolBar& bar)
{
    bar.addAction(mUI.actionPlay);
    bar.addAction(mUI.actionPause);
    bar.addSeparator();
    bar.addAction(mUI.actionStop);
    bar.addSeparator();
    bar.addAction(mUI.actionSave);
    bar.addSeparator();
    bar.addAction(mUI.actionNewLabel);
    bar.addAction(mUI.actionNewPushButton);
    bar.addAction(mUI.actionNewCheckBox);
    bar.addAction(mUI.actionNewGroupBox);
}
void UIWidget::AddActions(QMenu& menu)
{
    menu.addAction(mUI.actionPlay);
    menu.addAction(mUI.actionPause);
    menu.addSeparator();
    menu.addAction(mUI.actionStop);
    menu.addSeparator();
    menu.addAction(mUI.actionSave);
    menu.addSeparator();
    menu.addAction(mUI.actionNewLabel);
    menu.addAction(mUI.actionNewPushButton);
    menu.addAction(mUI.actionNewCheckBox);
    menu.addAction(mUI.actionNewGroupBox);
}
bool UIWidget::SaveState(Settings& settings) const
{
    settings.saveWidget("UI", mUI.scaleX);
    settings.saveWidget("UI", mUI.scaleY);
    settings.saveWidget("UI", mUI.rotation);
    settings.saveWidget("UI", mUI.chkShowOrigin);
    settings.saveWidget("UI", mUI.chkShowGrid);
    settings.saveWidget("UI", mUI.chkSnap);
    settings.saveWidget("UI", mUI.cmbGrid);
    settings.saveWidget("UI", mUI.zoom);
    settings.saveWidget("UI", mUI.widget);
    settings.setValue("UI", "camera_offset_x", mState.camera_offset_x);
    settings.setValue("UI", "camera_offset_y", mState.camera_offset_y);
    data::JsonObject json;
    mState.window.IntoJson(json);
    settings.setValue("UI", "content", base64::Encode(json.ToString()));
    return true;
}
bool UIWidget::LoadState(const Settings& settings)
{
    settings.loadWidget("UI", mUI.scaleX);
    settings.loadWidget("UI", mUI.scaleY);
    settings.loadWidget("UI", mUI.rotation);
    settings.loadWidget("UI", mUI.chkShowOrigin);
    settings.loadWidget("UI", mUI.chkShowGrid);
    settings.loadWidget("UI", mUI.chkSnap);
    settings.loadWidget("UI", mUI.cmbGrid);
    settings.loadWidget("UI", mUI.zoom);
    settings.loadWidget("UI", mUI.widget);
    settings.getValue("UI", "camera_offset_x", &mState.camera_offset_x);
    settings.getValue("UI", "camera_offset_y", &mState.camera_offset_y);
    mCameraWasLoaded = true;

    std::string base64;
    settings.getValue("UI", "content", &base64);

    data::JsonObject json;
    auto [ok, error] = json.ParseString(base64::Decode(base64));
    if (!ok)
    {
        ERROR("Failed to parse content JSON. '%1'", error);
        return false;
    }
    auto window = uik::Window::FromJson(json);
    if (!window.has_value())
    {
        ERROR("Failed to load window state from JSON.");
        return false;
    }
    mState.window = std::move(window.value());
    mUI.tree->Rebuild();
    LoadStyle(app::FromUtf8(mState.window.GetStyleName()));
    DEBUG("Loaded UI widget state successfully.");
    return true;
}

bool UIWidget::CanTakeAction(Actions action, const Clipboard* clipboard) const
{
    switch (action)
    {
        case Actions::CanPaste:
            if (!mUI.widget->hasInputFocus())
                return false;
            else if (clipboard->IsEmpty())
                return false;
            else if (clipboard->GetType() != "application/binary/ui")
                return false;
            return true;
        case Actions::CanCut:
        case Actions::CanCopy:
            if (!mUI.widget->hasInputFocus())
                return false;
            else if (!GetCurrentWidget())
                return false;
            else if (IsRootWidget(GetCurrentWidget()))
                return false;
            return true;
        case Actions::CanUndo:
            return mUndoStack.size() > 1;
        case Actions::CanZoomIn:
        {
            const auto max = mUI.zoom->maximum();
            const auto val = mUI.zoom->value();
            return val < max;
        }
        case Actions::CanZoomOut:
        {
            const auto min = mUI.zoom->minimum();
            const auto val = mUI.zoom->value();
            return val > min;
        }
        case Actions::CanReloadShaders:
        case Actions::CanReloadTextures:
            return true;
    }
    return false;
}

void UIWidget::Cut(Clipboard& clipboard)
{
    if (const auto* widget = GetCurrentWidget())
    {
        if (IsRootWidget(widget))
            return;
        // we can make a copy here. the widget acts only as
        // a source for creating duplicates (clones) when
        // actual paste operation is done.
        clipboard.SetObject(widget->Copy());
        clipboard.SetType("application/binary/ui");

        mState.window.DeleteWidget(widget);
        mUI.tree->Rebuild();
        mUI.tree->ClearSelection();
    }
}
void UIWidget::Copy(Clipboard& clipboard)  const
{
    if (const auto* widget = GetCurrentWidget())
    {
        if (IsRootWidget(widget))
            return;

        // we can make a copy here. the widget acts only as
        // a source for creating duplicates (clones) when
        // actual paste operation is done.
        clipboard.SetObject(widget->Copy());
        clipboard.SetType("application/binary/ui");
    }
}
void UIWidget::Paste(const Clipboard& clipboard)
{
    if (!mUI.widget->hasInputFocus())
        return;

    if (clipboard.IsEmpty())
    {
        NOTE("Clipboard is empty.");
        return;
    }
    else if (clipboard.GetType() != "application/binary/ui")
    {
        NOTE("No UI data object found in clipboard.");
        return;
    }

    gfx::Transform view;
    view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
    view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
    view.Rotate(qDegreesToRadians((float)GetValue(mUI.rotation)));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    const auto& mickey = mUI.widget->mapFromGlobal(QCursor::pos());
    const auto& view_to_scene  = glm::inverse(view.GetAsMatrix());
    const auto& mouse_in_scene = view_to_scene * ToVec4(mickey);

    const auto& src = clipboard.GetObject<uik::Widget>();

    auto dupe = src->Clone();
    uik::ForEachWidget([](uik::Widget* dupe) {
        const auto name = dupe->GetName();
        dupe->SetName(base::FormatString("Copy of %1", name));
    }, dupe.get());

    uik::Widget* child = nullptr;
    uik::FPoint hit_point;
    auto* widget = mState.window.HitTest(uik::FPoint(mouse_in_scene.x, mouse_in_scene.y), &hit_point);
    if (widget && widget->IsContainer())
    {
        dupe->SetPosition(hit_point);
        child = widget->AddChild(std::move(dupe));
    }
    else
    {
        dupe->SetPosition(mouse_in_scene.x, mouse_in_scene.y);
        child = mState.window.AddWidget(std::move(dupe));
    }
    mUI.tree->Rebuild();
    mUI.tree->SelectItemById(app::FromUtf8(child->GetId()));
    mState.window.Style(*mState.painter);
}

void UIWidget::ZoomIn()
{
    const auto value = mUI.zoom->value();
    mUI.zoom->setValue(value + 0.1);
}
void UIWidget::ZoomOut()
{
    const auto value = mUI.zoom->value();
    mUI.zoom->setValue(value - 0.1);
}
void UIWidget::ReloadShaders()
{
    mUI.widget->reloadShaders();
}
void UIWidget::ReloadTextures()
{
    mUI.widget->reloadTextures();
}
void UIWidget::Shutdown()
{
    mUI.widget->dispose();
}
void UIWidget::Update(double dt)
{
    if (mPlayState == PlayState::Playing)
    {
        mPlayTime += dt;
        mState.painter->Update(mPlayTime, dt);
        mState.active_window->Update(*mState.active_state, mPlayTime, dt);
    }
    mCurrentTime += dt;
}
void UIWidget::Render()
{
    mUI.widget->triggerPaint();
}
void UIWidget::Save()
{
    on_actionSave_triggered();
}

void UIWidget::Undo()
{
    if (mPlayState != PlayState::Stopped)
        return;

    if (mUndoStack.size() <= 1)
    {
        NOTE("No undo available.");
        return;
    }

    // if the timer has run the top of the undo stack
    // is the same copy as the actual scene object.
    if (mUndoStack.back().GetHash() == mState.window.GetHash())
        mUndoStack.pop_back();

    // bring back an older version.
    mState.window = mUndoStack.back();

    mState.tree->Rebuild();

    mState.painter->DeleteMaterialInstances();
    // since it's possible that the styling properties were modified
    // a brute force approach is to force a reload of styling for widgets.
    mState.window.ForEachWidget([this](const uik::Widget* widget) {
        mState.style->DeleteMaterials(widget->GetId());
        mState.style->DeleteProperties(widget->GetId());
    });
    mState.window.Style(*mState.painter);

    mUndoStack.pop_back();

    DisplayCurrentWidgetProperties();
    NOTE("Undo!");
}

bool UIWidget::HasUnsavedChanges() const
{
    if (!mOriginalHash)
        return false;
    const auto hash = mState.window.GetHash();
    if (hash != mOriginalHash)
        return true;
    return false;
}
bool UIWidget::ConfirmClose()
{
    const auto hash = mState.window.GetHash();
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
bool UIWidget::GetStats(Stats* stats) const
{
    stats->time  = mPlayTime;
    stats->graphics.valid = true;
    stats->graphics.fps   = mUI.widget->getCurrentFPS();
    stats->graphics.vsync = mUI.widget->haveVSYNC();
    return true;
}

void UIWidget::Refresh()
{
    // use a simple tick counter to purge the event
    // messages when we're playing the ui system.
    mRefreshTick++;
    if (((mRefreshTick % 3) == 0) && !mMessageQueue.empty())
        mMessageQueue.pop_front();

    // don't take an undo snapshot while things are in playback (only design state)
    if (mPlayState != PlayState::Stopped)
        return;
    // don't take an undo snapshot while something is being edited.
    if (mCurrentTool)
        return;
    // don't take an undo snapshot while the widget name is being edited.
    // todo: this is probably broken, the widget could have keyboard focus
    // while we edit the scene and then an undo would not be made.
    if (mUI.widgetName->hasFocus())
        return;

    // check if any of the style widgets is under a continuous
    // edit that would disable undo stack copying.
    WidgetStyleWidget* style_widgets[] = {
            mUI.widgetNormal, mUI.widgetDisabled, mUI.widgetFocused,
            mUI.widgetMoused, mUI.widgetPressed
    };
    for (auto* style_widget : style_widgets)
    {
        if (style_widget->IsUnderEdit())
            return;
    }

    if (mUndoStack.empty())
        mUndoStack.push_back(mState.window);

    const auto curr_hash = mState.window.GetHash();
    const auto undo_hash = mUndoStack.back().GetHash();
    if (curr_hash == undo_hash)
        return;

    mUndoStack.push_back(mState.window);
    DEBUG("Created undo copy. stack size: %1", mUndoStack.size());
}

void UIWidget::on_actionPlay_triggered()
{
    mPlayState = PlayState::Playing;
    mState.active_window = std::make_unique<uik::Window>(mState.window);
    mState.active_state  = std::make_unique<uik::State>();
    SetEnabled(mUI.actionPlay,  false);
    SetEnabled(mUI.actionPause, true);
    SetEnabled(mUI.actionStop,  true);
    SetEnabled(mUI.baseProperties,      false);
    SetEnabled(mUI.viewTransform,       false);
    SetEnabled(mUI.widgetTree,          false);
    SetEnabled(mUI.widgetProperties,    false);
    SetEnabled(mUI.widgetStyle,         false);
    SetEnabled(mUI.widgetData,          false);
    SetEnabled(mUI.actionNewCheckBox,   false);
    SetEnabled(mUI.actionNewGroupBox,   false);
    SetEnabled(mUI.actionNewLabel,      false);
    SetEnabled(mUI.actionNewPushButton, false);
    SetEnabled(mUI.cmbGrid,       false);
    SetEnabled(mUI.zoom,          false);
    SetEnabled(mUI.chkSnap,       false);
    SetEnabled(mUI.chkShowOrigin, false);
    SetEnabled(mUI.chkShowGrid,   false);
}
void UIWidget::on_actionPause_triggered()
{
    mPlayState = PlayState::Paused;
    SetEnabled(mUI.actionPlay,  true);
    SetEnabled(mUI.actionPause, false);
    SetEnabled(mUI.actionStop,  true);
}
void UIWidget::on_actionStop_triggered()
{
    mPlayTime  = 0.0;
    mPlayState = PlayState::Stopped;
    mState.active_state.reset();
    mState.active_window.reset();
    mState.painter->DeleteMaterialInstances();
    SetEnabled(mUI.actionPlay,  true);
    SetEnabled(mUI.actionPause, false);
    SetEnabled(mUI.actionStop,  false);
    SetEnabled(mUI.baseProperties,      true);
    SetEnabled(mUI.viewTransform,       true);
    SetEnabled(mUI.widgetTree,          true);
    SetEnabled(mUI.widgetProperties,    true);
    SetEnabled(mUI.widgetStyle,         true);
    SetEnabled(mUI.widgetData,          true);
    SetEnabled(mUI.actionNewCheckBox,   true);
    SetEnabled(mUI.actionNewLabel,      true);
    SetEnabled(mUI.actionNewPushButton, true);
    SetEnabled(mUI.actionNewGroupBox,   true);
    SetEnabled(mUI.cmbGrid,       true);
    SetEnabled(mUI.zoom,          true);
    SetEnabled(mUI.chkSnap,       true);
    SetEnabled(mUI.chkShowOrigin, true);
    SetEnabled(mUI.chkShowGrid,   true);

    mMessageQueue.clear();
}
void UIWidget::on_actionSave_triggered()
{
    if (!MustHaveInput(mUI.windowName))
        return;

    const QString& name = GetValue(mUI.windowName);
    mState.window.SetName(GetValue(mUI.windowName));
    app::UIResource resource(mState.window, name);
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
    mOriginalHash = mState.window.GetHash();
    INFO("Saved UI '%1'", name);
    NOTE("Saved UI '%1'", name);
    setWindowTitle(name);
}

void UIWidget::on_actionNewLabel_triggered()
{
    const auto snap = (bool)GetValue(mUI.chkSnap);
    const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
    mCurrentTool.reset(new PlaceWidgetTool(mState, std::make_unique<uik::Label>(), snap, (unsigned)grid));
}

void UIWidget::on_actionNewPushButton_triggered()
{
    const auto snap = (bool)GetValue(mUI.chkSnap);
    const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
    mCurrentTool.reset(new PlaceWidgetTool(mState, std::make_unique<uik::PushButton>(), snap, (unsigned)grid));
}

void UIWidget::on_actionNewCheckBox_triggered()
{
    const auto snap = (bool)GetValue(mUI.chkSnap);
    const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
    mCurrentTool.reset(new PlaceWidgetTool(mState, std::make_unique<uik::CheckBox>(), snap, (unsigned)grid));
}

void UIWidget::on_actionNewGroupBox_triggered()
{
    const auto snap = (bool)GetValue(mUI.chkSnap);
    const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
    mCurrentTool.reset(new PlaceWidgetTool(mState, std::make_unique<uik::GroupBox>(), snap, (unsigned)grid));
}

void UIWidget::on_actionWidgetDelete_triggered()
{
    if (auto* widget = GetCurrentWidget())
    {
        if (IsRootWidget(widget))
            return;

        mState.window.DeleteWidget(widget);
        mUI.tree->Rebuild();
        mUI.tree->ClearSelection();
    }
}
void UIWidget::on_actionWidgetDuplicate_triggered()
{
    if (auto* widget = GetCurrentWidget())
    {
        if (IsRootWidget(widget))
            return;
        // create a dupe recursively.
        auto dupe = widget->Clone();
        // the parent must be a container widget. a widget cannot
        // be parented under a widget that isn't a container and
        // at the the top-most level the window has a root widget
        // which is the top level container.
        auto* parent = mState.window.FindParent(widget);
        // Adjust the properties of each widget in the new duplicate
        // widget hierarchy starting at the top-most dupe.
        uik::ForEachWidget([](uik::Widget* dupe) {
            const auto name = dupe->GetName();
            dupe->SetName(base::FormatString("Copy of %1", name));
        }, dupe.get());

        auto* child = parent->AddChild(std::move(dupe));
        child->Translate(10.0f, 10.0f);
        mUI.tree->Rebuild();
        mUI.tree->SelectItemById(app::FromUtf8(child->GetId()));
        mState.window.Style(*mState.painter);
    }
}

void UIWidget::on_windowName_textChanged(const QString& text)
{
    mState.window.SetName(app::ToUtf8(text));
}

void UIWidget::on_baseStyle_currentIndexChanged(int)
{
    const auto& name = mState.window.GetStyleName();

    if (!LoadStyle(GetValue(mUI.baseStyle)))
        SetValue(mUI.baseStyle, name);
}

void UIWidget::on_widgetName_textChanged(const QString& text)
{
    TreeWidget::TreeItem* item = mUI.tree->GetSelectedItem();
    if (item == nullptr)
        return;
    auto* widget = static_cast<uik::Widget*>(item->GetUserData());
    widget->SetName(app::ToUtf8(text));
    item->SetText(text);
    mUI.tree->Update();
}

void UIWidget::on_widgetWidth_valueChanged(double value)
{
    UpdateCurrentWidgetProperties();
}
void UIWidget::on_widgetHeight_valueChanged(double value)
{
    UpdateCurrentWidgetProperties();
}
void UIWidget::on_widgetXPos_valueChanged(double value)
{
    UpdateCurrentWidgetProperties();
}
void UIWidget::on_widgetYPos_valueChanged(double value)
{
    UpdateCurrentWidgetProperties();
}
void UIWidget::on_widgetLineHeight_valueChanged(double value)
{
    UpdateCurrentWidgetProperties();
}
void UIWidget::on_widgetText_textChanged()
{
    UpdateCurrentWidgetProperties();
}
void UIWidget::on_widgetCheckBox_stateChanged(int)
{
    UpdateCurrentWidgetProperties();
}

void UIWidget::on_btnReloadStyle_clicked()
{
    LoadStyle(app::FromUtf8(mState.window.GetStyleName()));
}

void UIWidget::on_btnSelectStyle_clicked()
{
    const auto& file = QFileDialog::getOpenFileName(this,
        tr("Select JSON Style File"), "",
        tr("Style (*.json)"));
    if (file.isEmpty())
        return;
    LoadStyle(mState.workspace->MapFileToWorkspace(file));
}

void UIWidget::on_btnEditStyle_clicked()
{

}

void UIWidget::on_btnViewPlus90_clicked()
{
    const float value = GetValue(mUI.rotation);
    mUI.rotation->setValue(math::clamp(-180.0f, 180.0f, value + 90.0f));
    mViewTransformRotation  = value;
    mViewTransformStartTime = mCurrentTime;
}
void UIWidget::on_btnViewMinus90_clicked()
{
    const float value = GetValue(mUI.rotation);
    mUI.rotation->setValue(math::clamp(-180.0f, 180.0f, value - 90.0f));
    mViewTransformRotation  = value;
    mViewTransformStartTime = mCurrentTime;
}

void UIWidget::on_btnResetTransform_clicked()
{
    const auto width = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto rotation = mUI.rotation->value();
    mState.camera_offset_x = width * 0.5f - mState.window.GetWidth() * 0.5;
    mState.camera_offset_y = height * 0.5f - mState.window.GetHeight() * 0.5;
    mViewTransformRotation = rotation;
    mViewTransformStartTime = mCurrentTime;
    // set new camera offset to the center of the widget.
    mUI.translateX->setValue(0);
    mUI.translateY->setValue(0);
    mUI.scaleX->setValue(1.0f);
    mUI.scaleY->setValue(1.0f);
    mUI.rotation->setValue(0);
}

void UIWidget::on_chkWidgetEnabled_stateChanged(int)
{
    UpdateCurrentWidgetProperties();
}
void UIWidget::on_chkWidgetVisible_stateChanged(int)
{
    UpdateCurrentWidgetProperties();
}

void UIWidget::on_tree_customContextMenuRequested(QPoint)
{
    const auto* widget = GetCurrentWidget();
    SetEnabled(mUI.actionWidgetDelete, false);
    SetEnabled(mUI.actionWidgetDuplicate, false);
    if (widget && !IsRootWidget(widget))
    {
        SetEnabled(mUI.actionWidgetDelete , true);
        SetEnabled(mUI.actionWidgetDuplicate , true);
    }

    QMenu menu(this);
    menu.addAction(mUI.actionWidgetDuplicate);
    menu.addSeparator();
    menu.addAction(mUI.actionWidgetDelete);
    menu.exec(QCursor::pos());
}

void UIWidget::TreeCurrentWidgetChangedEvent()
{
    if (const auto* widget = GetCurrentWidget())
    {
        SetEnabled(mUI.widgetProperties, true);
        SetEnabled(mUI.widgetStyle, true);
        SetEnabled(mUI.widgetData, true);
    }
    else
    {
        SetEnabled(mUI.widgetProperties, false);
        SetEnabled(mUI.widgetStyle, false);
        SetEnabled(mUI.widgetData, false);
    }
    DisplayCurrentWidgetProperties();
}

void UIWidget::TreeDragEvent(TreeWidget::TreeItem* item, TreeWidget::TreeItem* target)
{
    auto* src_widget = static_cast<uik::Widget*>(item->GetUserData());
    auto* dst_widget = static_cast<uik::Widget*>(target->GetUserData());
    if (!dst_widget->IsContainer())
        return;

    // check if we're trying to drag a parent onto its own child.
    // we can perform this check by seeing if the src widget is in fact
    // a parent of the destination by seeing if we can find the destination
    // starting from source.
    if (uik::FindWidget([dst_widget](uik::Widget* widget) {
        return widget == dst_widget;
    }, src_widget))
        return;

    // todo: probably want to retain the position of the widget
    // relative to window. this means that the widget's position
    // needs to be transformed relative to the new parent

    dst_widget->AddChild(src_widget->Copy());

    mState.window.DeleteWidget(src_widget);
    mUI.tree->Rebuild();
}
void UIWidget::TreeClickEvent(TreeWidget::TreeItem* item)
{
    if (auto* widget = GetCurrentWidget())
    {
        const bool visibility = widget->TestFlag(uik::Widget::Flags::VisibleInEditor);
        widget->SetFlag(uik::Widget::Flags::VisibleInEditor, !visibility);
        item->SetIconMode(visibility ? QIcon::Disabled : QIcon::Normal);
    }
}

void UIWidget::NewResourceAvailable(const app::Resource* resource)
{
    if (!resource->IsMaterial())
        return;
    RebuildCombos();
}
void UIWidget::ResourceToBeDeleted(const app::Resource* resource)
{
    if (!resource->IsMaterial())
        return;
    UpdateDeletedResourceReferences();
    RebuildCombos();
    DisplayCurrentWidgetProperties();
    mState.painter->DeleteMaterialInstanceByClassId(resource->GetIdUtf8());
}
void UIWidget::ResourceUpdated(const app::Resource* resource)
{
    if (!resource->IsMaterial())
        return;
    // purge material instance (if any)
    mState.painter->DeleteMaterialInstanceByClassId(resource->GetIdUtf8());
}

void UIWidget::PaintScene(gfx::Painter& painter, double sec)
{
    const auto surface_width  = mUI.widget->width();
    const auto surface_height = mUI.widget->height();
    const auto zoom   = (float)GetValue(mUI.zoom);
    const auto xs     = (float)GetValue(mUI.scaleX);
    const auto ys     = (float)GetValue(mUI.scaleY);
    const auto grid   = (GridDensity)GetValue(mUI.cmbGrid);
    const auto window_width  = mState.window.GetWidth();
    const auto window_height = mState.window.GetHeight();

    const auto view_rotation_time = math::clamp(0.0, 1.0, mCurrentTime - mViewTransformStartTime);
    const auto view_rotation_angle = math::interpolate(mViewTransformRotation, (float)mUI.rotation->value(),
        view_rotation_time, math::Interpolation::Cosine);

    gfx::Transform view;
    view.Scale(xs, ys);
    view.Scale(zoom, zoom);
    view.Rotate(qDegreesToRadians(view_rotation_angle));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    if (GetValue(mUI.chkShowGrid) && mPlayState == PlayState::Stopped)
    {
        DrawCoordinateGrid(painter, view, grid, zoom, xs, ys, surface_width, surface_height);
    }
    if (GetValue(mUI.chkShowOrigin) && mPlayState == PlayState::Stopped)
    {
        DrawBasisVectors(painter , view);
    }

    gfx::FRect rect(10.0f, 10.0f, 500.0f, 20.0f);
    for (const auto& print : mMessageQueue)
    {
        gfx::FillRect(painter, rect, gfx::Color4f(gfx::Color::Black, 0.4f));
        gfx::DrawTextRect(painter, print, "app://fonts/orbitron-medium.otf", 14, rect,
                          gfx::Color::HotPink, gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignVCenter);
        rect.Translate(0.0f, 20.0f);
    }

    painter.SetViewMatrix(view.GetAsMatrix());
    painter.SetPixelRatio(glm::vec2(xs * zoom, ys * zoom));
    mState.painter->SetPainter(&painter);
    mState.painter->SetStyle(mState.style.get());
    if (mPlayState == PlayState::Stopped)
    {
        class PaintHook : public uik::PaintHook
        {
        public:
            virtual bool InspectPaint(const uik::Widget* widget, uik::Widget::PaintEvent& event, uik::State& state) override
            {
                if (!widget->TestFlag(uik::Widget::Flags::VisibleInEditor))
                    return false;
                return true;
            }
        private:
        } hook;

        // paint the design state copy of the window.
        uik::State s;
        mState.window.Paint(s , *mState.painter, 0.0, &hook);

        // draw the window outline
        gfx::DrawRectOutline(painter, gfx::FRect(0.0f, 0.0f,mState.window.GetSize()), gfx::Color::HotPink);
    }
    else
    {
        const auto show_window = true;
        // paint the active playback window.
        mState.active_window->Paint(*mState.active_state,
                                    *mState.painter,
                                    mPlayTime);
    }

    if (const auto* widget = GetCurrentWidget())
    {
        const auto& widget_rect = mState.window.FindWidgetRect(widget);
        gfx::FRect size_box;
        size_box.Resize(10.0f, 10.0f);
        size_box.Move(widget_rect.GetPosition());
        size_box.Translate(widget_rect.GetWidth()-10, widget_rect.GetHeight()-10);
        gfx::DrawRectOutline(painter, widget_rect, gfx::Color::Green, 1.0f);
        gfx::DrawRectOutline(painter, size_box, gfx::Color::Green, 1.0f);
    }

    if (mCurrentTool)
    {
        mCurrentTool->Render(painter, view);
    }

    painter.SetOrthographicView(0.0f , 0.0f , surface_width , surface_height);
    painter.ResetViewMatrix();
}

void UIWidget::MouseMove(QMouseEvent* mickey)
{
    gfx::Transform view;
    view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
    view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
    view.Rotate(qDegreesToRadians(mUI.rotation->value()));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    if (mPlayState == PlayState::Playing)
    {
        // todo: refactor the duplicated code
        const auto& pos = mickey->pos();
        const auto& view_to_scene  = glm::inverse(view.GetAsMatrix());
        const auto& mouse_in_scene = view_to_scene * ToVec4(mickey->pos());

        uik::Window::MouseEvent m;
        m.native_mouse_pos = uik::FPoint(pos.x(), pos.y());
        m.window_mouse_pos = uik::FPoint(mouse_in_scene.x, mouse_in_scene.y);
        m.time = mPlayTime;
        if (mickey->button() == Qt::LeftButton)
            m.button = uik::MouseButton::Left;
        else if (mickey->button() == Qt::RightButton)
            m.button = uik::MouseButton::Right;
        else if (mickey->button() == Qt::MiddleButton)
            m.button = uik::MouseButton::Wheel;
        const auto& action = mState.active_window->MouseMove(m, *mState.active_state);
        if (action.type != uik::WidgetActionType::None)
            mMessageQueue.push_back(base::FormatString("Event: %1, widget: '%2'", action.type, action.name));
    }
    else if (mCurrentTool)
    {
        mCurrentTool->MouseMove(mickey, view);
    }

    // Update the UI based on the properties that might have changed
    // as a result of application of the current tool.
    DisplayCurrentWidgetProperties();
    DisplayCurrentCameraLocation();
}
void UIWidget::MousePress(QMouseEvent* mickey)
{
    gfx::Transform view;
    view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
    view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
    view.Rotate(qDegreesToRadians(mUI.rotation->value()));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    if (mPlayState == PlayState::Playing)
    {
        // todo: refactor the duplicated code
        const auto& pos = mickey->pos();
        const auto& view_to_scene  = glm::inverse(view.GetAsMatrix());
        const auto& mouse_in_scene = view_to_scene * ToVec4(mickey->pos());

        uik::Window::MouseEvent m;
        m.native_mouse_pos = uik::FPoint(pos.x(), pos.y());
        m.window_mouse_pos = uik::FPoint(mouse_in_scene.x, mouse_in_scene.y);
        m.time = mPlayTime;
        if (mickey->button() == Qt::LeftButton)
            m.button = uik::MouseButton::Left;
        else if (mickey->button() == Qt::RightButton)
            m.button = uik::MouseButton::Right;
        else if (mickey->button() == Qt::MiddleButton)
            m.button = uik::MouseButton::Wheel;
        const auto& action = mState.active_window->MousePress(m, *mState.active_state);
        if (action.type != uik::WidgetActionType::None)
            mMessageQueue.push_back(base::FormatString("Event: %1, widget: '%2'", action.type, action.name));
    }
    else if (!mCurrentTool)
    {
        const auto snap = (bool)GetValue(mUI.chkSnap);
        const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
        const auto grid_size = (unsigned)grid;
        const auto& view_to_scene  = glm::inverse(view.GetAsMatrix());
        const auto& mouse_in_scene = view_to_scene * ToVec4(mickey->pos());

        uik::FPoint widget_hit_point;
        auto* widget = mState.window.HitTest(uik::FPoint(mouse_in_scene.x, mouse_in_scene.y), &widget_hit_point);
        if (widget)
        {
            const auto& widget_rect = mState.window.FindWidgetRect(widget);
            gfx::FRect size_box;
            size_box.Resize(10.0f, 10.0f);
            size_box.Move(widget_rect.GetPosition());
            size_box.Translate(widget_rect.GetWidth()-10.0f, widget_rect.GetHeight()-10.0f);
            //DEBUG("Hit widget: %1", widget->GetName());
            //DEBUG("Hit pos: %1,%2", widget_hit_point.GetX(), widget_hit_point.GetY());

            if (size_box.TestPoint(mouse_in_scene.x, mouse_in_scene.y))
                mCurrentTool.reset(new ResizeWidgetTool(mState, widget));
            else mCurrentTool.reset(new MoveWidgetTool(mState, widget, snap, grid_size));
            mUI.tree->SelectItemById(app::FromUtf8(widget->GetId()));
        }
        else
        {
            mUI.tree->ClearSelection();
            mCurrentTool.reset(new MoveCameraTool(mState));
        }
    }
    if (mCurrentTool)
        mCurrentTool->MousePress(mickey, view);
}

void UIWidget::MouseRelease(QMouseEvent* mickey)
{
    gfx::Transform view;
    view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
    view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
    view.Rotate(qDegreesToRadians(mUI.rotation->value()));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    if (mPlayState == PlayState::Playing)
    {
        // todo: refactor the duplicated code
        const auto& pos = mickey->pos();
        const auto& view_to_scene  = glm::inverse(view.GetAsMatrix());
        const auto& mouse_in_scene = view_to_scene * ToVec4(mickey->pos());

        uik::Window::MouseEvent m;
        m.native_mouse_pos = uik::FPoint(pos.x(), pos.y());
        m.window_mouse_pos = uik::FPoint(mouse_in_scene.x, mouse_in_scene.y);
        m.time = mPlayTime;
        if (mickey->button() == Qt::LeftButton)
            m.button = uik::MouseButton::Left;
        else if (mickey->button() == Qt::RightButton)
            m.button = uik::MouseButton::Right;
        else if (mickey->button() == Qt::MiddleButton)
            m.button = uik::MouseButton::Wheel;
        const auto& action = mState.active_window->MouseRelease(m, *mState.active_state);
        if (action.type != uik::WidgetActionType::None)
            mMessageQueue.push_back(base::FormatString("Event: %1, widget: '%2'", action.type, action.name));
    }
    else if (mCurrentTool && mCurrentTool->MouseRelease(mickey, view))
    {
        mCurrentTool.reset();
    }
}
void UIWidget::MouseDoubleClick(QMouseEvent* mickey)
{

}

bool UIWidget::KeyPress(QKeyEvent* key)
{
    if (mCurrentTool && mCurrentTool->KeyPress(key))
        return true;

    // Handle key press
    switch (key->key())
    {
        case Qt::Key_Delete:
            on_actionWidgetDelete_triggered();
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
            TranslateCurrentWidget(-20.0f, 0.0f);
            break;
        case Qt::Key_Right:
            TranslateCurrentWidget(20.0f, 0.0f);
            break;
        case Qt::Key_Up:
            TranslateCurrentWidget(0.0f, -20.0f);
            break;
        case Qt::Key_Down:
            TranslateCurrentWidget(0.0f, 20.0f);
            break;
        case Qt::Key_Escape:
            if (mCurrentTool)
                mCurrentTool.reset();
            else mUI.tree->ClearSelection();
            break;
        default:
            return false;
    }
    return true;
}

void UIWidget::UpdateCurrentWidgetProperties()
{
    if (auto* widget = GetCurrentWidget())
    {
        // width / size
        widget->SetSize(GetValue(mUI.widgetWidth) , GetValue(mUI.widgetHeight));
        widget->SetPosition(GetValue(mUI.widgetXPos) , GetValue(mUI.widgetYPos));
        widget->SetFlag(uik::Widget::Flags::VisibleInGame, GetValue(mUI.chkWidgetVisible));
        widget->SetFlag(uik::Widget::Flags::Enabled, GetValue(mUI.chkWidgetEnabled));

        // the widget style data is set in the WidgetStyleWidget whenever
        // those UI values are changed.

        // set widget data.
        if (auto* label = dynamic_cast<uik::Label*>(widget))
        {
            label->SetText(GetValue(mUI.widgetText));
            label->SetLineHeight(GetValue(mUI.widgetLineHeight));
        }
        else if (auto* pushbtn = dynamic_cast<uik::PushButton*>(widget))
        {
            pushbtn->SetText(GetValue(mUI.widgetText));
        }
        else if (auto* chkbox = dynamic_cast<uik::CheckBox*>(widget))
        {
            chkbox->SetText(GetValue(mUI.widgetText));
            chkbox->SetChecked(GetValue(mUI.widgetCheckBox));
        }
    }
}

void UIWidget::TranslateCamera(float dx, float dy)
{
    mState.camera_offset_x += dx;
    mState.camera_offset_y += dy;
    DisplayCurrentCameraLocation();
}

void UIWidget::TranslateCurrentWidget(float dx, float dy)
{
    if (auto* widget = GetCurrentWidget())
    {
        widget->Translate(dx, dy);
        const auto& pos = widget->GetPosition();
        SetValue(mUI.widgetXPos, pos.GetX());
        SetValue(mUI.widgetYPos, pos.GetY());
    }
}

void UIWidget::DisplayCurrentWidgetProperties()
{
    SetValue(mUI.widgetId, QString(""));
    SetValue(mUI.widgetName, QString(""));
    SetValue(mUI.widgetWidth, 0.0f);
    SetValue(mUI.widgetHeight, 0.0f);
    SetValue(mUI.widgetXPos, 0.0f);
    SetValue(mUI.widgetYPos, 0.0f);
    SetValue(mUI.widgetText, QString(""));
    SetValue(mUI.widgetCheckBox, false);
    SetValue(mUI.widgetLineHeight, 1.0f);
    SetValue(mUI.chkWidgetEnabled, true);
    SetValue(mUI.chkWidgetVisible, true);
    SetEnabled(mUI.widgetProperties, false);
    SetEnabled(mUI.widgetStyleTab,   false);
    SetEnabled(mUI.widgetData,       false);
    SetEnabled(mUI.widgetLineHeight, false);
    SetEnabled(mUI.widgetText,       false);
    SetEnabled(mUI.widgetCheckBox,   false);

    if (const auto* widget = GetCurrentWidget())
    {
        SetEnabled(mUI.widgetProperties, true);
        SetEnabled(mUI.widgetStyleTab,   true);
        SetEnabled(mUI.widgetData,       true);

        const auto& id = widget->GetId();
        const auto& size = widget->GetSize();
        const auto& pos = widget->GetPosition();
        SetValue(mUI.widgetId , widget->GetId());
        SetValue(mUI.widgetName , widget->GetName());
        SetValue(mUI.widgetWidth , size.GetWidth());
        SetValue(mUI.widgetHeight , size.GetHeight());
        SetValue(mUI.widgetXPos , pos.GetX());
        SetValue(mUI.widgetYPos , pos.GetY());
        SetValue(mUI.chkWidgetEnabled, widget->TestFlag(uik::Widget::Flags::Enabled));
        SetValue(mUI.chkWidgetVisible, widget->TestFlag(uik::Widget::Flags::VisibleInGame));

        if (const auto* label = dynamic_cast<const uik::Label*>(widget))
        {
            SetValue(mUI.widgetText, label->GetText());
            SetValue(mUI.widgetLineHeight, label->GetLineHeight());
            SetEnabled(mUI.widgetText, true);
            SetEnabled(mUI.widgetLineHeight, true);
        }
        else if (const auto* pushbtn = dynamic_cast<const uik::PushButton*>(widget))
        {
            SetValue(mUI.widgetText, pushbtn->GetText());
            SetEnabled(mUI.widgetText, true);
        }
        else if (const auto* chkbox = dynamic_cast<const uik::CheckBox*>(widget))
        {
            SetValue(mUI.widgetText, chkbox->GetText());
            SetValue(mUI.widgetCheckBox, chkbox->IsChecked());
            SetEnabled(mUI.widgetText, true);
            SetEnabled(mUI.widgetCheckBox, true);
        }
    }

    // update widget style widget.
    WidgetStyleWidget* style_widgets[] = {
      mUI.widgetNormal, mUI.widgetDisabled, mUI.widgetFocused,
      mUI.widgetMoused, mUI.widgetPressed
    };
    for (auto* style_widget : style_widgets)
    {
        style_widget->SetWorkspace(mState.workspace);
        style_widget->SetPainter(mState.painter.get());
        style_widget->SetStyle(mState.style.get());
        style_widget->SetWidget(GetCurrentWidget());
    }
}

void UIWidget::DisplayCurrentCameraLocation()
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto camera_center_x = width * 0.5f - mState.window.GetWidth() * 0.5;
    const auto camera_center_y = height * 0.5f - mState.window.GetHeight() * 0.5;

    const auto dist_x = mState.camera_offset_x - camera_center_x;
    const auto dist_y = mState.camera_offset_y - camera_center_y;
    SetValue(mUI.translateX, dist_x);
    SetValue(mUI.translateY, dist_y);
}

void UIWidget::RebuildCombos()
{
    std::vector<ListItem> materials;
    for (size_t i=0; i<mState.workspace->GetNumResources(); ++i)
    {
        const auto& resource = mState.workspace->GetResource(i);
        if (!resource.IsMaterial())
            continue;
        ListItem item;
        item.name = resource.GetName();
        item.id   = resource.GetId();
        materials.push_back(std::move(item));
    }
    std::sort(materials.begin(), materials.end(), [](const auto& a, const auto& b) {
                  return a.name < b.name;
              });

    // little special case in order to be able to have "default", "nothing" and material reference
    // all available in the same UI element.
    ListItem item;
    item.name = "None";
    item.id   = "_none";
    materials.insert(materials.begin(), std::move(item));

    mUI.widgetNormal->RebuildMaterialCombos(materials);
    mUI.widgetDisabled->RebuildMaterialCombos(materials);
    mUI.widgetFocused->RebuildMaterialCombos(materials);
    mUI.widgetMoused->RebuildMaterialCombos(materials);
    mUI.widgetPressed->RebuildMaterialCombos(materials);
}

uik::Widget* UIWidget::GetCurrentWidget()
{
    if (mPlayState == PlayState::Playing)
        return nullptr;

    TreeWidget::TreeItem* item = mUI.tree->GetSelectedItem();
    if (item == nullptr)
        return nullptr;
    return static_cast<uik::Widget*>(item->GetUserData());
}
const uik::Widget* UIWidget::GetCurrentWidget() const
{
    if (mPlayState == PlayState::Playing)
        return nullptr;

    const TreeWidget::TreeItem* item = mUI.tree->GetSelectedItem();
    if (item == nullptr)
        return nullptr;
    return static_cast<const uik::Widget*>(item->GetUserData());
}

bool UIWidget::LoadStyle(const QString& name)
{
    const auto& data = mState.workspace->LoadGameData(app::ToUtf8(name));
    if (!data)
    {
        ERROR("Failed to load style file: '%1'", name);
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("Failed to load style file.\n%1").arg(name));
        msg.exec();
        return false;
    }

    auto style = std::make_unique<game::UIStyle>();
    style->SetClassLibrary(mState.workspace);
    if (!style->LoadStyle(*data))
    {
        WARN("Errors were found while parsing the style.");
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setIcon(QMessageBox::Warning);
        msg.setText(tr("Errors were found while parsing the style.\n"
                       "Styling might be incomplete or unusable.\n"
                       "Do you want to continue?"));
        if (msg.exec() == QMessageBox::No)
            return false;
    }
    mState.style = std::move(style);
    INFO("Loaded UI style '%1'", name);

    mState.painter->SetStyle(mState.style.get());
    mState.painter->DeleteMaterialInstances();
    // reapply the styling for each widget.
    mState.window.Style(*mState.painter);
    mState.window.SetStyleName(app::ToUtf8(name));
    SetValue(mUI.baseStyle, name);
    return true;
}


bool UIWidget::IsRootWidget(const uik::Widget* widget) const
{
    return mState.window.IsRootWidget(widget);
}

void UIWidget::UpdateDeletedResourceReferences()
{
    // the only resource references are materials.
    // for any given material that is referenced but
    // no longer available we're simply going to remove
    // the styling material property essentially putting
    // it back to default.
    if (mState.style->PurgeUnavailableMaterialReferences())
    {
        mState.window.ForEachWidget([this](uik::Widget* widget) {
            auto style = mState.style->MakeStyleString(widget->GetId());
            widget->SetStyleString(style);
        });
        // todo: improve the message/information about which widgets were affected.
        WARN("Some UI material references are no longer available and have been defaulted.");
    }
}

} // namespace
