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
#  include <boost/algorithm/string/erase.hpp>
#include "warnpop.h"

#include <cmath>

#include "base/assert.h"
#include "base/format.h"
#include "base/utility.h"
#include "data/json.h"
#include "editor/app/eventlog.h"
#include "editor/app/resource.h"
#include "editor/app/utility.h"
#include "editor/app/workspace.h"
#include "editor/gui/clipboard.h"
#include "editor/gui/dlgmaterial.h"
#include "editor/gui/dlgstyleproperties.h"
#include "editor/gui/dlgstylestring.h"
#include "editor/gui/drawing.h"
#include "editor/gui/scriptwidget.h"
#include "editor/gui/settings.h"
#include "editor/gui/tool.h"
#include "editor/gui/uiwidget.h"
#include "editor/gui/utility.h"
#include "graphics/drawing.h"
#include "uikit/op.h"
#include "uikit/state.h"
#include "uikit/widget.h"
#include "uikit/window.h"

namespace {
std::vector<app::ListItem> ListMaterials(const app::Workspace* workspace)
{
    std::vector<app::ListItem> materials;
    for (size_t i=0; i<workspace->GetNumResources(); ++i)
    {
        const auto& resource = workspace->GetResource(i);
        if (!resource.IsMaterial())
            continue;
        app::ListItem item;
        item.name = resource.GetName();
        item.id   = resource.GetId();
        materials.push_back(std::move(item));
    }
    std::sort(materials.begin(), materials.end(), [](const auto& a, const auto& b) {
        return a.name < b.name;
    });

    // little special case in order to be able to have "default", "nothing" and material reference
    // all available in the same UI element.
    app::ListItem item;

    item.name = "UI_Gradient";
    item.id   = "_ui_gradient";
    materials.insert(materials.begin(), item);

    item.name = "UI_Color";
    item.id   = "_ui_color";
    materials.insert(materials.begin(), item);

    item.name = "UI_None";
    item.id   = "_ui_none";
    materials.insert(materials.begin(), item);
    return materials;
}
} // namespace

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
            virtual void EnterNode(uik::Widget* widget) override
            {
                TreeWidget::TreeItem item;
                item.SetId(widget ? app::FromUtf8(widget->GetId()) : "root");
                item.SetText(widget ? app::FromUtf8(widget->GetName()) : "Root");
                item.SetLevel(mLevel);
                if (widget)
                {
                    const bool visible = widget->TestFlag(uik::Widget::Flags::VisibleInEditor);
                    item.SetUserData(widget);
                    item.SetIcon(QIcon("icons:eye.png"));
                    item.SetIconMode(visible ? QIcon::Normal : QIcon::Disabled);
                }
                mList.push_back(std::move(item));
                mLevel++;
            }
            virtual void LeaveNode(uik::Widget* widget) override
            { mLevel--; }
        private:
            unsigned mLevel = 0;
            std::vector<TreeWidget::TreeItem>& mList;
        };
        if (!mState.window.GetNumWidgets())
            return;
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
            child = mState.window.AddWidget(std::move(mWidget));
            mState.window.LinkChild(parent, child);
        }
        else
        {
            mWidget->SetName(CreateName());
            mWidget->SetPosition(mWidgetPos.x , mWidgetPos.y);
            child = mState.window.AddWidget(std::move(mWidget));
            mState.window.LinkChild(nullptr, child);
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
    ResizeWidgetTool(State& state, uik::Widget* widget, bool snap = false, unsigned grid = 0)
      : mState(state)
      , mWidget(widget)
      , mSnapGrid(snap)
      , mGridSize(grid)
    {}
    virtual void MouseMove(QMouseEvent* mickey, gfx::Transform& view) override
    {
        const auto& view_to_scene = glm::inverse(view.GetAsMatrix());
        const auto& mouse_pos_view = ToVec4(mickey->pos());
        const auto& mouse_pos_scene = view_to_scene * mouse_pos_view;
        const auto& mouse_delta = mouse_pos_scene - mMousePos;
        mWidget->Grow(mouse_delta.x, mouse_delta.y);
        mMousePos = mouse_pos_scene;
        mWasSized = true;
    }
    virtual void MousePress(QMouseEvent* mickey, gfx::Transform& view) override
    {
        const auto& view_to_scene = glm::inverse(view.GetAsMatrix());
        const auto& mouse_pos_view = ToVec4(mickey->pos());
        mMousePos = view_to_scene * mouse_pos_view;
    }
    virtual bool MouseRelease(QMouseEvent* mickey, gfx::Transform& view) override
    {
        if (!mWasSized)
            return true;
        if (mickey->modifiers() & Qt::ControlModifier)
            mSnapGrid = !mSnapGrid;

        if (mSnapGrid)
        {
            const auto pos  = mWidget->GetPosition();
            const auto size = mWidget->GetSize();
            const auto pos_x = pos.GetX();
            const auto pos_y = pos.GetY();
            // see where the bottom right corner is right now
            const auto bottom_right_corner_x = pos_x + size.GetWidth();
            const auto bottom_right_corner_y = pos_y + size.GetHeight();
            // snap the bottom right corner to a grid coordinate
            const auto bottom_aligned_pos_x = std::round(bottom_right_corner_x / mGridSize) * mGridSize;
            const auto bottom_aligned_pos_y = std::round(bottom_right_corner_y / mGridSize) * mGridSize;
            // the delta between the position aligned on the grid corner and
            // the current bottom right corner is the size by which the
            // widget needs to grow in order to snap to the corner.
            const auto corner_delta_x = bottom_aligned_pos_x - bottom_right_corner_x;
            const auto corner_delta_y = bottom_aligned_pos_y - bottom_right_corner_y;
            mWidget->Grow(corner_delta_x, corner_delta_y);
        }
        return true;
    }
private:
    UIWidget::State& mState;
    uik::Widget* mWidget = nullptr;
    glm::vec4 mMousePos;
    bool mWasSized = false;
    bool mSnapGrid = false;
    unsigned mGridSize = 0;
};

