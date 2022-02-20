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

#ifndef LOGTAG
#  define LOGTAG "engine"
#endif

#include "config.h"

#include "warnpush.h"
#  include <glm/vec2.hpp>
#include "warnpop.h"

#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <optional>

#include "base/math.h"
#include "base/utility.h"
#include "data/fwd.h"
#include "game/types.h"

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
            // SetFlag actuator sets a a binary flag to the specific state on the node.
            SetFlag,
            // Material actuator changes material parameters
            Material
        };
        // dtor.
        virtual ~ActuatorClass() = default;
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
        // Get the type of the represented actuator.
        virtual Type GetType() const = 0;
        // Get the normalized start time when this actuator starts.
        virtual float GetStartTime() const = 0;
        // Get the normalized duration of this actuator.
        virtual float GetDuration() const = 0;
        // Set a new start time for the actuator.
        virtual void SetStartTime(float start) = 0;
        // Set the new duration value for the actuator.
        virtual void SetDuration(float duration) = 0;
        // Set the ID of the node affected by this actuator.
        virtual void SetNodeId(const std::string& id) = 0;
        // Serialize the actuator class object into JSON.
        virtual void IntoJson(data::Writer& data) const = 0;
        // Load the actuator class object state from JSON. Returns true
        // successful otherwise false and the object is not valid state.
        virtual bool FromJson(const data::Reader& data) = 0;
    private:
    };

    class SetFlagActuatorClass : public ActuatorClass
    {
    public:
        enum class FlagName {
            Drawable_VisibleInGame,
            Drawable_UpdateMaterial,
            Drawable_UpdateDrawable,
            Drawable_Restart,
            Drawable_FlipVertically,
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

        SetFlagActuatorClass()
        { mId = base::RandomString(10); }
        SetFlagActuatorClass(const std::string& node) : mNodeId(node)
        { mId = base::RandomString(10); }
        virtual Type GetType() const override
        { return Type::SetFlag;}
        virtual void SetNodeId(const std::string& id) override
        { mNodeId = id; }
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetNodeId() const override
        { return mNodeId; }
        virtual std::size_t GetHash() const override;
        virtual std::unique_ptr<ActuatorClass> Copy() const override
        { return std::make_unique<SetFlagActuatorClass>(*this); }
        virtual std::unique_ptr<ActuatorClass> Clone() const override
        {
            auto ret = std::make_unique<SetFlagActuatorClass>(*this);
            ret->mId = base::RandomString(10);
            return ret;
        }
        virtual float GetStartTime() const override
        { return mStartTime; }
        virtual float GetDuration() const override
        { return mDuration; }
        virtual void SetStartTime(float start) override
        { mStartTime = start; }
        virtual void SetDuration(float duration) override
        { mDuration = duration; }
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
        // id of the actuator.
        std::string mId;
        // id of the node that the action will be applied onto
        std::string mNodeId;
        // Normalized start time.
        float mStartTime = 0.0f;
        // Normalized duration.
        float mDuration = 1.0f;
        FlagAction mFlagAction = FlagAction::Off;
        FlagName   mFlagName   = FlagName::Drawable_FlipVertically;
    };

    // Modify the kinematic physics body properties, i.e.
    // the instantaneous linear and angular velocities.
    class KinematicActuatorClass : public ActuatorClass
    {
    public:
        // The interpolation method.
        using Interpolation = math::Interpolation;

        KinematicActuatorClass()
        { mId = base::RandomString(10); }
        Interpolation GetInterpolation() const
        { return mInterpolation; }
        void SetInterpolation(Interpolation method)
        { mInterpolation = method; }
        glm::vec2 GetEndLinearVelocity() const
        { return mEndLinearVelocity; }
        float GetEndAngularVelocity() const
        { return mEndAngularVelocity;}
        void SetEndLinearVelocity(const glm::vec2 velocity)
        { mEndLinearVelocity = velocity; }
        void SetEndAngularVelocity(float velocity)
        { mEndAngularVelocity = velocity; }
        virtual void SetNodeId(const std::string& id) override
        { mNodeId = id; }
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetNodeId() const override
        { return mNodeId; }
        virtual std::size_t GetHash() const override;
        virtual std::unique_ptr<ActuatorClass> Copy() const override
        { return std::make_unique<KinematicActuatorClass>(*this); }
        virtual std::unique_ptr<ActuatorClass> Clone() const override
        {
            auto ret = std::make_unique<KinematicActuatorClass>(*this);
            ret->mId = base::RandomString(10);
            return ret;
        }
        virtual Type GetType() const override
        { return Type::Kinematic; }
        virtual float GetStartTime() const override
        { return mStartTime; }
        virtual float GetDuration() const override
        { return mDuration; }
        virtual void SetStartTime(float start) override
        { mStartTime = start; }
        virtual void SetDuration(float duration) override
        { mDuration = duration; }
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;

    private:
        // id of the actuator.
        std::string mId;
        // id of the node that the action will be applied on.
        std::string mNodeId;
        // the interpolation method to be used.
        Interpolation mInterpolation = Interpolation::Linear;
        // Normalized start time of the action
        float mStartTime = 0.0f;
        // Normalized duration of the action.
        float mDuration = 1.0f;
        // The ending linear velocity in meters per second.
        glm::vec2 mEndLinearVelocity = {0.0f, 0.0f};
        // The ending angular velocity in radians per second.
        float mEndAngularVelocity = 0.0f;
    };

    // Modify node parameter value over time.
    class SetValueActuatorClass : public ActuatorClass
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

        SetValueActuatorClass()
        { mId = base::RandomString(10); }
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
        virtual void SetNodeId(const std::string& id) override
        { mNodeId = id; }
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetNodeId() const override
        { return mNodeId; }
        virtual std::size_t GetHash() const override;
        virtual std::unique_ptr<ActuatorClass> Copy() const override
        { return std::make_unique<SetValueActuatorClass>(*this); }
        virtual std::unique_ptr<ActuatorClass> Clone() const override
        {
            auto ret = std::make_unique<SetValueActuatorClass>(*this);
            ret->mId = base::RandomString(10);
            return ret;
        }
        virtual Type GetType() const override
        { return Type::SetValue; }
        virtual float GetStartTime() const override
        { return mStartTime; }
        virtual float GetDuration() const override
        { return mDuration; }
        virtual void SetStartTime(float start) override
        { mStartTime = start; }
        virtual void SetDuration(float duration) override
        { mDuration = duration; }
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;
    private:
        // id of the actuator.
        std::string mId;
        // id of the node that the action will be applied on.
        std::string mNodeId;
        // the interpolation method to be used.
        Interpolation mInterpolation = Interpolation::Linear;
        // which parameter to adjust
        ParamName mParamName = ParamName::DrawableTimeScale;
        // Normalized start time of the action
        float mStartTime = 0.0f;
        // Normalized duration of the action.
        float mDuration = 1.0f;
        // the end value
        ParamValue mEndValue;
    };

    // TransformActuatorClass holds the transform data for some
    // particular type of linear transform of a node.
    class TransformActuatorClass : public ActuatorClass
    {
    public:
        // The interpolation method.
        using Interpolation = math::Interpolation;

        TransformActuatorClass()
        { mId = base::RandomString(10); }
        TransformActuatorClass(const std::string& node) : mNodeId(node)
        { mId = base::RandomString(10); }
        virtual Type GetType() const override
        { return Type::Transform; }
        virtual std::string GetNodeId() const override
        { return mNodeId; }
        virtual float GetStartTime() const override
        { return mStartTime; }
        virtual float GetDuration() const override
        { return mDuration; }
        virtual void SetStartTime(float start) override
        { mStartTime = math::clamp(0.0f, 1.0f, start); }
        virtual void SetDuration(float duration) override
        { mDuration = math::clamp(0.0f, 1.0f, duration); }
        virtual void SetNodeId(const std::string& id) override
        { mNodeId = id; }
        virtual std::string GetId() const override
        { return mId; }
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

        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;
        virtual std::size_t GetHash() const override;

        virtual std::unique_ptr<ActuatorClass> Copy() const override
        { return std::make_unique<TransformActuatorClass>(*this); }
        virtual std::unique_ptr<ActuatorClass> Clone() const override
        {
            auto ret = std::make_unique<TransformActuatorClass>(*this);
            ret->mId = base::RandomString(10);
            return ret;
        }

    private:
        // id of the actuator.
        std::string mId;
        // id of the node we're going to change.
        std::string mNodeId;
        // the interpolation method to be used.
        Interpolation mInterpolation = Interpolation::Linear;
        // Normalized start time.
        float mStartTime = 0.0f;
        // Normalized duration.
        float mDuration = 1.0f;
        // the ending state of the the transformation.
        // the ending position (translation relative to parent)
        glm::vec2 mEndPosition = {0.0f, 0.0f};
        // the ending size
        glm::vec2 mEndSize = {1.0f, 1.0f};
        // the ending scale.
        glm::vec2 mEndScale = {1.0f, 1.0f};
        // the ending rotation.
        float mEndRotation = 0.0f;
    };

    class MaterialActuatorClass : public ActuatorClass
    {
    public:
        using Interpolation = math::Interpolation;
        using MaterialParam = std::variant<float, int,
            Color4f,
            glm::vec2, glm::vec3, glm::vec4>;
        MaterialActuatorClass()
        { mId = base::RandomString(10); }
        Interpolation GetInterpolation() const
        { return mInterpolation; }
        std::string GetParamName() const
        { return mParamName; }
        MaterialParam GetParamValue() const
        { return mParamValue; }
        template<typename T>
        const T* GetParamValue() const
        { return std::get_if<T>(&mParamValue); }
        template<typename T>
        T* GetParamValue()
        { return std::get_if<T>(&mParamValue); }
        void SetParamName(const std::string& name)
        { mParamName = name; }
        void SetParamValue(const MaterialParam& value)
        { mParamValue = value; }
        void SetInterpolation(Interpolation method)
        { mInterpolation = method; }
        virtual void SetNodeId(const std::string& id) override
        { mNodeId = id; }
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetNodeId() const override
        { return mNodeId; }
        virtual std::unique_ptr<ActuatorClass> Copy() const override
        { return std::make_unique<MaterialActuatorClass>(*this); }
        virtual std::unique_ptr<ActuatorClass> Clone() const override
        {
            auto ret = std::make_unique<MaterialActuatorClass>(*this);
            ret->mId = base::RandomString(10);
            return ret;
        }
        virtual Type GetType() const override
        { return Type::Material; }
        virtual float GetStartTime() const override
        { return mStartTime; }
        virtual float GetDuration() const override
        { return mDuration; }
        virtual void SetStartTime(float start) override
        { mStartTime = start; }
        virtual void SetDuration(float duration) override
        { mDuration = duration; }
        virtual void IntoJson(data::Writer& data) const override;
        virtual bool FromJson(const data::Reader& data) override;
        virtual std::size_t GetHash() const override;
    private:
        // Id of the actuator class.
        std::string mId;
        // Id of the node class that the actuator applies on
        std::string mNodeId;
        // Interpolation method used to change the value.
        Interpolation mInterpolation = Interpolation::Linear;
        // Actuator start time on the timeline.
        float mStartTime = 0.0f;
        // Actuator duration on the timeline.
        float mDuration  = 1.0f;
        // The name of the material parameter that is going to be changed (uniform name)
        std::string mParamName;
        // The value of the material parameter.
        MaterialParam mParamValue;
    };

    // An instance of ActuatorClass object.
    class Actuator
    {
    public:
        using Type = ActuatorClass::Type;
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
        // Create an exact copy of this actuator object.
        virtual std::unique_ptr<Actuator> Copy() const = 0;
    private:
    };

    // Apply a kinematic change to a rigid body's linear or angular velocity.
    class KinematicActuator : public Actuator
    {
    public:
        KinematicActuator(const std::shared_ptr<const KinematicActuatorClass>& klass)
          : mClass(klass)
        {}
        KinematicActuator(const KinematicActuatorClass& klass)
          : mClass(std::make_shared<KinematicActuatorClass>(klass))
        {}
        KinematicActuator(KinematicActuatorClass&& klass)
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
        virtual std::unique_ptr<Actuator> Copy() const override
        { return std::make_unique<KinematicActuator>(*this); }
    private:
        std::shared_ptr<const KinematicActuatorClass> mClass;
        glm::vec2 mStartLinearVelocity = {0.0f, 0.0f};
        float mStartAngularVelocity = 0.0f;
    };

    class SetFlagActuator : public Actuator
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
        virtual std::unique_ptr<Actuator> Copy() const override
        { return std::make_unique<SetFlagActuator>(*this); }
        bool CanApply(EntityNode& node, bool verbose) const;
    private:
        std::shared_ptr<const SetFlagActuatorClass> mClass;
        bool mStartState = false;
    };

    // Modify node parameter over time.
    class SetValueActuator : public Actuator
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
        virtual std::unique_ptr<Actuator> Copy() const override
        { return std::make_unique<SetValueActuator>(*this); }
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
    class TransformActuator : public Actuator
    {
    public:
        TransformActuator(const std::shared_ptr<const TransformActuatorClass>& klass)
            : mClass(klass)
        {}
        TransformActuator(const TransformActuatorClass& klass)
            : mClass(std::make_shared<TransformActuatorClass>(klass))
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
        virtual std::unique_ptr<Actuator> Copy() const override
        { return std::make_unique<TransformActuator>(*this); }
    private:
        std::shared_ptr<const TransformActuatorClass> mClass;
        // The starting state for the transformation.
        // the transform actuator will then interpolate between the
        // current starting and expected ending state.
        glm::vec2 mStartPosition = {0.0f, 0.0f};
        glm::vec2 mStartSize  = {1.0f, 1.0f};
        glm::vec2 mStartScale = {1.0f, 1.0f};
        float mStartRotation  = 0.0f;
    };

    class MaterialActuator : public Actuator
    {
    public:
        using Interpolation = MaterialActuatorClass::Interpolation;
        using MaterialParam = MaterialActuatorClass::MaterialParam;
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
        virtual std::unique_ptr<Actuator> Copy() const override
        { return std::make_unique<MaterialActuator>(*this); }
    private:
        template<typename T>
        T Interpolate(float t)
        {
            const auto method = mClass->GetInterpolation();
            const auto end    = mClass->GetParamValue();
            const auto start  = mStartValue;
            ASSERT(std::holds_alternative<T>(end));
            ASSERT(std::holds_alternative<T>(start));
            return math::interpolate(std::get<T>(start), std::get<T>(end), t, method);
        }
    private:
        std::shared_ptr<const MaterialActuatorClass> mClass;
        MaterialParam mStartValue;
    };

    // AnimationClass defines a new type of animation that includes the
    // static state of the animation such as the modified ending results
    // of the nodes involved.
    class AnimationClass
    {
    public:
        AnimationClass();
        // Create a deep copy of the class object.
        AnimationClass(const AnimationClass& other);
        AnimationClass(AnimationClass&& other);

        // Set the human-readable name for the animation track.
        void SetName(const std::string& name)
        { mName = name; }
        // Set the animation duration in seconds.
        void SetDuration(float duration)
        { mDuration = duration; }
        // Set animation delay in seconds.
        void SetDelay(float delay)
        { mDelay = delay; }
        // Enable/disable looping flag. A looping animation will never end
        // and will reset after the reaching the end. I.e. all the actuators
        // involved will have their states reset to the initial state which
        // will be re-applied to the node instances. For an animation without
        // any perceived jumps or discontinuity it's important that the animation
        // should transform nodes back to their initial state before the end
        // of the animation track.
        void SetLooping(bool looping)
        { mLooping = looping; }

        // Get the human-readable name of the animation track.
        std::string GetName() const
        { return mName; }
        // Get the id of this animation class object.
        std::string GetId() const
        { return mId; }
        // Get the normalized duration of the animation track.
        float GetDuration() const
        { return mDuration; }
        float GetDelay() const
        { return mDelay; }
        // Returns true if the animation track is looping or not.
        bool IsLooping() const
        { return mLooping; }

        // Add a new actuator that applies state update/action on some animation node.
        template<typename Actuator>
        void AddActuator(const Actuator& actuator)
        {
            std::shared_ptr<ActuatorClass> foo(new Actuator(actuator));
            mActuators.push_back(std::move(foo));
        }
        // Add a new actuator that applies state update/action on some animation node.
        void AddActuator(std::shared_ptr<ActuatorClass> actuator)
        { mActuators.push_back(std::move(actuator)); }

        void DeleteActuator(size_t index);

        bool DeleteActuatorById(const std::string& id);

        ActuatorClass* FindActuatorById(const std::string& id);

        const ActuatorClass* FindActuatorById(const std::string& id) const;

        void Clear()
        { mActuators.clear(); }

        // Get the number of animation actuator class objects currently
        // in this animation track.
        size_t GetNumActuators() const
        { return mActuators.size(); }

        ActuatorClass& GetActuatorClass(size_t i)
        { return *mActuators[i]; }
        // Get the animation actuator class object at index i.
        const ActuatorClass& GetActuatorClass(size_t i) const
        { return *mActuators[i]; }

        // Create an instance of some actuator class type at the given index.
        // For example if the type of actuator class at index N is
        // TransformActuatorClass then the returned object will be an
        // instance of TransformActuator.
        std::unique_ptr<Actuator> CreateActuatorInstance(size_t i) const;

        // Get the hash value based on the static data.
        std::size_t GetHash() const;

        // Serialize into JSON.
        void IntoJson(data::Writer& json) const;

        // Try to create new instance of AnimationClass based on the data
        // loaded from JSON. On failure returns std::nullopt otherwise returns
        // an instance of the class object.
        static std::optional<AnimationClass> FromJson(const data::Reader& data);

        AnimationClass Clone() const;
        // Do a deep copy on the assignment of a new object.
        AnimationClass& operator=(const AnimationClass& other);
    private:
        std::string mId;
        // The list of animation actuators that apply transforms
        std::vector<std::shared_ptr<ActuatorClass>> mActuators;
        // Human-readable name of the animation.
        std::string mName;
        // One time delay before starting the playback.
        float mDelay = 0.0f;
        // the duration of this track.
        float mDuration = 1.0f;
        // Loop animation or not. If looping then never completes.
        bool mLooping = false;
    };


    // Animation is an instance of some type of AnimationClass.
    // It contains the per instance data of the animation track which is
    // modified over time through updates to the track and its actuators states.
    class Animation
    {
    public:
        // Create a new animation instance based on the given class object.
        Animation(const std::shared_ptr<const AnimationClass>& klass);
        // Create a new animation based on the given class object.
        // Makes a copy of the klass object.
        Animation(const AnimationClass& klass);
        // Create a new animation track based on the given class object.
        // Makes a copy of the klass object.
        Animation(const Animation& other);
        // Move ctor.
        Animation(Animation&& other);

        // Update the animation track state.
        void Update(float dt);
        // Apply animation actions, such as transformations or material changes
        // onto the given entity node.
        void Apply(EntityNode& node) const;
        // Prepare the animation track to restart.
        void Restart();
        // Returns true if the animation is complete, i.e. all the
        // actions have been performed.
        bool IsComplete() const;

        // Set one time animation delay that takes place
        // before the animation starts.
        void SetDelay(float delay)
        { mDelay = delay; }
        // Returns whether the animation is looping or not.
        bool IsLooping() const
        { return mClass->IsLooping(); }
        // Get the human-readable name of the animation track.
        std::string GetName() const
        { return mClass->GetName(); }
        // get the current time.
        float GetCurrentTime() const
        { return mCurrentTime; }
        float GetDelay() const
        { return mDelay; }
        // Access for the tracks class object.
        const AnimationClass& GetClass() const
        { return *mClass; }
        // Shortcut operator for accessing the members of the track's class object.
        const AnimationClass* operator->() const
        { return mClass.get(); }
    private:
        // the class object
        std::shared_ptr<const AnimationClass> mClass;
        // For each node we keep a list of actions that are to be performed
        // at specific times.
        struct NodeTrack {
            std::string node;
            std::unique_ptr<Actuator> actuator;
            mutable bool started = false;
            mutable bool ended   = false;
        };
        std::vector<NodeTrack> mTracks;
        // One time delay before starting the animation.
        float mDelay = 0.0f;
        // current play back time for this track.
        float mCurrentTime = 0.0f;
    };

    std::unique_ptr<Animation> CreateAnimationInstance(std::shared_ptr<const AnimationClass> klass);

} // namespace
