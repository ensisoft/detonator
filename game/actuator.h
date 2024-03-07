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
#  include <glm/vec2.hpp>
#include "warnpop.h"

#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <optional>
#include <unordered_map>

#include "base/math.h"
#include "base/utility.h"
#include "base/bitflag.h"
#include "data/fwd.h"
#include "game/types.h"

#include "base/snafu.h"

namespace game
{
    class EntityNode;

    // ActuatorClass defines an interface for classes of actuators.
    // Actuators are objects that modify the state of some render
    // tree node over time. For example a transform actuator will
    // perform linear interpolation of the node's transform over time.
    class ActuatorClass
    {
    public:
        // The type of the actuator class.
        enum class Type {
            // Transform actuators modify the transform state of the node
            // i.e. the translation, scale and rotation variables directly.
            Transform,
            // Kinematic actuators modify the kinematic physics properties
            // for example, linear or angular velocity, of the node's rigid body.
            // This will result in a kinematically driven change in the nodes
            // transform.
            Kinematic,
            // SetValue actuators sets some parameter to the specific value on the node.
            SetValue,
            // SetFlag actuator sets a binary flag to the specific state on the node.
            SetFlag,
            // Material actuator changes material parameters
            Material
        };
        enum class Flags {
            StaticInstance
        };
        // dtor.
        virtual ~ActuatorClass() = default;
        // Get human-readable name of the actuator class.
        virtual std::string GetName() const = 0;
        // Get the id of this actuator
        virtual std::string GetId() const = 0;
        // Get the ID of the node affected by this actuator.
        virtual std::string GetNodeId() const = 0;
        // Get the hash of the object state.
        virtual std::size_t GetHash() const = 0;
        // Create an exact copy of this actuator class object.
        virtual std::unique_ptr<ActuatorClass> Copy() const = 0;
        // Create a new actuator class instance with same property values
        // with this object but with a unique id.
        virtual std::unique_ptr<ActuatorClass> Clone() const = 0;
        // Get the dynamic type of the represented actuator.
        virtual Type GetType() const = 0;
        // Get the normalized start time when this actuator starts.
        virtual float GetStartTime() const = 0;
        // Get the normalized duration of this actuator.
        virtual float GetDuration() const = 0;
        // Set a class flag to on/off.
        virtual void SetFlag(Flags flag, bool on_off) = 0;
        // Test a class flag.
        virtual bool TestFlag(Flags flag) const = 0;
        // Set a new normalized start time for the actuator.
        // The value will be clamped to [0.0f, 1.0f].
        virtual void SetStartTime(float start) = 0;
        // Set a new normalized duration value for the actuator.
        // The value will be clamped to [0.0f, 1.0f].
        virtual void SetDuration(float duration) = 0;
        // Set the ID of the node affected by this actuator.
        virtual void SetNodeId(const std::string& id) = 0;
        // Set the human-readable name of the actuator class.
        virtual void SetName(const std::string& name) = 0;
        // Serialize the actuator class object into JSON.
        virtual void IntoJson(data::Writer& data) const = 0;
        // Load the actuator class object state from JSON. Returns true
        // successful otherwise false and the object is not valid state.
        virtual bool FromJson(const data::Reader& data) = 0;
    private:
    };

    namespace detail {
        template<typename T>
        class ActuatorClassBase : public ActuatorClass
        {
        public:
            virtual void SetNodeId(const std::string& id) override
            { mNodeId = id; }
            virtual void SetName(const std::string& name) override
            { mName = name; }
            virtual std::string GetName() const override
            { return mName; }
            virtual std::string GetId() const override
            { return mId; }
            virtual std::string GetNodeId() const override
            { return mNodeId; }
            virtual float GetStartTime() const override
            { return mStartTime; }
            virtual float GetDuration() const override
            { return mDuration; }
            virtual void SetFlag(Flags flag, bool on_off) override
            { mFlags.set(flag, on_off); }
            virtual bool TestFlag(Flags flag) const override
            { return mFlags.test(flag); }
            virtual void SetStartTime(float start) override
            { mStartTime = math::clamp(0.0f, 1.0f, start); }
            virtual void SetDuration(float duration) override
            { mDuration = math::clamp(0.0f, 1.0f, duration); }
            virtual std::unique_ptr<ActuatorClass> Copy() const override
            { return std::make_unique<T>(*static_cast<const T*>(this)); }
            virtual std::unique_ptr<ActuatorClass> Clone() const override
            {
                auto ret = std::make_unique<T>(*static_cast<const T*>(this));
                ret->mId = base::RandomString(10);
                return ret;
            }
        protected:
            ActuatorClassBase()
            {
                mId = base::RandomString(10);
                mFlags.set(Flags::StaticInstance, true);
            }
           ~ActuatorClassBase() = default;
        protected:
            // ID of the actuator class.
            std::string mId;
            // Human-readable name of the actuator class
            std::string mName;
            // id of the node that the action will be applied onto
            std::string mNodeId;
            // Normalized start time.
            float mStartTime = 0.0f;
            // Normalized duration.
            float mDuration = 1.0f;
            // bitflags set on the class.
            base::bitflag<Flags> mFlags;
        private:
        };
    } // namespace detail

