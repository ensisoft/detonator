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
#  include <glm/mat4x4.hpp>
#  include <glm/vec2.hpp>
#  include <glm/glm.hpp>
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <unordered_map>

#include "base/assert.h"
#include "base/bitflag.h"
#include "base/utility.h"
#include "base/logging.h"
#include "base/math.h"
#include "graphics/material.h"
#include "graphics/drawable.h"
#include "graphics/types.h"
#include "tree.h"

namespace gfx {
    class Drawable;
    class DrawableClass;
    class Material;
    class MaterialClass;
    class Painter;
    class Transform;
} // namespace

namespace game
{
    class ClassLibrary;

    // AnimationNodeClass holds the data for some particular type of an animation node.
    // This includes the drawable shape and the associated material and the node's
    // transformation data (relative to its parent).
    class AnimationNodeClass
    {
    public:
        enum class RenderPass {
            Draw,
            Mask
        };
        using RenderStyle = gfx::Drawable::Style;

        enum class Flags {
            // Only pertains to editor (todo: maybe this flag should be removed)
            VisibleInEditor,
            // Whether the node should generate render packets or not.
            DoesRender,
            // Whether the node should update material or not
            UpdateMaterial,
            // Whether the node should update drawable or not
            UpdateDrawable,
            // Restart drawables or not
            RestartDrawable,
            // Override the material alpha value.
            OverrideAlpha
        };

        // This will construct an "empty" node that cannot be
        // drawn yet. You'll need to se the various properties
        // for drawable/material etc.
        AnimationNodeClass();

        // Set the drawable object (shape) for this component.
        // The name identifies the resource in the gfx resource loader.
        void SetDrawable(const std::shared_ptr<const gfx::DrawableClass>& klass)
        {
            mDrawableId  = klass->GetId();
            mDrawableClass = klass;
            mDrawable.reset();
        }
        void SetDrawable(const std::string& id)
        {
            mDrawableId= id;
            mMaterialClass.reset();
            mMaterial.reset();
        }
        void ResetDrawable()
        {
            mDrawableId.clear();
            mDrawableClass.reset();
            mDrawable.reset();
        }
        void ResetMaterial()
        {
            mMaterialId.clear();
            mMaterialClass.reset();
            mMaterial.reset();
        }
        // Set the material object for this component.
        // The name identifies the runtime material resource in the gfx resource loader.
        void SetMaterial(const std::shared_ptr<const gfx::MaterialClass>& klass)
        {
            mMaterialId    = klass->GetId();
            mMaterialClass = klass;
            mMaterial.reset();
        }
        void SetMaterial(const std::string& id)
        {
            mMaterialId = id;
            mMaterialClass.reset();
            mMaterial.reset();
        }
        void SetTranslation(const glm::vec2& pos)
        { mPosition = pos; }
        void SetName(const std::string& name)
        { mName = name;}
        void SetScale(const glm::vec2& scale)
        { mScale = scale; }
        void SetSize(const glm::vec2& size)
        { mSize = size; }
        void SetLayer(int layer)
        { mLayer = layer; }
        void SetRenderPass(RenderPass pass)
        { mRenderPass = pass; }
        void SetRotation(float value)
        { mRotation = value; }
        void SetRenderStyle(RenderStyle style)
        { mRenderStyle = style; }
        void SetLineWidth(float value)
        { mLineWidth = value; }
        void SetAlpha(float value)
        { mAlpha = value; }
        void SetFlag(Flags f, bool on_off)
        { mBitFlags.set(f, on_off); }
        bool TestFlag(Flags f) const
        { return mBitFlags.test(f); }

        RenderPass GetRenderPass() const
        { return mRenderPass; }
        RenderStyle GetRenderStyle() const
        { return mRenderStyle; }
        int GetLayer() const
        { return mLayer; }
        std::string GetId() const
        { return mId; }
        std::string GetName() const
        { return mName; }
        std::shared_ptr<const gfx::MaterialClass> GetMaterialClass() const
        { return mMaterialClass; }
        std::string GetMaterialId() const
        { return mMaterialId; }
        std::shared_ptr<const gfx::DrawableClass> GetDrawableClass() const
        { return mDrawableClass; }
        std::string GetDrawableId() const
        { return mDrawableId; }
        glm::vec2 GetTranslation() const
        { return mPosition; }
        glm::vec2 GetSize() const
        { return mSize; }
        glm::vec2 GetScale() const
        { return mScale; }
        float GetRotation() const
        { return mRotation; }
        float GetLineWidth() const
        { return mLineWidth; }
        float GetAlpha() const
        { return mAlpha; }
        const base::bitflag<Flags>& GetFlags() const
        { return mBitFlags; }
        base::bitflag<Flags>& GetFlags()
        { return mBitFlags; }
        bool HasMaterial() const
        { return !mMaterialId.empty(); }
        bool HasDrawable() const
        { return !mDrawableId.empty(); }

        // Shim functions to support generic RenderTree draw even for the
        // class object.
        std::shared_ptr<const gfx::Drawable> GetDrawable() const;
        std::shared_ptr<const gfx::Material> GetMaterial() const;
        std::shared_ptr<gfx::Drawable> GetDrawable();
        std::shared_ptr<gfx::Material> GetMaterial();

        void SetDrawableInstance(std::unique_ptr<gfx::Drawable> drawable)
        { mDrawable = std::move(drawable); }
        void SetMaterialInstance(std::unique_ptr<gfx::Material> material)
        { mMaterial = std::move(material); }

        // Create new instance of drawable for this type of node.
        std::unique_ptr<gfx::Drawable> CreateDrawableInstance() const;

        // Create new instance of material for this type of node.
        std::unique_ptr<gfx::Material> CreateMaterialInstance() const;

        // Get this node's transformation that applies
        // to the hierarchy of nodes.
        glm::mat4 GetNodeTransform() const;

