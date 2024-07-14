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
#include <algorithm>
#include <optional>

#include "base/assert.h"
#include "base/bitflag.h"
#include "base/utility.h"
#include "base/hash.h"
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
            ScrollArea
        };

        enum class Flags {
            Enabled,
            VisibleInGame,
            VisibleInEditor,
            ClipChildren
        };

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

        // The current transient state of the widget for painting operations.
        struct PaintEvent {
            // The widget has the current keyboard focus.
            bool focused = false;
            // The mouse is currently on top of the widget.
            bool moused = false;
            // The widget is enabled.
            bool enabled = true;
            // The current time. The starting point is unspecified
            // so one should only use this to measure elapsed times
            // between timed events and not presume any other semantics.
            double time = 0.0f;
            // The rectangle in which the widget should be painted
            // in window coordinates.
            FRect rect;
            // The clip rect against which the painting should be clipped
            // in window coordinates.
            FRect clip;
        };
        // Paint the widget.
        virtual void Paint(const PaintEvent& paint, const TransientState& state, Painter& painter) const = 0;

        virtual void Update(TransientState& state, double time, float dt)
        {}

        struct MouseEvent {
            // Mouse pointer position in the native actual window
            // that reported the mouse event.
            FPoint native_mouse_pos;
            // Mouse pointer position in the uik::Window, relative to the
            // uiK::Window coordinate system. (Top left being 0,0)
            FPoint window_mouse_pos;
            // Mouse pointer position relative to the widget's top left
            // corner. This position is always within the widget's width
            // and height or otherwise the widget would not be receiving
            // the event.
            FPoint widget_mouse_pos;
            // The widget's rectangle in the uik::Window.
            FRect  widget_window_rect;
            // The mouse button that is pressed if any.
            MouseButton button = MouseButton::None;
            // The current time. The starting point is unspecified
            // so one should only use this to measure elapsed times
            // between timed events and not presume any other semantics.
            double time = 0.0f;
        };

        struct KeyEvent {
            // The virtual key that was pressed/released.
            VirtualKey key = VirtualKey::None;
            // The current time. The starting point is unspecified
            // so one should only use this to measure elapsed times
            // between timed events and not presume any other semantics.
            double time = 0.0;
        };

        using ActionType = WidgetActionType;

        struct Action {
            WidgetActionType type = WidgetActionType::None;
            WidgetActionValue value;
        };

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

    namespace detail {
        using Widget  = uik::Widget;
        using Painter = uik::Painter;
        using MouseEvent = uik::Widget::MouseEvent;
        using PaintEvent = uik::Widget::PaintEvent;
        using KeyEvent   = uik::Widget::KeyEvent;
        using WidgetAction = uik::Widget::Action;
        struct PaintStruct {
            std::string widgetId;
            std::string widgetName;
            const StylePropertyMap* style_properties = nullptr;
            const StyleMaterialMap* style_materials = nullptr;
            const TransientState* state = nullptr;
            Painter* painter = nullptr;
        };
        struct MouseStruct {
            std::string widgetId;
            std::string widgetName;
            TransientState* state = nullptr;
        };
        struct UpdateStruct {
            std::string widgetId;
            std::string widgetName;
            TransientState* state = nullptr;
            double time  = 0.0;
            float dt = 0.0f;
        };
        struct PollStruct {
            std::string widgetId;
            std::string widgetName;
            TransientState* state = nullptr;
            double time  = 0.0;
            float dt     = 0.0f;
            // Widget's local rect
            FRect rect;
        };
        struct KeyStruct {
            std::string widgetId;
            std::string widgetName;
            TransientState* state = nullptr;
        };

        class FormModel
        {
        public:
            inline std::size_t GetHash(size_t hash) const
            { return hash; }
            inline void IntoJson(data::Writer&) const
            {}
            inline bool FromJson(const data::Reader&)
            { return true; }
            void Paint(const PaintEvent& paint, const PaintStruct& ps) const;
        private:
        };

        class ProgressBarModel
        {
        public:
            void SetText(const std::string& text)
            { mText = text; }
            void SetValue(float value)
            { mValue = value; }
            void ClearValue()
            { mValue.reset(); }
            bool HasValue() const
            { return mValue.has_value(); }
            std::optional<float> GetValue() const
            { return mValue; }
            float GetValue(float backup) const
            { return mValue.value_or(backup); }
            std::string GetText() const
            { return mText; }
            std::size_t GetHash(size_t hash) const;
            void Paint(const PaintEvent& paint, const PaintStruct& ps) const;
            void IntoJson(data::Writer& data) const;
            bool FromJson(const data::Reader& data);
        private:
            std::optional<float> mValue;
            std::string mText;
        };

        class SliderModel
        {
        public:
            void SetValue(float value)
            { mValue = value; }
            float GetValue() const
            { return mValue; }
            std::size_t GetHash(size_t hash) const;
            void Paint(const PaintEvent& paint, const PaintStruct& ps) const;
            void IntoJson(data::Writer& data) const;
            bool FromJson(const data::Reader& data);
            WidgetAction MouseEnter(const MouseStruct&);
            WidgetAction MousePress(const MouseEvent& mouse, const MouseStruct&);
            WidgetAction MouseMove(const MouseEvent& mouse, const MouseStruct& ms);
            WidgetAction MouseRelease(const MouseEvent& mouse, const MouseStruct& ms);
            WidgetAction MouseLeave(const MouseStruct&);
            WidgetAction KeyDown(const KeyEvent& key, const KeyStruct& ks);
            WidgetAction KeyUp(const KeyEvent& key, const KeyStruct& ks);
        private:
            void ComputeLayout(const FRect& rect, FRect* slider, FRect* knob) const;
        private:
            float mValue = 0.5f;
        };

        class SpinBoxModel
        {
        public:
            SpinBoxModel();
            void SetMin(int min)
            { mMinVal = min; }
            void SetMax(int max)
            { mMaxVal = max; }
            void SetValue(int value)
            { mValue = value; }
            int GetValue() const
            { return mValue; }
            int GetMin() const
            { return mMinVal; }
            int GetMax() const
            { return mMaxVal; }
            std::size_t GetHash(size_t hash) const;
            void Paint(const PaintEvent& paint, const PaintStruct& ps) const;
            void IntoJson(data::Writer& data) const;
            bool FromJson(const data::Reader& data);
            WidgetAction PollAction(const PollStruct& poll);
            WidgetAction MouseEnter(const MouseStruct&);
            WidgetAction MousePress(const MouseEvent& mouse, const MouseStruct&);
            WidgetAction MouseMove(const MouseEvent& mouse, const MouseStruct& ms);
            WidgetAction MouseRelease(const MouseEvent& mouse, const MouseStruct& ms);
            WidgetAction MouseLeave(const MouseStruct&);
            WidgetAction KeyDown(const KeyEvent& key, const KeyStruct& ks);
            WidgetAction KeyUp(const KeyEvent& key, const KeyStruct& ks);
        private:
           void ComputeBoxes(const FRect& rect, FRect* btn_inc, FRect* btn_dec, FRect* edit =nullptr) const;
           WidgetAction UpdateValue(const std::string& id, TransientState& state);
        private:
            int mValue  = 0;
            int mMinVal = 0;
            int mMaxVal = 0;
        };

        class LabelModel
        {
        public:
            LabelModel()
            { mText = "Label"; }
            LabelModel(const std::string& text)
            { mText = text; }
            std::size_t GetHash(size_t hash) const;
            std::string GetText() const
            { return mText; }
            float GetLineHeight() const
            { return mLineHeight; }
            void SetText(const std::string& text)
            { mText = text; }
            void SetText(std::string&& text)
            { mText = std::move(text); }
            void SetLineHeight(float value)
            { mLineHeight = value; }
            void Paint(const PaintEvent& paint, const PaintStruct& ps) const;
            void IntoJson(data::Writer& data) const;
            bool FromJson(const data::Reader& data);
        private:
            std::string mText;
            float mLineHeight = 1.0f;
        };

        class PushButtonModel
        {
        public:
            PushButtonModel()
            { mText = "PushButton"; }
            PushButtonModel(const std::string& text)
            { mText = text; }
            std::size_t GetHash(size_t hash) const;
            std::string GetText() const
            { return mText; }
            void SetText(const std::string& text)
            { mText = text; }
            void SetText(std::string&& text)
            { mText = text; }
            void Paint(const PaintEvent& paint, const PaintStruct& ps) const;
            void IntoJson(data::Writer& data) const;
            bool FromJson(const data::Reader& data);
            WidgetAction MousePress(const MouseEvent& mouse, const MouseStruct& ms);
            WidgetAction MouseRelease(const MouseEvent& mouse, const MouseStruct& ms);
            WidgetAction MouseLeave(const MouseStruct& ms);
            inline WidgetAction MouseMove(const MouseEvent& mouse, const MouseStruct&)
            { return WidgetAction{}; }
            inline WidgetAction MouseEnter(const MouseStruct&)
            { return WidgetAction{}; }
            WidgetAction KeyDown(const KeyEvent& key, const KeyStruct& ks);
            WidgetAction KeyUp(const KeyEvent& key, const KeyStruct& ks);
        private:
            std::string mText;
        };

        class CheckBoxModel
        {
        public:
            // where is the check mark with respect to the
            // text.
            enum class Check {
                // Check mark is on the left, text on the right
                Left,
                // Check mark is on the right, text on the left
                Right
            };

            CheckBoxModel()
            { mText = "Check"; }
            CheckBoxModel(const std::string& text)
            { mText = text; }
            void SetText(const std::string& text)
            { mText = text; }
            void SetChecked(bool on_off)
            { mChecked = on_off; }
            bool IsChecked() const
            { return mChecked; }
            std::string GetText() const
            { return mText; }
            void SetCheckLocation(Check location)
            { mCheck = location; }
            Check GetCheckLocation() const
            { return mCheck; }
            std::size_t GetHash(size_t hash) const;
            void Paint(const PaintEvent& paint, const PaintStruct& ps) const;
            void IntoJson(data::Writer& data) const;
            bool FromJson(const data::Reader& data);
            inline WidgetAction MouseEnter(const MouseStruct&)
            { return WidgetAction{}; }
            inline WidgetAction MousePress(const MouseEvent& mouse, const MouseStruct&)
            { return WidgetAction{}; }
            WidgetAction MouseMove(const MouseEvent& mouse, const MouseStruct&);
            WidgetAction MouseRelease(const MouseEvent& mouse, const MouseStruct& ms);
            WidgetAction MouseLeave(const MouseStruct&);
            WidgetAction KeyDown(const KeyEvent& key, const KeyStruct& ks);
            WidgetAction KeyUp(const KeyEvent& key, const KeyStruct& ks);
        private:
            void ComputeLayout(const FRect& rect, FRect* text, FRect* check) const;
        private:
            std::string mText;
            bool mChecked = false;
            Check mCheck  = Check::Left;
        };

        class ToggleBoxModel
        {
        public:
            void SetChecked(bool on_off)
            { mChecked = on_off; }
            bool IsChecked() const
            { return mChecked; }
            std::size_t GetHash(size_t hash) const;
            void Update(const UpdateStruct& update) const;
            void Paint(const PaintEvent& paint, const PaintStruct& ps) const;
            void IntoJson(data::Writer& data) const;
            bool FromJson(const data::Reader& data);
            WidgetAction PollAction(const PollStruct& poll);
            inline WidgetAction MouseEnter(const MouseStruct&)
            { return WidgetAction{}; }
            inline WidgetAction MousePress(const MouseEvent& mouse, const MouseStruct&)
            { return WidgetAction{}; }
            WidgetAction MouseMove(const MouseEvent& mouse, const MouseStruct&);
            WidgetAction MouseRelease(const MouseEvent& mouse, const MouseStruct& ms);
            WidgetAction MouseLeave(const MouseStruct&);
            WidgetAction KeyDown(const KeyEvent& key, const KeyStruct& ks);
            WidgetAction KeyUp(const KeyEvent& key, const KeyStruct& ks);
        private:
            bool mChecked = false;
        };

        class RadioButtonModel
        {
        public:
            // where is the check mark with respect to the
            // text.
            enum class Check {
                // Check mark is on the left, text on the right
                Left,
                // Check mark is on the right, text on the left
                Right
            };
            RadioButtonModel()
              : mText("RadioButton")
            {}
            RadioButtonModel(const std::string& text)
              : mText(text)
            {}

            void Select()
            { mRequestSelection = true; }
            void SetSelected(bool selected)
            { mSelected = selected; }
            bool IsSelected() const
            { return mSelected; }
            void SetText(const std::string& text)
            { mText = text; }
            void SetCheckLocation(Check location)
            { mCheck = location; }
            const std::string& GetText() const
            { return mText; }
            Check GetCheckLocation() const
            { return mCheck; }

            std::size_t GetHash(size_t hash) const;
            void Paint(const PaintEvent& paint, const PaintStruct& ps) const;
            void IntoJson(data::Writer& data) const;
            bool FromJson(const data::Reader& data);
            inline WidgetAction MouseEnter(const MouseStruct&)
            { return WidgetAction{}; }
            inline WidgetAction MousePress(const MouseEvent& mouse, const MouseStruct&)
            { return WidgetAction{}; }
            WidgetAction PollAction(const PollStruct& poll);
            WidgetAction MouseMove(const MouseEvent& mouse, const MouseStruct&);
            WidgetAction MouseRelease(const MouseEvent& mouse, const MouseStruct& ms);
            WidgetAction MouseLeave(const MouseStruct&);
            WidgetAction KeyDown(const KeyEvent& key, const KeyStruct& ks);
            WidgetAction KeyUp(const KeyEvent& key, const KeyStruct& ks);
        private:
            void ComputeLayout(const FRect& rect, FRect* text, FRect* check) const;
        private:
            std::string mText;
            Check mCheck = Check::Left;
            bool mSelected = false;
            bool mRequestSelection = false;
        };

        class GroupBoxModel
        {
        public:
            GroupBoxModel()
            { mText = "GroupBox"; }
            void SetText(const std::string& text)
            { mText = text; }
            std::string GetText() const
            { return mText; }
            std::size_t GetHash(size_t hash) const;
            void Paint(const PaintEvent& paint, const PaintStruct& ps) const;
            void IntoJson(data::Writer& data) const;
            bool FromJson(const data::Reader& data);
        private:
            std::string mText;
        };

        class ScrollAreaModel
        {
        public:
            enum class ScrollBarMode {
                Automatic, AlwaysOff, AlwaysOn
            };
            enum class Flags {
                ShowVerticalScrollButtons,
                ShowHorizontalScrollButtons
            };

            ScrollAreaModel()
            {
                mFlags.set(Flags::ShowHorizontalScrollButtons, true);
                mFlags.set(Flags::ShowVerticalScrollButtons, true);
            }

            inline ScrollBarMode GetVerticalScrollBarMode() const noexcept
            { return mVerticalScrollBarMode; }
            inline ScrollBarMode GetHorizontalScrollBarMode() const noexcept
            { return mHorizontalScrollBarMode; }
            inline void SetVerticalScrollBarMode(ScrollBarMode mode) noexcept
            { mVerticalScrollBarMode = mode; }
            inline void SetHorizontalScrollBarMode(ScrollBarMode mode) noexcept
            { mHorizontalScrollBarMode = mode; }
            inline void ShowVerticalScrollButtons(bool on_off) noexcept
            { SetFlag(Flags::ShowVerticalScrollButtons, on_off); }
            inline void ShowHorizontalScrollButtons(bool on_off) noexcept
            { SetFlag(Flags::ShowHorizontalScrollButtons, on_off); }
            inline bool AreVerticalScrollButtonsVisible() const noexcept
            { return TestFlag(Flags::ShowVerticalScrollButtons); }
            inline bool AreHorizontalScrollButtonsVisible() const noexcept
            { return TestFlag(Flags::ShowHorizontalScrollButtons); }
            inline bool TestFlag(Flags flag) const noexcept
            { return mFlags.test(flag); }
            inline void SetFlag(Flags flag, bool on_off) noexcept
            { mFlags.set(flag, on_off); }

            void UpdateContentRect(const uik::FRect& content,
                                   const uik::FRect& widget,

                                   const std::string& widgetId,
                                   TransientState& state);

            FRect GetClippingRect(const FRect& widget_rect) const noexcept;

            FPoint GetChildOffset(const FRect& widget_rect) const noexcept;

            WidgetAction MouseEnter(const MouseStruct&);
            WidgetAction MousePress(const MouseEvent& mouse, const MouseStruct&);
            WidgetAction MouseMove(const MouseEvent& mouse, const MouseStruct& ms);
            WidgetAction MouseRelease(const MouseEvent& mouse, const MouseStruct& ms);
            WidgetAction MouseLeave(const MouseStruct&);
            WidgetAction PollAction(const PollStruct& ps);

            size_t GetHash(size_t hash) const;
            void QueryStyle(const Painter& painter, const std::string& widgetId);
            void Paint(const PaintEvent& paint, const PaintStruct& ps) const;
            void IntoJson(data::Writer& data) const;
            bool FromJson(const data::Reader& data);

            struct Viewport {
                bool vertical_scrollbar_visible = false;
                bool horizontal_scrollbar_visible = false;
                FRect rect;
            };
            void ComputeViewport(const FRect& widget_rect,
                                 const FRect& content_rect,
                                 Viewport* viewport) const;
        private:
            bool ComputeVerticalScrollBar(const FRect& widget_rect,
                                          const FRect& viewport_rect,
                                          const FRect& content_rect,
                                          bool vertical_scrollbar,
                                          bool horizontal_scrollbar,
                                          FRect* btn_up,
                                          FRect* btn_down,
                                          FRect* scroll_bar,
                                          FRect* scroll_bar_handle,
                                          float handle_pos) const;

            bool ComputeHorizontalScrollBar(const FRect& widget_rect,
                                            const FRect& viewport_rect,
                                            const FRect& content_rect,
                                            bool vertical_scrollbar,
                                            bool horizontal_scrollbar,
                                            FRect* btn_left,
                                            FRect* btn_right,
                                            FRect* scroll_bar,
                                            FRect* scroll_bar_handle,
                                            float handle_pos) const;

            float GetVerticalScrollBarWidth() const;
            float GetHorizontalScrollBarHeight() const;
            float ComputeContentWidth(const FRect& content) const;
            float ComputeContentHeight(const FRect& content) const;

        private:
            ScrollBarMode mVerticalScrollBarMode = ScrollBarMode::Automatic;
            ScrollBarMode mHorizontalScrollBarMode = ScrollBarMode::Automatic;
            // strictly speaking this should be in transient state but
            // it's a bit too complicated. Also there could be a parameter
            // to indicate how this is computed, i.e. it could also be
            // statically configured when the UI is designed.
            FRect mContentRect;
            float mVerticalScrollPos = 0.0f;
            float mHorizontalScrollPos = 0.0f;
            float mVerticalScrollBarWidth = 25.0f;
            float mHorizontalScrollBarHeight = 25.0f;
            base::bitflag<Flags> mFlags;
        };


        struct WidgetTraits {
            static constexpr auto InitialWidth      = 100;
            static constexpr auto InitialHeight     = 30;
            static constexpr auto WantsMouseEvents  = false;
            static constexpr auto WantsKeyEvents    = false;
            static constexpr auto WantsUpdate       = false;
            static constexpr auto WantsPoll         = false;
            static constexpr auto HasClippingRect   = false;
            static constexpr auto HasViewportRect   = false;
            static constexpr auto HasChildTransform = false;
            static constexpr auto HasStyleQuery     = false;
        };

        template<typename WidgetModel>
        struct WidgetModelTraits;

        template<>
        struct WidgetModelTraits<ScrollAreaModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::ScrollArea;
            static constexpr auto InitialWidth      = 200;
            static constexpr auto InitialHeight     = 200;
            static constexpr auto WantsMouseEvents  = true;
            static constexpr auto WantsPoll         = true;
            static constexpr auto HasClippingRect   = true;
            static constexpr auto HasChildTransform = true;
            static constexpr auto HasStyleQuery     = true;
        };

        template<>
        struct WidgetModelTraits<FormModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::Form;
            static constexpr auto InitialWidth  = 1024;
            static constexpr auto InitialHeight = 768;
        };

        template<>
        struct WidgetModelTraits<LabelModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::Label;
        };
        template<>
        struct WidgetModelTraits<PushButtonModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::PushButton;
            static constexpr auto WantsMouseEvents = true;
            static constexpr auto WantsKeyEvents   = true;
        };
        template<>
        struct WidgetModelTraits<CheckBoxModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::CheckBox;
            static constexpr auto WantsMouseEvents = true;
            static constexpr auto WantsKeyEvents   = true;
        };
        template<>
        struct WidgetModelTraits<ToggleBoxModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::ToggleBox;
            static constexpr auto WantsMouseEvents = true;
            static constexpr auto WantsKeyEvents   = true;
            static constexpr auto WantsPoll        = true;
            static constexpr auto WantsUpdate      = true;
            static constexpr auto InitialWidth     = 60;
            static constexpr auto InitialHeight    = 30;
        };
        template<>
        struct WidgetModelTraits<RadioButtonModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::RadioButton;
            static constexpr auto WantsMouseEvents = true;
            static constexpr auto WantsKeyEvents   = true;
            static constexpr auto WantsPoll = true;
        };
        template<>
        struct WidgetModelTraits<GroupBoxModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::GroupBox;
            static constexpr auto InitialWidth  = 200;
            static constexpr auto InitialHeight = 200;
        };
        template<>
        struct WidgetModelTraits<SpinBoxModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::SpinBox;
            static constexpr auto WantsMouseEvents = true;
            static constexpr auto WantsKeyEvents   = true;
            static constexpr auto WantsPoll = true;
        };
        template<>
        struct WidgetModelTraits<SliderModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::Slider;
            static constexpr auto InitialWidth     = 200;
            static constexpr auto WantsMouseEvents = true;
            static constexpr auto WantsKeyEvents   = true;
        };

        template<>
        struct WidgetModelTraits<ProgressBarModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::ProgressBar;
            static constexpr auto InitialWidth = 200;
        };

        class BaseWidget : public Widget
        {
        public:
            using Flags = Widget::Flags;
            virtual std::string GetStyleString() const override
            { return mStyleString; }
            virtual std::string GetAnimationString() const override
            { return mAnimationString; }
            virtual std::string GetId() const override
            { return mId; }
            virtual std::string GetName() const override
            { return mName; }
            virtual uik::FSize GetSize() const override
            { return mSize; }
            virtual uik::FPoint GetPosition() const override
            { return mPosition; }
            virtual bool TestFlag(Flags flag) const override
            { return mFlags.test(flag); }
            virtual unsigned GetTabIndex() const override
            { return mTabIndex; }
            virtual void SetId(const std::string& id) override
            { mId = id; }
            virtual void SetName(const std::string& name) override
            { mName = name; }
            virtual void SetSize(const FSize& size) override
            { mSize = size; }
            virtual void SetPosition(const FPoint& pos) override
            { mPosition = pos; }
            virtual void SetStyleString(const std::string& style) override
            { mStyleString = style; }
            virtual void SetAnimationString(const std::string& animation) override
            { mAnimationString = animation; }
            virtual void SetFlag(Flags flag, bool on_off) override
            { mFlags.set(flag, on_off); }
            virtual void SetTabIndex(unsigned index) override
            { mTabIndex = index; }
            virtual size_t GetHash() const override;
            virtual void IntoJson(data::Writer& data) const override;
            virtual bool FromJson(const data::Reader& data) override;
            virtual void SetStyleProperty(const std::string& key, const StyleProperty& prop) override;
            virtual const StyleProperty* GetStyleProperty(const std::string& key) const override;
            virtual void DeleteStyleProperty(const std::string& key) override;
            virtual void SetStyleMaterial(const std::string& key, const std::string& material) override;
            virtual const std::string* GetStyleMaterial(const std::string& key) const override;
            virtual void DeleteStyleMaterial(const std::string& key) override;
        protected:
            std::string mId;
            std::string mName;
            std::string mStyleString;
            std::string mAnimationString;
            uik::FPoint mPosition;
            uik::FSize  mSize;
            base::bitflag<Flags> mFlags;
            std::optional<StylePropertyMap> mStyleProperties;
            std::optional<StyleMaterialMap> mStyleMaterials;
            unsigned mTabIndex = 0;
        };

        template<typename WidgetModel>
        class BasicWidget : public BaseWidget,
                            public WidgetModel
        {
        public:
            using Traits = WidgetModelTraits<WidgetModel>;
            using Flags  = Widget::Flags;
            // Bring the non-virtual Widget utility functions into scope.
            // Without this they're hidden when the static type is BasicWidget<T>
            using Widget::SetSize;
            using Widget::SetPosition;
            using BasicWidgetType = BasicWidget;

            static constexpr auto RuntimeWidgetType = Traits::Type;

            BasicWidget()
            {
                InitDefault();
            }

            template<typename... Args>
            BasicWidget(Args&&... args) : WidgetModel(std::forward<Args>(args)...)
            {
                InitDefault();
            }

            virtual Type GetType() const override
            { return Traits::Type; }

            virtual std::size_t GetHash() const override
            {
                size_t hash = 0;
                hash = base::hash_combine(hash, BaseWidget::GetHash());
                hash = base::hash_combine(hash, WidgetModel::GetHash(0));
                return hash;
            }

            virtual void QueryStyle(const Painter& painter) override
            {
                if constexpr (Traits::HasStyleQuery)
                {
                    WidgetModel::QueryStyle(painter, mId);
                }
            }

            virtual void Paint(const PaintEvent& paint, const TransientState& state, Painter& painter) const override
            {
                PaintStruct ps;
                ps.widgetId    = mId;
                ps.widgetName  = mName;
                ps.painter     = &painter;
                ps.state       = &state;
                if (mStyleProperties)
                    ps.style_properties = &mStyleProperties.value();
                if (mStyleMaterials)
                    ps.style_materials = &mStyleMaterials.value();

                WidgetModel::Paint(paint, ps);
            }
            virtual void Update(TransientState& state, double time, float dt) override
            {
                if constexpr (Traits::WantsUpdate)
                {
                    UpdateStruct update;
                    update.state      = &state;
                    update.widgetId   = mId;
                    update.widgetName = mName;
                    update.time       = time;
                    update.dt         = dt;
                    WidgetModel::Update(update);
                }
            }

            virtual void IntoJson(data::Writer& data) const override
            {
                BaseWidget::IntoJson(data);
                WidgetModel::IntoJson(data);
            }
            virtual bool FromJson(const data::Reader& data) override
            {
                if (!BaseWidget::FromJson(data))
                    return false;
                return WidgetModel::FromJson(data);
            }
            virtual WidgetAction PollAction(TransientState& state, double time, float dt) override
            {
                if constexpr (Traits::WantsPoll)
                {
                    PollStruct ps;
                    ps.state      = &state;
                    ps.widgetId   = mId;
                    ps.widgetName = mName;
                    ps.time       = time;
                    ps.dt         = dt;
                    ps.rect       = GetRect();
                    return WidgetModel::PollAction(ps);
                }
                else return WidgetAction {};
            }

            virtual WidgetAction MouseEnter(TransientState& state) override
            {
                if constexpr (Traits::WantsMouseEvents)
                {
                    MouseStruct ms;
                    ms.state      = &state;
                    ms.widgetId   = mId;
                    ms.widgetName = mName;
                    return WidgetModel::MouseEnter(ms);
                }
                else return WidgetAction {};
            }
            virtual WidgetAction MousePress(const MouseEvent& mouse, TransientState& state) override
            {
                if constexpr (Traits::WantsMouseEvents)
                {
                    MouseStruct ms;
                    ms.state      = &state;
                    ms.widgetId   = mId;
                    ms.widgetName = mName;
                    return WidgetModel::MousePress(mouse, ms);
                }
                else return WidgetAction{};
            }
            virtual WidgetAction MouseRelease(const MouseEvent& mouse, TransientState& state) override
            {
                if constexpr (Traits::WantsMouseEvents)
                {
                    MouseStruct ms;
                    ms.state      = &state;
                    ms.widgetId   = mId;
                    ms.widgetName = mName;
                    return WidgetModel::MouseRelease(mouse, ms);
                }
                else return WidgetAction{};
            }
            virtual WidgetAction MouseMove(const MouseEvent& mouse, TransientState& state) override
            {
                if constexpr (Traits::WantsMouseEvents)
                {
                    MouseStruct ms;
                    ms.state      = &state;
                    ms.widgetId   = mId;
                    ms.widgetName = mName;
                    return WidgetModel::MouseMove(mouse, ms);
                }
                else return WidgetAction{};
            }
            virtual WidgetAction MouseLeave(TransientState& state) override
            {
                if constexpr (Traits::WantsMouseEvents)
                {
                    MouseStruct ms;
                    ms.state      = &state;
                    ms.widgetId   = mId;
                    ms.widgetName = mName;
                    return WidgetModel::MouseLeave(ms);
                }
                else return WidgetAction {};
            }
            virtual WidgetAction KeyDown(const KeyEvent& key, TransientState& state) override
            {
                if constexpr (Traits::WantsKeyEvents)
                {
                    KeyStruct ks;
                    ks.state      = &state;
                    ks.widgetId   = mId;
                    ks.widgetName = mName;
                    return WidgetModel::KeyDown(key, ks);
                } else return WidgetAction {};
            }
            virtual WidgetAction KeyUp(const KeyEvent& key, TransientState& state) override
            {
                if constexpr(Traits::WantsKeyEvents)
                {
                    KeyStruct ks;
                    ks.state      = &state;
                    ks.widgetId   = mId;
                    ks.widgetName = mName;
                    return WidgetModel::KeyUp(key, ks);
                } else return WidgetAction {};
            }

            virtual std::unique_ptr<Widget> Copy() const override
            {
                auto ret = std::make_unique<BasicWidget>(*this);
                return ret;
            }
            virtual std::unique_ptr<Widget> Clone() const override
            {
                auto ret = std::make_unique<BasicWidget>(*this);
                ret->mId = base::RandomString(10);
                return ret;
            }
            virtual void CopyStateFrom(const Widget* other) override
            {
                ASSERT(other->GetType() == this->GetType());

                *this = *static_cast<const BasicWidgetType *>(other);
            }

            virtual FRect GetClippingRect() const noexcept override
            {
                if constexpr(Traits::HasClippingRect)
                {
                    return WidgetModel::GetClippingRect(GetRect());
                }
                return GetRect();
            }

            virtual FRect GetViewportRect() const noexcept override
            {
                if constexpr (Traits::HasViewportRect)
                {
                    return WidgetModel::GetViewportRect(GetRect());
                }
                return GetRect();
            }

            virtual FPoint GetChildOffset() const noexcept override
            {
                if constexpr (Traits::HasChildTransform)
                {
                    return WidgetModel::GetChildOffset(GetRect());
                }
                return FPoint(0.0f, 0.0f);
            }
        protected:
        private:
            void InitDefault()
            {
                mId   = base::RandomString(10);
                mSize = FSize(Traits::InitialWidth, Traits::InitialHeight);
                mFlags.set(Flags::Enabled,         true);
                mFlags.set(Flags::VisibleInGame,   true);
                mFlags.set(Flags::VisibleInEditor, true);
                mFlags.set(Flags::ClipChildren,    true);
            }
        };
    } // detail

    using Label       = detail::BasicWidget<detail::LabelModel>;
    using PushButton  = detail::BasicWidget<detail::PushButtonModel>;
    using CheckBox    = detail::BasicWidget<detail::CheckBoxModel>;
    using ToggleBox   = detail::BasicWidget<detail::ToggleBoxModel>;
    using SpinBox     = detail::BasicWidget<detail::SpinBoxModel>;
    using Slider      = detail::BasicWidget<detail::SliderModel>;
    using ProgressBar = detail::BasicWidget<detail::ProgressBarModel>;
    using GroupBox    = detail::BasicWidget<detail::GroupBoxModel>;
    using Form        = detail::BasicWidget<detail::FormModel>;
    using RadioButton = detail::BasicWidget<detail::RadioButtonModel>;
    using ScrollArea  = detail::BasicWidget<detail::ScrollAreaModel>;

    template<typename T>
    T* WidgetCast(Widget* widget)
    {
        if (widget->GetType() == T::RuntimeWidgetType)
            return static_cast<T*>(widget);
        return nullptr;
    }
    template<typename T>
    const T* WidgetCast(const Widget* widget)
    {
        if (widget->GetType() == T::RuntimeWidgetType)
            return static_cast<const T*>(widget);
        return nullptr;
    }

    template<typename T>
    Widget* WidgetCast(std::unique_ptr<Widget>& widget)
    { return WidgetCast<T>(widget.get()); }
    template<typename T>
    const Widget* WidgetCast(const std::unique_ptr<Widget>& widget)
    { return WidgetCast<T>(widget.get()); }

    std::unique_ptr<Widget> CreateWidget(uik::Widget::Type type);

} // namespace