    class SetFlagActuatorClass : public detail::ActuatorClassBase<SetFlagActuatorClass>
    {
    public:
        enum class FlagName {
            Drawable_VisibleInGame,
            Drawable_UpdateMaterial,
            Drawable_UpdateDrawable,
            Drawable_Restart,
            Drawable_FlipHorizontally,
            RigidBody_Bullet,
            RigidBody_Sensor,
            RigidBody_Enabled,
            RigidBody_CanSleep,
            RigidBody_DiscardRotation,
            TextItem_VisibleInGame,
            TextItem_Blink,
            TextItem_Underline
        };

        enum class FlagAction {
            On, Off, Toggle
        };
        virtual Type GetType() const override
        { return Type::SetFlag;}
        virtual std::size_t GetHash() const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;

        FlagAction GetFlagAction() const
        { return mFlagAction; }
        FlagName GetFlagName() const
        { return mFlagName; }
        void SetFlagName(const FlagName name)
        { mFlagName = name; }
        void SetFlagAction(FlagAction action)
        { mFlagAction = action; }
    private:
        FlagAction mFlagAction = FlagAction::Off;
        FlagName   mFlagName   = FlagName::Drawable_FlipHorizontally;
    };

    // Modify the kinematic physics body properties, i.e.
    // the instantaneous linear and angular velocities.
    class KinematicActuatorClass : public detail::ActuatorClassBase<KinematicActuatorClass>
    {
    public:
        enum class Target {
            RigidBody,
            Transformer
        };
        // The interpolation method.
        using Interpolation = math::Interpolation;

        inline Target GetTarget() const noexcept
        { return mTarget; }
        inline Interpolation GetInterpolation() const noexcept
        { return mInterpolation; }
        inline void SetInterpolation(Interpolation method) noexcept
        { mInterpolation = method; }
        inline void SetTarget(Target target) noexcept
        { mTarget = target; }
        inline glm::vec2 GetEndLinearVelocity() const noexcept
        { return mEndLinearVelocity; }
        inline glm::vec2 GetEndLinearAcceleration() const noexcept
        { return mEndLinearAcceleration; }
        inline float GetEndAngularVelocity() const noexcept
        { return mEndAngularVelocity;}
        inline float GetEndAngularAcceleration() const noexcept
        { return mEndAngularAcceleration; }
        inline void SetEndLinearVelocity(glm::vec2 velocity) noexcept
        { mEndLinearVelocity = velocity; }
        inline void SetEndLinearAcceleration(glm::vec2 acceleration) noexcept
        { mEndLinearAcceleration = acceleration; }
        inline void SetEndAngularVelocity(float velocity) noexcept
        { mEndAngularVelocity = velocity; }
        inline void SetEndAngularAcceleration(float acceleration) noexcept
        { mEndAngularAcceleration = acceleration; }

        virtual Type GetType() const override
        { return Type::Kinematic; }
        virtual std::size_t GetHash() const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;
    private:
        // the interpolation method to be used.
        Interpolation mInterpolation = Interpolation::Linear;
        Target mTarget = Target::RigidBody;
        // The ending linear velocity in meters per second.
        glm::vec2 mEndLinearVelocity     = {0.0f, 0.0f};
        glm::vec2 mEndLinearAcceleration = {0.0f, 0.0f};
        // The ending angular velocity in radians per second.
        float mEndAngularVelocity = 0.0f;
        float mEndAngularAcceleration = 0.0f;
    };

    // Modify node parameter value over time.
    class SetValueActuatorClass : public detail::ActuatorClassBase<SetValueActuatorClass>
    {
    public:
        // Enumeration of support node parameters that can be changed.
        enum class ParamName {
            DrawableTimeScale,
            LinearVelocityX,
            LinearVelocityY,
            LinearVelocity,
            AngularVelocity,
            TextItemText,
            TextItemColor
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
        { return Type::SetValue; }
        virtual std::size_t GetHash() const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;
    private:
        // the interpolation method to be used.
        Interpolation mInterpolation = Interpolation::Linear;
        // which parameter to adjust
        ParamName mParamName = ParamName::DrawableTimeScale;
        // the end value
        ParamValue mEndValue;
    };