        // Get this node's transformation that applies to this
        // node's drawable object(s).
        glm::mat4 GetModelTransform() const;

        // Get the node hash based on the member values.
        std::size_t GetHash() const;

        // Update node, i.e. its material and drawable if the relevant
        // update flags are set.
        void Update(float time, float dt);

        // Reset runtime state to empty. If you call this you'll need to
        // call Prepare also again.
        void Reset();

        // Prepare this animation node class object
        // by loading all the needed runtime resources.
        void LoadDependentClasses(const ClassLibrary& loader) const;

        // Make a new unique copy of this animation node class with
        // all the same properties but a different unique ID.
        AnimationNodeClass Clone() const;

        // serialize the component properties into JSON.
        nlohmann::json ToJson() const;

        // Create a new AnimationNodeClass object based on the JSON.
        // Returns std::nullopt if the deserialization failed.
        // Note that this does not yet create/load any runtime objects
        // such as materials or such but they are loaded later when the
        // class object is prepared.
        static std::optional<AnimationNodeClass> FromJson(const nlohmann::json& object);
    private:
        // generic properties.
        std::string mId;
        // this is the human readable name and can be used when for example
        // programmatically looking up an animation node.
        std::string mName;
        // visual properties. we keep the material/drawable names
        // around so that we we know which resources to load at runtime.
        mutable std::string mMaterialId;
        mutable std::string mDrawableId;
        mutable std::shared_ptr<const gfx::MaterialClass> mMaterialClass;
        mutable std::shared_ptr<const gfx::DrawableClass> mDrawableClass;
        // transformation properties.
        // translation offset relative to the animation.
        glm::vec2 mPosition = {0.0f, 0.0f};
        // size is the size of this object in some units
        // (for example pixels)
        glm::vec2 mSize = {1.0f, 1.0f};
        // scale applies an additional scale to this hierarchy.
        glm::vec2 mScale = {1.0f, 1.0f};
        // rotation around z axis positive rotation is CW
        float mRotation = 0.0f;
        // alpha value. 0.0f = fully transparent 1.0f = fully opaque.
        // only works with materials that support alpha blending.
        float mAlpha = 1.0f;
        // rendering properties. which layer and which pass.
        int mLayer = 0;
        RenderPass mRenderPass = RenderPass::Draw;
        RenderStyle mRenderStyle = RenderStyle::Solid;
        // applies only when RenderStyle is either Outline or Wireframe
        float mLineWidth = 1.0f;
        // bitflags that apply to node.
        base::bitflag<Flags> mBitFlags;
        // cached material instance and drawable instance
        // to avoid having to recreate new instances all the time
        // when the *class* object is drawn.
        mutable std::shared_ptr<gfx::Material> mMaterial;
        mutable std::shared_ptr<gfx::Drawable> mDrawable;
    };

    // AnimationNode is an instance of some specific type of AnimationNodeClass
    // and it contains the per instance animation node data.
    class AnimationNode
    {
    public:
        using Flags = AnimationNodeClass::Flags;
        using RenderPass = AnimationNodeClass::RenderPass;

        AnimationNode(const std::shared_ptr<const AnimationNodeClass>& klass)
            : mClass(klass)
        {
            Reset();
        }
        AnimationNode(const AnimationNodeClass& klass)
            : AnimationNode(std::make_shared<AnimationNodeClass>(klass))
        {}

        // setters for instance state.
        void SetTranslation(float x, float y)
        { mPosition = glm::vec2(x, y); }
        void SetTranslation(const glm::vec2& pos)
        { mPosition = pos; }
        void SetScale(float x, float y)
        { mScale = glm::vec2(x, y); }
        void SetScale(float scale)
        { mScale = glm::vec2(scale, scale); }
        void SetScale(const glm::vec2& scale)
        { mScale = scale; }
        void SetSize(float x, float y)
        { mSize = glm::vec2(x, y); }
        void SetSize(const glm::vec2& size)
        { mSize = size; }
        void SetRotation(float value)
        { mRotation = value; }
        void SetAlpha(float value)
        { mAlpha = value; }

        // getters for instance state.
        glm::vec2 GetTranslation() const
        { return mPosition; }
        glm::vec2 GetSize() const
        { return mSize; }
        glm::vec2 GetScale() const
        { return mScale; }
        float GetRotation() const
        { return mRotation; }
        std::shared_ptr<const gfx::Material> GetMaterial() const;
        std::shared_ptr<const gfx::Drawable> GetDrawable() const;
        std::shared_ptr<gfx::Material> GetMaterial();
        std::shared_ptr<gfx::Drawable> GetDrawable();

        // Getters for class object state.
        bool TestFlag(AnimationNodeClass::Flags flags) const
        { return mClass->TestFlag(flags); }
        std::shared_ptr<const gfx::MaterialClass> GetMaterialClass() const
        { return mClass->GetMaterialClass(); }
        std::shared_ptr<const gfx::DrawableClass> GetDrawableClass() const
        { return mClass->GetDrawableClass(); }
        AnimationNodeClass::RenderPass GetRenderPass() const
        { return mClass->GetRenderPass(); }
        int GetLayer() const
        { return mClass->GetLayer(); }
        std::string GetName() const
        { return mClass->GetName(); }
        std::string GetId() const
        { return mClass->GetId(); }
        float GetLineWidth() const
        { return mClass->GetLineWidth(); }
        float GetAlpha() const
        { return mAlpha; }

        // Reset node's state to the initial state
        void Reset();

        //  Update node's state.
        void Update(float time, float dt);

        // Get this node's transformation that applies
        // to the hierarchy of nodes.
        glm::mat4 GetNodeTransform() const;

        // Get this node's transformation that applies to this
        // node's drawable object(s).
        glm::mat4 GetModelTransform() const;

        // accessor the node's class type object.
        const AnimationNodeClass& GetClass() const
        { return *mClass.get(); }

        // shortcut operator for accessing the members of the node's class type.
        const AnimationNodeClass* operator->() const
        { return mClass.get(); }

