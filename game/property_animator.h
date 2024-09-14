// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#include <variant>

#include "game/color.h"
#include "game/animator_base.h"

#include "base/snafu.h"

namespace game
{
    // Modify node parameter value over time.
    class PropertyAnimatorClass : public detail::AnimatorClassBase<PropertyAnimatorClass>
    {
    public:
        // Enumeration of support node parameters that can be changed.
        enum class ParamName {
            Drawable_TimeScale,
            Drawable_RotationX,
            Drawable_RotationY,
            Drawable_RotationZ,
            Drawable_TranslationX,
            Drawable_TranslationY,
            Drawable_TranslationZ,
            Drawable_SizeZ,
            RigidBody_LinearVelocityX,
            RigidBody_LinearVelocityY,
            RigidBody_LinearVelocity,
            RigidBody_AngularVelocity,
            TextItem_Text,
            TextItem_Color,
            Transformer_LinearVelocity,
            Transformer_LinearVelocityX,
            Transformer_LinearVelocityY,
            Transformer_LinearAcceleration,
            Transformer_LinearAccelerationX,
            Transformer_LinearAccelerationY,
            Transformer_AngularVelocity,
            Transformer_AngularAcceleration
        };

        using ParamValue = std::variant<float,
                std::string, glm::vec2, Color4f>;

        // The interpolation method.
        using Interpolation = math::Interpolation;

        Interpolation GetInterpolation() const
        { return mInterpolation; }
        ParamName GetParamName() const
        { return mParamName; }
        void SetParamName(ParamName name)
        { mParamName = name; }
        void SetInterpolation(Interpolation method)
        { mInterpolation = method; }
        ParamValue GetEndValue() const
        { return mEndValue; }
        template<typename T>
        const T* GetEndValue() const
        { return std::get_if<T>(&mEndValue); }
        template<typename T>
        T* GetEndValue()
        { return std::get_if<T>(&mEndValue); }
        void SetEndValue(ParamValue value)
        { mEndValue = value; }

        virtual Type GetType() const override
        { return Type::PropertyAnimator; }
        virtual std::size_t GetHash() const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;
    private:
        // the interpolation method to be used.
        Interpolation mInterpolation = Interpolation::Linear;
        // which parameter to adjust
        ParamName mParamName = ParamName::Drawable_TimeScale;
        // the end value
        ParamValue mEndValue;
    };


    class BooleanPropertyAnimatorClass : public detail::AnimatorClassBase<BooleanPropertyAnimatorClass>
    {
    public:
        enum class Property {
            Drawable_VisibleInGame,
            Drawable_UpdateMaterial,
            Drawable_UpdateDrawable,
            Drawable_Restart,
            Drawable_FlipHorizontally,
            Drawable_FlipVertically,
            Drawable_DoubleSided,
            Drawable_DepthTest,
            Drawable_PPEnableBloom,
            RigidBody_Bullet,
            RigidBody_Sensor,
            RigidBody_Enabled,
            RigidBody_CanSleep,
            RigidBody_DiscardRotation,
            TextItem_VisibleInGame,
            TextItem_Blink,
            TextItem_Underline,
            TextItem_PPEnableBloom,
            SpatialNode_Enabled,
            Transformer_Enabled
        };

        enum class PropertyAction {
            On, Off, Toggle
        };
        virtual Type GetType() const override
        { return Type::BooleanPropertyAnimator;}
        virtual std::size_t GetHash() const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;

        inline PropertyAction GetFlagAction() const noexcept
        { return mFlagAction; }
        inline Property GetFlagName() const noexcept
        { return mFlagName; }
        inline float GetTime() const noexcept
        { return mTime; }
        inline void SetFlagName(const Property name) noexcept
        { mFlagName = name; }
        inline void SetFlagAction(PropertyAction action) noexcept
        { mFlagAction = action; }
        inline void SetTime(float time) noexcept
        { mTime = math::clamp(0.0f, 1.0f, time); }
    private:
        PropertyAction mFlagAction = PropertyAction::Off;
        Property   mFlagName   = Property::Drawable_FlipHorizontally;
        float      mTime       = 1.0f;
    };

