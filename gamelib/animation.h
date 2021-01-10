// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#pragma once

#ifndef LOGTAG
#  define LOGTAG "gamelib"
#endif

#include "config.h"

#include "warnpush.h"
#  include <glm/vec2.hpp>
#  include <nlohmann/json_fwd.hpp>
#include "warnpop.h"

#include <string>
#include <vector>
#include <memory>

#include "base/utility.h"
#include "base/math.h"

namespace game
{
    class EntityNode;

    // ActuatorClass defines an interface for classes of actuators.
    // Actuators are objects that modify the state of some render
    // tree node over time. For example a Kinematic actuator will
    // perform linear interpolation of the node's transform over time.
    class ActuatorClass
    {
    public:
        // The type of the actuator class.
        enum class Type {
            // Animatic actuators modify the transform state of the node
            // i.e. the translation, scale and rotation variables directly.
            Animatic,
            // Kinematic actuators modify the kinematic physics properties
            // for example, linear or angular velocity, of the node's rigid body.
            // This will result in a kinematically driven change in the nodes
            // transform.
            Kinematic,
            // Material actuators modify the material instance state/parameters
            // of some particular animation node.
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
        virtual nlohmann::json ToJson() const = 0;
        // Load the actuator class object state from JSON. Returns true
        // successful otherwise false and the object is not valid state.
        virtual bool FromJson(const nlohmann::json& object) = 0;
    private:
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
        virtual nlohmann::json ToJson() const override;
        virtual bool FromJson(const nlohmann::json& json) override;

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

    // MaterialActuatorClass holds the data to change a node's
    // material in some particular way. The changeable parameters
    // are the possible material instance parameters.
    class MaterialActuatorClass : public ActuatorClass
    {
    public:
        // The interpolation method.
        using Interpolation = math::Interpolation;

        MaterialActuatorClass()
        { mId = base::RandomString(10); }
        Interpolation GetInterpolation() const
        { return mInterpolation; }
        void SetInterpolation(Interpolation method)
        { mInterpolation = method; }
        float GetEndAlpha() const
        { return mEndAlpha; }
        void SetEndAlpha(float alpha)
        { mEndAlpha = alpha; }
        virtual void SetNodeId(const std::string& id) override
        { mNodeId = id; }
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetNodeId() const override
        { return mNodeId; }
        virtual std::size_t GetHash() const override;
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
        virtual nlohmann::json ToJson() const override;
        virtual bool FromJson(const nlohmann::json& json) override;
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
        // Material parameters to change over time.
        // The ending alpha value.
        float mEndAlpha = 1.0f;
    };

    // TransformActuatorClass holds the transform data for some
    // particular type of linear transform of a node.
    class AnimaticActuatorClass : public ActuatorClass
    {
    public:
        // The interpolation method.
        using Interpolation = math::Interpolation;

        AnimaticActuatorClass()
        { mId = base::RandomString(10); }
        AnimaticActuatorClass(const std::string& node) : mNodeId(node)
        { mId = base::RandomString(10); }
        virtual Type GetType() const override
        { return Type::Animatic; }
        virtual std::string GetNodeId() const
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

        virtual nlohmann::json ToJson() const override;
        virtual bool FromJson(const nlohmann::json& json) override;
        virtual std::size_t GetHash() const override;