    // TransformActuatorClass holds the transform data for some
    // particular type of linear transform of a node.
    class TransformActuatorClass : public detail::ActuatorClassBase<TransformActuatorClass>
    {
    public:
        // The interpolation method.
        using Interpolation = math::Interpolation;

        Interpolation GetInterpolation() const
        { return mInterpolation; }
        glm::vec2 GetEndPosition() const
        { return mEndPosition; }
        glm::vec2 GetEndSize() const
        { return mEndSize; }
        glm::vec2 GetEndScale() const
        { return mEndScale; }
        float GetEndRotation() const
        { return mEndRotation; }

        void SetInterpolation(Interpolation interp)
        { mInterpolation = interp; }
        void SetEndPosition(const glm::vec2& pos)
        { mEndPosition = pos; }
        void SetEndPosition(float x, float y)
        { mEndPosition = glm::vec2(x, y); }
        void SetEndSize(const glm::vec2& size)
        { mEndSize = size; }
        void SetEndSize(float x, float y)
        { mEndSize = glm::vec2(x, y); }
        void SetEndRotation(float rot)
        { mEndRotation = rot; }
        void SetEndScale(const glm::vec2& scale)
        { mEndScale = scale; }
        void SetEndScale(float x, float y)
        { mEndScale = glm::vec2(x, y); }

        virtual Type GetType() const override
        { return Type::Transform; }
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;
        virtual std::size_t GetHash() const override;
    private:
        // the interpolation method to be used.
        Interpolation mInterpolation = Interpolation::Linear;
        // the ending state of the transformation.
        // the ending position (translation relative to parent)
        glm::vec2 mEndPosition = {0.0f, 0.0f};
        // the ending size
        glm::vec2 mEndSize = {1.0f, 1.0f};
        // the ending scale.
        glm::vec2 mEndScale = {1.0f, 1.0f};
        // the ending rotation.
        float mEndRotation = 0.0f;
    };

    class MaterialActuatorClass : public detail::ActuatorClassBase<MaterialActuatorClass>
    {
    public:
        using Interpolation = math::Interpolation;
        using MaterialParam = std::variant<float, int,
            std::string,
            Color4f,
            glm::vec2, glm::vec3, glm::vec4>;
        using MaterialParamMap = std::unordered_map<std::string, MaterialParam>;

        Interpolation GetInterpolation() const
        { return mInterpolation; }
        void SetInterpolation(Interpolation method)
        { mInterpolation = method; }

        void SetMaterialParam(const std::string& name, const MaterialParam& value)
        { mMaterialParams[name] = value; }
        MaterialParamMap GetMaterialParams()
        { return mMaterialParams; }
        const MaterialParamMap& GetMaterialParams() const
        { return mMaterialParams; }
        bool HasMaterialParam(const std::string& name) const
        { return base::SafeFind(mMaterialParams, name) != nullptr; }
        MaterialParam* FindMaterialParam(const std::string& name)
        { return base::SafeFind(mMaterialParams, name); }
        const MaterialParam* FindMaterialParam(const std::string& name) const
        { return base::SafeFind(mMaterialParams, name); }
        template<typename T>
        T* GetMaterialParamValue(const std::string& name)
        {
            if (auto* ptr = base::SafeFind(mMaterialParams, name))
                return std::get_if<T>(ptr);
            return nullptr;
        }
        template<typename T>
        const T* GetMaterialParamValue(const std::string& name) const
        {
            if (auto* ptr = base::SafeFind(mMaterialParams, name))
                return std::get_if<T>(ptr);
            return nullptr;
        }
        void DeleteMaterialParam(const std::string& name)
        { mMaterialParams.erase(name); }
        void SetMaterialParams(const MaterialParamMap& map)
        { mMaterialParams = map; }
        void SetMaterialParams(MaterialParamMap&& map)
        { mMaterialParams = std::move(map); }

        virtual Type GetType() const override
        { return Type::Material; }
        virtual std::size_t GetHash() const override;
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;
    private:
        // Interpolation method used to change the value.
        Interpolation mInterpolation = Interpolation::Linear;

        MaterialParamMap mMaterialParams;
    };

    class KinematicActuator;
    class TransformActuator;
    class MaterialActuator;
    class SetFlagActuator;
    class SetValueActuator;

