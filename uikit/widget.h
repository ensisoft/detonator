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
    class State;
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
            // Groupbox is a container widget for grouping widgets together
            // within a visually representable container.
            GroupBox,
            // Spinbox has 2 buttons for incrementing and decrementing an integer value.
            SpinBox,
            // A slider with a knob that moves left/right or up/down
            Slider,
            // Progress indicator
            ProgressBar
        };

        enum class Flags {
            Enabled,
            VisibleInGame,
            VisibleInEditor
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
        // Set a widget flag on or off.
        virtual void SetFlag(Flags flag, bool on_off) = 0;
        // Serialize the widget's state into JSON.
        virtual void IntoJson(data::Writer& data) const = 0;
        // Load the widget's state from the given JSON object. Returns
        // true if successful otherwise false to indicate there was some
        // problem.
        virtual bool FromJson(const data::Reader& data) = 0;

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
        virtual void Paint(const PaintEvent& paint, State& state, Painter& painter) const = 0;

        virtual void Update(State& state, double time, float dt)
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

        using ActionType = WidgetActionType;

        struct Action {
            WidgetActionType type = WidgetActionType::None;
            WidgetActionValue value;
        };

        // Poll the widget for an action. The widget might generate
        // actions when for example a button is being held.
        // Time is the current time and dt is the delta time since
        // the last PollAction call.
        virtual Action PollAction(State& state, double time, float dt)
        { return Action {}; }

        // Mouse event handler to indicate that the mouse has moved
        // on top of the widget. This will always be called before any
        // other mouse event will be called.
        virtual Action MouseEnter(State& state) = 0;
        // Mouse event handler to handle mouse button presses while the mouse
        // is on top of the widget.
        virtual Action MousePress(const MouseEvent& mouse, State& state) = 0;
        // Mouse event handler to handle mouse button releases while the
        // mouse is on top of the widget.
        virtual Action MouseRelease(const MouseEvent& mouse, State& state) = 0;
        // Mouse event handler to handle mouse movement while the mouse
        // is on top of the widget.
        virtual Action MouseMove(const MouseEvent& mouse, State& state) = 0;
        // Mouse event handler to be called right after the mouse has left
        // the widget and is no longer on top of the widget.
        virtual Action MouseLeave(State& state) = 0;

        // Create an exact copy of this widget with the same properties
        // and same widget ID. Be sure to know how to use this right.
        // If you just want to copy/paste some widgets then you probably
        // want Clone() instead.
        virtual std::unique_ptr<Widget> Copy() const = 0;

        // Create a functionally equivalent clone of this widget, i.e. a
        // widget that has the same type and properties but a different
        // widget id.
        virtual std::unique_ptr<Widget> Clone() const = 0;

        // Get the rectangle within the widget in which painting/click
        // testing and child clipping  should take place.
        // This could be a sub rectangle inside the extents of the widget
        // with a smaller area when the widget has some margins
        virtual FRect GetRect() const
        { return FRect(0.0f, 0.0f, GetSize()); }

        // helpers
        inline bool IsContainer() const
        {
            const auto type = GetType();
            if (type == Type::GroupBox || type == Type::Form)
                return true;
            return false;
        }
        inline bool IsEnabled() const
        { return TestFlag(Flags::Enabled);  }
        inline bool IsVisible() const
        { return TestFlag(Flags::VisibleInGame); }
        inline void SetSize(float width, float height)
        { SetSize(FSize(width , height)); }
        inline void SetPosition(float x, float y)
        { SetPosition(FPoint(x , y)); }

        inline void Grow(float dw, float dh)
        { SetSize(ClampSize(GetSize() + FSize(dw, dh))); }
        inline void Translate(float dx, float dy)
        { SetPosition(GetPosition() + FPoint(dx, dy)); }

        inline float GetWidth() const
        { return GetSize().GetWidth(); }
        inline float GetHeight() const
        { return GetSize().GetHeight(); }
        inline std::string GetClassName() const
        { return GetWidgetClassName(this->GetType()); }

        static FSize ClampSize(const FSize& size)
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
        using WidgetAction = uik::Widget::Action;
        struct PaintStruct {
            std::string widgetId;
            std::string widgetName;
            Painter* painter = nullptr;
            State* state = nullptr;
        };
        struct MouseStruct {
            std::string widgetId;
            std::string widgetName;
            State* state = nullptr;
        };
        struct UpdateStruct {
            std::string widgetId;
            std::string widgetName;
            State* state = nullptr;
            double time  = 0.0;
            float dt = 0.0f;
        };
        struct PollStruct {
            std::string widgetId;
            std::string widgetName;
            State* state = nullptr;
            double time  = 0.0;
            float dt     = 0.0f;
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
        private:
           void ComputeBoxes(const FRect& rect, FRect* btn_inc, FRect* btn_dec, FRect* edit =nullptr) const;
           WidgetAction UpdateValue(const std::string& id, State& state);
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
        private:
            void ComputeLayout(const FRect& rect, FRect* text, FRect* check) const;
        private:
            std::string mText;
            bool mChecked = false;
            Check mCheck  = Check::Left;
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

        struct WidgetTraits {
            static constexpr auto InitialWidth  = 100;
            static constexpr auto InitialHeight = 30;
            static constexpr auto WantsMouseEvents = false;
            static constexpr auto WantsUpdate = false;
            static constexpr auto WantsPoll = false;
        };

        template<typename WidgetModel>
        struct WidgetModelTraits;

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
        };
        template<>
        struct WidgetModelTraits<CheckBoxModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::CheckBox;
            static constexpr auto WantsMouseEvents = true;
        };
        template<>
        struct WidgetModelTraits<RadioButtonModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::RadioButton;
            static constexpr auto WantsMouseEvents = true;
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
            static constexpr auto WantsPoll = true;
        };
        template<>
        struct WidgetModelTraits<SliderModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::Slider;
            static constexpr auto InitialWidth     = 200;
            static constexpr auto WantsMouseEvents = true;
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
            { return mStyle; }
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
            virtual void SetId(const std::string& id) override
            { mId = id; }
            virtual void SetName(const std::string& name) override
            { mName = name; }
            virtual void SetSize(const FSize& size) override
            { mSize = size; }
            virtual void SetPosition(const FPoint& pos) override
            { mPosition = pos; }
            virtual void SetStyleString(const std::string& style) override
            { mStyle = style; }
            virtual void SetFlag(Flags flag, bool on_off) override
            { mFlags.set(flag, on_off); }
            virtual size_t GetHash() const override;
            virtual void IntoJson(data::Writer& data) const override;
            virtual bool FromJson(const data::Reader& data) override;
        protected:
            std::string mId;
            std::string mName;
            std::string mStyle;
            uik::FPoint mPosition;
            uik::FSize  mSize;
            base::bitflag<Flags> mFlags;
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

            static constexpr auto RuntimeWidgetType = Traits::Type;

            BasicWidget()
            {
                mId   = base::RandomString(10);
                mSize = FSize(Traits::InitialWidth, Traits::InitialHeight);
                mFlags.set(Flags::Enabled, true);
                mFlags.set(Flags::VisibleInGame, true);
                mFlags.set(Flags::VisibleInEditor, true);
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

            virtual void Paint(const PaintEvent& paint, State& state, Painter& painter) const override
            {
                PaintStruct ps;
                ps.widgetId    = mId;
                ps.widgetName  = mName;
                ps.painter     = &painter;
                ps.state       = &state;
                WidgetModel::Paint(paint, ps);
            }
            virtual void Update(State& state, double time, float dt) override
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
            virtual WidgetAction PollAction(State& state, double time, float dt) override
            {
                if constexpr (Traits::WantsPoll)
                {
                    PollStruct ps;
                    ps.state      = &state;
                    ps.widgetId   = mId;
                    ps.widgetName = mName;
                    ps.time       = time;
                    ps.dt         = dt;
                    return WidgetModel::PollAction(ps);
                }
                else return WidgetAction {};
            }

            virtual WidgetAction MouseEnter(State& state) override
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
            virtual WidgetAction MousePress(const MouseEvent& mouse, State& state) override
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
            virtual WidgetAction MouseRelease(const MouseEvent& mouse, State& state) override
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
            virtual WidgetAction MouseMove(const MouseEvent& mouse, State& state) override
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
            virtual WidgetAction MouseLeave(State& state) override
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
        protected:
        };
    } // detail

    using Label       = detail::BasicWidget<detail::LabelModel>;
    using PushButton  = detail::BasicWidget<detail::PushButtonModel>;
    using CheckBox    = detail::BasicWidget<detail::CheckBoxModel>;
    using SpinBox     = detail::BasicWidget<detail::SpinBoxModel>;
    using Slider      = detail::BasicWidget<detail::SliderModel>;
    using ProgressBar = detail::BasicWidget<detail::ProgressBarModel>;
    using GroupBox    = detail::BasicWidget<detail::GroupBoxModel>;
    using Form        = detail::BasicWidget<detail::FormModel>;
    using RadioButton = detail::BasicWidget<detail::RadioButtonModel>;

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
