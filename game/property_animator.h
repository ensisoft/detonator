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
#include <string>

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
        enum class PropertyName {
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
            Transformer_AngularAcceleration,
            RigidBodyJoint_MotorTorque,
            RigidBodyJoint_MotorSpeed,
            RigidBodyJoint_MotorForce,
            RigidBodyJoint_Stiffness,
            RigidBodyJoint_Damping,
            BasicLight_Direction,
            BasicLight_Translation,
            BasicLight_AmbientColor,
            BasicLight_DiffuseColor,
            BasicLight_SpecularColor,
            BasicLight_SpotHalfAngle,
            BasicLight_ConstantAttenuation,
            BasicLight_LinearAttenuation,
            BasicLight_QuadraticAttenuation
        };

        using PropertyValue = std::variant<float,
                std::string, glm::vec2, glm::vec3, Color4f>;

        // The interpolation method.
        using Interpolation = math::Interpolation;

        Interpolation GetInterpolation() const
        { return mInterpolation; }
        PropertyName GetPropertyName() const
        { return mParamName; }
        void SetPropertyName(PropertyName name)
        { mParamName = name; }
        void SetInterpolation(Interpolation method)
        { mInterpolation = method; }
        PropertyValue GetEndValue() const
        { return mEndValue; }
        template<typename T>
        const T* GetEndValue() const
        { return std::get_if<T>(&mEndValue); }
        template<typename T>
        T* GetEndValue()
        { return std::get_if<T>(&mEndValue); }
        void SetEndValue(PropertyValue value)
        { mEndValue = std::move(value); }

        void SetJointId(std::string id) noexcept
        { mJointId = std::move(id); }

        std::string GetJointId() const
        { return mJointId; }

        bool RequiresJoint() const noexcept
        {
            if (mParamName == PropertyName::RigidBodyJoint_MotorTorque ||
                mParamName == PropertyName::RigidBodyJoint_MotorSpeed ||
                mParamName == PropertyName::RigidBodyJoint_MotorForce ||
                mParamName == PropertyName::RigidBodyJoint_Stiffness ||
                mParamName == PropertyName::RigidBodyJoint_Damping)
                return true;
            return false;
        }

        Type GetType() const override
        { return Type::PropertyAnimator; }
        std::size_t GetHash() const override;
        void IntoJson(data::Writer& data) const override;
        bool FromJson(const data::Reader& data) override;
    private:
        // the interpolation method to be used.
        Interpolation mInterpolation = Interpolation::Linear;
        // which parameter to adjust
        PropertyName mParamName = PropertyName::Drawable_TimeScale;
        // the end value
        PropertyValue mEndValue;
        // if the property applies to a joint we need a joint ID
        std::string mJointId;
    };


    class BooleanPropertyAnimatorClass : public detail::AnimatorClassBase<BooleanPropertyAnimatorClass>
    {
    public:
        enum class PropertyName {
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
            Transformer_Enabled,
            RigidBodyJoint_EnableMotor,
            RigidBodyJoint_EnableLimits,
            BasicLight_Enabled
        };

        enum class PropertyAction {
            On, Off, Toggle
        };
        Type GetType() const override
        { return Type::BooleanPropertyAnimator;}
        std::size_t GetHash() const override;
        void IntoJson(data::Writer& data) const override;
        bool FromJson(const data::Reader& data) override;

        inline PropertyAction GetFlagAction() const noexcept
        { return mFlagAction; }
        inline PropertyName GetFlagName() const noexcept
        { return mFlagName; }
        inline float GetTime() const noexcept
        { return mTime; }
        inline void SetFlagName(const PropertyName name) noexcept
        { mFlagName = name; }
        inline void SetFlagAction(PropertyAction action) noexcept
        { mFlagAction = action; }
        inline void SetTime(float time) noexcept
        { mTime = math::clamp(0.0f, 1.0f, time); }

        inline void SetJointId(std::string jointId) noexcept
        { mJointId = std::move(jointId); }
        inline std::string GetJointId() const
        { return mJointId; }

        bool RequiresJoint() const noexcept
        {
            if (mFlagName == PropertyName::RigidBodyJoint_EnableMotor ||
                mFlagName == PropertyName::RigidBodyJoint_EnableLimits)
                return true;
            return false;
        }

    private:
        PropertyAction mFlagAction = PropertyAction::Off;
        PropertyName   mFlagName   = PropertyName::Drawable_FlipHorizontally;
        float mTime = 1.0f;
        std::string mJointId;
    };

        // Modify node parameter over time.
    class PropertyAnimator final : public Animator
    {
    public:
        using PropertyName  = PropertyAnimatorClass::PropertyName;
        using PropertyValue = PropertyAnimatorClass::PropertyValue;
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
        void Start(EntityNode& node) override;
        void Apply(EntityNode& node, float t) override;
        void Finish(EntityNode& node) override;

        float GetStartTime() const override
        { return mClass->GetStartTime(); }
        float GetDuration() const override
        { return mClass->GetDuration(); }
        std::string GetNodeId() const override
        { return mClass->GetNodeId(); }
        std::string GetClassId() const override
        { return mClass->GetId(); }
        std::string GetClassName() const override
        { return mClass->GetName(); }
        std::unique_ptr<Animator> Copy() const override
        { return std::make_unique<PropertyAnimator>(*this); }
        Type GetType() const override
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
        PropertyValue mStartValue;
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
        void Start(EntityNode& node) override;
        void Apply(EntityNode& node, float t) override;
        void Finish(EntityNode& node) override;
        float GetStartTime() const override
        { return mClass->GetStartTime(); }
        float GetDuration() const override
        { return mClass->GetDuration(); }
        std::string GetNodeId() const override
        { return mClass->GetNodeId(); }
        std::string GetClassId() const override
        { return mClass->GetId(); }
        std::string GetClassName() const override
        { return mClass->GetName(); }
        std::unique_ptr<Animator> Copy() const override
        { return std::make_unique<BooleanPropertyAnimator>(*this); }
        Type GetType() const override
        { return Type::BooleanPropertyAnimator; }

        bool CanApply(EntityNode& node, bool verbose) const;
        void SetFlag(EntityNode& node) const;
    private:
        std::shared_ptr<const BooleanPropertyAnimatorClass> mClass;
        bool mStartState = false;
        float mTime = 0.0f;
    };

    ANIMATOR_INSTANCE_CASTING_MACROS(PropertyAnimator, Animator::Type::PropertyAnimator)
    ANIMATOR_CLASS_CASTING_MACROS(PropertyAnimatorClass, Animator::Type::PropertyAnimator)

    ANIMATOR_INSTANCE_CASTING_MACROS(BooleanPropertyAnimator, Animator::Type::BooleanPropertyAnimator)
    ANIMATOR_CLASS_CASTING_MACROS(BooleanPropertyAnimatorClass, Animator::Type::BooleanPropertyAnimator)
} // namespace