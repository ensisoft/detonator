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

#pragma once

#include "config.h"

#include <string>
#include <memory>
#include <functional>
#include <vector>

#include "data/fwd.h"
#include "base/tree.h"
#include "base/bitflag.h"
#include "uikit/animation.h"
#include "uikit/widget.h"
#include "uikit/types.h"
#include "uikit/op.h"

namespace uik
{
    class TransientState;
    class Painter;

    // Paint hook interface is for intercepting and inspecting the widget
    // paint operations. It's mostly useful for things such as filtering out
    // widget paints based on some property while doing UI design in the
    // Editor application.
    class PaintHook
    {
    public:
        virtual ~PaintHook() = default;
        using PaintEvent = Widget::PaintEvent;

        // Inspect the paint event that will take place for the given widget.
        // Should return true if the painting is to proceed or false to omit
        // the actual painting.
        virtual bool InspectPaint(const Widget* widget, const TransientState& state, PaintEvent& paint)
        { return true; }
        // When widget painting is taking place this is called right before
        // the widget is asked to paint itself.
        virtual void BeginPaintWidget(const Widget* widget, const TransientState& state, const PaintEvent& paint, Painter& painter) {}
        // When widget painting is taking place this called right after
        // the widget has painted itself.
        virtual void EndPaintWidget(const Widget* widget, const TransientState& state, const PaintEvent& paint, Painter& painter) {}
    private:
    };

    // Window is the main class for creating new UI interfaces by creating
    // a (recursive) structure of widgets. The window provides some facilities
    // to help designing and building new widget structures and also to provide
    // the runtime mechanics to respond to input events such as keyboard and mouse.
    // At runtime the events are propagated to the right widgets based on the
    // current state of the UI system.
    // The window always provides the root widget object which is the root object
    // of the widget hierarchy and cannot be deleted/removed. All other widgets
    // are descendants of the root. Any traversal of the widget hierarchy can
    // thus be started from the root.
    class Window
    {
    public:
        using RenderTree    = base::RenderTree<Widget>;
        using Visitor       = RenderTree::Visitor;
        using ConstVisitor  = RenderTree::ConstVisitor;

        enum class Flags {
            // Whether to enable virtual key handling.
            EnableVirtualKeys,
            // Whether to pass keyboard events to the window script or not.
            WantsKeyEvents,
            // Whether to pass mouse events to the window script or not.
            WantsMouseEvents,
        };

        Window();
        Window(const Window& other);

        // Add a new widget instance to the window.
        Widget* AddWidget(std::unique_ptr<Widget> widget)
        {
            return AddWidgetPtr(std::move(widget));
        }

        // Add a new widget instance to the window.
        template<typename WidgetType>
        auto AddWidget(WidgetType&& widget)
        {
            auto w = std::make_unique<std::decay_t<WidgetType>>(std::forward<WidgetType>(widget));
            return (std::decay_t<WidgetType>*)AddWidgetPtr(std::move(w));
        }

        Widget* DuplicateWidget(const Widget* widget);

        void LinkChild(Widget* parent, Widget* child);

        // Delete the given widget from the window's widget hierarchy.
        // Note that the root widget cannot be deleted and such calls
        // will be silently ignored. The widget and all its ascendants
        // are deleted and thus the caller must be sure not to use the
        // carcass pointer anymore since it'll no longer be valid.
        void DeleteWidget(const Widget* carcass);

        // Find the first widget (if any) that satisfies the given predicate.
        // The predicate should return true to indicate a match. If no widget
        // matches the predicate then nullptr is returned.
        template<typename Predicate>
        Widget* FindWidget(Predicate predicate)
        {
            for (auto& widget : mWidgets)
            {
                if (predicate(widget.get()))
                    return widget.get();
            }
            return nullptr;
        }

        // Find the parent widget of the given child. Returns nullptr if the
        // widget has no parent. I.e. the widget is the root widget or a widget
        // pointer that is not part of this widget hierarchy.
        Widget* FindParent(Widget* child)
        { return mRenderTree.GetParent(child); }

        // Get the widget at the specified index. The index must be valid.
        Widget& GetWidget(size_t index)
        { return *base::SafeIndex(mWidgets, index); }

        // Perform widget hit testing and find widget that contains the given window point
        // inside its the widget rect.
        // if widget_point is not null the point is transformed into widget coordinates
        // and stored in widget_point.
        Widget* HitTest(const FPoint& window_point, FPoint* widget_point = nullptr, bool consider_flags = false);

        // Find the first widget (if any) that satisfies the given predicate.
        // The predicate should return true to indicate a match. If no widget
        // matches the predicate then nullptr is returned.
        template<typename Predicate>
        const Widget* FindWidget(Predicate predicate) const
        {
            for (const auto& widget : mWidgets)
            {
                if (predicate(widget.get()))
                    return widget.get();
            }
            return nullptr;
        }

        // Find the parent widget of the given child. Returns nullptr if the
        // widget has no parent. I.e. the widget is the root widget or a widget
        // pointer that is not part of this widget hierarchy.
        const Widget* FindParent(const Widget* child) const
        { return mRenderTree.GetParent(child); }

