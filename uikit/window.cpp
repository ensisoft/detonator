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

#include "config.h"

#include <stack>

#include "base/logging.h"
#include "base/assert.h"
#include "base/utility.h"
#include "base/hash.h"
#include "base/math.h"
#include "base/treeop.h"
#include "data/reader.h"
#include "data/writer.h"
#include "uikit/window.h"
#include "uikit/widget.h"
#include "uikit/painter.h"
#include "uikit/state.h"

namespace {
using namespace uik;
template<typename T>
class HitTestVisitor : public base::RenderTree<Widget>::TVisitor<T> {
public:
    HitTestVisitor(const FPoint& point, bool check_flags)
        : mPoint(point)
        , mCheckFlags(check_flags)
    {
        WidgetState state;
        state.enabled = true;
        state.visible = true;
        mState.push(state);
    }
    virtual void EnterNode(T* widget) override
    {
        if (!widget)
            return;
        const auto state = mState.top();
        const bool visible = state.visible & widget->TestFlag(Widget::Flags::VisibleInGame);
        const bool enabled = state.enabled & widget->TestFlag(Widget::Flags::Enabled);
        if ((mCheckFlags && visible && enabled) || !mCheckFlags)
        {
            FRect rect = widget->GetRect();
            rect.Translate(mWidgetOrigin);
            if (rect.TestPoint(mPoint))
            {
                mHitWidget  = widget;
                mHitPoint   = rect.MapToLocal(mPoint);
                mWidgetRect = rect;
            }
        }
        WidgetState next;
        next.enabled = enabled;
        next.visible = visible;
        mState.push(next);
        mWidgetOrigin += widget->GetPosition();
    }
    virtual void LeaveNode(T* widget) override
    {
        if (!widget)
            return;
        mWidgetOrigin -= widget->GetPosition();
        mState.pop();
    }
    FPoint GetHitPoint() const
    { return mHitPoint; }
    FRect GetWidgetRect() const
    { return mWidgetRect; }
    T* GetHitWidget() const
    { return mHitWidget; }
private:
    const FPoint& mPoint;
    const bool mCheckFlags = false;
    struct WidgetState {
        bool visible = true;
        bool enabled = true;
    };
    std::stack<WidgetState> mState;
    T* mHitWidget = nullptr;
    FPoint mHitPoint;
    FPoint mWidgetOrigin;
    FRect  mWidgetRect;
};


const Widget* hit_test(const RenderTree& tree, const FPoint& point, FPoint* where, FRect* rect, bool flags)
{
    HitTestVisitor<const Widget>visitor(point, flags);
    tree.PreOrderTraverse(visitor);
    if (where)
        *where = visitor.GetHitPoint();
    if (rect)
        *rect = visitor.GetWidgetRect();
    return visitor.GetHitWidget();
}

Widget* hit_test(RenderTree& tree, const FPoint& point, FPoint* where, FRect* rect, bool flags)
{
    HitTestVisitor<Widget>visitor(point, flags);
    tree.PreOrderTraverse(visitor);
    if (where)
        *where = visitor.GetHitPoint();
    if (rect)
        *rect = visitor.GetWidgetRect();
    return visitor.GetHitWidget();
}

} // namespace