UIWidget::UIWidget(app::Workspace* workspace) : mUndoStack(3)
{
    DEBUG("Create UIWidget");

    mWidgetTree.reset(new TreeModel(mState));

    mUI.setupUi(this);
    mUI.tree->SetModel(mWidgetTree.get());
    mUI.tree->Rebuild();
    mUI.widgetNormal->SetPropertySelector("");
    mUI.widgetDisabled->SetPropertySelector("/disabled");
    mUI.widgetFocused->SetPropertySelector("/focused");
    mUI.widgetMoused->SetPropertySelector("/mouse-over");
    mUI.widgetPressed->SetPropertySelector("/pressed");

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

    connect(mUI.widgetNormal,   &WidgetStyleWidget::StyleEdited, this, &UIWidget::WidgetStyleEdited);
    connect(mUI.widgetDisabled, &WidgetStyleWidget::StyleEdited, this, &UIWidget::WidgetStyleEdited);
    connect(mUI.widgetFocused,  &WidgetStyleWidget::StyleEdited, this, &UIWidget::WidgetStyleEdited);
    connect(mUI.widgetMoused,   &WidgetStyleWidget::StyleEdited, this, &UIWidget::WidgetStyleEdited);
    connect(mUI.widgetPressed,  &WidgetStyleWidget::StyleEdited, this, &UIWidget::WidgetStyleEdited);

    uik::Form form;
    form.SetSize(1024, 768);
    form.SetName("Form");

    mState.tree  = mUI.tree;
    mState.style = std::make_unique<engine::UIStyle>();
    mState.style->SetClassLibrary(workspace);
    mState.workspace = workspace;
    mState.painter.reset(new engine::UIPainter);
    mState.painter->SetStyle(mState.style.get());
    mState.window.SetName("My UI");
    mState.window.AddWidget(form);
    mState.window.LinkChild(nullptr, &mState.window.GetWidget(0));

    LoadStyleQuiet("app://ui/default.json");

    PopulateUIStyles(mUI.windowStyleFile);
    PopulateFromEnum<uik::CheckBox::Check>(mUI.chkPlacement);
    PopulateFromEnum<uik::RadioButton::Check>(mUI.rbPlacement);
    PopulateFromEnum<GridDensity>(mUI.cmbGrid);
    SetValue(mUI.windowID, mState.window.GetId());
    SetValue(mUI.windowName, mState.window.GetName());
    SetValue(mUI.windowStyleFile, mState.window.GetStyleName());
    SetValue(mUI.windowStyleString, mState.window.GetStyleString());
    SetValue(mUI.windowScriptFile, mState.window.GetScriptFile());
    SetValue(mUI.chkRecvMouseEvents, mState.window.TestFlag(uik::Window::Flags::WantsMouseEvents));
    SetValue(mUI.chkRecvKeyEvents, mState.window.TestFlag(uik::Window::Flags::WantsKeyEvents));
    SetValue(mUI.cmbGrid, GridDensity::Grid50x50);
    SetEnabled(mUI.actionPause, false);
    SetEnabled(mUI.actionStop, false);
    SetEnabled(mUI.btnEditScript, mState.window.HasScriptFile());
    setWindowTitle(GetValue(mUI.windowName));

    RebuildCombos();
    DisplayCurrentWidgetProperties();
    mOriginalHash = mState.window.GetHash();
}

UIWidget::UIWidget(app::Workspace* workspace, const app::Resource& resource) : UIWidget(workspace)
{
    DEBUG("Editing UI: '%1'", resource.GetName());

    const uik::Window* window = nullptr;
    resource.GetContent(&window);

    mState.window = *window;
    mOriginalHash = mState.window.GetHash();

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

    if (!LoadStyleQuiet(mState.window.GetStyleName()))
        mState.window.SetStyleName("");

    SetValue(mUI.windowName, window->GetName());
    SetValue(mUI.windowID, window->GetId());
    SetValue(mUI.windowStyleFile, window->GetStyleName());
    SetValue(mUI.windowStyleString, window->GetStyleString());
    SetValue(mUI.windowScriptFile, ListItemId(window->GetScriptFile()));
    SetValue(mUI.chkRecvMouseEvents, mState.window.TestFlag(uik::Window::Flags::WantsMouseEvents));
    SetValue(mUI.chkRecvKeyEvents, mState.window.TestFlag(uik::Window::Flags::WantsKeyEvents));
    SetEnabled(mUI.btnEditScript, window->HasScriptFile());
    setWindowTitle(GetValue(mUI.windowName));

    UpdateDeletedResourceReferences();
    DisplayCurrentCameraLocation();
    DisplayCurrentWidgetProperties();

    mUI.tree->Rebuild();
}

UIWidget::~UIWidget()
{
    DEBUG("Destroy UIWidget");
}

QString UIWidget::GetId() const
{
    return GetValue(mUI.windowID);
}

