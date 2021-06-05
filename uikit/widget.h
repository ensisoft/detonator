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

#include "warnpush.h"
#  include <nlohmann/json_fwd.hpp>
#include "warnpop.h"

#include <string>
#include <algorithm>

#include "base/assert.h"
#include "base/bitflag.h"
#include "base/utility.h"
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
            // Widget is a checkbox which has a boolean on/off toggle.
            CheckBox,
            // Groupbox is a container widget for grouping widgets together
            // within a visually representable container.
            GroupBox
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
        // Get the human readable name assigned to the widget
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
        virtual void IntoJson(nlohmann::json& json) const = 0;
        // Load the widget's state from the given JSON object. Returns
        // true if successful otherwise false to indicate there was some
        // problem.
        virtual bool FromJson(const nlohmann::json& json) = 0;

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
        // instance id.
        virtual std::unique_ptr<Widget> Clone() const = 0;

        // Return the number of child widgets this widget has. By default
        // widget's cannot contain other widgets unless the widget is a
        // container widget.
        virtual size_t GetNumChildren() const
        { return 0; }
        // Get a child widget at the given index. The index must be a valid index.
        virtual Widget& GetChild(size_t index)
        { BUG("Widget has no children."); }
        // Get a child widget at the given index. The index must be a valid index.
        virtual const Widget& GetChild(size_t index) const
        { BUG("Widget has no children."); }
        virtual bool DeleteChild(const Widget* child)
        { BUG("Widget has no children."); }
        virtual bool IsContainer() const
        { return false; }
        virtual Widget* AddChild(std::unique_ptr<Widget> child)
        { BUG("Widget is not a container.");}

        // Get the rectangle within the widget in which painting/click
        // testing and child clipping  should take place.
        // This could be a sub rectangle inside the extents of the widget
        // with a smaller area when the widget has some margins
        virtual FRect GetRect() const
        { return FRect(0.0f, 0.0f, GetSize()); }

        // helpers
        inline bool IsEnabled() const
        { return TestFlag(Flags::Enabled);  }
        inline bool IsVisible() const
        { return TestFlag(Flags::VisibleInGame); }
        inline bool HasChildren() const
        { return GetNumChildren() != 0; }
        inline void SetSize(float width, float height)
        { SetSize(FSize(width , height)); }
        inline void SetPosition(float x, float y)
        { SetPosition(FPoint(x , y)); }

        inline void Grow(float dw, float dh)
        { SetSize(GetSize() + FSize(dw, dh)); }
        inline void Translate(float dx, float dy)
        { SetPosition(GetPosition() + FPoint(dx, dy)); }
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

        class FormModel
        {
        public:
            inline std::size_t GetHash(size_t hash) const
            { return hash; }
            inline void IntoJson(nlohmann::json&) const
            {}
            inline bool FromJson(const nlohmann::json& json)
            { return true; }
            void Paint(const PaintEvent& paint, const PaintStruct& ps) const;
        private:
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
            void IntoJson(nlohmann::json& json) const;
            bool FromJson(const nlohmann::json& json);
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
            void IntoJson(nlohmann::json& json)  const;
            bool FromJson(const nlohmann::json& json);
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
            std::size_t GetHash(size_t hash) const;
            void Paint(const PaintEvent& paint, const PaintStruct& ps) const;
            void IntoJson(nlohmann::json& json) const;
            bool FromJson(const nlohmann::json& json);
            inline WidgetAction MouseEnter(const MouseStruct&)
            { return WidgetAction{}; }
            inline WidgetAction MousePress(const MouseEvent& mouse, const MouseStruct&)
            { return WidgetAction{}; }
            inline WidgetAction MouseMove(const MouseEvent& mouse, const MouseStruct&)
            { return WidgetAction{}; }
            WidgetAction MouseRelease(const MouseEvent& mouse, const MouseStruct& ms);
            inline WidgetAction MouseLeave(const MouseStruct&)
            { return WidgetAction{}; }
        private:
            std::string mText;
            bool mChecked = false;
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
            void IntoJson(nlohmann::json& json) const;
            bool FromJson(const nlohmann::json& json);
        private:
            std::string mText;
        };

        struct WidgetTraits {
            static constexpr auto InitialWidth  = 100;
            static constexpr auto InitialHeight = 30;
            static constexpr auto WantsMouseEvents = false;
            static constexpr auto WantsUpdate = false;
        };

        template<typename WidgetModel>
        struct WidgetModelTraits;

        template<>
        struct WidgetModelTraits<FormModel>
        {
            static constexpr auto Type = Widget::Type::Form;
            static constexpr auto InitialWidth  = 1024;
            static constexpr auto InitialHeight = 768;
            static constexpr auto WantsMouseEvents = false;
            static constexpr auto WantsUpdate = false;
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
        struct WidgetModelTraits<GroupBoxModel> : public WidgetTraits
        {
            static constexpr auto Type = Widget::Type::GroupBox;
            static constexpr auto InitialWidth  = 200;
            static constexpr auto InitialHeight = 200;
        };

        class WidgetBase
        {
        public:
            using Flags = Widget::Flags;
            WidgetBase()
            { mId   = base::RandomString(10); }
            size_t GetHash(size_t hash) const;
            void IntoJson(nlohmann::json& json) const;
            bool FromJson(const nlohmann::json& json);
        protected:
            std::string mId;
            std::string mName;
            std::string mStyle;
            uik::FPoint mPosition;
            uik::FSize mSize;
            base::bitflag<Flags> mFlags;
        };

        class WidgetContainer
        {
        public:
            template<typename WidgetType>
            Widget* AddWidget(WidgetType&& widget)
            {
                auto w = std::make_unique<std::remove_reference_t<WidgetType>>(std::forward<WidgetType>(widget));
                mWidgets.push_back(std::move(w));
                return mWidgets.back().get();
            }
        protected:
            static constexpr auto IsContainer = true;
            WidgetContainer() = default;
            WidgetContainer(const WidgetContainer& other)
            {
                for (const auto& w : other.mWidgets)
                    mWidgets.push_back(w->Copy());
            }

            inline void CopyWidgets(const WidgetContainer& other)
            {
                for (const auto& w : other.mWidgets)
                    mWidgets.push_back(w->Copy());
            }
            inline void CloneWidgets(const WidgetContainer& other)
            {
                for (const auto& w : other.mWidgets)
                    mWidgets.push_back(w->Clone());
            }

            inline std::size_t GetNumWidgets() const
            { return mWidgets.size(); }

            inline Widget* AddWidget(std::unique_ptr<Widget> widget)
            {
                mWidgets.push_back(std::move(widget));
                return mWidgets.back().get();
            }
            inline bool DeleteWidget(const Widget* widget)
            {
                auto it = std::find_if(mWidgets.begin(), mWidgets.end(),[widget](auto& w) {
                    return w.get() == widget;
                });
                if (it == mWidgets.end())
                    return false;
                mWidgets.erase(it);
                return true;
            }
            inline Widget& GetWidget(size_t index)
            { return *base::SafeIndex(mWidgets, index).get(); }
            inline const Widget& GetWidget(size_t index) const
            { return *base::SafeIndex(mWidgets, index).get(); }

            WidgetContainer& operator=(const WidgetContainer& other)
            {
                if (this == &other)
                    return *this;
                WidgetContainer tmp(other);
                std::swap(mWidgets, tmp.mWidgets);
                return *this;
            }
        private:
            std::vector<std::unique_ptr<Widget>> mWidgets;
        };

        class NullContainer
        {
        protected:
            static constexpr auto IsContainer = false;
            inline std::size_t GetNumWidgets() const
            { return 0; }
            inline Widget* AddWidget(std::unique_ptr<Widget> widget)
            { BUG("Widget is not a container."); }
            inline bool DeleteWidget(const Widget* widget)
            { BUG("Widget is not a container."); }
            inline Widget& GetWidget(size_t index)
            { BUG("Widget is not a container."); }
            inline const Widget& GetWidget(size_t index) const
            { BUG("Widget is not a container."); }
        private:
        };

        template<typename WidgetModel,
                typename WidgetContainer = NullContainer>
        class BasicWidget : public Widget,
                            public WidgetModel,
                            public WidgetContainer,
                            private WidgetBase
        {
        public:
            using Traits = WidgetModelTraits<WidgetModel>;
            using Flags  = Widget::Flags;
            // Bring the non-virtual Widget utility functions into scope.
            // Without this they're hidden when the static type is BasicWidget<T>
            using Widget::SetSize;
            using Widget::SetPosition;

            BasicWidget()
            {
                mSize = FSize(Traits::InitialWidth, Traits::InitialHeight);
                mFlags.set(Flags::Enabled, true);
                mFlags.set(Flags::VisibleInGame, true);
                mFlags.set(Flags::VisibleInEditor, true);
            }
            virtual std::size_t GetHash() const override
            {
                size_t hash = 0;
                hash = WidgetBase::GetHash(hash);
                hash = WidgetModel::GetHash(hash);
                return hash;
            }
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
            virtual Type GetType() const override
            { return Traits::Type; }
            virtual bool TestFlag(Flags flag) const override
            { return mFlags.test(flag); }
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
                    WidgetModel::Update(state, time, dt);
            }

            virtual void IntoJson(nlohmann::json& json) const override
            {
                WidgetBase::IntoJson(json);
                WidgetModel::IntoJson(json);
            }
            virtual bool FromJson(const nlohmann::json& json) override
            {
                if (!WidgetBase::FromJson(json) || !WidgetModel::FromJson(json))
                    return false;
                return true;
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
            { return std::make_unique<BasicWidget>(*this); }
            virtual std::unique_ptr<Widget> Clone() const override
            {
                auto ret = std::make_unique<BasicWidget>();
                ret->mId       = base::RandomString(10);
                ret->mName     = mName;
                ret->mStyle    = mStyle;
                ret->mPosition = mPosition;
                ret->mSize     = mSize;
                ret->mFlags    = mFlags;
                if constexpr (WidgetContainer::IsContainer)
                    ret->CloneWidgets(*this);
                return ret;
            }

            // child widget management
            virtual size_t GetNumChildren() const override
            { return WidgetContainer::GetNumWidgets(); }
            virtual Widget& GetChild(size_t index) override
            { return WidgetContainer::GetWidget(index); }
            virtual const Widget& GetChild(size_t index) const override
            { return WidgetContainer::GetWidget(index); }
            virtual bool DeleteChild(const Widget* child) override
            { return WidgetContainer::DeleteWidget(child); }
            virtual bool IsContainer() const override
            { return WidgetContainer::IsContainer; }
            virtual Widget* AddChild(std::unique_ptr<Widget> child) override
            { return WidgetContainer::AddWidget(std::move(child)); }
        private:
        };
    } // detail

    using Label      = detail::BasicWidget<detail::LabelModel>;
    using PushButton = detail::BasicWidget<detail::PushButtonModel>;
    using CheckBox   = detail::BasicWidget<detail::CheckBoxModel>;
    using GroupBox   = detail::BasicWidget<detail::GroupBoxModel, detail::WidgetContainer>;
    using Form       = detail::BasicWidget<detail::FormModel, detail::WidgetContainer>;

} // namespace