    // An instance of ActuatorClass object.
    class Actuator
    {
    public:
        using Type  = ActuatorClass::Type;
        using Flags = ActuatorClass::Flags;
        virtual ~Actuator() = default;
        // Start the action/transition to be applied by this actuator.
        // The node is the node in question that the changes will be applied to.
        virtual void Start(EntityNode& node) = 0;
        // Apply an interpolation of the state based on the time value t onto to the node.
        virtual void Apply(EntityNode& node, float t) = 0;
        // Finish the action/transition to be applied by this actuator.
        // The node is the node in question that the changes will (were) applied to.
        virtual void Finish(EntityNode& node) = 0;
        // Get the normalized start time when this actuator begins to take effect.
        virtual float GetStartTime() const = 0;
        // Get the normalized duration of the duration of the actuator's transformation.
        virtual float GetDuration() const = 0;
        // Get the id of the node that will be modified by this actuator.
        virtual std::string GetNodeId() const = 0;
        // Get the actuator class ID.
        virtual std::string GetClassId() const = 0;
        // Get the actuator class name.
        virtual std::string GetClassName() const = 0;
        // Create an exact copy of this actuator object.
        virtual std::unique_ptr<Actuator> Copy() const = 0;
        // Get the dynamic type of the actuator.
        virtual Type GetType() const = 0;

        inline KinematicActuator* AsKinematicActuator()
        { return Cast<KinematicActuator>(Type::Kinematic); }
        inline const KinematicActuator* AsKinematicActuator() const
        { return Cast<KinematicActuator>(Type::Kinematic); }

        inline TransformActuator* AsTransformActuator()
        { return Cast<TransformActuator>(Type::Transform); }
        inline const TransformActuator* AsTransformActuator() const
        { return Cast<TransformActuator>(Type::Transform); }

        inline MaterialActuator* AsMaterialActuator()
        { return Cast<MaterialActuator>(Type::Material); }
        inline const MaterialActuator* AsMaterialActuator() const
        { return Cast<MaterialActuator>(Type::Material); }

        inline SetValueActuator* AsValueActuator()
        { return Cast<SetValueActuator>(Type::SetValue); }
        inline const SetValueActuator* AsValueActuator() const
        { return Cast<SetValueActuator>(Type::SetValue); }

        inline SetFlagActuator* AsFlagActuator()
        { return Cast<SetFlagActuator>(Type::SetValue); }
        inline const SetFlagActuator* AsFlagActuator() const
        { return Cast<SetFlagActuator>(Type::SetValue); }
    private:
        template<typename T>
        T* Cast(Type desire)
        {
            if (GetType() == desire)
                return static_cast<T*>(this);
            return nullptr;
        }
        template<typename T>
        const T* Cast(Type desire) const
        {
            if (GetType() == desire)
                return static_cast<const T*>(this);
            return nullptr;
        }
    };

    // Apply a kinematic change to a rigid body's linear or angular velocity.
    class KinematicActuator final : public Actuator
    {
    public:
        explicit KinematicActuator(std::shared_ptr<const KinematicActuatorClass> klass) noexcept
          : mClass(std::move(klass))
        {}
        explicit KinematicActuator(const KinematicActuatorClass& klass)
          : mClass(std::make_shared<KinematicActuatorClass>(klass))
        {}
        explicit KinematicActuator(KinematicActuatorClass&& klass)
          : mClass(std::make_shared<KinematicActuatorClass>(std::move(klass)))
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
        virtual std::unique_ptr<Actuator> Copy() const override
        { return std::make_unique<KinematicActuator>(*this); }
        virtual Type GetType() const override
        { return Type::Kinematic;}
    private:
        std::shared_ptr<const KinematicActuatorClass> mClass;
        glm::vec2 mStartLinearVelocity     = {0.0f, 0.0f};
        glm::vec2 mStartLinearAcceleration = {0.0f, 0.0f};
        float mStartAngularVelocity     = 0.0f;
        float mStartAngularAcceleration = 0.0f;
    };

    class SetFlagActuator final : public Actuator
    {
    public:
        using FlagAction = SetFlagActuatorClass::FlagAction;

        SetFlagActuator(const std::shared_ptr<const SetFlagActuatorClass>& klass)
            : mClass(klass)
        {}
        SetFlagActuator(const SetFlagActuatorClass& klass)
            : mClass(std::make_shared<SetFlagActuatorClass>(klass))
        {}
        SetFlagActuator(SetFlagActuatorClass&& klass)
            : mClass(std::make_shared<SetFlagActuatorClass>(std::move(klass)))
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
        virtual std::unique_ptr<Actuator> Copy() const override
        { return std::make_unique<SetFlagActuator>(*this); }
        virtual Type GetType() const override
        { return Type::SetFlag; }
        bool CanApply(EntityNode& node, bool verbose) const;
    private:
        std::shared_ptr<const SetFlagActuatorClass> mClass;
        bool mStartState = false;
    };