        // Get the widget at the specified index. The index must be valid.
        const Widget& GetWidget(size_t index) const
        { return *base::SafeIndex(mWidgets, index); }

        // Perform widget hit testing and find widget that contains the given window point
        // inside its the widget rect.
        // if widget_point is not null the point is transformed into widget coordinates
        // and stored in widget_point.
        const Widget* HitTest(const FPoint& window_point, FPoint* widget_point = nullptr, bool consider_flags = false) const;

        // Paint the window and its widgets.
        void Paint(TransientState& state, Painter& painter, double time = 0.0, PaintHook* hook = nullptr) const;

        void Open(TransientState& state, AnimationStateArray* animations = nullptr);

        void Close(TransientState& state, AnimationStateArray* animations = nullptr);

        bool IsClosed(const TransientState& state, const AnimationStateArray* animations = nullptr) const;
        bool IsClosing(const TransientState& state) const;

        // Update the window and its widgets.
        void Update(TransientState& state, double time, float dt, AnimationStateArray* animations = nullptr);

        // Apply the style information from each widget (and the window) onto the
        // painter. With painters that support styling and parsing widgets' style
        // strings this will prime the painter for subsequent paint operations.
        void Style(Painter& painter) const;

        // Visit the window's widget hierarchy starting from the given
        // root widget.
        void Visit(Visitor& visitor, Widget* root = nullptr)
        { mRenderTree.PreOrderTraverse(visitor, root); }

        // Visit the window's widget hierarchy starting from the given
        // root widget.
        void Visit(ConstVisitor& visitor, const Widget* root = nullptr) const
        { mRenderTree.PreOrderTraverse(visitor, root); }

        template<typename Function>
        void VisitEach(Function callback, Widget* root = nullptr)
        { mRenderTree.PreOrderTraverseForEach(std::move(callback), root); }

        template<typename Function>
        void VisitEach(Function callback, const Widget* root = nullptr) const
        { mRenderTree.PreOrderTraverseForEach(std::move(callback), root); }

        // Invoke the given callback function for each widget in the window.
        template<typename Function>
        void ForEachWidget(Function callback)
        {
            for (const auto& widget : mWidgets)
                callback(widget.get());
        }

        // Invoke the given callback function for each widget in the window.
        template<typename Function>
        void ForEachWidget(Function callback) const
        {
            for (const auto& widget : mWidgets)
                callback(widget.get());
        }

        // Find the window coordinate rectangle for the given widget.
        // If the widget is not found the result is undefined.
        FRect FindWidgetRect(const Widget* widget) const;

        FRect GetBoundingRect() const;

        // Aggregate of mouse event data when responding to
        // mouse events.
        struct MouseEvent {
            // The native mouse position in the current *actual*
            // window. This is typically the mouse position as
            // reported by the underlying window system such as X11
            // or Win32 and is the in window's client coordinate system.
            FPoint native_mouse_pos;
            // The mouse position relative to *this* window. The caller
            // must transform the native mouse events into uik::Window
            // coordinates.
            FPoint window_mouse_pos;
            // The mouse button that is pressed if any.
            MouseButton button = MouseButton::None;
            // Time of the event.
            double time = 0.0;
        };

        using KeyEvent = Widget::KeyEvent;

        // WidgetAction defines the response to some event.
        // For example when a push button receives a mouse button
        // down followed by a release it'll generate a ButtonPress
        // widget action. The caller can then choose the appropriate
        // action to take on such a response.
        // Not all events result in an action in which case the type
        // will be None.
        struct WidgetAction {
            // Name of the widget that generated the action.
            // Note that this isn't necessarily unique but depends on entirely
            // how the widgets have been named. If you're relying on this
            // for identifying the source of the event (such as OK button)
            // then make sure to use unique names.
            std::string name;
            // ID of the widget that generated the action. Unlike the name
            // the widget IDs are always created uniquely when a widget is
            // created (except when they're copied bitwise!). In terms of
            // identifying which widget generated the action this is more
            // reliable than the name.
            std::string id;
            // The action that is happening, for example ButtonPress,
            // ValueChange, ItemSelectionChanged etc.
            WidgetActionType type = WidgetActionType::None;
            // The actual value of the action if any. Depends on the
            // type of the action. see uikit/types.h for more details.
            WidgetActionValue value;
        };

        // Notes about event dispatching. In general there will
        // be only one widget that will receive the input events.
        // However, keyboard and mouse events can be dispatched to
        // different widgets. Keyboard events are dispatched to the
        // widget that currently has the keyboard focus while mouse
        // actions are dispatched to the widget that is under the mouse
        // regardless of whether this widget has keyboard input focus
        // or not.
        //

        // Poll widgets for an action.
        // The widget might generate actions when for example a button
        // or a key is pressed and held for some duration of time or
        // when a widget API call from the client enqueues an action
        // that must be dispatched.
        // Time is the current time and dt is the delta time since
        // the last PollAction call.
        std::vector<WidgetAction> PollAction(TransientState& state, double time, float dt);