void UIWidget::Initialize(const UISettings& settings)
{
    SetValue(mUI.zoom,          settings.zoom);
    SetValue(mUI.cmbGrid,       settings.grid);
    SetValue(mUI.chkSnap,       settings.snap_to_grid);
    SetValue(mUI.chkShowOrigin, settings.show_origin);
    SetValue(mUI.chkShowGrid,   settings.show_grid);
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
    bar.addAction(mUI.actionNewForm);
    bar.addAction(mUI.actionNewLabel);
    bar.addAction(mUI.actionNewPushButton);
    bar.addAction(mUI.actionNewCheckBox);
    bar.addAction(mUI.actionNewGroupBox);
    bar.addAction(mUI.actionNewSpinBox);
    bar.addAction(mUI.actionNewSlider);
    bar.addAction(mUI.actionNewProgressBar);
    bar.addAction(mUI.actionNewRadioButton);
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
    menu.addAction(mUI.actionNewForm);
    menu.addAction(mUI.actionNewLabel);
    menu.addAction(mUI.actionNewPushButton);
    menu.addAction(mUI.actionNewCheckBox);
    menu.addAction(mUI.actionNewGroupBox);
    menu.addAction(mUI.actionNewSpinBox);
    menu.addAction(mUI.actionNewSlider);
    menu.addAction(mUI.actionNewProgressBar);
    menu.addAction(mUI.actionNewRadioButton);
}
bool UIWidget::SaveState(Settings& settings) const
{
    data::JsonObject json;
    mState.window.IntoJson(json);
    settings.SetValue("UI", "content", json);
    settings.SetValue("UI", "hash", mOriginalHash);
    settings.SetValue("UI", "camera_offset_x", mState.camera_offset_x);
    settings.SetValue("UI", "camera_offset_y", mState.camera_offset_y);
    settings.SaveWidget("UI", mUI.scaleX);
    settings.SaveWidget("UI", mUI.scaleY);
    settings.SaveWidget("UI", mUI.rotation);
    settings.SaveWidget("UI", mUI.chkShowOrigin);
    settings.SaveWidget("UI", mUI.chkShowGrid);
    settings.SaveWidget("UI", mUI.chkSnap);
    settings.SaveWidget("UI", mUI.cmbGrid);
    settings.SaveWidget("UI", mUI.zoom);
    settings.SaveWidget("UI", mUI.widget);
    return true;
}
bool UIWidget::LoadState(const Settings& settings)
{
    data::JsonObject json;
    settings.GetValue("UI", "content", &json);
    settings.GetValue("UI", "hash", &mOriginalHash);
    settings.GetValue("UI", "camera_offset_x", &mState.camera_offset_x);
    settings.GetValue("UI", "camera_offset_y", &mState.camera_offset_y);
    mCameraWasLoaded = true;

    settings.LoadWidget("UI", mUI.scaleX);
    settings.LoadWidget("UI", mUI.scaleY);
    settings.LoadWidget("UI", mUI.rotation);
    settings.LoadWidget("UI", mUI.chkShowOrigin);
    settings.LoadWidget("UI", mUI.chkShowGrid);
    settings.LoadWidget("UI", mUI.chkSnap);
    settings.LoadWidget("UI", mUI.cmbGrid);
    settings.LoadWidget("UI", mUI.zoom);
    settings.LoadWidget("UI", mUI.widget);

    auto window = uik::Window::FromJson(json);
    mState.window = std::move(window.value());

    if (!LoadStyleQuiet(mState.window.GetStyleName()))
        mState.window.SetStyleName("");

    SetValue(mUI.windowID, mState.window.GetId());
    SetValue(mUI.windowName, mState.window.GetName());
    SetValue(mUI.windowStyleFile, mState.window.GetStyleName());
    SetValue(mUI.windowStyleString, mState.window.GetStyleString());
    SetValue(mUI.windowScriptFile, ListItemId(mState.window.GetScriptFile()));
    SetValue(mUI.chkRecvMouseEvents, mState.window.TestFlag(uik::Window::Flags::WantsMouseEvents));
    SetValue(mUI.chkRecvKeyEvents, mState.window.TestFlag(uik::Window::Flags::WantsKeyEvents));
    SetEnabled(mUI.btnEditScript, mState.window.HasScriptFile());
    setWindowTitle(GetValue(mUI.windowName));

    DisplayCurrentCameraLocation();
    DisplayCurrentWidgetProperties();

    mUI.tree->Rebuild();
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
            else if (clipboard->GetType() != "application/json/ui")
                return false;
            return true;
        case Actions::CanCut:
        case Actions::CanCopy:
            if (!mUI.widget->hasInputFocus())
                return false;
            else if (!GetCurrentWidget())
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
        data::JsonObject json;
        const auto& tree = mState.window.GetRenderTree();
        uik::RenderTreeIntoJson(tree, json, widget);

        clipboard.SetText(json.ToString());
        clipboard.SetType("application/json/ui");

        mState.window.DeleteWidget(widget);
        mUI.tree->Rebuild();
        mUI.tree->ClearSelection();
        NOTE("Copied JSON to application clipboard.");
    }
}
void UIWidget::Copy(Clipboard& clipboard)  const
{
    if (const auto* widget = GetCurrentWidget())
    {
        data::JsonObject json;
        const auto& tree = mState.window.GetRenderTree();
        uik::RenderTreeIntoJson(tree, json, widget);

        clipboard.SetText(json.ToString());
        clipboard.SetType("application/json/ui");
        NOTE("Copied JSON to application clipboard.");
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
    else if (clipboard.GetType() != "application/json/ui")
    {
        NOTE("No UI JSON found in clipboard.");
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

    data::JsonObject json;
    auto [success, _] = json.ParseString(clipboard.GetText());
    if (!success)
    {
        NOTE("Clipboard JSON parse failed.");
        return;
    }

    // use a temporary vector in case there's a problem
    std::vector<std::unique_ptr<uik::Widget>> nodes;
    uik::RenderTree tree;
    uik::RenderTreeFromJson(json, tree, nodes);
    if (nodes.empty())
    {
        NOTE("No widgets were parsed from JSON.");
        return;
    }

    // generate new IDs for the widgets and copy and
    // update the style information if any.
    for (auto& w : nodes)
    {
        const auto old_id = w->GetId();
        const auto new_id = base::RandomString(10);
        w->SetId(new_id);

        // gather the style information
        auto style = mState.style->MakeStyleString(old_id);
        if (style.empty())
            continue;
        // replace previous (old) widget ID with the new ID
        boost::erase_all(style, old_id + "/");
        // update widget with new style information
        w->SetStyleString(style);
        // update the styling
        mState.style->ParseStyleString(new_id, style);
    }

    auto* paste_root = nodes[0].get();

    uik::FPoint hit_point;
    auto* widget = mState.window.HitTest(uik::FPoint(mouse_in_scene.x, mouse_in_scene.y), &hit_point);
    if (widget && widget->IsContainer())
    {
        paste_root->SetPosition(hit_point);
        mState.window.LinkChild(widget, paste_root);
    }
    else
    {
        paste_root->SetPosition(mouse_in_scene.x, mouse_in_scene.y);
        mState.window.LinkChild(nullptr, paste_root);
    }
    // if we got this far, nodes should contain the nodes to be added
    // into the scene and tree should contain their hierarchy.
    for (auto& node : nodes)
    {
        // moving the unique ptr means that node address stays the same
        // thus the tree is still valid!
        mState.window.AddWidget(std::move(node));
    }
    nodes.clear();

    // walk the tree and link the nodes into the scene.
    tree.PreOrderTraverseForEach([&nodes, this, &tree](uik::Widget* node) {
        if (node == nullptr)
            return;
        auto* parent = tree.GetParent(node);
        if (parent == nullptr)
            return;
        mState.window.LinkChild(parent, node);
    });

    mUI.tree->Rebuild();
    mUI.tree->SelectItemById(app::FromUtf8(paste_root->GetId()));
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
        const auto& actions = mState.active_window->PollAction(*mState.active_state, mPlayTime, dt);
        for (const auto& action : actions)
        {
            mMessageQueue.push_back(base::FormatString("Event: %1, widget: '%2'", action.type, action.name));
        }
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
    SetValue(mUI.windowName, mState.window.GetName());
    SetValue(mUI.windowStyleFile, mState.window.GetStyleName());
    SetValue(mUI.windowStyleString, mState.window.GetStyleString());
    SetValue(mUI.windowScriptFile, ListItemId(mState.window.GetScriptFile()));
    SetValue(mUI.chkRecvMouseEvents, mState.window.TestFlag(uik::Window::Flags::WantsMouseEvents));
    SetValue(mUI.chkRecvKeyEvents, mState.window.TestFlag(uik::Window::Flags::WantsKeyEvents));
    SetEnabled(mUI.btnEditScript, mState.window.HasScriptFile());
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

bool UIWidget::OnEscape()
{
    if (mCurrentTool)
    {
        mCurrentTool.reset();
        UncheckPlacementActions();
    }
    else mUI.tree->ClearSelection();
    return true;
}

bool UIWidget::GetStats(Stats* stats) const
{
    stats->time  = mPlayTime;
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

void UIWidget::on_widgetColor_colorChanged(QColor color)
{
    mUI.widget->SetClearColor(ToGfx(color));
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
    SetEnabled(mUI.actionNewSpinBox,    false);
    SetEnabled(mUI.actionNewSlider,     false);
    SetEnabled(mUI.actionNewProgressBar, false);
    SetEnabled(mUI.actionNewRadioButton, false);
    SetEnabled(mUI.actionNewGroupBox,   false);
    SetEnabled(mUI.actionNewForm,       false);
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
    SetEnabled(mUI.actionNewForm,       true);
    SetEnabled(mUI.actionNewCheckBox,   true);
    SetEnabled(mUI.actionNewLabel,      true);
    SetEnabled(mUI.actionNewPushButton, true);
    SetEnabled(mUI.actionNewGroupBox,   true);
    SetEnabled(mUI.actionNewSpinBox,    true);
    SetEnabled(mUI.actionNewSlider,     true);
    SetEnabled(mUI.actionNewProgressBar, true);
    SetEnabled(mUI.actionNewRadioButton, true);
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

    app::UIResource resource(mState.window, GetValue(mUI.windowName));
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
    setWindowTitle(GetValue(mUI.windowName));
}

void UIWidget::on_actionNewForm_triggered()
{
    const auto snap = (bool)GetValue(mUI.chkSnap);
    const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
    mCurrentTool.reset(new PlaceWidgetTool(mState, std::make_unique<uik::Form>(), snap, (unsigned)grid));
    UncheckPlacementActions();
    mUI.actionNewForm->setChecked(true);
}

void UIWidget::on_actionNewLabel_triggered()
{
    const auto snap = (bool)GetValue(mUI.chkSnap);
    const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
    mCurrentTool.reset(new PlaceWidgetTool(mState, std::make_unique<uik::Label>(), snap, (unsigned)grid));
    UncheckPlacementActions();
    mUI.actionNewLabel->setChecked(true);
}

void UIWidget::on_actionNewPushButton_triggered()
{
    const auto snap = (bool)GetValue(mUI.chkSnap);
    const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
    mCurrentTool.reset(new PlaceWidgetTool(mState, std::make_unique<uik::PushButton>(), snap, (unsigned)grid));
    UncheckPlacementActions();
    mUI.actionNewPushButton->setChecked(true);
}

void UIWidget::on_actionNewCheckBox_triggered()
{
    const auto snap = (bool)GetValue(mUI.chkSnap);
    const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
    mCurrentTool.reset(new PlaceWidgetTool(mState, std::make_unique<uik::CheckBox>(), snap, (unsigned)grid));
    UncheckPlacementActions();
    mUI.actionNewCheckBox->setChecked(true);
}

void UIWidget::on_actionNewSpinBox_triggered()
{
    const auto snap = (bool)GetValue(mUI.chkSnap);
    const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
    auto spin = std::make_unique<uik::SpinBox>();
    spin->SetMin(0);
    spin->SetMax(100);
    spin->SetValue(GetValue(mUI.spinVal));
    mCurrentTool.reset(new PlaceWidgetTool(mState, std::move(spin), snap, (unsigned)grid));
    UncheckPlacementActions();
    mUI.actionNewSpinBox->setChecked(true);
}

void UIWidget::on_actionNewSlider_triggered()
{
    const auto snap = (bool)GetValue(mUI.chkSnap);
    const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
    mCurrentTool.reset(new PlaceWidgetTool(mState, std::make_unique<uik::Slider>(), snap, (unsigned)grid));
    UncheckPlacementActions();
    mUI.actionNewSlider->setChecked(true);
}

void UIWidget::on_actionNewProgressBar_triggered()
{
    const auto snap = (bool)GetValue(mUI.chkSnap);
    const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
    auto widget = std::make_unique<uik::ProgressBar>();
    widget->SetText("%1%");
    widget->SetValue(0.5f);
    mCurrentTool.reset(new PlaceWidgetTool(mState, std::move(widget), snap, (unsigned)grid));
    UncheckPlacementActions();
    mUI.actionNewProgressBar->setChecked(true);
}

void UIWidget::on_actionNewRadioButton_triggered()
{
    const auto snap = (bool)GetValue(mUI.chkSnap);
    const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
    auto widget = std::make_unique<uik::RadioButton>();
    mCurrentTool.reset(new PlaceWidgetTool(mState, std::move(widget), snap, (unsigned)grid));
    UncheckPlacementActions();
    mUI.actionNewRadioButton->setChecked(true);
}

void UIWidget::on_actionNewGroupBox_triggered()
{
    const auto snap = (bool)GetValue(mUI.chkSnap);
    const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
    mCurrentTool.reset(new PlaceWidgetTool(mState, std::make_unique<uik::GroupBox>(), snap, (unsigned)grid));
    UncheckPlacementActions();
    mUI.actionNewGroupBox->setChecked(true);
}

void UIWidget::on_actionWidgetDelete_triggered()
{
    if (auto* widget = GetCurrentWidget())
    {
        mState.window.DeleteWidget(widget);
        mUI.tree->Rebuild();
        mUI.tree->ClearSelection();
    }
}
void UIWidget::on_actionWidgetDuplicate_triggered()
{
    if (auto* widget = GetCurrentWidget())
    {
        auto* dupe = mState.window.DuplicateWidget(widget);
        // update the translation for the parent of the new hierarchy
        // so that it's possible to tell it apart from the source of the copy.
        dupe->Translate(10.0f, 10.0f);

        mState.window.VisitEach([](uik::Widget* widget) {
            const auto& name = widget->GetName();
            widget->SetName(base::FormatString("Copy of %1", name));
        }, dupe);

        mUI.tree->Rebuild();
        mUI.tree->SelectItemById(app::FromUtf8(dupe->GetId()));
        mState.window.Style(*mState.painter);
    }
}

void UIWidget::on_windowName_textChanged(const QString& text)
{
    mState.window.SetName(app::ToUtf8(text));
}

void UIWidget::on_windowStyleFile_currentIndexChanged(int)
{
    const auto& name = mState.window.GetStyleName();

    if (!LoadStyleVerbose(GetValue(mUI.windowStyleFile)))
        SetValue(mUI.windowStyleFile, name);
}

void UIWidget::on_windowScriptFile_currentIndexChanged(int index)
{
    mState.window.SetScriptFile(GetItemId(mUI.windowScriptFile));
    SetEnabled(mUI.btnEditScript, true);
}

void UIWidget::on_chkRecvMouseEvents_stateChanged(int)
{
    mState.window.SetFlag(uik::Window::Flags::WantsMouseEvents, GetValue(mUI.chkRecvMouseEvents));
}
void UIWidget::on_chkRecvKeyEvents_stateChanged(int)
{
    mState.window.SetFlag(uik::Window::Flags::WantsKeyEvents, GetValue(mUI.chkRecvKeyEvents));
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
void UIWidget::on_rbText_textChanged()
{
    UpdateCurrentWidgetProperties();
}
void UIWidget::on_rbPlacement_currentIndexChanged(int)
{
    UpdateCurrentWidgetProperties();
}
void UIWidget::on_rbSelected_stateChanged(int)
{
    UpdateCurrentWidgetProperties();
}
void UIWidget::on_btnText_textChanged()
{
    UpdateCurrentWidgetProperties();
}
void UIWidget::on_lblText_textChanged()
{
    UpdateCurrentWidgetProperties();
}
void UIWidget::on_lblLineHeight_valueChanged(double)
{
    UpdateCurrentWidgetProperties();
}
void UIWidget::on_chkText_textChanged()
{
    UpdateCurrentWidgetProperties();
}
void UIWidget::on_chkPlacement_currentIndexChanged(int)
{
    UpdateCurrentWidgetProperties();
}

void UIWidget::on_chkCheck_stateChanged(int)
{
    UpdateCurrentWidgetProperties();
}
void UIWidget::on_spinMin_valueChanged(int)
{
    UpdateCurrentWidgetProperties();
}
void UIWidget::on_spinMax_valueChanged(int)
{
    UpdateCurrentWidgetProperties();
}
void UIWidget::on_spinVal_valueChanged(int)
{
    UpdateCurrentWidgetProperties();
}

void UIWidget::on_sliderVal_valueChanged(double)
{
    UpdateCurrentWidgetProperties();
}

void UIWidget::on_progVal_valueChanged(int)
{
    UpdateCurrentWidgetProperties();
}
void UIWidget::on_progText_textChanged()
{
    UpdateCurrentWidgetProperties();
}

void UIWidget::on_btnResetProgVal_clicked()
{
    SetValue(mUI.progVal, -1);
    UpdateCurrentWidgetProperties();
}

void UIWidget::on_btnReloadStyle_clicked()
{
    const auto& style = mState.window.GetStyleName();
    if (style.empty())
        return;
    LoadStyleVerbose(app::FromUtf8(style));
}

void UIWidget::on_btnSelectStyle_clicked()
{
    const auto& file = QFileDialog::getOpenFileName(this,
        tr("Select JSON Style File"), "",
        tr("Style (*.json)"));
    if (file.isEmpty())
        return;
    const auto& name = mState.workspace->MapFileToWorkspace(file);
    if (LoadStyleVerbose(name))
        SetValue(mUI.windowStyleFile, name);
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
    const auto& form_size = GetFormSize();
    const auto width = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto rotation = mUI.rotation->value();
    mState.camera_offset_x = width * 0.5f - form_size.GetWidth() * 0.5;
    mState.camera_offset_y = height * 0.5f - form_size.GetHeight() * 0.5;
    mViewTransformRotation = rotation;
    mViewTransformStartTime = mCurrentTime;
    // set new camera offset to the center of the widget.
    mUI.translateX->setValue(0);
    mUI.translateY->setValue(0);
    mUI.scaleX->setValue(1.0f);
    mUI.scaleY->setValue(1.0f);
    mUI.rotation->setValue(0);
}

void UIWidget::on_btnEditWidgetStyle_clicked()
{
    if (auto* widget = GetCurrentWidget())
    {
        // copy the current style string and use the copy
        // to restore the style if the dialog is cancelled.
        std::string style_string = widget->GetStyleString();

        DlgWidgetStyleProperties dlg(this, mState.style.get(), mState.workspace);
        dlg.SetPainter(mState.painter.get());
        dlg.SetMaterials(ListMaterials(mState.workspace));
        dlg.SetWidget(widget);
        if (dlg.exec() == QDialog::Rejected)
        {
            // delete all properties including both changes we want to discard
            // *and* properties we want to keep (that are in the old style string)
            mState.style->DeleteMaterials(widget->GetId());
            mState.style->DeleteProperties(widget->GetId());
            mState.painter->DeleteMaterialInstances(widget->GetId());

            // restore properties from the old style string.
            if (!style_string.empty())
                mState.style->ParseStyleString(widget->GetId(), style_string);

            // restore old style string.
            widget->SetStyleString(std::move(style_string));
            return;
        }

        // update widget's style string so that it contains the
        // latest widget style.

        // gather the style properties for this widget into a single style string
        // in the styling engine specific format.
        style_string = mState.style->MakeStyleString(widget->GetId());
        // this is a bit of a hack but we know that the style string
        // contains the widget id for each property. removing the
        // widget id from the style properties:
        // a) saves some space
        // b) makes the style string copyable from one widget to another as-s
        const auto id = widget->GetId();
        boost::erase_all(style_string, id + "/");
        // set the actual style string.
        widget->SetStyleString(std::move(style_string));

        DisplayCurrentWidgetProperties();
    }
}
void UIWidget::on_btnEditWidgetStyleString_clicked()
{
    if (auto* widget = GetCurrentWidget())
    {
        std::string old_style_string = widget->GetStyleString();

        const auto& widget_id = widget->GetId();
        const auto& style_key = widget_id + "/";
        boost::erase_all(old_style_string, style_key);

        DlgWidgetStyleString dlg(this, app::FromUtf8(old_style_string));
        if (dlg.exec() == QDialog::Rejected)
            return;

        const auto& new_style_string = app::ToUtf8(dlg.GetStyleString());

        mState.style->DeleteMaterials(widget->GetId());
        mState.style->DeleteProperties(widget->GetId());
        mState.painter->DeleteMaterialInstances(widget->GetId());

        if (!mState.style->ParseStyleString(widget->GetId(), new_style_string))
        {
            ERROR("Widget style string contains errors and cannot be used.[widget='%1']", widget->GetName());
            mState.style->ParseStyleString(widget->GetId(), old_style_string);
            return;
        }
        widget->SetStyleString(new_style_string);
        SetValue(mUI.widgetStyleString, new_style_string);
    }
}

void UIWidget::on_btnResetWidgetStyle_clicked()
{
    if (auto* widget = GetCurrentWidget())
    {
        widget->SetStyleString("");
        mState.style->DeleteMaterials(widget->GetId());
        mState.style->DeleteProperties(widget->GetId());
        mState.painter->DeleteMaterialInstances(widget->GetId());

        DisplayCurrentWidgetProperties();
    }
}

void UIWidget::on_btnEditWindowStyle_clicked()
{
    std::string style_string = mState.window.GetStyleString();

    DlgWidgetStyleProperties dlg(this, mState.style.get(), mState.workspace);
    dlg.SetPainter(mState.painter.get());
    dlg.SetMaterials(ListMaterials(mState.workspace));
    if (dlg.exec() == QDialog::Rejected)
    {
        // delete all properties including both changes we want to discard
        // *and* properties we want to keep (that are in the old style string)
        mState.style->DeleteMaterials("window");
        mState.style->DeleteProperties("window");
        mState.painter->DeleteMaterialInstances("window");

        // restore properties from the old style string.
        if (!style_string.empty())
            mState.style->ParseStyleString("window", style_string);

        // restore old style string.
        mState.window.SetStyleString(std::move(style_string));
        return;
    }

    // gather the style properties for this widget into a single style string
    // in the styling engine specific format.
    style_string = mState.style->MakeStyleString("window");

    SetValue(mUI.windowStyleString, style_string);

    // this is a bit of a hack but we know that the style string
    // contains the prefix "window" for each property. removing the
    // prefix from the style properties:
    // a) saves some space
    // b) makes the style string copyable from one widget to another as-s
    boost::erase_all(style_string, "window/");
    // set the actual style string.
    mState.window.SetStyleString(std::move(style_string));
}

void UIWidget::on_btnEditWindowStyleString_clicked()
{
    std::string old_style_string = mState.window.GetStyleString();

    boost::erase_all(old_style_string, "window/");

    DlgWidgetStyleString dlg(this, app::FromUtf8(old_style_string));
    if (dlg.exec() == QDialog::Rejected)
        return;

    const auto& new_style_string = app::ToUtf8(dlg.GetStyleString());

    mState.style->DeleteMaterials("window");
    mState.style->DeleteProperties("window");
    mState.painter->DeleteMaterialInstances("window");

    if (!mState.style->ParseStyleString("window", new_style_string))
    {
        ERROR("Window style string contains errors and cannot be used. [window='%1']", mState.window.GetName());
        mState.style->ParseStyleString("window", old_style_string);
        return;
    }
    mState.window.SetStyleString(new_style_string);
    SetValue(mUI.windowStyleString, new_style_string);
}

void UIWidget::on_btnResetWindowStyle_clicked()
{
    mState.window.ResetStyleString();
    mState.style->DeleteMaterials("window");
    mState.style->DeleteProperties("window");
    mState.painter->DeleteMaterialInstances("window");

    SetValue(mUI.windowStyleString, QString(""));
}

void UIWidget::on_btnEditScript_clicked()
{
    const auto id = (QString)GetItemId(mUI.windowScriptFile);
    if (id.isEmpty())
        return;
    emit OpenResource(id);
}
void UIWidget::on_btnAddScript_clicked()
{
    app::Script script;
    // use the script ID as the file name so that we can
    // avoid naming clashes and always find the correct lua
    // file even if the entity is later renamed.
    const auto& filename = app::FromUtf8(script.GetId());
    const auto& fileuri  = QString("ws://lua/%1.lua").arg(filename);
    const auto& filepath = mState.workspace->MapFileToFilesystem(fileuri);
    const auto& name = GetValue(mUI.windowName);
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
        ERROR("Failed to open file for writing. [file='%1, error='%2']", filepath, io.error());
        QMessageBox msg(this);
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
    stream << QString("-- UI '%1' script.\n\n").arg(name);
    stream << QString("-- This script will be called for every instance of '%1.'\n").arg(name);
    stream << "-- You're free to delete functions you don't need.\n\n";
    stream << "-- Called whenever the UI has been opened.\n";
    stream << "function OnUIOpen(ui)\nend\n\n";
    stream << " -- Called whenever the UI is about to be closed.\n";
    stream << "-- result is the value passed in the exit_code in the call to CloseUI.\n";
    stream << "function OnUIClose(ui, result)\nend\n\n";
    stream << "-- Called whenever some UI action such as button click etc. occurs\n";
    stream << "function OnUIAction(ui, action)\nend\n\n";
    stream << "-- Optionally called on mouse events when the flag is set.\n";
    stream << "function OnMouseMove(ui, mouse)\nend\n\n";
    stream << "-- Optionally called on mouse events when the flag is set.\n";
    stream << "function OnMousePress(ui, mouse)\nend\n\n";
    stream << "-- Optionally called on mouse events when the flag is set.\n";
    stream << "function OnMouseRelease(ui, mouse)\nend\n\n";
    stream << "-- Optionally called on keyboard events when the flag is set.\n";
    stream << "function OnKeyDown(ui, symbol, modifier_bits)\nend\n\n";
    stream << "-- Optionally called on keyboard events when the flag is set.\n";
    stream << "function OnKeyUp(ui, symbol, modifier_bits)\nend\n\n";

    io.flush();
    io.close();

    script.SetFileURI(app::ToUtf8(fileuri));
    app::ScriptResource resource(script, name);
    mState.workspace->SaveResource(resource);
    mState.window.SetScriptFile(script.GetId());

    ScriptWidget* widget = new ScriptWidget(mState.workspace, resource);
    emit OpenNewWidget(widget);

    SetValue(mUI.windowScriptFile, ListItemId(script.GetId()));
    SetEnabled(mUI.btnEditScript, true);
}

void UIWidget::on_btnResetScript_clicked()
{
    mState.window.ResetScriptFile();
    SetValue(mUI.windowScriptFile, -1);
    SetEnabled(mUI.btnEditScript, false);
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
    if (widget)
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
    if (dst_widget && !dst_widget->IsContainer())
        return;
    auto& tree = mState.window.GetRenderTree();

    // check if we're trying to drag a parent onto its own child.
    if (base::SearchChild(tree, dst_widget, src_widget))
        return;
    // todo: probably want to retain the position of the widget
    // relative to window. this means that the widget's position
    // needs to be transformed relative to the new parent
    tree.ReparentChild(dst_widget, src_widget);
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
    if (!(resource->IsMaterial() || resource->IsScript()))
        return;
    RebuildCombos();
}
void UIWidget::ResourceToBeDeleted(const app::Resource* resource)
{
    if (!(resource->IsMaterial() || resource->IsScript()))
        return;
    UpdateDeletedResourceReferences();
    RebuildCombos();
    DisplayCurrentWidgetProperties();
    if (resource->IsMaterial())
        mState.painter->DeleteMaterialInstanceByClassId(resource->GetIdUtf8());
}
void UIWidget::ResourceUpdated(const app::Resource* resource)
{
    if (!(resource->IsMaterial() || resource->IsScript()))
        return;
    RebuildCombos();
    DisplayCurrentWidgetProperties();
    if (resource->IsMaterial())
        mState.painter->DeleteMaterialInstanceByClassId(resource->GetIdUtf8());
}

void UIWidget::WidgetStyleEdited()
{
    if (auto* widget = GetCurrentWidget())
    {
        SetValue(mUI.widgetStyleString, widget->GetStyleString());
    }
}

void UIWidget::PaintScene(gfx::Painter& painter, double sec)
{
    const auto& form_size = GetFormSize();
    const auto surface_width  = mUI.widget->width();
    const auto surface_height = mUI.widget->height();
    const auto zoom   = (float)GetValue(mUI.zoom);
    const auto xs     = (float)GetValue(mUI.scaleX);
    const auto ys     = (float)GetValue(mUI.scaleY);
    const auto grid   = (GridDensity)GetValue(mUI.cmbGrid);
    const auto window_width  = form_size.GetWidth();
    const auto window_height = form_size.GetHeight();

    SetValue(mUI.widgetColor, mUI.widget->GetCurrentClearColor());

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
        gfx::DrawRectOutline(painter, gfx::FRect(0.0f, 0.0f, GetFormSize()), gfx::Color::HotPink);
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

    painter.SetOrthographicProjection(0.0f , 0.0f , surface_width , surface_height);
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
    else if (!mCurrentTool && (mickey->button() == Qt::LeftButton))
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
                mCurrentTool.reset(new ResizeWidgetTool(mState, widget, snap, grid_size));
            else mCurrentTool.reset(new MoveWidgetTool(mState, widget, snap, grid_size));
            mUI.tree->SelectItemById(app::FromUtf8(widget->GetId()));
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
        UncheckPlacementActions();
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
        if (auto* label = uik::WidgetCast<uik::Label>(widget))
        {
            label->SetText(GetValue(mUI.lblText));
            label->SetLineHeight(GetValue(mUI.lblLineHeight));
        }
        else if (auto* pushbtn = uik::WidgetCast<uik::PushButton>(widget))
        {
            pushbtn->SetText(GetValue(mUI.btnText));
        }
        else if (auto* chkbox = uik::WidgetCast<uik::CheckBox>(widget))
        {
            chkbox->SetText(GetValue(mUI.chkText));
            chkbox->SetChecked(GetValue(mUI.chkCheck));
            chkbox->SetCheckLocation(GetValue(mUI.chkPlacement));
        }
        else if (auto* slider = uik::WidgetCast<uik::Slider>(widget))
        {
            slider->SetValue(GetValue(mUI.sliderVal));
        }
        else if (auto* spin = uik::WidgetCast<uik::SpinBox>(widget))
        {
            spin->SetMin(GetValue(mUI.spinMin));
            spin->SetMax(GetValue(mUI.spinMax));
            spin->SetValue(GetValue(mUI.spinVal));
        }
        else if (auto* prog = uik::WidgetCast<uik::ProgressBar>(widget))
        {
            const int val = GetValue(mUI.progVal);
            if (val == -1)
                prog->ClearValue();
            else prog->SetValue(val / 100.0f);
            prog->SetText(GetValue(mUI.progText));
        }
        else if (auto* radio = uik::WidgetCast<uik::RadioButton>(widget))
        {
            radio->SetText(GetValue(mUI.rbText));
            radio->SetCheckLocation(GetValue(mUI.rbPlacement));
            radio->SetSelected(GetValue(mUI.rbSelected));
            if (radio->IsSelected())
            {
                std::vector<uik::Widget*> siblings;
                auto& tree = mState.window.GetRenderTree();
                base::ListSiblings(tree, widget, &siblings);
                for (auto* sibling : siblings)
                {
                    if (auto* radio = uik::WidgetCast<uik::RadioButton>(sibling))
                        radio->SetSelected(false);
                }
            }
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
    SetValue(mUI.widgetStyleString, QString(""));
    SetValue(mUI.widgetWidth, 0.0f);
    SetValue(mUI.widgetHeight, 0.0f);
    SetValue(mUI.widgetXPos, 0.0f);
    SetValue(mUI.widgetYPos, 0.0f);
    SetValue(mUI.chkWidgetEnabled, true);
    SetValue(mUI.chkWidgetVisible, true);
    SetEnabled(mUI.widgetProperties, false);
    SetEnabled(mUI.widgetStyleTab,   false);
    SetEnabled(mUI.widgetData,       false);
    mUI.stackedWidget->setCurrentWidget(mUI.blankPage);

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
        SetValue(mUI.widgetStyleString, widget->GetStyleString());
        SetValue(mUI.widgetWidth , size.GetWidth());
        SetValue(mUI.widgetHeight , size.GetHeight());
        SetValue(mUI.widgetXPos , pos.GetX());
        SetValue(mUI.widgetYPos , pos.GetY());
        SetValue(mUI.chkWidgetEnabled, widget->TestFlag(uik::Widget::Flags::Enabled));
        SetValue(mUI.chkWidgetVisible, widget->TestFlag(uik::Widget::Flags::VisibleInGame));

        if (const auto* label = uik::WidgetCast<uik::Label>(widget))
        {
            mUI.stackedWidget->setCurrentWidget(mUI.lblPage);
            SetValue(mUI.lblText,       label->GetText());
            SetValue(mUI.lblLineHeight, label->GetLineHeight());
        }
        else if (const auto* button = uik::WidgetCast<uik::PushButton>(widget))
        {
            mUI.stackedWidget->setCurrentWidget(mUI.btnPage);
            SetValue(mUI.btnText, button->GetText());
        }
        else if (const auto* chkbox = uik::WidgetCast<uik::CheckBox>(widget))
        {
            mUI.stackedWidget->setCurrentWidget(mUI.chkPage);
            SetValue(mUI.chkText,      chkbox->GetText());
            SetValue(mUI.chkPlacement, chkbox->GetCheckLocation());
            SetValue(mUI.chkCheck,     chkbox->IsChecked());
        }
        else if (const auto* spin = uik::WidgetCast<uik::SpinBox>(widget))
        {
            mUI.stackedWidget->setCurrentWidget(mUI.spinPage);
            SetValue(mUI.spinMin, spin->GetMin());
            SetValue(mUI.spinMax, spin->GetMax());
            SetValue(mUI.spinVal, spin->GetValue());
        }
        else if (const auto* slider = uik::WidgetCast<uik::Slider>(widget))
        {
            mUI.stackedWidget->setCurrentWidget(mUI.sliderPage);
            SetValue(mUI.sliderVal, slider->GetValue());
        }
        else if (const auto* prog = uik::WidgetCast<uik::ProgressBar>(widget))
        {
            mUI.stackedWidget->setCurrentWidget(mUI.progPage);
            const auto val = prog->GetValue();
            if (val.has_value())
                SetValue(mUI.progVal, 100 * val.value());
            else SetValue(mUI.progVal, -1);
            SetValue(mUI.progText, prog->GetText());
        }
        else if (const auto* radio = uik::WidgetCast<uik::RadioButton>(widget))
        {
            mUI.stackedWidget->setCurrentWidget(mUI.rbPage);
            SetValue(mUI.rbText, radio->GetText());
            SetValue(mUI.rbPlacement, radio->GetCheckLocation());
            SetValue(mUI.rbSelected, radio->IsSelected());
        }
        else
        {
            SetEnabled(mUI.widgetData, false);
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
    const auto& form = GetFormSize();
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto camera_center_x = width * 0.5f - form.GetWidth() * 0.5;
    const auto camera_center_y = height * 0.5f - form.GetHeight() * 0.5;

    const auto dist_x = mState.camera_offset_x - camera_center_x;
    const auto dist_y = mState.camera_offset_y - camera_center_y;
    SetValue(mUI.translateX, dist_x);
    SetValue(mUI.translateY, dist_y);
}

void UIWidget::RebuildCombos()
{
    const auto& materials = ListMaterials(mState.workspace);
    mUI.widgetNormal->RebuildMaterialCombos(materials);
    mUI.widgetDisabled->RebuildMaterialCombos(materials);
    mUI.widgetFocused->RebuildMaterialCombos(materials);
    mUI.widgetMoused->RebuildMaterialCombos(materials);
    mUI.widgetPressed->RebuildMaterialCombos(materials);

    SetList(mUI.windowScriptFile, mState.workspace->ListUserDefinedScripts());
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

bool UIWidget::LoadStyleVerbose(const QString& name)
{
    const auto& data = mState.workspace->LoadGameData(app::ToUtf8(name));
    if (!data)
    {
        ERROR("Failed to load style file: '%1'.", name);
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("Failed to load style file.\n%1").arg(name));
        msg.exec();
        return false;
    }

    auto style = std::make_unique<engine::UIStyle>();
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
    mState.painter->SetStyle(mState.style.get());
    mState.painter->DeleteMaterialInstances();
    mState.window.Style(*mState.painter);
    mState.window.SetStyleName(app::ToUtf8(name));
    INFO("Loaded UI style '%1'.", name);
    NOTE("Loaded UI style '%1'.", name);
    return true;
}

bool UIWidget::LoadStyleQuiet(const std::string& uri)
{
    const auto& data = mState.workspace->LoadGameData(uri);
    if (!data)
    {
        ERROR("Failed to load style file: '%1'.", uri);
        return false;
    }
    auto style = std::make_unique<engine::UIStyle>();
    style->SetClassLibrary(mState.workspace);
    if (!style->LoadStyle(*data))
    {
        ERROR("Errors were found while parsing the style '%1'.", uri);
        return false;
    }
    mState.style = std::move(style);
    mState.painter->SetStyle(mState.style.get());
    mState.painter->DeleteMaterialInstances();
    mState.window.Style(*mState.painter);
    mState.window.SetStyleName(uri);
    INFO("Loaded UI style '%1'.", uri);
    return true;
}

void UIWidget::UncheckPlacementActions()
{
    mUI.actionNewForm->setChecked(false);
    mUI.actionNewLabel->setChecked(false);
    mUI.actionNewPushButton->setChecked(false);
    mUI.actionNewCheckBox->setChecked(false);
    mUI.actionNewSpinBox->setChecked(false);
    mUI.actionNewSlider->setChecked(false);
    mUI.actionNewProgressBar->setChecked(false);
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

    if (mState.window.HasScriptFile())
    {
        const auto& script = mState.window.GetScriptFile();
        if (!mState.workspace->IsValidScript(script))
        {
            WARN("Window '%1' script is no longer available.", mState.window.GetName());
            mState.window.ResetScriptFile();
            SetValue(mUI.windowScriptFile, -1);
            SetEnabled(mUI.btnEditScript, false);
        }
    }
}

uik::FSize UIWidget::GetFormSize() const
{
    //return mState.window.FindWidgetByType(uik::Widget::Type::Form)->GetSize();
    return mState.window.GetBoundingRect().GetSize();
}

} // namespace
