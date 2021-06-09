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
#include <optional>
#include <functional>

#include "data/fwd.h"
#include "uikit/widget.h"
#include "uikit/types.h"
#include "uikit/op.h"

namespace uik
{
    class State;
    class Painter;

    // Paint hook interface is for intercepting and inspecting the widget
    // paint operations. It's mostly useful for things such as filtering out
    // widget paints based on some property while doing UI design in the
    // Editor application.
    class PaintHook
    {
    public:
        virtual ~PaintHook() = default;
        // Inspect the paint event that will take place for the given widget.
        // Should return true if the painting is to proceed or false to omit
        // the actual painting. When the painting is omitted no begin/end paints
        // are then called.
        virtual bool InspectPaint(const Widget* widget, Widget::PaintEvent& event, State& state)
        { return true; }
        // When widget painting is to take place (see InspectPaint) this is called
        // before the widget is asked to paint itself.
        virtual void BeginPaintWidget(const Widget* widget, State& state, Painter& painter) {}
        // When widget painting is to take place (see InspectPaint) this called
        // right after the widget has painted itself.
        virtual void EndPaintWidget(const Widget* widget, State& state, Painter& painter) {}
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
        using Visitor       = detail::TVisitor<Widget*>;
        using ConstVisitor  = detail::TVisitor<const Widget*>;

        Window();
        Window(const Window& other);

        // Add a new widget instance to the window's root widget.
        // Returns pointer to the widget that was added.
        Widget* AddWidget(std::unique_ptr<Widget> widget)
        { return mRoot->AddChild(std::move(widget)); }

        // Add a new widget instance to the window's root widget.
        // Returns a pointer to the widget that was added.
        template<typename WidgetType>
        Widget* AddWidget(WidgetType&& widget)
        { return mRoot->AddWidget(std::forward<WidgetType>(widget)); }

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
        { return uik::FindWidget<Widget*>(std::move(predicate), mRoot.get()); }

        // Find the parent widget of the given child. Returns nullptr if the
        // widget has no parent. I.e. the widget is the root widget or a widget
        // pointer that is not part of this widget hierarchy.
        Widget* FindParent(Widget* child)
        { return uik::FindParent<Widget*>(child, mRoot.get()); }

        // Get the widget at the specified index. The index must be valid.
        Widget& GetWidget(size_t index)
        { return mRoot->GetChild(index); }

        // Perform hit testing based on the given window coordinates. Returns
        // the first widget whose rectangle contains the test point.
        Widget* HitTest(const FPoint& window, FPoint* widget = nullptr, bool flags = false);

        // Find the first widget (if any) that satisfies the given predicate.
        // The predicate should return true to indicate a match. If no widget
        // matches the predicate then nullptr is returned.
        template<typename Predicate>
        const Widget* FindWidget(Predicate predicate) const
        { return uik::FindWidget<const Widget*>(std::move(predicate), mRoot.get());  }

        // Find the parent widget of the given child. Returns nullptr if the
        // widget has no parent. I.e. the widget is the root widget or a widget
        // pointer that is not part of this widget hierarchy.
        const Widget* FindParent(Widget* child) const
        { return uik::FindParent<const Widget*>(child, mRoot.get()); }

        // Get the widget at the specified index. The index must be valid.
        const Widget& GetWidget(size_t index) const
        { return mRoot->GetChild(index); }

        // Find the first widget (if any) that satisfies the given predicate.
        // The predicate should return true to indicate a match. If no widget
        // matches the predicate then nullptr is returned.
        const Widget* HitTest(const FPoint& window, FPoint* widget = nullptr, bool flags = false) const;

        // Paint the window and its widgets.
        void Paint(State& state, Painter& painter, double time = 0.0, PaintHook* hook = nullptr) const;

        // Update the window and its widgets.
        void Update(State& state, double time, float dt);

        // Apply the style information from each widget (and the window) onto the
        // painter. With painters that support styling and parsing widgets' style
        // strings this will prime the painter for subsequent paint operations.
        void Style(Painter& painter) const;

        // Visit the window's whole widget hierarchy.
        void Visit(Visitor& visitor)
        { detail::VisitRecursive<Widget*>(visitor, mRoot.get()); }

        // Visit the window's whole widget hierarchy.
        void Visit(ConstVisitor& visitor) const
        { detail::VisitRecursive<const Widget*>(visitor, mRoot.get()); }

        // Invoke the given callback function for each widget in the window.
        template<typename Function>
        void ForEachWidget(Function callback)
        { uik::ForEachWidget<Widget*>(std::move(callback), mRoot.get()); }

        // Invoke the given callback function for each widget in the window.
        template<typename Function>
        void ForEachWidget(Function callback) const
        { uik::ForEachWidget<const Widget*>(std::move(callback), mRoot.get()); }