        // Dispatch mouse press event.
        std::vector<WidgetAction> MousePress(const MouseEvent& mouse, TransientState& state);
        // Dispatch mouse release event.
        std::vector<WidgetAction> MouseRelease(const MouseEvent& mouse, TransientState& state);
        // Dispatch mouse move event.
        std::vector<WidgetAction> MouseMove(const MouseEvent& mouse, TransientState& state);
        // Dispatch key event to the widget that has the current
        // keyboard input focus.
        std::vector<WidgetAction> KeyDown(const KeyEvent& key, TransientState& state);
        // Dispatch key event to the widget that has the current
        // keyboard input focus.
        std::vector<WidgetAction> KeyUp(const KeyEvent& key, TransientState& state);

        void TriggerAnimations(const std::vector<WidgetAction>& actions, TransientState& state, AnimationStateArray& animations);

        // Serialize the window and its widget hierarchy into JSON.
        void IntoJson(data::Writer& data) const;

        // Create a clone of this window object. The clone will contain
        // all the same properties except the object identifies will be
        // different.
        Window Clone() const;

        void ClearWidgets();

        // Setters.
        void SetName(const std::string& name)
        { mName = name; }
        void SetName(std::string&& name)
        { mName = std::move(name); }
        void SetStyleName(const std::string& style)
        { mStyleFile = style; }
        void SetStyleString(const std::string& style)
        { mStyleString = style; }
        void SetScriptFile(const std::string& file)
        { mScriptFile = file; }
        void ResetStyleString()
        { mStyleString.clear(); }
        void ResetScriptFile()
        { mScriptFile.clear(); }
        void SetFlag(Flags flag, bool on_off)
        { mFlags.set(flag, on_off); }
        void SetKeyMapFile(const std::string& file)
        { mKeyMapFile = file; }
        void ResetKeyMapFile()
        { mKeyMapFile.clear(); }
        void EnableVirtualKeys(bool enabled)
        { mFlags.set(Flags::EnableVirtualKeys, enabled); }

        // Getters.
        std::size_t GetHash() const;
        std::string GetScriptFile() const
        { return mScriptFile; }
        std::string GetStyleName() const
        { return mStyleFile; }
        std::string GetStyleString() const
        { return mStyleString; }
        std::size_t GetNumWidgets() const
        { return mWidgets.size(); }
        std::string GetId() const
        { return mId; }
        std::string GetName() const
        { return mName; }
        RenderTree& GetRenderTree()
        { return mRenderTree; }
        const RenderTree& GetRenderTree() const
        { return mRenderTree; }
        bool HasScriptFile() const
        { return !mScriptFile.empty(); }
        bool TestFlag(Flags flag) const
        { return mFlags.test(flag); }
        std::string GetKeyMapFile() const
        { return mKeyMapFile; }

        const Widget* GetFocusedWidget(const TransientState& state) const;

        // Helpers
        Widget* FindWidgetByName(const std::string& name)
        { return FindWidget([&name](Widget* widget) { return widget->GetName() == name; }); }
        Widget* FindWidgetById(const std::string& id)
        { return FindWidget([&id](Widget* widget) { return widget->GetId() == id; }); }
        Widget* FindWidgetByType(Widget::Type type)
        { return FindWidget([type](Widget* widget) { return widget->GetType() == type; }); }
        Widget* HitTest(float x, float y, FPoint* widget_coord = nullptr, bool consider_flags = false)
        { return HitTest(FPoint(x, y), widget_coord, consider_flags); }

        const Widget* FindWidgetByName(const std::string& name) const
        { return FindWidget([&name](const Widget* widget) {  return widget->GetName() == name; }); }
        const Widget* FindWidgetById(const std::string& id) const
        { return FindWidget([&id](const Widget* widget) { return widget->GetId() == id; }); }
        const Widget* FindWidgetByType(Widget::Type type) const
        { return FindWidget([type](Widget* widget) { return widget->GetType() == type; }); }
        const Widget* HitTest(float x, float y, FPoint* widget_coord = nullptr, bool consider_flags = false) const
        { return HitTest(FPoint(x, y), widget_coord, consider_flags); }

        Window& operator=(const Window& other);

        bool FromJson(const data::Reader& data);
    private:
        enum class MouseEventType {
            ButtonPress, ButtonRelease, MouseMove
        };
        using MouseHandler = Widget::Action (Widget::*)(const Widget::MouseEvent&, TransientState&);
        std::vector<WidgetAction> send_mouse_event(const MouseEvent& mouse, MouseHandler which, TransientState& state, bool mouse_press);

        Widget* AddWidgetPtr(std::unique_ptr<Widget> widget);

        bool CanFocus(const Widget* widget) const;
    private:
        std::string mId;
        std::string mName;
        std::string mScriptFile;
        std::string mStyleFile;
        std::string mStyleString;
        std::string mKeyMapFile;
        std::vector<std::unique_ptr<Widget>> mWidgets;
        RenderTree mRenderTree;
        base::bitflag<Flags> mFlags;
    };
} // namespace