    private:
        // the static class object.
        std::shared_ptr<const AnimationNodeClass> mClass;
        // the material instance.
        std::shared_ptr<gfx::Material> mMaterial;
        // the drawable instance.
        std::shared_ptr<gfx::Drawable> mDrawable;
        // the instance state of this animation node.
        // transformation properties.
        // translation offset relative to the parent node.
        glm::vec2 mPosition = {0.0f, 0.0f};
        // size of the node in absolute units (not relative to parent)
        glm::vec2 mSize = {1.0f, 1.0f};
        // scale of the node. applies to the hierarchy. could be
        // used to define the "size" of the node relative to the parent.
        glm::vec2 mScale = {1.0f, 1.0f};
        // rotation of the node (relative to parent)
        float mRotation = 0.0f;
        // current alpha value. 0.0f = fully transparent
        // 1.0f = fully opaque. only works with materials
        // that support transparency (alpha blending)
        float mAlpha = 1.0f;
    };

    // AnimationActuatorClass defines an interface for classes of
    // animation actuators. Animation actuators are objects that
    // modify the state of some animation nodes possibly over time
    // by for example doing interpolation between different nodes states
    // or by toggling some state flags at specific points in time.
    class AnimationActuatorClass
    {
    public:
        // The type of the actuator class.
        enum class Type {
            // Transform actuators modify the transform state of the node
            // i.e. the translation, scale and rotation variables.
            Transform,
            // Material actuators modify the material instance state/parameters
            // of some particular animation node.
            Material
        };
        // dtor.
        virtual ~AnimationActuatorClass() = default;
        // Get the id of this actuator
        virtual std::string GetId() const = 0;
        // Get the ID of the node affected by this actuator.
        virtual std::string GetNodeId() const = 0;
        // Get the hash of the object state.
        virtual std::size_t GetHash() const = 0;
        // Create an exact copy of this actuator class object.
        virtual std::unique_ptr<AnimationActuatorClass> Copy() const = 0;
        // Create a new actuator class instance with same property values
        // with this object but with a unique id.
        virtual std::unique_ptr<AnimationActuatorClass> Clone() const = 0;
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
        // Serialize the actuator class object into JSON.
        virtual nlohmann::json ToJson() const = 0;
        // Load the actuator class object state from JSON. Returns true
        // successful otherwise false and the object is not valid state.
        virtual bool FromJson(const nlohmann::json& object) = 0;
    private:
    };

    // MaterialActuatorClass holds the data to change a node's
    // material in some particular way. The changeable parameters
    // are the possible material instance parameters.
    class MaterialActuatorClass : public AnimationActuatorClass
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
        void SetNodeId(const std::string& id)
        { mNodeId = id; }
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetNodeId() const override
        { return mNodeId; }
        virtual std::size_t GetHash() const override
        {
            std::size_t hash = 0;
            hash = base::hash_combine(hash, mId);
            hash = base::hash_combine(hash, mNodeId);
            hash = base::hash_combine(hash, mInterpolation);
            hash = base::hash_combine(hash, mStartTime);
            hash = base::hash_combine(hash, mDuration);
            hash = base::hash_combine(hash, mEndAlpha);
            return hash;
        }
        virtual std::unique_ptr<AnimationActuatorClass> Copy() const override
        { return std::make_unique<MaterialActuatorClass>(*this); }
        virtual std::unique_ptr<AnimationActuatorClass> Clone() const override
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
        virtual nlohmann::json ToJson() const override
        {
            nlohmann::json json;
            base::JsonWrite(json, "id", mId);
            base::JsonWrite(json, "node", mNodeId);
            base::JsonWrite(json, "method", mInterpolation);
            base::JsonWrite(json, "starttime", mStartTime);
            base::JsonWrite(json, "duration", mDuration);
            base::JsonWrite(json, "alpha", mEndAlpha);
            return json;
        }
        virtual bool FromJson(const nlohmann::json& json) override
        {
            return base::JsonReadSafe(json, "id", &mId) &&
                   base::JsonReadSafe(json, "node", &mNodeId) &&
                   base::JsonReadSafe(json, "method", &mInterpolation) &&
                   base::JsonReadSafe(json, "starttime", &mStartTime) &&
                   base::JsonReadSafe(json, "duration", &mDuration) &&
                   base::JsonReadSafe(json, "alpha", &mEndAlpha);
        }
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

    // AnimationTransformActuatorClass holds the transform data for some
    // particular type of transform type object.
    class AnimationTransformActuatorClass : public AnimationActuatorClass
    {
    public:
        // The interpolation method.
        using Interpolation = math::Interpolation;

        AnimationTransformActuatorClass()
        { mId = base::RandomString(10); }
        AnimationTransformActuatorClass(const std::string& node) : mNodeId(node)
        { mId = base::RandomString(10); }
        virtual Type GetType() const override
        { return Type::Transform; }
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


        void SetNodeId(const std::string& id)
        { mNodeId = id; }

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