namespace uik
{

Window::Window()
{
    mId = base::RandomString(10);
    mFlags.set(Flags::EnableVirtualKeys, false);
    mFlags.set(Flags::WantsKeyEvents, false);
    mFlags.set(Flags::WantsMouseEvents, false);
}

Window::Window(const Window& other)
{
    mId          = other.mId;
    mName        = other.mName;
    mScriptFile  = other.mScriptFile;
    mStyleFile   = other.mStyleFile;
    mStyleString = other.mStyleString;
    mKeyMapFile  = other.mKeyMapFile;
    mFlags       = other.mFlags;

    std::unordered_map<const Widget*, const Widget*> map;

    for (auto& w : other.mWidgets)
    {
        auto copy = w->Copy();
        map[w.get()] = copy.get();
        mWidgets.push_back(std::move(copy));
    }
    mRenderTree.FromTree(other.mRenderTree, [&map](const Widget* widget) {
        return map[widget];
    });
}

Widget* Window::AddWidgetPtr(std::unique_ptr<Widget> widget)
{
    if (widget->CanFocus())
    {
        int max_tab_index = -1;
        for (auto& w : mWidgets)
        {
            if (!w->CanFocus())
                continue;
            max_tab_index = std::max(max_tab_index, (int)w->GetTabIndex());
        }
        widget->SetTabIndex(max_tab_index + 1);
    }

    auto ret = widget.get();
    mWidgets.push_back(std::move(widget));
    return ret;
}

bool Window::CanFocus(const Widget* widget) const
{
    // check whether the widget can gain keyboard focus or not.

    // first check if the widget itself can accept keyboard focus.
    if (!widget->IsEnabled() || !widget->CanFocus() || !widget->IsVisible())
        return false;

    // check if any of the parents are either disabled or invisible.
    std::vector<const Widget*> nodes;
    base::SearchParent(mRenderTree, widget, (const Widget*)nullptr, &nodes);
    for (const auto* node : nodes)
    {
        if (node == nullptr)
            continue;
        if (!node->IsEnabled() || !node->IsVisible())
            return false;
    }

    return true;
}

Widget* Window::DuplicateWidget(const Widget* widget)
{
    std::vector<std::unique_ptr<Widget>> clones;

    unsigned max_tab_index = 0;
    for (auto& w : mWidgets)
    {
        if (!w->CanFocus())
            continue;
        max_tab_index = std::max(max_tab_index, w->GetTabIndex());
    }

    auto* ret = uik::DuplicateWidget(mRenderTree, widget, &clones);
    for (auto& clone : clones)
    {
        if (clone->CanFocus())
            clone->SetTabIndex(++max_tab_index);
        mWidgets.push_back(std::move(clone));
    }
    return ret;
}

void Window::LinkChild(Widget* parent, Widget* child)
{
    mRenderTree.LinkChild(parent, child);
}

void Window::DeleteWidget(const Widget* carcass)
{
    std::unordered_set<const Widget*> graveyard;

    std::vector<Widget*> taborder;
    for (auto& w : mWidgets)
    {
        if (!w->CanFocus())
            continue;
        const auto index = w->GetTabIndex();
        if (index >= taborder.size())
            taborder.resize(index+1);
        taborder[index] = w.get();
    }
    // if there are some holes in the tab order list (really this is a BUG somewhere)
    // remove any such nullptrs from the taborder list.
    taborder.erase(std::remove(taborder.begin(), taborder.end(), nullptr), taborder.end());

    mRenderTree.PreOrderTraverseForEach([&graveyard](const Widget* widget) {
        graveyard.insert(widget);
    }, carcass);

    // delete from the tree.
    mRenderTree.DeleteNode(carcass);

    // delete from the container.
    mWidgets.erase(std::remove_if(mWidgets.begin(), mWidgets.end(), [&graveyard](const auto& node) {
        return graveyard.find(node.get()) != graveyard.end();
    }), mWidgets.end());

    // delete the widgets that were removed from the tab order list.
    taborder.erase(std::remove_if(taborder.begin(), taborder.end(), [&graveyard](const auto* widget) {
        return graveyard.find(widget) != graveyard.end();
    }), taborder.end());

    // reindex the taborder widgets and assign new tab indices.
    for (unsigned i=0; i<taborder.size(); ++i)
    {
        taborder[i]->SetTabIndex(i);
    }
}

Widget* Window::HitTest(const FPoint& window, FPoint* widget, bool flags)
{
    return hit_test(mRenderTree, window, widget, nullptr, flags);
}

const Widget* Window::HitTest(const FPoint& window, FPoint* widget, bool flags)const
{
    return hit_test(mRenderTree, window, widget, nullptr, flags);
}

void Window::Paint(TransientState& state, Painter& painter, double time, PaintHook* hook) const
{
    // the pre-order traversal and painting is simple and leads to a
    // correct transformation hierarchy in terms of the relative
    // positions of widgets when they're being contained inside other
    // widgets.
    // The problem however is whether a container will cover non-container
    // widgets or not. For example if we have 2 widgets, a button and
    // a group box, the group box may or may not end up obscuring the button
    // depending on what is their relative order in the children array.
    //
    // A possible way to solve this could be to use a breadth first traversal
    // of the widget tree or use a depth first traversal but with buffering
    // of paint widgets and then sorting the paint events to the appropriate
    // order. This would also be solved in the hit testing in order to make
    // sure that the widget on top obscures the widget below from getting
    // the hits.
    //
    // However, for example Qt seems to have similar issue. If one adds a
    // container widget such as a TabWidget first followed by a button
    // the button can render on top of the container even when not inside
    // the tab widget.
    //

    painter.BeginDrawWidgets();

    class PaintVisitor : public ConstVisitor {
    public:
        PaintVisitor(const Window& w, double time, Painter& p, TransientState& s, PaintHook* h)
          : mWindow(w)
          , mCurrentTime(time)
          , mPainter(p)
          , mWidgetState(s)
          , mPaintHook(h)
        {
            PaintState ps;
            ps.enabled = true;
            ps.visible = true;
            mState.push(ps);
            mWidgetState.GetValue(w.mId + "/focused-widget", &mFocusedWidget);
            mWidgetState.GetValue(w.mId + "/widget-under-mouse", &mWidgetUnderMouse);
        }
        virtual void EnterNode(const Widget* widget) override
        {
            if (!widget)
                return;

            const auto state = mState.top();
            const bool visible = state.visible && widget->TestFlag(Widget::Flags::VisibleInGame) &&
                                                  widget->TestFlag(Widget::Flags::VisibleInEditor);
            const bool enabled = state.enabled && widget->TestFlag(Widget::Flags::Enabled);

            FRect widget_rect = widget->GetRect();
            widget_rect.Translate(mParentWidgetOrigin);

            if (visible)
            {
                //mPainter.RealizeMask();

                Widget::PaintEvent paint;
                paint.clip    = state.clip;
                paint.rect    = widget_rect;
                paint.focused = (widget == mFocusedWidget);
                paint.moused  = (widget == mWidgetUnderMouse);
                paint.enabled = enabled;
                paint.time    = mCurrentTime;
                if (mPaintHook)
                {
                    mPaintHook->BeginPaintWidget(widget, mWidgetState, paint, mPainter);
                    if (mPaintHook->InspectPaint(widget, mWidgetState, paint))
                    {
                        widget->Paint(paint , mWidgetState , mPainter);
                    }
                    mPaintHook->EndPaintWidget(widget, mWidgetState, paint, mPainter);
                }
                else
                {
                    widget->Paint(paint , mWidgetState , mPainter);
                }
                if (widget->IsContainer() && widget->ClipChildren() &&
                    mWindow.mRenderTree.HasChildren(widget))
                {
                    Painter::MaskStruct mask;
                    mask.id    = widget->GetId();
                    mask.klass = widget->GetClassName();
                    mask.name  = widget->GetName();
                    mask.rect  = widget_rect;

                    // Push the current widget's clipping mask onto the painter's clip stack.
                    // The clipping mask is added to the previous masks and is then used
                    // to clip the child widgets (during subsequent draw operations)
                    mPainter.PushMask(mask);
                }
            }

            PaintState ps;
            ps.enabled = enabled;
            ps.visible = visible;
            ps.clip    = widget_rect;
            mState.push(ps);

            mParentWidgetOrigin += widget->GetPosition();
        }
        virtual void LeaveNode(const Widget* widget) override
        {
            if (!widget)
                return;

            mParentWidgetOrigin -= widget->GetPosition();
            mState.pop();

            const auto state = mState.top();
            const bool visible = state.visible && widget->TestFlag(Widget::Flags::VisibleInGame) &&
                                                  widget->TestFlag(Widget::Flags::VisibleInEditor);
            const bool enabled = state.enabled && widget->TestFlag(Widget::Flags::Enabled);

            if (visible)
            {
                if (widget->IsContainer() && widget->ClipChildren() &&
                    mWindow.mRenderTree.HasChildren(widget))
                    mPainter.PopMask();
            }
        }
    private:
        const Widget* mFocusedWidget = nullptr;
        const Widget* mWidgetUnderMouse = nullptr;
        const Window& mWindow;
        const double mCurrentTime = 0.0;
        Painter& mPainter;
        TransientState& mWidgetState;
        PaintHook* mPaintHook = nullptr;
        struct PaintState {
            FRect clip;
            bool  visible = true;
            bool  enabled = true;
        };
        std::stack<PaintState> mState;
        FPoint mParentWidgetOrigin;
    };

    PaintVisitor visitor(*this, time, painter, state, hook);
    mRenderTree.PreOrderTraverse(visitor);

    painter.EndDrawWidgets();
}

void Window::Open(TransientState& state, AnimationStateArray* animations)
{
    if (animations)
    {
        animations->clear();

        // parse the animation strings if any.
        for (auto& widget: mWidgets)
        {
            const auto& str = widget->GetAnimationString();
            if (str.empty())
                continue;

            std::vector<Animation> widget_animations;
            if (!ParseAnimations(str, &widget_animations))
                WARN("UI widget animation string has errors. [window='%1', widget='%2']", mName, widget->GetName());

            for (auto& widget_animation : widget_animations)
            {
                widget_animation.widget = widget.get();
                animations->push_back(std::move(widget_animation));
            }
        }

        // start animations that trigger on $OnOpen
        for (auto& anim : *animations)
        {
            if (anim.trigger != Animation::Trigger::Open)
                continue;

            anim.state = Animation::State::Active;
            anim.time  = -anim.delay;
        }
    }

    if (!mFlags.test(Flags::EnableVirtualKeys))
        return;

    // find the first keyboard focusable widget if any.
    Widget* widget = nullptr;

    for (auto& w : mWidgets)
    {
        if (!CanFocus(w.get()))
            continue;

        if (!widget || w->GetTabIndex() < widget->GetTabIndex())
            widget = w.get();
    }
    if (!widget)
        return;

    state.SetValue(mId + "/focused-widget", widget);
}

void Window::Close(TransientState& state, AnimationStateArray* animations)
{
    if (state.HasValue(mId + "/closing"))
        return;

    if (animations)
    {
        // start animations that trigger on $OnClose
        for (auto& anim : *animations)
        {
            if (anim.trigger != Animation::Trigger::Close)
                continue;

            anim.state = Animation::State::Active;
            anim.time  = -anim.delay;
        }
    }
    state.SetValue(mId + "/closing", true);
}

bool Window::IsClosed(const TransientState& state, const AnimationStateArray* animations) const
{
    bool closing = false;
    state.GetValue(mId + "/closing", &closing);
    if (!closing)
        return false;

    if (!animations)
        return true;

    for (const auto& anim : *animations)
    {
        if (anim.trigger == Animation::Trigger::Close)
        {
            if (anim.state == Animation::State::Active)
                return false;
        }
    }
    return true;
}

bool Window::IsClosing(const TransientState& state) const
{
    bool closing = false;
    state.GetValue(mId + "/closing", &closing);
    return closing;
}

void Window::Update(TransientState& state, double time, float dt, AnimationStateArray* animations)
{
    for (auto& widget : mWidgets)
        widget->Update(state, time, dt);

    if (!animations)
        return;

    for (auto& anim : *animations)
    {
        if (anim.state == Animation::State::Inactive)
            continue;

        const auto prev_time = anim.time;

        anim.time += dt;

        auto* widget = anim.widget;

        // read the starting state once when the animation time goes above zero.
        if (prev_time <= 0.0f && anim.time > 0.0f)
        {
            // for each animation action read the current starting state that
            // is required for the widget state interpolation. note that
            // currently we can't interpolate style properties such as Color
            // because the starting color is not known here unless it's
            // specified explicitly in the animation string (or in the widget's
            // style string..)
            for (auto& action : anim.actions)
            {
                // in case the animation is looping we skip reading the initial
                // state on subsequent animation loop iterations.
                if (std::holds_alternative<std::monostate>(action.start_value))
                {
                    if (action.type == Animation::Action::Type::Move)
                        action.start_value = widget->GetPosition();
                    else if (action.type == Animation::Action::Type::Resize)
                        action.start_value = widget->GetSize();
                    else if (action.type == Animation::Action::Type::Translate)
                    {
                        action.start_value = widget->GetPosition();
                        action.end_value   = widget->GetPosition() + std::get<uik::FPoint>(action.end_value);
                    }
                    else if (action.type == Animation::Action::Type::Grow)
                    {
                        action.start_value = widget->GetSize();
                        action.end_value   = widget->GetSize() + std::get<uik::FSize>(action.end_value);
                    }
                }
            }
        }
        if (anim.time <= 0.0f)
            continue;

        const float t = math::clamp(0.0f, anim.duration, anim.time) / anim.duration;
        for (auto& action : anim.actions)
        {
            if (action.type == Animation::Action::Type::Resize || action.type ==Animation::Action::Type::Grow)
            {
                ASSERT(std::holds_alternative<uik::FSize>(action.start_value));
                ASSERT(std::holds_alternative<uik::FSize>(action.end_value));

                const auto start_value = std::get<uik::FSize>(action.start_value);
                const auto end_value   = std::get<uik::FSize>(action.end_value);
                const auto value       = math::interpolate(start_value, end_value, t, anim.interpolation);
                widget->SetSize(value);
            }
            else if (action.type == Animation::Action::Type::Move || action.type == Animation::Action::Type::Translate)
            {
                ASSERT(std::holds_alternative<uik::FPoint>(action.start_value));
                ASSERT(std::holds_alternative<uik::FPoint>(action.end_value));

                const auto start_value = std::get<uik::FPoint>(action.start_value);
                const auto end_value   = std::get<uik::FPoint>(action.end_value);
                const auto value       = math::interpolate(start_value, end_value, t, anim.interpolation);
                widget->SetPosition(value);
            }
            else if (action.type == Animation::Action::Type::SetProp)
            {
                if (t >= 0.5f)
                {
                    ASSERT(std::holds_alternative<StyleProperty>(action.end_value));

                    const auto& style_property = std::get<StyleProperty>(action.end_value);
                    widget->SetStyleProperty(action.name, style_property);
                }
            }
            else if (action.type == Animation::Action::Type::DelProp)
            {
                if (t >= 0.5f)
                {
                    widget->DeleteStyleProperty(action.name);
                }
            }
            else if (action.type == Animation::Action::Type::DelMaterial)
            {
                if (t >= 0.5f)
                {
                    widget->DeleteStyleMaterial(action.name);
                }
            }
            else if (action.type == Animation::Action::Type::SetFlag)
            {
                if (t >= 0.5f)
                {
                    const auto on_off = std::get<bool>(action.end_value);
                    if (action.name == "Enabled")
                        widget->SetFlag(Widget::Flags::Enabled, on_off);
                    else if (action.name == "Visible")
                        widget->SetFlag(Widget::Flags::VisibleInGame, on_off);
                    else WARN("Unknown widget flag in widget animation. [widget='%1', flag='%2']", widget->GetName(), action.name);
                }
            }
            else BUG("Unhandled widget animation action.");
        }
        if (anim.time >= anim.duration)
        {
            anim.state = Animation::State::Inactive;
            if (anim.loops == std::numeric_limits<unsigned>::max())
            {
                anim.time  = -anim.delay;
                anim.state = Animation::State::Active;
            }
            else if (anim.loops > 0)
            {
                if (--anim.loops > 0)
                {
                    anim.time = -anim.delay;
                    anim.state = Animation::State::Active;
                }
            }
        }
    }
}

void Window::Style(Painter& painter) const
{
    if (!mStyleString.empty())
        painter.ParseStyle("window", mStyleString);

    for (const auto& widget : mWidgets)
    {
        const auto& style = widget->GetStyleString();
        if (style.empty())
            continue;
        painter.ParseStyle(widget->GetId(), style);
    }
}

FRect Window::FindWidgetRect(const Widget* widget) const
{
    class PrivateVisitor : public ConstVisitor {
    public:
        PrivateVisitor(const Widget* widget) : mWidget(widget)
        {}
        virtual void EnterNode(const Widget* widget) override
        {
            if (widget == nullptr)
                return;

            if (mWidget == widget)
            {
                FRect rect = widget->GetRect();
                rect.Translate(mWidgetOrigin);
                mRect = rect;
                mDone = true;
                return;
            }
            mWidgetOrigin += widget->GetPosition();
        }
        virtual void LeaveNode(const Widget* widget) override
        {
            if (widget == nullptr)
                return;
            mWidgetOrigin -= widget->GetPosition();
        }
        virtual bool IsDone() const override
        { return mDone; }
        FRect GetRect() const
        { return mRect; }
    private:
        const Widget* mWidget = nullptr;
        FRect mRect;
        FPoint mWidgetOrigin;
        bool mDone = false;
    };
    PrivateVisitor visitor(widget);
    mRenderTree.PreOrderTraverse(visitor);
    return visitor.GetRect();
}

FRect Window::GetBoundingRect() const
{
    // if there's a top level form (i.e. a widget of type Form and without a parent)
    // we assume the widget's bounding rect is the same as the form's
    Widget* top_level_form = nullptr;

    for (auto& widget : mWidgets)
    {
        if (widget->GetType() != Widget::Type::Form)
            continue;
        else if (mRenderTree.GetParent(widget.get()))
            continue;

        if (top_level_form)
        {
            top_level_form = nullptr;
            break;
        }
        else top_level_form = widget.get();
    }

    if (top_level_form)
        return top_level_form->GetRect();

    // combine the bounding rect from the union of all widgets' rects

    class PrivateVisitor : public ConstVisitor {
    public:
        virtual void EnterNode(const Widget* widget) override
        {
            if (widget == nullptr)
                return;

            FRect rect = widget->GetRect();
            rect.Translate(mWidgetOrigin);
            mRect = Union(mRect, rect);
            mWidgetOrigin += widget->GetPosition();
        }
        virtual void LeaveNode(const Widget* widget) override
        {
            if (widget == nullptr)
                return;
            mWidgetOrigin -= widget->GetPosition();
        }
        FRect GetRect() const
        { return mRect; }
    private:
        const Widget* mWidget = nullptr;
        FRect mRect;
        FPoint mWidgetOrigin;
    };
    PrivateVisitor visitor;
    mRenderTree.PreOrderTraverse(visitor);
    return visitor.GetRect();
}

void Window::IntoJson(data::Writer& data) const
{
    data.Write("id", mId);
    data.Write("name", mName);
    data.Write("script_file", mScriptFile);
    data.Write("style_file", mStyleFile);
    data.Write("style_string", mStyleString);
    data.Write("keymap_file", mKeyMapFile);
    data.Write("flags", mFlags);
    RenderTreeIntoJson(mRenderTree, data, nullptr);
}

std::vector<Window::WidgetAction> Window::PollAction(TransientState& state, double time, float dt)
{
    std::vector<WidgetAction> actions;
    for (auto& widget : mWidgets)
    {
        const auto& ret = widget->PollAction(state, time, dt);
        if (ret.type == WidgetActionType::None)
            continue;

        WidgetAction action;
        action.id    = widget->GetId();
        action.name  = widget->GetName();
        action.type  = ret.type;
        action.value = ret.value;
        actions.push_back(std::move(action));

        if (ret.type == WidgetActionType::ValueChange && widget->GetType() == Widget::Type::RadioButton)
        {
            // implement radio button exclusivity by finding all sibling radio
            // buttons under the same parent and then toggling on only the one
            // that generated the value changed action.
            std::vector<Widget*> siblings;
            base::ListSiblings<Widget>(mRenderTree, widget.get(), &siblings);
            for (auto* widget : siblings)
            {
                if (auto* radio_btn = WidgetCast<RadioButton>(widget))
                {
                    radio_btn->SetSelected(false);
                }
            }
            const auto* parent = mRenderTree.HasParent(widget.get()) ?
                                 mRenderTree.GetParent(widget.get()) : nullptr;
            // generate a radio button selection event on the parent widget.
            if (parent)
            {
                WidgetAction action;
                action.id    = parent->GetId();
                action.name  = parent->GetName();
                action.value = widget->GetName();
                action.type  = WidgetActionType::RadioButtonSelect;
                actions.push_back(action);
            }
        }
    }
    return actions;
}

std::vector<Window::WidgetAction> Window::MousePress(const MouseEvent& mouse, TransientState& state)
{
    return send_mouse_event(mouse, &Widget::MousePress, state, true);
}
std::vector<Window::WidgetAction> Window::MouseRelease(const MouseEvent& mouse, TransientState& state)
{
    return send_mouse_event(mouse, &Widget::MouseRelease, state, false);
}
std::vector<Window::WidgetAction> Window::MouseMove(const MouseEvent& mouse, TransientState& state)
{
    return send_mouse_event(mouse, &Widget::MouseMove, state, false);
}

std::vector<Window::WidgetAction> Window::KeyDown(const KeyEvent& key, TransientState& state)
{
    Widget* focused_widget = nullptr;
    state.GetValue(mId + "/focused-widget", &focused_widget);

    std::vector<Window::WidgetAction> ret;

    // todo: when using keyboard navigation only consider a single radio-button
    // per container.
    if (key.key == VirtualKey::FocusNext || key.key == VirtualKey::FocusPrev)
    {
        std::vector<Widget*> taborder;
        for (auto& w : mWidgets)
        {
            if (!CanFocus(w.get()))
                continue;

            const auto tabindex = w->GetTabIndex();
            if (tabindex >= taborder.size())
                taborder.resize(tabindex+1);
            taborder[tabindex] = w.get();
        }
        taborder.erase(std::remove(taborder.begin(), taborder.end(), nullptr), taborder.end());
        if (taborder.empty())
            return {};

        Widget* next_widget = nullptr;

        if (focused_widget)
        {
            int index = 0;
            for (index=0; index<taborder.size(); ++index)
            {
                if (taborder[index] == focused_widget)
                    break;
            }
            if (key.key == VirtualKey::FocusNext)
            {
                index = math::wrap(0, (int)taborder.size()-1, ++index);
            }
            else
            {
                index = math::wrap(0, (int)taborder.size()-1, --index);
            }
            next_widget = taborder[index];
        }
        else
        {
            next_widget = taborder[0];
        }
        state.SetValue(mId + "/focused-widget", next_widget);

        if (focused_widget && (focused_widget != next_widget))
        {
            WidgetAction action;
            action.type  = WidgetActionType::FocusChange;
            action.value = false;
            action.name  = focused_widget->GetName();
            action.id    = focused_widget->GetId();
            ret.push_back(action);
        }
        if (next_widget && (focused_widget != next_widget))
        {
            WidgetAction action;
            action.type  = WidgetActionType::FocusChange;
            action.value = true;
            action.name  = next_widget->GetName();
            action.id    = next_widget->GetName();
            ret.push_back(action);
        }
    }
    else if (focused_widget)
    {
        if (focused_widget->GetType() == Widget::Type::RadioButton &&
            (key.key == VirtualKey::MoveDown || key.key == VirtualKey::MoveUp))
        {
            // implement radio button audio-exclusivity by looking for sibling
            // radio buttons under the same parent and then setting the appropriate
            // state on the siblings when up/down virtual key is handled.

            auto* parent = mRenderTree.HasParent(focused_widget) ?
                           mRenderTree.GetParent(focused_widget) : nullptr;

            // list all widgets under the same parent as the current focused widget.
            std::vector<Widget*> children;
            base::ListChildren(mRenderTree, parent, &children);

            // remove siblings that aren't radio buttons or are radio buttons but cannot
            // be selected, (i.e. are not visible or enabled)
            children.erase(std::remove_if(children.begin(), children.end(), [](Widget* w) {
                if (w->GetType() != Widget::Type::RadioButton)
                    return true;
                if (!w->IsEnabled() || !w->IsVisible())
                    return true;
                return false;
            }), children.end());

            std::sort(children.begin(), children.end(), [](const auto* lhs, const auto* rhs) {
                return lhs->GetTabIndex() < rhs->GetTabIndex();
            });

            // look for the currently selected radio button.
            unsigned radio_button_index = 0;
            for (radio_button_index=0; radio_button_index<children.size(); ++radio_button_index)
            {
                if (WidgetCast<RadioButton>(children[radio_button_index])->IsSelected())
                    break;
            }
            // the initial condition of radio buttons (in the editor) don't actually
            // specify any unique selection between the group of buttons. thus
            // it's possible to have a situation when not a single button has been
            // yet selected.
            if (radio_button_index == children.size())
                radio_button_index = 0;

            if (key.key == VirtualKey::MoveUp)
            {
                if (radio_button_index == 0)
                    return {};
                --radio_button_index;
            }
            else if (key.key == VirtualKey::MoveDown)
            {
                if (radio_button_index == children.size()-1)
                    return {};
                ++radio_button_index;
            }
            for (auto* btn : children)
            {
                WidgetCast<RadioButton>(btn)->SetSelected(false);
            }
            WidgetCast<RadioButton>(children[radio_button_index])->SetSelected(true);

            if (parent)
            {
                WidgetAction action;
                action.id    = parent->GetId();
                action.name  = parent->GetName();
                action.value = parent->GetName();
                action.type  = WidgetActionType::RadioButtonSelect;
                ret.push_back(action);
            }
        }
        else
        {
            const auto& key_ret = focused_widget->KeyDown(key, state);
            if (key_ret.type != WidgetActionType::None)
            {
                WidgetAction action;
                action.id    = focused_widget->GetId();
                action.name  = focused_widget->GetName();
                action.type  = key_ret.type;
                action.value = key_ret.value;
                ret.push_back(action);
            }
        }
    }
    return ret;
}

std::vector<Window::WidgetAction> Window::KeyUp(const KeyEvent& key, TransientState& state)
{
    Widget* focused_widget = nullptr;
    if (state.GetValue(mId + "/focused-widget", &focused_widget))
    {
        const auto& ret = focused_widget->KeyUp(key, state);
        if (ret.type == WidgetActionType::None)
            return {};

        WidgetAction action;
        action.id    = focused_widget->GetId();
        action.name  = focused_widget->GetName();
        action.type  = ret.type;
        action.value = ret.value;
        return {action};
    }
    return  {};
}

void Window::TriggerAnimations(const std::vector<WidgetAction>& actions, TransientState& state,
                               AnimationStateArray& animations)
{
    // for each action see if there's an animation (identified by the widget ID)
    // that is wired to trigger on the action from the widget.
    // only consider currently inactive animations.

    for (const auto& action : actions)
    {
        for (auto& anim : animations)
        {
            auto* widget = anim.widget;

            if (widget->GetId() != action.id)
                continue;
            else if (anim.state == Animation::State::Active)
                continue;

            bool should_trigger = false;

            if (action.type == WidgetActionType::FocusChange &&
                (anim.trigger == Animation::Trigger::LostFocus ||
                 anim.trigger == Animation::Trigger::GainFocus))
            {
                const auto has_focus = std::get<bool>(action.value);
                const auto trigger_gain_focus = anim.trigger == Animation::Trigger::GainFocus && has_focus;
                const auto trigger_lost_focus = anim.trigger == Animation::Trigger::LostFocus && !has_focus;
                if (trigger_gain_focus || trigger_gain_focus)
                {
                    should_trigger = true;
                }
            }
            else if (action.type == WidgetActionType::ButtonPress &&
                     anim.trigger == Animation::Trigger::Click)
            {
                if (widget->GetType() == Widget::Type::PushButton)
                {
                    should_trigger = true;
                }
            }
            else if (action.type == WidgetActionType::ValueChange &&
                     anim.trigger == Animation::Trigger::ValueChange)
                should_trigger = true;
            else if (action.type == WidgetActionType::MouseEnter &&
                     anim.trigger == Animation::Trigger::MouseEnter)
                should_trigger = true;
            else if(action.type == WidgetActionType::MouseLeave &&
                    anim.trigger == Animation::Trigger::MouseLeave)
                should_trigger = true;

            if (!should_trigger)
                continue;

            anim.time  = -anim.delay;
            anim.state = Animation::State::Active;
        }
    }
}

Window Window::Clone() const
{
    Window copy(*this);
    copy.mId = base::RandomString(10);
    return copy;
}

void Window::ClearWidgets()
{
    mRenderTree.Clear();
    mWidgets.clear();
}

const Widget* Window::GetFocusedWidget(const TransientState& state) const
{
    const Widget* widget = nullptr;
    state.GetValue(mId + "/focused-widget", &widget);
    return widget;
}


size_t Window::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mScriptFile);
    hash = base::hash_combine(hash, mStyleFile);
    hash = base::hash_combine(hash, mStyleString);
    hash = base::hash_combine(hash, mKeyMapFile);
    hash = base::hash_combine(hash, mFlags);
    mRenderTree.PreOrderTraverseForEach([&hash](const Widget* widget) {
        if (widget == nullptr)
            return;
        hash = base::hash_combine(hash, widget->GetHash());
    });
    return hash;
}