    // Modify node parameter over time.
    class SetValueActuator final : public Actuator
    {
    public:
        using ParamName  = SetValueActuatorClass::ParamName;
        using ParamValue = SetValueActuatorClass::ParamValue;
        using Inteprolation = SetValueActuatorClass::Interpolation;
        SetValueActuator(const std::shared_ptr<const SetValueActuatorClass>& klass)
           : mClass(klass)
        {}
        SetValueActuator(const SetValueActuatorClass& klass)
            : mClass(std::make_shared<SetValueActuatorClass>(klass))
        {}
        SetValueActuator(SetValueActuatorClass&& klass)
            : mClass(std::make_shared<SetValueActuatorClass>(std::move(klass)))
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
        virtual std::unique_ptr<Actuator> Copy() const override
        { return std::make_unique<SetValueActuator>(*this); }
        virtual Type GetType() const override
        { return Type::SetValue; }
        bool CanApply(EntityNode& node, bool verbose) const;
    private:
        template<typename T>
        T Interpolate(float t)
        {
            const auto method = mClass->GetInterpolation();
            const auto end    = mClass->GetEndValue();
            const auto start  = mStartValue;
            ASSERT(std::holds_alternative<T>(end));
            ASSERT(std::holds_alternative<T>(start));
            return math::interpolate(std::get<T>(start), std::get<T>(end), t, method);
        }
    private:
        std::shared_ptr<const SetValueActuatorClass> mClass;
        ParamValue mStartValue;
    };

    // Apply change to the target nodes' transform.
    class TransformActuator final : public Actuator
    {
    public:
        TransformActuator(const std::shared_ptr<const TransformActuatorClass>& klass);
        TransformActuator(const TransformActuatorClass& klass)
            : TransformActuator(std::make_shared<TransformActuatorClass>(klass))
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
        virtual std::unique_ptr<Actuator> Copy() const override
        { return std::make_unique<TransformActuator>(*this); }
        virtual Type GetType() const override
        { return Type::Transform; }

        void SetEndPosition(const glm::vec2& pos);
        void SetEndScale(const glm::vec2& scale);
        void SetEndSize(const glm::vec2& size);
        void SetEndRotation(float angle);

        inline void SetEndPosition(float x, float y)
        { SetEndPosition(glm::vec2(x, y)); }
        inline void SetEndScale(float x, float y)
        { SetEndScale(glm::vec2(x, y)); }
        inline void SetEndSize(float x, float y)
        { SetEndSize(glm::vec2(x, y)); }
    private:
        struct Instance {
            glm::vec2 end_position;
            glm::vec2 end_size;
            glm::vec2 end_scale;
            float end_rotation = 0.0f;
        };
        Instance GetInstance() const;
    private:
        std::shared_ptr<const TransformActuatorClass> mClass;
        // exists only if StaticInstance is not set.
        std::optional<Instance> mDynamicInstance;
        // The starting state for the transformation.
        // the transform actuator will then interpolate between the
        // current starting and expected ending state.
        glm::vec2 mStartPosition = {0.0f, 0.0f};
        glm::vec2 mStartSize  = {1.0f, 1.0f};
        glm::vec2 mStartScale = {1.0f, 1.0f};
        float mStartRotation  = 0.0f;
    };

    class MaterialActuator final : public Actuator
    {
    public:
        using Interpolation    = MaterialActuatorClass::Interpolation;
        using MaterialParam    = MaterialActuatorClass::MaterialParam;
        using MaterialParamMap = MaterialActuatorClass::MaterialParamMap;
        MaterialActuator(const std::shared_ptr<const MaterialActuatorClass>& klass)
          : mClass(klass)
        {}
        MaterialActuator(const MaterialActuatorClass& klass)
          : mClass(std::make_shared<MaterialActuatorClass>(klass))
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
        virtual std::unique_ptr<Actuator> Copy() const override
        { return std::make_unique<MaterialActuator>(*this); }
        virtual Type GetType() const override
        { return Type::Material; }
    private:
        template<typename T>
        T Interpolate(const MaterialParam& start, const MaterialParam& end, float t)
        {
            const auto method = mClass->GetInterpolation();
            ASSERT(std::holds_alternative<T>(end));
            ASSERT(std::holds_alternative<T>(start));
            return math::interpolate(std::get<T>(start), std::get<T>(end), t, method);
        }
    private:
        std::shared_ptr<const MaterialActuatorClass> mClass;
        MaterialParamMap mStartValues;
    };

} // namespace