        virtual nlohmann::json ToJson() const override
        {
            nlohmann::json json;
            base::JsonWrite(json, "id", mId);
            base::JsonWrite(json, "node",      mNodeId);
            base::JsonWrite(json, "method",    mInterpolation);
            base::JsonWrite(json, "starttime", mStartTime);
            base::JsonWrite(json, "duration",  mDuration);
            base::JsonWrite(json, "position",  mEndPosition);
            base::JsonWrite(json, "size",      mEndSize);
            base::JsonWrite(json, "scale",     mEndScale);
            base::JsonWrite(json, "rotation",  mEndRotation);
            return json;
        }
        virtual bool FromJson(const nlohmann::json& json) override
        {
            return base::JsonReadSafe(json, "id", &mId) &&
                   base::JsonReadSafe(json, "node", &mNodeId) &&
                   base::JsonReadSafe(json, "starttime", &mStartTime) &&
                   base::JsonReadSafe(json, "duration",  &mDuration) &&
                   base::JsonReadSafe(json, "position",  &mEndPosition) &&
                   base::JsonReadSafe(json, "size",      &mEndSize) &&
                   base::JsonReadSafe(json, "scale",     &mEndScale) &&
                   base::JsonReadSafe(json, "rotation",  &mEndRotation) &&
                   base::JsonReadSafe(json, "method",    &mInterpolation);
        }
        virtual std::size_t GetHash() const override
        {
            std::size_t hash = 0;
            hash = base::hash_combine(hash, mId);
            hash = base::hash_combine(hash, mNodeId);
            hash = base::hash_combine(hash, mInterpolation);
            hash = base::hash_combine(hash, mStartTime);
            hash = base::hash_combine(hash, mDuration);
            hash = base::hash_combine(hash, mEndPosition);
            hash = base::hash_combine(hash, mEndSize);
            hash = base::hash_combine(hash, mEndScale);
            hash = base::hash_combine(hash, mEndRotation);
            return hash;
        }
        virtual std::unique_ptr<AnimationActuatorClass> Copy() const override
        { return std::make_unique<AnimationTransformActuatorClass>(*this); }
        virtual std::unique_ptr<AnimationActuatorClass> Clone() const override
        {
            auto ret = std::make_unique<AnimationTransformActuatorClass>(*this);
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

    // Apply action/transformation or some other change on an animation node
    // possibly by using an interpolation between the starting and the ending
    // state.
    class AnimationActuator
    {
    public:
        using Type = AnimationActuatorClass::Type;
        // Start the action/transition to be applied by this actuator.
        // The node is the node in question that the changes will be applied to.
        virtual void Start(AnimationNode& node) = 0;
        // Apply an interpolation of the state based on the time value t onto to the node.
        virtual void Apply(AnimationNode& node, float t) = 0;
        // Finish the action/transition to be applied by this actuator.
        // The node is the node in question that the changes will (were) applied to.
        virtual void Finish(AnimationNode& node) = 0;
        // Get the normalized start time when this actuator begins to take effect.
        virtual float GetStartTime() const = 0;
        // Get the normalized duration of the duration of the actuator's transformation.
        virtual float GetDuration() const = 0;
        // Get the id of the node that will be modified by this actuator.
        virtual std::string GetNodeId() const = 0;
        // Create an exact copy of this actuator object.
        virtual std::unique_ptr<AnimationActuator> Copy() const = 0;
    private:
    };

    // Apply a material change on a node's material instance.
    class MaterialActuator : public AnimationActuator
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
        virtual void Start(AnimationNode& node) override
        {
            const bool has_alpha_blending = node.GetMaterialClass()->GetSurfaceType() ==
                    gfx::MaterialClass::SurfaceType::Transparent;
            if (!has_alpha_blending) {
                WARN("Material doesn't enable blending.");
            }

            if (node.TestFlag(AnimationNode::Flags::OverrideAlpha))
                mStartAlpha = node.GetAlpha();
            else mStartAlpha = node.GetMaterial()->GetAlpha();
        }
        virtual void Apply(AnimationNode& node, float t) override
        {
            const auto method = mClass->GetInterpolation();
            const float value = math::interpolate(mStartAlpha, mClass->GetEndAlpha(), t, method);
            if (node.TestFlag(AnimationNode::Flags::OverrideAlpha))
                node.SetAlpha(value);
            else node.GetMaterial()->SetAlpha(value);
        }
        virtual void Finish(AnimationNode& node) override
        {
            if (node.TestFlag(AnimationNode::Flags::OverrideAlpha))
                node.SetAlpha(mClass->GetEndAlpha());
            else node.GetMaterial()->SetAlpha(mClass->GetEndAlpha());
        }
        virtual float GetStartTime() const override
        { return mClass->GetStartTime(); }
        virtual float GetDuration() const override
        { return mClass->GetDuration(); }
        virtual std::string GetNodeId() const override
        { return mClass->GetNodeId(); }
        virtual std::unique_ptr<AnimationActuator> Copy() const override
        { return std::make_unique<MaterialActuator>(*this); }
    private:
        std::shared_ptr<const MaterialActuatorClass> mClass;
        float mStartAlpha = 1.0f;
    };

    // Apply a change to node's transformation, i.e. one of the following
    // properties: size, position or translation.
    class AnimationTransformActuator : public AnimationActuator
    {
    public:
        AnimationTransformActuator(const std::shared_ptr<const AnimationTransformActuatorClass>& klass)
            : mClass(klass)
        {}
        AnimationTransformActuator(const AnimationTransformActuatorClass& klass)
            : mClass(std::make_shared<AnimationTransformActuatorClass>(klass))
        {}
        virtual void Start(AnimationNode& node) override
        {
            mStartPosition = node.GetTranslation();
            mStartSize     = node.GetSize();
            mStartScale    = node.GetScale();
            mStartRotation = node.GetRotation();
        }
        virtual void Apply(AnimationNode& node, float t) override
        {
            // apply interpolated state on the node.
            const auto method = mClass->GetInterpolation();
            const auto& p = math::interpolate(mStartPosition, mClass->GetEndPosition(), t, method);
            const auto& s = math::interpolate(mStartSize,     mClass->GetEndSize(),     t, method);
            const auto& r = math::interpolate(mStartRotation, mClass->GetEndRotation(), t, method);
            const auto& f = math::interpolate(mStartScale,    mClass->GetEndScale(),    t, method);
            node.SetTranslation(p);
            node.SetSize(s);
            node.SetRotation(r);
            node.SetScale(f);
        }
        virtual void Finish(AnimationNode& node) override
        {
            node.SetTranslation(mClass->GetEndPosition());
            node.SetRotation(mClass->GetEndRotation());
            node.SetSize(mClass->GetEndSize());
            node.SetScale(mClass->GetEndScale());
        }
        virtual float GetStartTime() const override
        { return mClass->GetStartTime(); }
        virtual float GetDuration() const override
        { return mClass->GetDuration(); }
        virtual std::string GetNodeId() const override
        { return mClass->GetNodeId(); }
        virtual std::unique_ptr<AnimationActuator> Copy() const override
        { return std::make_unique<AnimationTransformActuator>(*this); }
    private:
        std::shared_ptr<const AnimationTransformActuatorClass> mClass;
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
        AnimationTrackClass(const AnimationTrackClass& other)
        {
            for (const auto& a : other.mActuators)
            {
                mActuators.push_back(a->Copy());
            }
            mId       = other.mId;
            mName     = other.mName;
            mDuration = other.mDuration;
            mLooping  = other.mLooping;
        }
        AnimationTrackClass(AnimationTrackClass&& other)
        {
            mId        = std::move(other.mId);
            mActuators = std::move(other.mActuators);
            mName      = std::move(other.mName);
            mDuration  = other.mDuration;
            mLooping   = other.mLooping;
        }
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
        void AddActuator(std::shared_ptr<AnimationActuatorClass> actuator)
        {
            mActuators.push_back(std::move(actuator));
        }
        // Add a new actuator that applies state update/action on some animation node.
        template<typename Actuator>
        void AddActuator(const Actuator& actuator)
        {
            std::shared_ptr<AnimationActuatorClass> foo(new Actuator(actuator));
            mActuators.push_back(std::move(foo));
        }