        // Find the window coordinate rectangle for the given widget.
        // If the widget is not found the result is undefined.
        FRect FindWidgetRect(const Widget* widget) const
        { return uik::FindWidgetRect<const Widget*>(widget, mRoot.get()); }

        // Resize the window to a new size. The size of the window is used
        // painting and in any possible scaling operations when painting the
        // window in some viewport.
        void Resize(float width, float height)
        { Resize(FSize(width, height)); }
        void Resize(const FSize& size)
        { mRoot->SetSize(size); }

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

        // WidgetAction defines the response to some event.
        // For example when a push button receives a mouse button
        // down followed by a release it'll generate a ButtonPress
        // widget action. The caller can then choose the appropriate
        // action to take on such a response.
        // Not all events result in an action in which case the type
        // will be None.
        struct WidgetAction {
            // Name of the widget that generated the action.
            // Note that this isn't necessarily unique but depends entirely
            // how the widgets have been named. If you're relying on this
            // for identifying the source of the event (such as OK button)
            // then make sure to use unique names.
            std::string name;
            // Id of the widget that generated the action. Unlike the name
            // the widget Ids are always created uniquely when a widget is
            // created (except when they're copied bitwise!). In terms of
            // identifying which widget generated the action this is more
            // reliable than the name.
            std::string id;
            // The action that is happening, for example ButtonPress,
            // ValueChanged, ItemSelectionChanged etc.
            WidgetActionType type = WidgetActionType::None;
            // The actual value of the action if any. Depends on the
            // type of the action. see uikit/types.h for more details.
            WidgetActionValue value;
        };

        // Notes about event dispatching. In general there will
        // be only one widget that will receive the input events.
        // However keyboard and mouse events can be dispatched to
        // different widgets. Keyboard events are dispatched to the
        // widget that currently has the keyboard focus while mouse
        // actions are dispatched to the widget that is under the mouse
        // regardless of whether this widget has keyboard input focus
        // or not.
        //

        // Dispatch mouse press event.
        WidgetAction MousePress(const MouseEvent& mouse, State& state);
        // Dispatch mouse release event.
        WidgetAction MouseRelease(const MouseEvent& mouse, State& state);
        // Dispatch mouse move event.
        WidgetAction MouseMove(const MouseEvent& mouse, State& state);

        // TBD keyboard events.

        // Serialize the window and its widget hierarchy into JSON.
        void IntoJson(data::Writer& data) const;

        // Create a clone of this window object. The clone will contain
        // all the same properties except the object identifies will be
        // different.
        Window Clone() const;

        // Setters.
        void SetName(const std::string& name)
        { mName = name; }
        void SetName(std::string&& name)
        { mName = std::move(name); }
        void SetStyleName(const std::string& style)
        { mStyle = style; }

        // Getters.
        std::size_t GetHash() const;
        std::string GetStyleName() const
        { return mStyle; }
        std::size_t GetNumWidgets() const
        { return mRoot->GetNumChildren(); }
        std::string GetId() const
        { return mId; }
        std::string GetName() const
        { return mName; }
        FSize GetSize() const
        { return mRoot->GetSize(); }
        float GetWidth() const
        { return mRoot->GetSize().GetWidth(); }
        float GetHeight() const
        { return mRoot->GetSize().GetHeight(); }
        Widget& GetRootWidget()
        { return *mRoot; }
        const Widget& GetRootWidget() const
        { return *mRoot; }
        bool IsRootWidget(const Widget* other) const
        { return other == mRoot.get(); }

        // Helpers
        Widget* FindWidgetByName(const std::string& name)
        { return FindWidget([&name](Widget* widget) { return widget->GetName() == name; }); }
        Widget* FindWidgetById(const std::string& id)
        { return FindWidget([&id](Widget* widget) { return widget->GetId() == id; }); }
        Widget* HitTest(float x, float y)
        { return HitTest(FPoint(x, y)); }

        const Widget* FindWidgetByName(const std::string& name) const
        { return FindWidget([&name](const Widget* widget) {  return widget->GetName() == name; }); }
        const Widget* FindWidgetById(const std::string& id) const
        { return FindWidget([&id](const Widget* widget) { return widget->GetId() == id; }); }
        const Widget* HitTest(float x, float y) const
        { return HitTest(FPoint(x, y)); }

        Window& operator=(const Window& other);

        static std::optional<Window> FromJson(const data::Reader& data);
    private:
        enum class MouseEventType {
            ButtonPress, ButtonRelease, MouseMove
        };
        using MouseHandler = Widget::Action (Widget::*)(const Widget::MouseEvent&, State&);
        WidgetAction send_mouse_event(const MouseEvent& mouse, MouseHandler which, State& state);
    private:
        std::string mId;
        std::string mName;
        std::string mStyle;
        std::unique_ptr<Form> mRoot;
    };
} // namespace
