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

#include "base/assert.h"
#include "base/utility.h"
#include "base/hash.h"
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
            rect.Translate(widget->GetPosition());
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
}

Window::Window(const Window& other)
{
    mId    = other.mId;
    mName  = other.mName;
    mStyle = other.mStyle;

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

Widget* Window::AddWidget(std::unique_ptr<Widget> widget)
{
    auto ret = widget.get();
    mWidgets.push_back(std::move(widget));
    return ret;
}

Widget* Window::DuplicateWidget(const Widget* widget)
{
    std::vector<std::unique_ptr<Widget>> clones;

    auto* ret = uik::DuplicateWidget(mRenderTree, widget, &clones);
    for (auto& clone : clones)
        mWidgets.push_back(std::move(clone));
    return ret;
}

void Window::LinkChild(Widget* parent, Widget* child)
{
    mRenderTree.LinkChild(parent, child);
}

void Window::DeleteWidget(const Widget* carcass)
{
    std::unordered_set<const Widget*> graveyard;

    mRenderTree.PreOrderTraverseForEach([&graveyard](const Widget* widget) {
        graveyard.insert(widget);
    }, carcass);

    // delete from the tree.
    mRenderTree.DeleteNode(carcass);

    // delete from the container.
    mWidgets.erase(std::remove_if(mWidgets.begin(), mWidgets.end(), [&graveyard](const auto& node) {
        return graveyard.find(node.get()) != graveyard.end();
    }), mWidgets.end());
}

Widget* Window::HitTest(const FPoint& window, FPoint* widget, bool flags)
{
    return hit_test(mRenderTree, window, widget, nullptr, flags);
}

const Widget* Window::HitTest(const FPoint& window, FPoint* widget, bool flags)const
{
    return hit_test(mRenderTree, window, widget, nullptr, flags);
}

void Window::Paint(State& state, Painter& painter, double time, PaintHook* hook) const
{
    // the pre-order traversal and painting is simple and leads to a
    // correct transformation hierarchy in terms of the relative
    // positions of widgets when they're being contained inside other
    // widgets.
    // The problem however is whether a container will cover non-container
    // widgets or not. For example if we have a 2 widgets, a button and
    // a group box, the group box may or may not end up obscuring the button
    // depending what is their relative order in the children array.
    //
    // A possible way to solve this could be to use a breadth first traversal
    // of the widget tree or use a depth first traversal but with buffering
    // of paint widgets and then sorting the paint events to the appropriate
    // order. This would also be solved in the hit testing in order to make
    // sure that the widget on top obscures the widget below from getting
    // the hits.
    //
    // However for example Qt seems to have similar issue. If one adds a
    // container widget such as a TabWidget first followed by a button
    // the button can render on top of the container even when not inside
    // the tab widget.
    //
    //
    class PaintVisitor : public ConstVisitor {
    public:
        PaintVisitor(const Window& w, double time, Painter& p, State& s, PaintHook* h)
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
            const bool visible = state.visible & widget->TestFlag(Widget::Flags::VisibleInGame);
            const bool enabled = state.enabled & widget->TestFlag(Widget::Flags::Enabled);
            if (visible)
            {
                FRect rect = widget->GetRect();
                rect.Translate(widget->GetPosition());
                rect.Translate(mWidgetOrigin);

                Widget::PaintEvent paint;
                paint.clip    = state.clip;
                paint.rect    = rect;
                paint.focused = (widget == mFocusedWidget);
                paint.moused  = (widget == mWidgetUnderMouse);
                paint.enabled = enabled;
                paint.time    = mCurrentTime;
                if (mPaintHook)
                {
                    if (mPaintHook->InspectPaint(widget, paint, mWidgetState))
                    {
                        mPaintHook->BeginPaintWidget(widget, mWidgetState, mPainter);
                        widget->Paint(paint , mWidgetState , mPainter);
                        mPaintHook->EndPaintWidget(widget, mWidgetState, mPainter);
                    }
                }
                else
                {
                    widget->Paint(paint , mWidgetState , mPainter);
                }
            }

            PaintState ps;
            ps.enabled = enabled;
            ps.visible = visible;
            ps.clip    = FRect(mWidgetOrigin + widget->GetPosition(), widget->GetSize());

            mState.push(ps);
            mWidgetOrigin += widget->GetPosition();
        }
        virtual void LeaveNode(const Widget* widget) override
        {
            if (!widget)
                return;

            mWidgetOrigin -= widget->GetPosition();
            mState.pop();
        }
    private:
        const Widget* mFocusedWidget = nullptr;
        const Widget* mWidgetUnderMouse = nullptr;
        const Window& mWindow;
        const double mCurrentTime = 0.0;
        Painter& mPainter;
        State& mWidgetState;
        PaintHook* mPaintHook = nullptr;
        struct PaintState {
            FRect clip;
            bool  visible = true;
            bool  enabled = true;
        };
        std::stack<PaintState> mState;
        FPoint mWidgetOrigin;
    };

    PaintVisitor visitor(*this, time, painter, state, hook);
    mRenderTree.PreOrderTraverse(visitor);
}