        void DeleteActuator(size_t index)
        {
            ASSERT(index < mActuators.size());
            auto it = mActuators.begin();
            std::advance(it, index);
            mActuators.erase(it);
        }

        bool DeleteActuatorById(const std::string& id)
        {
            for (auto it = mActuators.begin(); it != mActuators.end(); ++it)
            {
                if ((*it)->GetId() == id) {
                    mActuators.erase(it);
                    return true;
                }
            }
            return false;
        }
        AnimationActuatorClass* FindActuatorById(const std::string& id)
        {
            for (auto& actuator : mActuators) {
                if (actuator->GetId() == id)
                    return actuator.get();
            }
            return nullptr;
        }
        const AnimationActuatorClass* FindActuatorById(const std::string& id) const
        {
            for (auto& actuator : mActuators) {
                if (actuator->GetId() == id)
                    return actuator.get();
            }
            return nullptr;
        }
        void Clear()
        { mActuators.clear(); }

        // Get the number of animation actuator class objects currently
        // in this animation track.
        size_t GetNumActuators() const
        { return mActuators.size(); }

        // Get the animation actuator class object at index i.
        const AnimationActuatorClass& GetActuatorClass(size_t i) const
        { return *mActuators[i]; }

        // Create an instance of some actuator class type at the given index.
        // For example if the type of actuator class at index N is
        // AnimationTransformActuatorClass then the returned object will be an
        // instance of AnimationTransformActuator.
        std::unique_ptr<AnimationActuator> CreateActuatorInstance(size_t i) const
        {
            const auto& klass = mActuators[i];
            if (klass->GetType() == AnimationActuatorClass::Type::Transform)
                return std::make_unique<AnimationTransformActuator>(std::static_pointer_cast<AnimationTransformActuatorClass>(klass));
            else if (klass->GetType() == AnimationActuatorClass::Type::Material)
                return std::make_unique<MaterialActuator>(std::static_pointer_cast<MaterialActuatorClass>(klass));
            BUG("Unknown actuator type");
            return {};
        }

        // Get the hash value based on the static data.
        std::size_t GetHash() const
        {
            std::size_t hash = 0;
            hash = base::hash_combine(hash, mId);
            hash = base::hash_combine(hash, mName);
            hash = base::hash_combine(hash, mDuration);
            hash = base::hash_combine(hash, mLooping);
            for (const auto& actuator : mActuators)
                hash = base::hash_combine(hash, actuator->GetHash());
            return hash;
        }

        // Serialize into JSON.
        nlohmann::json ToJson() const
        {
            nlohmann::json json;
            base::JsonWrite(json, "id", mId);
            base::JsonWrite(json, "name",     mName);
            base::JsonWrite(json, "duration", mDuration);
            base::JsonWrite(json, "looping",  mLooping);
            for (const auto& actuator : mActuators)
            {
                nlohmann::json js;
                base::JsonWrite(js, "type", actuator->GetType());
                base::JsonWrite(js, "actuator", *actuator);
                json["actuators"].push_back(std::move(js));
            }
            return json;
        }
        // Try to create new instance of AnimationTrackClass based on the data
        // loaded from JSON. On failure returns std::nullopt otherwise returns
        // an instance of the class object.
        static std::optional<AnimationTrackClass> FromJson(const nlohmann::json& json)
        {
            AnimationTrackClass ret;
            if (!base::JsonReadSafe(json, "id", &ret.mId) ||
                !base::JsonReadSafe(json, "name", &ret.mName) ||
                !base::JsonReadSafe(json, "duration", &ret.mDuration) ||
                !base::JsonReadSafe(json, "looping", &ret.mLooping))
                return std::nullopt;
            if (!json.contains("actuators"))
                return ret;
            for (const auto& json_actuator : json["actuators"].items())
            {
                const auto& obj = json_actuator.value();
                AnimationActuatorClass::Type type;
                if (!base::JsonReadSafe(obj, "type", &type))
                    return std::nullopt;
                std::shared_ptr<AnimationActuatorClass> actuator;
                if (type == AnimationActuatorClass::Type::Transform)
                    actuator = std::make_shared<AnimationTransformActuatorClass>();
                else if (type == AnimationActuatorClass::Type::Material)
                    actuator = std::make_shared<MaterialActuatorClass>();
                else BUG("Unknown actuator type.");

                if (!actuator->FromJson(obj["actuator"]))
                    return std::nullopt;
                ret.mActuators.push_back(actuator);
            }
            return ret;
        }

