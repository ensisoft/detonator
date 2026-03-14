// Copyright (C) 2020-2026 Sami Väisänen
// Copyright (C) 2020-2026 Ensisoft http://www.ensisoft.com
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
#include <optional>
#include <cstddef>
#include <cstdint>

#include "base/types.h"
#include "base/bitflag.h"
#include "data/fwd.h"
#include "uikit/types.h"

namespace uik
{
    class Widget;
    class Painter;
    class TransientState;

    namespace detail {
        using Painter    = uik::Painter;
        using MouseEvent = uik::MouseEvent;
        using PaintEvent = uik::PaintEvent;
        using KeyEvent   = uik::KeyEvent;
        using WidgetAction = WidgetAction;
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
            void SetText(std::string text) noexcept
            { mText = std::move(text); }

            void SetValue(float value) noexcept
            { mValue = value; }

            void ClearValue() noexcept
            { mValue.reset(); }

            auto HasValue() const noexcept
            { return mValue.has_value(); }

            auto GetValue() const noexcept
            { return mValue; }

            float GetValue(float backup) const noexcept
            { return mValue.value_or(backup); }

            auto GetText() const
            { return mText; }

            auto GetOrientation() const noexcept
            { return mOrientation; }

            void SetOrientation(WidgetOrientation orientation) noexcept
            { mOrientation = orientation; }

            std::size_t GetHash(size_t hash) const;
            void Paint(const PaintEvent& paint, const PaintStruct& ps) const;
            void IntoJson(data::Writer& data) const;
            bool FromJson(const data::Reader& data);
        private:
            std::optional<float> mValue;
            std::string mText;
            WidgetOrientation mOrientation = WidgetOrientation::Horizontal;
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
                mScrollAreaFlags.set(Flags::ShowHorizontalScrollButtons, true);
                mScrollAreaFlags.set(Flags::ShowVerticalScrollButtons, true);
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
            { SetScrollAreaFlag(Flags::ShowVerticalScrollButtons, on_off); }
            inline void ShowHorizontalScrollButtons(bool on_off) noexcept
            { SetScrollAreaFlag(Flags::ShowHorizontalScrollButtons, on_off); }
            inline bool AreVerticalScrollButtonsVisible() const noexcept
            { return TestScrollAreaFlag(Flags::ShowVerticalScrollButtons); }
            inline bool AreHorizontalScrollButtonsVisible() const noexcept
            { return TestScrollAreaFlag(Flags::ShowHorizontalScrollButtons); }
            inline bool TestScrollAreaFlag(Flags flag) const noexcept
            { return mScrollAreaFlags.test(flag); }
            inline void SetScrollAreaFlag(Flags flag, bool on_off) noexcept
            { mScrollAreaFlags.set(flag, on_off); }

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
            base::bitflag<Flags> mScrollAreaFlags;
        };

        class ShapeModel
        {
        public:
            ShapeModel() = default;
            ShapeModel(std::string drawableId, std::string materialId)
              : mDrawableId(std::move(drawableId))
              , mMaterialId(std::move(materialId))
            {}

            inline void SetDrawableId(std::string id)
            { mDrawableId = std::move(id); }
            inline void SetMaterialId(std::string id)
            { mMaterialId = std::move(id); }
            inline std::string GetDrawableId() const
            { return mDrawableId; }
            inline std::string GetMaterialId() const
            { return mMaterialId; }
            inline uik::FPoint GetContentRotationCenter() const noexcept
            { return mContentRotationCenter; }

            inline void SetContentRotationCenter(uik::FPoint point)
            { mContentRotationCenter = point; }

            template<typename Unit>
            inline void SetContentRotation(const FAngle<Unit> angle) noexcept
            { mContentRotation = angle; }

            template<typename Unit>
            inline void RotateContent(const FAngle<Unit> angle) noexcept
            { mContentRotation += angle; }

            inline FRadians GetContentRotation() const noexcept
            { return mContentRotation; }

            std::size_t GetHash(size_t hash) const;
            void Paint(const PaintEvent& paint, const PaintStruct& ps) const;
            void IntoJson(data::Writer& data) const;
            bool FromJson(const data::Reader& data);
        private:
            std::string mDrawableId;
            std::string mMaterialId;
            uik::FRadians mContentRotation;
            uik::FPoint  mContentRotationCenter;
        };
    } // namespace
} // namespace