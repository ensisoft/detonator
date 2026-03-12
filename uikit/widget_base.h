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

#include "base/utility.h"
#include "base/hash.h"
#include "uikit/widget.h"
#include "uikit/widget_traits.h"
#include "uikit/types.h"

namespace uik {
    namespace detail {
        class BaseWidget : public Widget
        {
        public:
            using Flags = Widget::Flags;
            std::string GetStyleString() const override
            { return mStyleString; }
            std::string GetAnimationString() const override
            { return mAnimationString; }
            std::string GetId() const override
            { return mId; }
            std::string GetName() const override
            { return mName; }
            uik::FSize GetSize() const override
            { return mSize; }
            uik::FPoint GetPosition() const override
            { return mPosition; }
            bool TestFlag(Flags flag) const override
            { return mFlags.test(flag); }
            unsigned GetTabIndex() const override
            { return mTabIndex; }
            void SetId(const std::string& id) override
            { mId = id; }
            void SetName(const std::string& name) override
            { mName = name; }
            void SetSize(const FSize& size) override
            { mSize = size; }
            void SetPosition(const FPoint& pos) override
            { mPosition = pos; }
            void SetStyleString(const std::string& style) override
            { mStyleString = style; }
            void SetAnimationString(const std::string& animation) override
            { mAnimationString = animation; }
            void SetFlag(Flags flag, bool on_off) override
            { mFlags.set(flag, on_off); }
            void SetTabIndex(unsigned index) override
            { mTabIndex = index; }
            size_t GetHash() const override;
            void IntoJson(data::Writer& data) const override;
            bool FromJson(const data::Reader& data) override;
            void SetStyleProperty(const std::string& key, const StyleProperty& prop) override;
            const StyleProperty* GetStyleProperty(const std::string& key) const override;
            void DeleteStyleProperty(const std::string& key) override;
            void SetStyleMaterial(const std::string& key, const std::string& material) override;
            const std::string* GetStyleMaterial(const std::string& key) const override;
            void DeleteStyleMaterial(const std::string& key) override;
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
        class BasicModelWidget : public BaseWidget, public WidgetModel
        {
        public:
            using Traits = WidgetModelTraits<WidgetModel>;
            using Flags  = Widget::Flags;
            // Bring the non-virtual Widget utility functions into scope.
            // Without this they're hidden when the static type is BasicWidget<T>
            using Widget::SetSize;
            using Widget::SetPosition;
            using BasicWidgetType = BasicModelWidget;

            static constexpr auto RuntimeWidgetType = Traits::Type;

            BasicModelWidget()
            {
                InitDefault();
            }

            template<typename... Args>
            BasicModelWidget(Args&&... args) : WidgetModel(std::forward<Args>(args)...)
            {
                InitDefault();
            }

            Type GetType() const override
            { return Traits::Type; }

            std::size_t GetHash() const override
            {
                size_t hash = 0;
                hash = base::hash_combine(hash, BaseWidget::GetHash());
                hash = base::hash_combine(hash, WidgetModel::GetHash(0));
                return hash;
            }

            void QueryStyle(const Painter& painter) override
            {
                if constexpr (Traits::HasStyleQuery)
                {
                    WidgetModel::QueryStyle(painter, mId);
                }
            }

            void Paint(const PaintEvent& paint, const TransientState& state, Painter& painter) const override
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
            void Update(TransientState& state, double time, float dt) override
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

            void IntoJson(data::Writer& data) const override
            {
                BaseWidget::IntoJson(data);
                WidgetModel::IntoJson(data);
            }
            bool FromJson(const data::Reader& data) override
            {
                if (!BaseWidget::FromJson(data))
                    return false;
                return WidgetModel::FromJson(data);
            }
            WidgetAction PollAction(TransientState& state, double time, float dt) override
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

            WidgetAction MouseEnter(TransientState& state) override
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
            WidgetAction MousePress(const MouseEvent& mouse, TransientState& state) override
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
            WidgetAction MouseRelease(const MouseEvent& mouse, TransientState& state) override
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
            WidgetAction MouseMove(const MouseEvent& mouse, TransientState& state) override
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
            WidgetAction MouseLeave(TransientState& state) override
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
            WidgetAction KeyDown(const KeyEvent& key, TransientState& state) override
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
            WidgetAction KeyUp(const KeyEvent& key, TransientState& state) override
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

            std::unique_ptr<Widget> Copy() const override
            {
                auto ret = std::make_unique<BasicModelWidget>(*this);
                return ret;
            }
            std::unique_ptr<Widget> Clone() const override
            {
                auto ret = std::make_unique<BasicModelWidget>(*this);
                ret->mId = base::RandomString(10);
                return ret;
            }
            void CopyStateFrom(const Widget* other) override
            {
                ASSERT(other->GetType() == this->GetType());

                *this = *static_cast<const BasicWidgetType *>(other);
            }

            FRect GetClippingRect() const noexcept override
            {
                if constexpr(Traits::HasClippingRect)
                {
                    return WidgetModel::GetClippingRect(GetRect());
                }
                return GetRect();
            }

            FRect GetViewportRect() const noexcept override
            {
                if constexpr (Traits::HasViewportRect)
                {
                    return WidgetModel::GetViewportRect(GetRect());
                }
                return GetRect();
            }

            FPoint GetChildOffset() const noexcept override
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
    } // namespace
} // namespace