        virtual std::unique_ptr<ActuatorClass> Copy() const override
        { return std::make_unique<AnimaticActuatorClass>(*this); }
        virtual std::unique_ptr<ActuatorClass> Clone() const override
        {
            auto ret = std::make_unique<AnimaticActuatorClass>(*this);
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

    // An instance of ActuatorClass object.
    class Actuator
    {
    public:
        using Type = ActuatorClass::Type;
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

    // Apply a material change on a node's material instance.
    // todo: refactor the API for material actuator so that the
    // change is applied on a gfx::Material. The material lives
    // inside the renderer, so this needs to be worked somehow
    // into Renderer::Update
    class MaterialActuator : public Actuator
    {
    public:
        MaterialActuator(const std::shared_ptr<const MaterialActuatorClass>& klass)
           : mClass(klass)
        {}
        MaterialActuator(const MaterialActuatorClass& klass)
            : mClass(std::make_shared<MaterialActuatorClass>(klass))
        {}
        MaterialActuator(MaterialActuatorClass&& klass)
            : mClass(std::make_shared<MaterialActuatorClass>(std::move(klass)))
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
        std::shared_ptr<const MaterialActuatorClass> mClass;
        float mStartAlpha = 1.0f;
    };

    // Animatic actuator instance.
    class AnimaticActuator : public Actuator
    {
    public:
        AnimaticActuator(const std::shared_ptr<const AnimaticActuatorClass>& klass)
            : mClass(klass)
        {}
        AnimaticActuator(const AnimaticActuatorClass& klass)
            : mClass(std::make_shared<AnimaticActuatorClass>(klass))
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
        { return std::make_unique<AnimaticActuator>(*this); }
    private:
        std::shared_ptr<const AnimaticActuatorClass> mClass;
        // The starting state for the transformation.
        // the transform actuator will then interpolate between the
        // current starting and expected ending state.
        glm::vec2 mStartPosition = {0.0f, 0.0f};
        glm::vec2 mStartSize  = {1.0f, 1.0f};
        glm::vec2 mStartScale = {1.0f, 1.0f};
        float mStartRotation  = 0.0f;
    };

    // AnimationTrackClass defines a new type of animation track that includes
    // the static state of the animation such as the modified ending results
    // of the nodes involved.
    class AnimationTrackClass
    {
    public:
        AnimationTrackClass()
        { mId = base::RandomString(10); }
        // Create a deep copy of the class object.
        AnimationTrackClass(const AnimationTrackClass& other);
        AnimationTrackClass(AnimationTrackClass&& other);

        // Set the human readable name for the animation track.
        void SetName(const std::string& name)
        { mName = name; }
        // Set the duration for the animation track. These could be seconds
        // but ultimately it's up to the application to define what is the
        // real world meaning of the units in question.
        void SetDuration(float duration)
        { mDuration = duration; }
        // Enable/disable looping flag. A looping animation will never end
        // and will reset after the reaching the end. I.e. all the actuators
        // involved will have their states reset to the initial state which
        // will be re-applied to the node instances. For an animation without
        // any perceived jumps or discontinuity it's important that the animation
        // should transform nodes back to their initial state before the end
        // of the animation track.
        void SetLooping(bool looping)
        { mLooping = looping; }

        // Get the human readable name of the animation track.
        std::string GetName() const
        { return mName; }
        // Get the id of this animation class object.
        std::string GetId() const
        { return mId; }
        // Get the normalized duration of the animation track.
        float GetDuration() const
        { return mDuration; }
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
        nlohmann::json ToJson() const;

        // Try to create new instance of AnimationTrackClass based on the data
        // loaded from JSON. On failure returns std::nullopt otherwise returns
        // an instance of the class object.
        static std::optional<AnimationTrackClass> FromJson(const nlohmann::json& json);

        AnimationTrackClass Clone() const;
        // Do a deep copy on the assignment of a new object.
        AnimationTrackClass& operator=(const AnimationTrackClass& other);
    private:
        std::string mId;
        // The list of animation actuators that apply transforms
        std::vector<std::shared_ptr<ActuatorClass>> mActuators;
        // Human readable name of the track.
        std::string mName;
        // the duration of this track.
        float mDuration = 1.0f;
        // Loop animation or not. If looping then never completes.
        bool mLooping = false;
    };


    // AnimationTrack is an instance of some type of AnimationTrackClass.
    // It contains the per instance data of the animation track which is
    // modified over time through updates to the track and its actuators states.
    class AnimationTrack
    {
    public:
        // Create a new animation track based on the given class object.
        AnimationTrack(const std::shared_ptr<const AnimationTrackClass>& klass);
        // Create a new animation track based on the given class object
        // which will be copied.
        AnimationTrack(const AnimationTrackClass& klass);
        // Create a new animation track based on the given class object.
        AnimationTrack(const AnimationTrack& other);
        // Move ctor.
        AnimationTrack(AnimationTrack&& other);

        // Update the animation track state.
        void Update(float dt);
        // Apply state/transition updates/interpolated intermediate states (if any)
        // onto the given node.
        void Apply(EntityNode& node) const;
        // Prepare the animation track to restart.
        void Restart();
        // Returns true if the animation is complete, i.e. all the
        // actions have been performed.
        bool IsComplete() const;

        // Returns whether the animation is looping or not.
        bool IsLooping() const
        { return mClass->IsLooping(); }
        // Get the human readable name of the animation track.
        std::string GetName() const
        { return mClass->GetName(); }
        // get the current time.
        float GetCurrentTime() const
        { return mCurrentTime; }
        // Access for the tracks class object.
        const AnimationTrackClass& GetClass() const
        { return *mClass; }
        // Shortcut operator for accessing the members of the track's class object.
        const AnimationTrackClass* operator->() const
        { return mClass.get(); }
    private:
        // the class object
        std::shared_ptr<const AnimationTrackClass> mClass;
        // For each node we keep a list of actions that are to be performed
        // at specific times.
        struct NodeTrack {
            std::string node;
            std::unique_ptr<Actuator> actuator;
            mutable bool started = false;
            mutable bool ended   = false;
        };
        std::vector<NodeTrack> mTracks;

        // current play back time for this track.
        float mCurrentTime = 0.0f;
    };

    std::unique_ptr<AnimationTrack> CreateAnimationTrackInstance(std::shared_ptr<const AnimationTrackClass> klass);

} // namespace