        // Modify node parameter over time.
    class PropertyAnimator final : public Animator
    {
    public:
        using ParamName     = PropertyAnimatorClass::ParamName;
        using ParamValue    = PropertyAnimatorClass::ParamValue;
        using Inteprolation = PropertyAnimatorClass::Interpolation;
        explicit PropertyAnimator(std::shared_ptr<const PropertyAnimatorClass> klass) noexcept
           : mClass(std::move(klass))
        {}
        explicit PropertyAnimator(const PropertyAnimatorClass& klass)
            : mClass(std::make_shared<PropertyAnimatorClass>(klass))
        {}
        explicit PropertyAnimator(PropertyAnimatorClass&& klass)
            : mClass(std::make_shared<PropertyAnimatorClass>(std::move(klass)))
        {}
        virtual void Start(EntityNode& node) override;
        virtual void Apply(EntityNode& node, float t) override;
        virtual void Finish(EntityNode& node) override;

        virtual float GetStartTime() const override
        { return mClass->GetStartTime(); }
        virtual float GetDuration() const override
        { return mClass->GetDuration(); }
        virtual std::string GetNodeId() const override
        { return mClass->GetNodeId(); }
        virtual std::string GetClassId() const override
        { return mClass->GetId(); }
        virtual std::string GetClassName() const override
        { return mClass->GetName(); }
        virtual std::unique_ptr<Animator> Copy() const override
        { return std::make_unique<PropertyAnimator>(*this); }
        virtual Type GetType() const override
        { return Type::PropertyAnimator; }
        bool CanApply(EntityNode& node, bool verbose) const;
    private:
        template<typename T>
        T Interpolate(float t, bool interpolate) const
        {
            const auto method = mClass->GetInterpolation();
            const auto end    = mClass->GetEndValue();
            const auto start  = mStartValue;

            if (!interpolate)
                return std::get<T>(end);

            ASSERT(std::holds_alternative<T>(end));
            ASSERT(std::holds_alternative<T>(start));
            return math::interpolate(std::get<T>(start), std::get<T>(end), t, method);
        }
        Color4f Interpolate(float t, bool interpolate) const
        {
            const auto method = mClass->GetInterpolation();
            const auto end    = mClass->GetEndValue();
            const auto start  = mStartValue;
            if (!interpolate)
                return std::get<Color4f>(end); // this is already sRGB encoded.

            ASSERT(std::holds_alternative<Color4f>(end));
            ASSERT(std::holds_alternative<Color4f>(start));
            auto ret = math::interpolate(sRGB_Decode(std::get<Color4f>(start)),
                                         sRGB_Decode(std::get<Color4f>(end)), t, method);
            return sRGB_Encode(ret);
        }
        void SetValue(EntityNode& node, float t, bool interpolate) const;
    private:
        std::shared_ptr<const PropertyAnimatorClass> mClass;
        ParamValue mStartValue;
    };

    class BooleanPropertyAnimator final : public Animator
    {
    public:
        using FlagAction = BooleanPropertyAnimatorClass::PropertyAction;

        explicit BooleanPropertyAnimator(std::shared_ptr<const BooleanPropertyAnimatorClass> klass) noexcept
            : mClass(std::move(klass))
            , mTime(mClass->GetTime())
        {}
        explicit BooleanPropertyAnimator(const BooleanPropertyAnimatorClass& klass)
            : mClass(std::make_shared<BooleanPropertyAnimatorClass>(klass))
            , mTime(mClass->GetTime())
        {}
        explicit BooleanPropertyAnimator(BooleanPropertyAnimatorClass&& klass)
            : mClass(std::make_shared<BooleanPropertyAnimatorClass>(std::move(klass)))
            , mTime(mClass->GetTime())
        {}
        virtual void Start(EntityNode& node) override;
        virtual void Apply(EntityNode& node, float t) override;
        virtual void Finish(EntityNode& node) override;
        virtual float GetStartTime() const override
        { return mClass->GetStartTime(); }
        virtual float GetDuration() const override
        { return mClass->GetDuration(); }
        virtual std::string GetNodeId() const override
        { return mClass->GetNodeId(); }
        virtual std::string GetClassId() const override
        { return mClass->GetId(); }
        virtual std::string GetClassName() const override
        { return mClass->GetName(); }
        virtual std::unique_ptr<Animator> Copy() const override
        { return std::make_unique<BooleanPropertyAnimator>(*this); }
        virtual Type GetType() const override
        { return Type::BooleanPropertyAnimator; }

        bool CanApply(EntityNode& node, bool verbose) const;
        void SetFlag(EntityNode& node) const;
    private:
        std::shared_ptr<const BooleanPropertyAnimatorClass> mClass;
        bool mStartState = false;
        float mTime = 0.0f;
    };


} // namespace