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

#include "base/math.h"
#include "data/fwd.h"
#include "uikit/types.h"

namespace uik
{
    class TransientState;
    class Painter;

    class Widget
    {
    public:
        // The actual widget implementation type.
        enum class Type {
            // Widget is an inactive top level container/background widget.
            Form,
            // Widget is a label which shows just static text.
            Label,
            // Widget is a push button that can be clicked/triggered.
            PushButton,
            // Widget is a radio button with automatic exclusion between
            // radio buttons within a container.
            RadioButton,
            // Widget is a checkbox which has a boolean on/off toggle.
            CheckBox,
            // Widget is a togglebox which has a boolean on/off toggle.
            ToggleBox,
            // Groupbox is a container widget for grouping widgets together
            // within a visually representable container.
            GroupBox,
            // Spinbox has 2 buttons for incrementing and decrementing an integer value.
            SpinBox,
            // A slider with a knob that moves left/right or up/down
            Slider,
            // Progress indicator
            ProgressBar,
            // An area that can be bigger than the widget's own size
            // and lets the user to scroll the contents vertically/horizontally
            ScrollArea,
            // A widget that refers to a drawable shape and material
            ShapeWidget
        };

        enum class Flags {
            Enabled,
            VisibleInGame,
            VisibleInEditor,
            LockedInEditor,
            ClipChildren
        };

        using ActionType = uik::WidgetActionType;
        using Action     = detail::WidgetAction;
        using PaintEvent = uik::PaintEvent;
        using MouseEvent = uik::MouseEvent;
        using KeyEvent   = uik::KeyEvent;

        // dtor.
        virtual ~Widget() = default;
        // Get the unique Widget ID.
        virtual std::string GetId() const = 0;
        // Get the human-readable name assigned to the widget
        virtual std::string GetName() const = 0;
        // Get the current hash value based on the widget's state.
        // The hash can be used to detect changes in the widget's state.
        virtual std::size_t GetHash() const = 0;
        // Get the style string (if any). The style string specifies widget
        // specific style properties that the painter could take into account
        // when performing paint operations after having parsed the widget's style string.
        virtual std::string GetStyleString() const = 0;
        // Get the animation string (if any). The animation string specifies widget
        // specific transformations that will take place on certain triggers such as
        // when the widget is shown for the first time.
        virtual std::string GetAnimationString() const = 0;
        // Get the current size of the widget.
        virtual uik::FSize GetSize() const = 0;
        // Get the current position of the widget. The position is always relative
        // to the widget's parent when it's contained inside another widget
        // such as a frame or panel. If the widget doesn't have a parent then
        // the position is relative to the window.
        virtual uik::FPoint GetPosition() const = 0;
        // Get the type of the widget.
        virtual Type GetType() const = 0;
        // Test a widget flag. Returns true if the flag is set, otherwise false.
        virtual bool TestFlag(Flags flag) const = 0;
        // Get widgets tab index for keyboard navigation.
        virtual unsigned GetTabIndex() const = 0;
        // Set the unique Widget ID.
        virtual void SetId(const std::string& id) = 0;
        // Set the widget name.
        virtual void SetName(const std::string& name) = 0;
        // Set the widget size.
        virtual void SetSize(const FSize& size) = 0;
        // Set the widget's position relative to it's parent if any.
        // If the widget doesn't have a parent widget then the position
        // is relative to the window's origin, i.e its top left corner.
        virtual void SetPosition(const FPoint& pos) = 0;
        // Set the widget's style string. The style string is used to
        // associate some widget specific styling information with the widget
        // that can then be used by the painter to modify the painting
        // output for the widget.
        virtual void SetStyleString(const std::string& style) = 0;
        // Set the widget's animation widget.
        virtual void SetAnimationString(const std::string& animation) = 0;
        // Set a widget flag on or off.
        virtual void SetFlag(Flags flag, bool on_off) = 0;
        // Set the tab index that is used to navigate between widgets
        // when using keyboard navigation. No two widgets should have
        // the same index value assigned.
        virtual void SetTabIndex(unsigned index) = 0;
        // Serialize the widget's state into JSON.
        virtual void IntoJson(data::Writer& data) const = 0;
        // Load the widget's state from the given JSON object. Returns
        // true if successful otherwise false to indicate there was some
        // problem.
        virtual bool FromJson(const data::Reader& data) = 0;

        // query the painter (and the underlying style) for style settings
        // that impact the widget's functionality. for example line height
        // or scroll bar width. These are the style settings that are needed
        // in order to perform the widget logic properly and are thus
        // not only stylistic but also functional.
        virtual void QueryStyle(const Painter& painter) {}

        // Paint the widget.
        virtual void Paint(const PaintEvent& paint, const TransientState& state, Painter& painter) const = 0;

        virtual void Update(TransientState& state, double time, float dt)
        {}

        // Poll the widget for an action. The widget might generate
        // actions when for example a button is being held.
        // Time is the current time and dt is the delta time since
        // the last PollAction call.
        virtual Action PollAction(TransientState& state, double time, float dt)
        { return Action {}; }