void Window::Update(State& state, double time, float dt)
{
    for (auto& widget : mWidgets)
        widget->Update(state, time, dt);
}

void Window::Style(Painter& painter) const
{
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
                rect.Translate(widget->GetPosition());
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
    class PrivateVisitor : public ConstVisitor {
    public:
        virtual void EnterNode(const Widget* widget) override
        {
            if (widget == nullptr)
                return;

            FRect rect = widget->GetRect();
            rect.Translate(widget->GetPosition());
            rect.Translate(mWidgetOrigin);
            if (mRect.IsEmpty())
                mRect = rect;
            else mRect = Union(mRect, rect);

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
    data.Write("style", mStyle);
    RenderTreeIntoJson(mRenderTree, data, nullptr);
}

Window::WidgetAction Window::PollAction(State& state, double time, float dt)
{
    Widget* active_widget = nullptr;
    state.GetValue(mId + "/active-widget", &active_widget);
    if (!active_widget)
        return WidgetAction {};

    const auto& ret = active_widget->PollAction(state, time, dt);
    if (ret.type == WidgetActionType::None)
        return WidgetAction {};

    WidgetAction action;
    action.id    = active_widget->GetId();
    action.name  = active_widget->GetName();
    action.type  = ret.type;
    action.value = ret.value;
    return action;
}

Window::WidgetAction Window::MousePress(const MouseEvent& mouse, State& state)
{
    return send_mouse_event(mouse, &Widget::MousePress, state);
}
Window::WidgetAction Window::MouseRelease(const MouseEvent& mouse, State& state)
{
    return send_mouse_event(mouse, &Widget::MouseRelease, state);
}
Window::WidgetAction Window::MouseMove(const MouseEvent& mouse, State& state)
{
    return send_mouse_event(mouse, &Widget::MouseMove, state);
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

size_t Window::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mStyle);
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
    std::swap(mStyle, copy.mStyle);
    std::swap(mWidgets, copy.mWidgets);
    std::swap(mRenderTree, copy.mRenderTree);
    return *this;
}

// static
std::optional<Window> Window::FromJson(const data::Reader& data)
{
    Window ret;
    if (!data.Read("id",   &ret.mId) ||
        !data.Read("name", &ret.mName) ||
        !data.Read("style", &ret.mStyle))
        return std::nullopt;

    if (!data.GetNumChunks("widgets"))
        return ret;

    const auto& chunk = data.GetReadChunk("widgets", 0);
    if (!chunk)
        return ret;
    if (!RenderTreeFromJson(*chunk, ret.mRenderTree, ret.mWidgets))
        return std::nullopt;
    return ret;
}

Window::WidgetAction Window::send_mouse_event(const MouseEvent& mouse, MouseHandler which, State& state)
{
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
            old_widget_under_mouse->MouseLeave(state);
        if (new_widget_under_mouse)
            new_widget_under_mouse->MouseEnter(state);
    }
    state.SetValue(mId + "/widget-under-mouse", new_widget_under_mouse);
    state.SetValue(mId + "/active-widget", new_widget_under_mouse);

    if (new_widget_under_mouse == nullptr)
        return WidgetAction{};

    Widget::MouseEvent widget_mouse_event;
    widget_mouse_event.widget_mouse_pos = widget_pos;
    widget_mouse_event.window_mouse_pos = mouse.window_mouse_pos;
    widget_mouse_event.native_mouse_pos = mouse.native_mouse_pos;
    widget_mouse_event.widget_window_rect = widget_rect;
    widget_mouse_event.button = mouse.button;
    const auto& ret = (new_widget_under_mouse->*which)(widget_mouse_event, state);
    if (ret.type == WidgetActionType::None)
        return WidgetAction{};

    WidgetAction action;
    action.id    = new_widget_under_mouse->GetId();
    action.name  = new_widget_under_mouse->GetName();
    action.type  = ret.type;
    action.value = ret.value;
    return action;
}

} // namespace