        AnimationTrackClass Clone() const
        {
            AnimationTrackClass ret;
            ret.mName     = mName;
            ret.mDuration = mDuration;
            ret.mLooping  = mLooping;
            for (const auto& klass : mActuators)
                ret.mActuators.push_back(klass->Clone());
            return ret;
        }

        // Do a deep copy on the assignment of a new object.
        AnimationTrackClass& operator=(const AnimationTrackClass& other)
        {
            if (this == &other)
                return *this;
            AnimationTrackClass copy(other);
            std::swap(mId, copy.mId);
            std::swap(mActuators, copy.mActuators);
            std::swap(mName, copy.mName);
            std::swap(mDuration, copy.mDuration);
            std::swap(mLooping, copy.mLooping);
            return *this;
        }
    private:
        std::string mId;
        // The list of animation actuators that apply transforms
        std::vector<std::shared_ptr<AnimationActuatorClass>> mActuators;
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
        AnimationTrack(const std::shared_ptr<const AnimationTrackClass>& klass)
            : mClass(klass)
        {
            for (size_t i=0; i<mClass->GetNumActuators(); ++i)
            {
                NodeTrack track;
                track.actuator = mClass->CreateActuatorInstance(i);
                track.node     = track.actuator->GetNodeId();
                track.ended    = false;
                track.started  = false;
                mTracks.push_back(std::move(track));
            }
        }
        // Create a new animation track based on the given class object
        // which will be copied.
        AnimationTrack(const AnimationTrackClass& klass)
            : AnimationTrack(std::make_shared<AnimationTrackClass>(klass))
        {}
        // Create a new animation track baed on the given class object.
        AnimationTrack(const AnimationTrack& other) : mClass(other.mClass)
        {
            for (size_t i=0; i<other.mTracks.size(); ++i)
            {
                NodeTrack track;
                track.node     = other.mTracks[i].node;
                track.actuator = other.mTracks[i].actuator->Copy();
                track.ended    = other.mTracks[i].ended;
                track.started  = other.mTracks[i].started;
                mTracks.push_back(std::move(track));
            }
            mCurrentTime = other.mCurrentTime;
        }
        // Move ctor.
        AnimationTrack(AnimationTrack&& other)
        {
            mClass       = other.mClass;
            mTracks      = std::move(other.mTracks);
            mCurrentTime = other.mCurrentTime;
        }

        // Update the animation track state.
        void Update(float dt);
        // Apply state/transition updates/interpolated intermediate states (if any)
        // onto the given node.
        void Apply(AnimationNode& node) const;
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
            std::unique_ptr<AnimationActuator> actuator;
            mutable bool started = false;
            mutable bool ended   = false;
        };
        std::vector<NodeTrack> mTracks;

        // current play back time for this track.
        float mCurrentTime = 0.0f;
    };

    struct AnimationDrawPacket {
        // shortcut to the node's material.
        std::shared_ptr<const gfx::Material> material;
        // shortcut to the node's drawable.
        std::shared_ptr<const gfx::Drawable> drawable;
        // transform that pertains to the draw.
        glm::mat4 transform;
        // the animation layer this draw belongs to.
        int layer = 0;
        // the render pass this draw belongs to.
        AnimationNode::RenderPass pass = AnimationNode::RenderPass::Draw;
    };

    template<typename Node>
    class AnimationDrawHook
    {
    public:
        virtual ~AnimationDrawHook() = default;
        // This is a hook function to inspect and  modify the the draw packet produced by the
        // given animation node. The return value can be used to indicate filtering.
        // If the function returns false thepacket is dropped. Otherwise it's added to the
        // current drawlist with any possible modifications.
        virtual bool InspectPacket(const Node* node, AnimationDrawPacket& packet) { return true; }
        // This is a hook function to append extra draw packets to the current drawlist
        // based on the node.
        // Transform is the combined transformation hierarchy containing the transformations
        // from this current node to "view".
        virtual void AppendPackets(const Node* node, gfx::Transform& trans,
            std::vector<AnimationDrawPacket>& packets) {}
    protected:
    };

    // AnimationClass holds the data for some particular type of animation,
    // i.e. the AnimationNodes that form the visual look and the appearance
    // of the animation combined in a transformation hierarchy.
    class AnimationClass
    {
    public:
        using RenderTree     = TreeNode<AnimationNodeClass>;
        using RenderTreeNode = TreeNode<AnimationNodeClass>;
        using DrawHook   = AnimationDrawHook<AnimationNodeClass>;
        using DrawPacket = AnimationDrawPacket;

        AnimationClass()
        { mId = base::RandomString(10); }
        AnimationClass(const AnimationClass& other);

        // Add a new animation node. Returns pointer to the node
        // that was added to the animation.
        AnimationNodeClass* AddNode(AnimationNodeClass&& node);
        // Add a new animation node. Returns a pointer to the node
        // that was added to the anímation.
        AnimationNodeClass* AddNode(const AnimationNodeClass& node);
        // Add a new animation node. Returns a pointer to the node
        // that was added to the anímation.
        AnimationNodeClass* AddNode(std::unique_ptr<AnimationNodeClass> node);

        // Delete a node by the given index.
        void DeleteNodeByIndex(size_t i);
        // Delete a node by the given id. Returns tre if the node
        // was found and deleted otherwise false.
        bool DeleteNodeById(const std::string& id);
        // Delete a node by the given name. Returns true if the node
        // was found and deleted otherwise false.
        bool DeleteNodeByName(const std::string& name);

        // Get the animation node class object by index.
        // The index must be valid.
        AnimationNodeClass& GetNode(size_t i);
        // Find animation node by the given name. Returns nullptr if no such
        // node could be found.
        AnimationNodeClass* FindNodeByName(const std::string& name);
        // Find animation node by the given id. Returns nullptr if no such
        // node could be found.
        AnimationNodeClass* FindNodeById(const std::string& id);