        // Mouse event handler to indicate that the mouse has moved
        // on top of the widget. This will always be called before any
        // other mouse event will be called.
        virtual Action MouseEnter(TransientState& state) = 0;
        // Mouse event handler to handle mouse button presses while the mouse
        // is on top of the widget.
        virtual Action MousePress(const MouseEvent& mouse, TransientState& state) = 0;
        // Mouse event handler to handle mouse button releases while the
        // mouse is on top of the widget.
        virtual Action MouseRelease(const MouseEvent& mouse, TransientState& state) = 0;
        // Mouse event handler to handle mouse movement while the mouse
        // is on top of the widget.
        virtual Action MouseMove(const MouseEvent& mouse, TransientState& state) = 0;
        // Mouse event handler to be called right after the mouse has left
        // the widget and is no longer on top of the widget.
        virtual Action MouseLeave(TransientState& state) = 0;
        // Keyboard key down event handler for virtual keys.
        virtual Action KeyDown(const KeyEvent& key, TransientState& state) = 0;
        // Keyboard key up event handler for virtual keys.
        virtual Action KeyUp(const KeyEvent& key, TransientState& state) = 0;

        // Create an exact copy of this widget with the same properties
        // and same widget ID. Be sure to know how to use this right.
        // If you just want to copy/paste some widgets then you probably
        // want Clone() instead.
        virtual std::unique_ptr<Widget> Copy() const = 0;

        // Create a functionally equivalent clone of this widget, i.e. a
        // widget that has the same type and properties but a different
        // widget id.
        virtual std::unique_ptr<Widget> Clone() const = 0;

        // Override a specific style property for this widget only by setting
        // a new property by the given key value. This property value will take
        // precedence over the inline style string associated with this widget
        // and any baseline style that would otherwise be applied.
        virtual void SetStyleProperty(const std::string& key, const StyleProperty& prop) = 0;
        // Get a specific style property by the given key. Returns nullptr if no such
        // property exists.
        virtual const StyleProperty* GetStyleProperty(const std::string& key) const = 0;
        // Delete a specific style property by the given key. Does nothing if no such
        // property exists.
        virtual void DeleteStyleProperty(const std::string& key) = 0;

        virtual void SetStyleMaterial(const std::string& key, const std::string& material) = 0;
        virtual const std::string* GetStyleMaterial(const std::string& key) const = 0;
        virtual void DeleteStyleMaterial(const std::string& key) = 0;

        virtual void CopyStateFrom(const Widget* other) = 0;

        // Get the rectangle of this widget wrt its parent.
        // In other words a child widget occupies some area inside
        // it's parent that area is described by this rectangle.
        virtual FRect GetRect() const noexcept
        { return FRect(GetPosition(), GetSize()); }

        virtual FRect GetClippingRect() const noexcept
        { return GetRect(); }

        virtual FRect GetViewportRect() const noexcept
        { return GetRect(); }

        virtual FPoint GetChildOffset() const noexcept
        { return FPoint(0.0f, 0.0); }

        // helpers
        void SetColor(const std::string& key, const Color4f& color);
        void SetMaterial(const std::string& key, const std::string& material);
        void SetGradient(const std::string& key,
                         const Color4f& top_left,
                         const Color4f& top_right,
                         const Color4f& bottom_left,
                         const Color4f& bottom_right);

        inline bool IsContainer() const noexcept
        {
            const auto type = GetType();
            if (type == Type::GroupBox || type == Type::Form || type == Type::ScrollArea)
                return true;
            return false;
        }
        inline bool CanFocus() const noexcept
        {
            const auto type = GetType();
            if (type == Type::GroupBox ||
                type == Type::Form ||
                type == Type::Label ||
                type == Type::ProgressBar ||
                type == Type::ScrollArea)
                return false;
            return true;
        }

        inline bool ClipChildren() const noexcept
        { return TestFlag(Flags::ClipChildren); }

        inline bool IsEnabled() const noexcept
        { return TestFlag(Flags::Enabled);  }
        inline bool IsVisible() const noexcept
        { return TestFlag(Flags::VisibleInGame); }
        inline void SetSize(float width, float height) noexcept
        { SetSize(FSize(width , height)); }
        inline void SetPosition(float x, float y) noexcept
        { SetPosition(FPoint(x , y)); }
        inline void SetEnabled(bool on_off) noexcept
        { SetFlag(Flags::Enabled, on_off); }
        inline void SetVisible(bool on_off) noexcept
        { SetFlag(Flags::VisibleInGame, on_off); }

        inline void Grow(float dw, float dh) noexcept
        { SetSize(ClampSize(GetSize() + FSize(dw, dh))); }
        inline void Translate(float dx, float dy) noexcept
        { SetPosition(GetPosition() + FPoint(dx, dy)); }

        inline float GetWidth() const noexcept
        { return GetSize().GetWidth(); }
        inline float GetHeight() const noexcept
        { return GetSize().GetHeight(); }
        inline std::string GetClassName() const noexcept
        { return GetWidgetClassName(this->GetType()); }

        static FSize ClampSize(const FSize& size) noexcept
        {
            const auto width  = math::clamp(0.0f, size.GetWidth(), size.GetWidth());
            const auto height = math::clamp(0.0f, size.GetHeight(), size.GetHeight());
            return FSize(width, height);
        }
        static std::string GetWidgetClassName(Type type);
        static Type GetWidgetClassType(const std::string& klass);
    private:
    };

} // namespace