Window& Window::operator=(const Window& other)
{
    if (this == &other)
        return *this;

    Window copy(other);
    std::swap(mId,    copy.mId);
    std::swap(mName,  copy.mName);
    std::swap(mScriptFile, copy.mScriptFile);
    std::swap(mStyleFile, copy.mStyleFile);
    std::swap(mStyleString, copy.mStyleString);
    std::swap(mKeyMapFile, copy.mKeyMapFile);
    std::swap(mWidgets, copy.mWidgets);
    std::swap(mRenderTree, copy.mRenderTree);
    std::swap(mFlags, copy.mFlags);
    return *this;
}

bool Window::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",           &mId);
    ok &= data.Read("name",         &mName);
    ok &= data.Read("script_file",  &mScriptFile);
    ok &= data.Read("style_string", &mStyleString);
    ok &= data.Read("keymap_file",  &mKeyMapFile);
    ok &= data.Read("flags",        &mFlags);
    if (!data.Read("style_file", &mStyleFile))
        ok &= data.Read("style", &mStyleFile); // old version before style_string and style_file

    if (!data.GetNumChunks("widgets"))
        return ok;

    const auto& chunk = data.GetReadChunk("widgets", 0);
    if (!chunk)
        return ok;
    ok &= RenderTreeFromJson(*chunk, mRenderTree, mWidgets);
    return ok;
}