        // Get the animation node class object by index.
        // The index must be valid.
        const AnimationNodeClass& GetNode(size_t i) const;
        // Find animation node by the given name. Returns nullptr if no such
        // node could be found.
        const AnimationNodeClass* FindNodeByName(const std::string& name) const;
        // Find animation node by the given id. Returns nullptr if no such
        // node could be found.
        const AnimationNodeClass* FindNodeById(const std::string& id) const;

        std::shared_ptr<const AnimationNodeClass> GetSharedAnimationNodeClass(size_t i) const;

        // Add a new animation track class object. Returns a pointer to the node that
        // was added to the animation.
        AnimationTrackClass* AddAnimationTrack(AnimationTrackClass&& track);
        // Add a new animation track class object. Returns a pointer to the node that
        // was added to the animation.
        AnimationTrackClass* AddAnimationTrack(const AnimationTrackClass& track);
        // Add a new animation track class object. Returns a pointer to the node that
        // was added to the animation.
        AnimationTrackClass* AddAnimationTrack(std::unique_ptr<AnimationTrackClass> track);
        // Delete an animation track by the given index.
        void DeleteAnimationTrack(size_t i);
        // Delete an animation track by the given name.
        bool DeleteAnimationTrackByName(const std::string& name);
        // Delete an animation track by the given id.
        bool DeleteAnimationTrackById(const std::string& id);
        // Get the animation track class object by index.
        // The index must be valid.
        AnimationTrackClass& GetAnimationTrack(size_t i);
        // Find animation track class object by name. Returns nullptr if no such
        // track could be found.
        AnimationTrackClass* FindAnimationTrackByName(const std::string& name);
        // Get the animation track class object by index.
        // The index must be valid.
        const AnimationTrackClass& GetAnimationTrack(size_t i) const;
        // Find animation track class object by name. Returns nullptr if no such
        // track could be found.
        const AnimationTrackClass* FindAnimationTrackByName(const std::string& name) const;

        std::shared_ptr<const AnimationTrackClass> GetSharedAnimationTrackClass(size_t i) const;

        // Update the animation and its node class objects.
        void Update(float time, float dt);

        // Reset the class object state.
        void Reset();

        // Prepare and load the runtime resources if not yet loaded.
        void LoadDependentClasses(const ClassLibrary& loader) const;

        // Draw a representation of the animation class instance.
        // This functionality is mostly to support editor functionality
        // and to simplify working with an AnimationClass instance.
        void Draw(gfx::Painter& painter, gfx::Transform& trans, DrawHook* hook = nullptr) const;

        // Perform coarse hit test to see if the given x,y point
        // intersects with any node's drawable in the animation.
        // The testing is coarse in the sense that it's done against the node's
        // drawable shapes size box and ignores things such as transparency.
        // The hit nodes are stored in the hits vector and the positions with the
        // nodes' hitboxes are (optionally) strored in the hitbox_positions vector.
        void CoarseHitTest(float x, float y, std::vector<AnimationNodeClass*>* hits,
            std::vector<glm::vec2>* hitbox_positions = nullptr);
        void CoarseHitTest(float x, float y, std::vector<const AnimationNodeClass*>* hits,
            std::vector<glm::vec2>* hitbox_positions = nullptr) const;

        // Map coordinates in some AnimationNode's (see AnimationNode::GetNodeTransform) space
        // into animation coordinate space.
        glm::vec2 MapCoordsFromNode(float x, float y, const AnimationNodeClass* node) const;
        // Map coordinates in animation coordinate space into some AnimationNode's coordinate space.
        glm::vec2 MapCoordsToNode(float x, float y, const AnimationNodeClass* node) const;

        // Compute the axis aligned bounding box for the give animation node
        // at the current time of animation.
        gfx::FRect GetBoundingBox(const AnimationNodeClass* node) const;
        // Compute the axis aligned bounding box for the whole animation
        // i.e. including all the nodes at the current time of animation.
        // This is a shortcut for getting the union of all the bounding boxes
        // of all the animation nodes.
        gfx::FRect GetBoundingBox() const;

        // Get the hash value based on the current properties of the animation
        // i.e. include each node and their drawables and materials but don't
        // include transient state such as current runtime.
        std::size_t GetHash() const;

        // Serialize the animation into JSON.
        nlohmann::json ToJson() const;

        // Lookup a AnimationNode based on the serialized ID in the JSON.
        AnimationNodeClass* TreeNodeFromJson(const nlohmann::json& json);

        // Create an animation object based on the JSON. Returns nullopt if
        // deserialization failed.
        static std::optional<AnimationClass> FromJson(const nlohmann::json& object);

        // Serialize an animation node contained in the RenderTree to JSON by doing
        // a shallow (id only) based serialization.
        // Later in TreeNodeFromJson when reading back the render tree we simply
        // look up the node based on the ID.
        static nlohmann::json TreeNodeToJson(const AnimationNodeClass* node);

        // Get the animation id.
        std::string GetId() const
        { return mId; }
        std::size_t GetNumTracks() const
        { return mAnimationTracks.size(); }
        std::size_t GetNumNodes() const
        { return mNodes.size(); }
        RenderTree& GetRenderTree()
        { return mRenderTree; }
        const RenderTree& GetRenderTree() const
        { return mRenderTree; }

        AnimationClass Clone() const;

        AnimationClass& operator=(const AnimationClass& other);

    private:
        std::string mId;
        // The list of animation nodes that belong to this animation.
        // we're allocating the nodes on the free store so that the
        // render tree pointers remain valid even if the vector is resized
        std::vector<std::shared_ptr<AnimationNodeClass>> mNodes;
        // scenegraph / render tree for hierarchical traversal
        // and transformation of the animation nodes. the tree defines
        // the parent-child transformation hierarchy.
        RenderTree mRenderTree;
        // the list of animation tracks that are pre-defined with this
        // type of animation.
        std::vector<std::shared_ptr<AnimationTrackClass>> mAnimationTracks;
    };

    // Animation is an instance of some type of AnimationClass. The "type"
    // i.e the functionality and the behaviour is defined by the AnimationClass
    // and animation holds the specific animation instance data.
    // The animation insta ce can be transformed by an AnimationTrack which
    // can apply transformations and changes on the nodes of the animation
    // over time.
    class Animation
    {
    public:
        using RenderTree     = TreeNode<AnimationNode>;
        using RenderTreeNode = TreeNode<AnimationNode>;
        using DrawHook       = AnimationDrawHook<AnimationNode>;
        using DrawPacket     = AnimationDrawPacket;

        // Create new animation instance based on some animation class.
        Animation(const std::shared_ptr<const AnimationClass>& klass);
        Animation(const AnimationClass& klass);

        // Update the animation and its nodes.
        // Triggers the actions and events specified on the timeline.
        void Update(float dt);

        // Play the given animation track.
        void Play(std::unique_ptr<AnimationTrack> track);
        void Play(const AnimationTrack& track)
        { Play(std::make_unique<AnimationTrack>(track)); }
        void Play(AnimationTrack&& track)
        { Play(std::make_unique<AnimationTrack>(std::move(track))); }
        // Play a previously recorded (stored in the animation class object)
        // animation track identified by name. Note that there could be
        // ambiquity between the names, i.e. multiple tracks with the same name.
        void PlayByName(const std::string& name);
        // Play a previously recorded (stored in the animation class object)
        // animation track identified by its track id.
        void PlayById(const std::string& id);

        // Returns true if an animation track is still playing.
        bool IsPlaying() const;
        // Get the current track if any. (when IsPlaying is true)
        AnimationTrack* GetCurrentTrack()
        { return mAnimationTrack.get(); }
        const AnimationTrack* GetCurrentTrack() const
        { return mAnimationTrack.get(); }

        // Draw the animation and its nodes.
        // Each node is transformed relative to the parent transformation "trans".
        // Optional draw hook can be used to modify the draw packets before submission to the
        // paint device.
        void Draw(gfx::Painter& painter, gfx::Transform& trans, DrawHook* hook = nullptr) const;

        // Perform coarse hit test to see if the given x,y point
        // intersects with any node's drawable in the animation.
        // The testing is coarse in the sense that it's done against the node's
        // drawable shapes size box and ignores things such as transparency.
        // The hit nodes are stored in the hits vector and the positions with the
        // nodes' hitboxes are (optionally) strored in the hitbox_positions vector.
        void CoarseHitTest(float x, float y, std::vector<AnimationNode*>* hits,
            std::vector<glm::vec2>* hitbox_positions = nullptr);
        void CoarseHitTest(float x, float y, std::vector<const AnimationNode*>* hits,
            std::vector<glm::vec2>* hitbox_positions = nullptr) const;

        // Map coordinates in some AnimationNode's (see AnimationNode::GetNodeTransform) space
        // into animation coordinate space.
        glm::vec2 MapCoordsFromNode(float x, float y, const AnimationNode* node) const;
        // Map coordinates in animation coordinate space into some AnimationNode's coordinate space.
        glm::vec2 MapCoordsToNode(float x, float y, const AnimationNode* node) const;

        // Compute the axis aligned bounding box for the give animation node
        // at the current time of animation.
        gfx::FRect GetBoundingBox(const AnimationNode* node) const;
        // Compute the axis aligned bounding box for the whole animation
        // i.e. including all the nodes at the current time of animation.
        // This is a shortcut for getting the union of all the bounding boxes
        // of all the animation nodes.
        gfx::FRect GetBoundingBox() const;

        // Reset the state of the animation to initial state.
        void Reset();

        // Get the class object instance.
        const AnimationClass& GetClass() const
        { return *mClass.get(); }

        // shortcut operator for accessing the class object instance
        const AnimationClass* operator->() const
        { return mClass.get(); }

        size_t GetNumNodes() const
        { return mNodes.size(); }

        // Get the animation node class object by index.
        // The index must be valid.
        AnimationNode& GetNode(size_t i)
        { return *mNodes[i]; }
        // Find animation node by the given name. Returns nullptr if no such
        // node could be found.
        AnimationNode* FindNodeByName(const std::string& name);
        // Find animation node by the given id. Returns nullptr if no such
        // node could be found.
        AnimationNode* FindNodeById(const std::string& id);

        // Get the animation node class object by index.
        // The index must be valid.
        const AnimationNode& GetNode(size_t i) const
        { return *mNodes[i]; }
        // Find animation node by the given name. Returns nullptr if no such
        // node could be found.
        const AnimationNode* FindNodeByName(const std::string& name) const;
        // Find animation node by the given id. Returns nullptr if no such
        // node could be found.
        const AnimationNode* FindNodeById(const std::string& id) const;

        RenderTree& GetRenderTree()
        { return mRenderTree; }
        const RenderTree& GetRenderTree() const
        { return mRenderTree; }

        // Get the accumulated animation time.
        float GetCurrentTime() const
        { return mCurrentTime; }

        AnimationNode* TreeNodeFromJson(const nlohmann::json& json);
    private:
        // the class object.
        std::shared_ptr<const AnimationClass> mClass;
        // the instance data for this animation.
        // The current animation track if any.
        std::unique_ptr<AnimationTrack> mAnimationTrack;
        // The animation node instances for this animation.
        std::vector<std::unique_ptr<AnimationNode>> mNodes;
        // The render tree for this animation
        RenderTree mRenderTree;
        // current playback time.
        float mCurrentTime = 0.0f;
    };


    std::unique_ptr<Animation> CreateAnimationInstance(const std::shared_ptr<const AnimationClass>& klass);
    std::unique_ptr<AnimationTrack> CreateAnimationTrackInstance(const std::shared_ptr<const AnimationTrackClass>& klass);

} // namespace