std::vector<Window::WidgetAction> Window::send_mouse_event(const MouseEvent& mouse, MouseHandler which, TransientState& state, bool is_mouse_press)
{
    std::vector<Window::WidgetAction> ret;

    // only consider widgets with the appropriate flags
    // i.e. enabled and visible.
    const bool consider_flags = true;
    Widget* old_widget_under_mouse = nullptr;
    state.GetValue(mId + "/widget-under-mouse", &old_widget_under_mouse);

    FPoint widget_pos;
    FRect widget_rect;
    Widget* new_widget_under_mouse = hit_test(mRenderTree, mouse.window_mouse_pos , &widget_pos, &widget_rect, consider_flags);
    if (new_widget_under_mouse != old_widget_under_mouse)
    {
        if (old_widget_under_mouse)
        {
            old_widget_under_mouse->MouseLeave(state);

            WidgetAction mouse_leave;
            mouse_leave.type = WidgetActionType::MouseLeave;
            mouse_leave.name = old_widget_under_mouse->GetName();
            mouse_leave.id   = old_widget_under_mouse->GetId();
            ret.push_back(mouse_leave);
        }
        if (new_widget_under_mouse)
        {
            new_widget_under_mouse->MouseEnter(state);

            WidgetAction mouse_enter;
            mouse_enter.type = WidgetActionType::MouseEnter;
            mouse_enter.name = new_widget_under_mouse->GetName();
            mouse_enter.id   = new_widget_under_mouse->GetId();
            ret.push_back(mouse_enter);
        }
    }
    state.SetValue(mId + "/widget-under-mouse", new_widget_under_mouse);

    if (new_widget_under_mouse && new_widget_under_mouse->CanFocus() && mFlags.test(Flags::EnableVirtualKeys) && is_mouse_press)
    {
        Widget* focused_widget = nullptr;
        state.GetValue(mId + "/focused-widget", &focused_widget);
        state.SetValue(mId + "/focused-widget", new_widget_under_mouse);
        if (focused_widget != new_widget_under_mouse)
        {
            WidgetAction focus_loss;
            focus_loss.type  = WidgetActionType::FocusChange;
            focus_loss.value = false;
            focus_loss.name  = focused_widget->GetName();
            focus_loss.id    = focused_widget->GetId();
            ret.push_back(focus_loss);

            WidgetAction focus_gain;
            focus_gain.type  = WidgetActionType::FocusChange;
            focus_gain.value = true;
            focus_gain.name  = new_widget_under_mouse->GetName();
            focus_gain.id    = new_widget_under_mouse->GetId();
            ret.push_back(focus_gain);
        }
    }

    if (new_widget_under_mouse == nullptr)
        return ret;

    Widget::MouseEvent widget_mouse_event;
    widget_mouse_event.widget_mouse_pos = widget_pos;
    widget_mouse_event.window_mouse_pos = mouse.window_mouse_pos;
    widget_mouse_event.native_mouse_pos = mouse.native_mouse_pos;
    widget_mouse_event.widget_window_rect = widget_rect;
    widget_mouse_event.button = mouse.button;
    const auto& mouse_ret = (new_widget_under_mouse->*which)(widget_mouse_event, state);
    if (mouse_ret.type == WidgetActionType::None)
        return ret;

    WidgetAction action;
    action.id    = new_widget_under_mouse->GetId();
    action.name  = new_widget_under_mouse->GetName();
    action.type  = mouse_ret.type;
    action.value = mouse_ret.value;
    ret.push_back(action);
    return ret;
}

} // namespace
