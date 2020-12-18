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
#include "graphics/types.h"
#include "graphics/drawable.h"
#include "gamelib/tree.h"
#include "gamelib/types.h"
#include "gamelib/enum.h"

namespace game
{

    class RigidBodyItemClass
    {
    public:
        // Simulation parameter determines the type of physics
        // simulation (or the lack of simulation) applied to the
        // rigid body by the physics engine.
        enum class Simulation {
            // Static bodies remain static in the physics simulation.
            // i.e the body exists in the physics world but no forces
            // are applied onto it.
            Static,
            // Kinematic bodies are driven by simple kinematic motion
            // i.e. by velocity of the body. No forces are applied to it.
            Kinematic,
            // Dynamic body is completely driven by the physics simulation.
            // I.e.the body is moved by the physical forces being applied to it.
            Dynamic
        };

        // Selection for collision shapes when the collision shape detection
        // is set to manual.
        enum class CollisionShape {
            // The collision shape is a box based on the size of node's box.
            Box,
            // The collision shape is a circle based on the largest extent of
            // the node's box.
            Circle,
            // The collision shape is a convex polygon. The polygon shape id
            // must then be selected in order to be able to extract the
            // polygon's convex hull.
            Polygon
        };

        enum class Flags {
            // Enable bullet physics, i.e. expect the object to be
            // fast moving object. this will increase the computational
            // effort required but will mitigate issues with fast traveling
            // objects.
            Bullet,
            // Sensor only flag enables object to only be used to
            // report collisions.
            Sensor,
            // Whether the rigid body simulation is enabled or not
            // for this body.
            Enabled
        };
        RigidBodyItemClass()
        {
            mBitFlags.set(Flags::Enabled, true);
        }

        std::size_t GetHash() const
        {
            size_t hash = 0;
            hash = base::hash_combine(hash, mSimulation);
            hash = base::hash_combine(hash, mCollisionShape);
            hash = base::hash_combine(hash, mBitFlags.value());
            hash = base::hash_combine(hash, mPolygonShapeId);
            hash = base::hash_combine(hash, mFriction);
            hash = base::hash_combine(hash, mRestitution);
            hash = base::hash_combine(hash, mAngularDamping);
            hash = base::hash_combine(hash, mLinearDamping);
            hash = base::hash_combine(hash, mDensity);
            return hash;
        }
        Simulation GetSimulation() const
        { return mSimulation; }
        CollisionShape GetCollisionShape() const
        { return mCollisionShape; }
        float GetFriction() const
        { return mFriction; }
        float GetRestitution() const
        { return mRestitution; }
        float GetAngularDamping() const
        { return mAngularDamping; }
        float GetLinearDamping() const
        { return mLinearDamping; }
        float GetDensity() const
        { return mDensity; }
        bool TestFlag(Flags flag) const
        { return mBitFlags.test(flag); }
        std::string GetPolygonShapeId() const
        { return mPolygonShapeId; }
        void ResetPolygonShapeId()
        { mPolygonShapeId.clear(); }

        void SetCollisionShape(CollisionShape shape)
        { mCollisionShape = shape; }
        void SetSimulation(Simulation simulation)
        { mSimulation = simulation; }
        void SetFlag(Flags flag, bool on_off)
        { mBitFlags.set(flag, on_off); }
        void SetFriction(float value)
        { mFriction = value; }
        void SetRestitution(float value)
        { mRestitution = value; }
        void SetAngularDamping(float value)
        { mAngularDamping = value; }
        void SetLinearDamping(float value)
        { mLinearDamping = value; }
        void SetDensity(float value)
        { mDensity = value; }
        void SetPolygonShapeId(const std::string& id)
        { mPolygonShapeId = id; }

        nlohmann::json ToJson() const
        {
            nlohmann::json json;
            base::JsonWrite(json, "simulation",      mSimulation);
            base::JsonWrite(json, "shape",           mCollisionShape);
            base::JsonWrite(json, "flags",     mBitFlags.value());
            base::JsonWrite(json, "polygon",         mPolygonShapeId);
            base::JsonWrite(json, "friction",        mFriction);
            base::JsonWrite(json, "restitution",     mRestitution);
            base::JsonWrite(json, "angular_damping", mAngularDamping);
            base::JsonWrite(json, "linear_damping",  mLinearDamping);
            base::JsonWrite(json, "density",         mDensity);
            return json;
        }
        static std::optional<RigidBodyItemClass> FromJson(const nlohmann::json& json)
        {
            unsigned bitflag = 0;
            RigidBodyItemClass ret;
            if (!base::JsonReadSafe(json, "simulation",      &ret.mSimulation)  ||
                !base::JsonReadSafe(json, "shape",           &ret.mCollisionShape)  ||
                !base::JsonReadSafe(json, "flags",           &bitflag)  ||
                !base::JsonReadSafe(json, "polygon",         &ret.mPolygonShapeId)  ||
                !base::JsonReadSafe(json, "friction",        &ret.mFriction)  ||
                !base::JsonReadSafe(json, "restitution",     &ret.mRestitution)  ||
                !base::JsonReadSafe(json, "angular_damping", &ret.mAngularDamping)  ||
                !base::JsonReadSafe(json, "linear_damping",  &ret.mLinearDamping)  ||
                !base::JsonReadSafe(json, "density",         &ret.mDensity))
                return std::nullopt;
            ret.mBitFlags.set_from_value(bitflag);
            return ret;
        }

    private:
        Simulation mSimulation = Simulation::Dynamic;
        CollisionShape mCollisionShape = CollisionShape::Box;
        base::bitflag<Flags> mBitFlags;
        std::string mPolygonShapeId;
        float mFriction = 0.3f;
        float mRestitution = 0.5f;
        float mAngularDamping = 0.5f;
        float mLinearDamping = 0.5f;
        float mDensity = 1.0f;
    };

    // AnimationItem holds an animation object
    class AnimationItemClass
    {
    public:
        std::size_t GetHash() const
        {
            size_t hash = 0;
            hash = base::hash_combine(hash, mAnimationId);
            return hash;
        }
        nlohmann::json ToJson() const
        {
            nlohmann::json json;
            base::JsonWrite(json, "animation", mAnimationId);
            return json;
        }
        bool FromJson(const nlohmann::json& json)
        {
            if (!base::JsonReadSafe(json, "animation", &mAnimationId))
                return false;
            return true;
        }

        void SetAnimationId(const std::string& id)
        { mAnimationId = id; }
        void ResetAnimation()
        { mAnimationId.clear(); }
    private:
        std::string mAnimationId;
    };

    // Drawble item defines a static (not animated) drawable item
    // and its material and properties that affect the rendering
    // of the item.
    // todo: this is very similar to AnimationNode, maybe these should converge.
    class DrawableItemClass
    {
    public:
        using RenderPass = game::RenderPass;
        using RenderStyle = gfx::Drawable::Style;

        enum class Flags {
            // Whether the item should update material or not
            UpdateMaterial,
            // Whether the item should update drawable or not
            UpdateDrawable,
            // Whether the item should restart drawables that have
            // finished, for example particle engines.
            RestartDrawable,
            // Whether the item should override the material alpha value.
            OverrideAlpha
        };
        DrawableItemClass()
        {
            mBitFlags.set(Flags::UpdateDrawable, true);
            mBitFlags.set(Flags::UpdateMaterial, true);
            mBitFlags.set(Flags::RestartDrawable, true);
            mBitFlags.set(Flags::OverrideAlpha, false);
        }

        std::size_t GetHash() const
        {
            size_t hash = 0;
            hash = base::hash_combine(hash, mBitFlags.value());
            hash = base::hash_combine(hash, mMaterialId);
            hash = base::hash_combine(hash, mDrawableId);
            hash = base::hash_combine(hash, mLayer);
            hash = base::hash_combine(hash, mAlpha);
            hash = base::hash_combine(hash, mLineWidth);
            hash = base::hash_combine(hash, mRenderPass);
            hash = base::hash_combine(hash, mRenderStyle);
            return hash;
        }
        nlohmann::json ToJson() const
        {
            nlohmann::json json;
            base::JsonWrite(json, "flags", mBitFlags.value());
            base::JsonWrite(json, "material",    mMaterialId);
            base::JsonWrite(json, "drawable",    mDrawableId);
            base::JsonWrite(json, "layer",       mLayer);
            base::JsonWrite(json, "alpha",       mAlpha);
            base::JsonWrite(json, "linewidth",   mLineWidth);
            base::JsonWrite(json, "renderpass",  mRenderPass);
            base::JsonWrite(json, "renderstyle", mRenderStyle);
            return json;
        }
        static
        std::optional<DrawableItemClass> FromJson(const nlohmann::json& json)
        {
            unsigned bitflags = 0;
            DrawableItemClass ret;
            if (!base::JsonReadSafe(json, "flags",       &bitflags) ||
                !base::JsonReadSafe(json, "material",    &ret.mMaterialId) ||
                !base::JsonReadSafe(json, "drawable",    &ret.mDrawableId) ||
                !base::JsonReadSafe(json, "layer",       &ret.mLayer) ||
                !base::JsonReadSafe(json, "alpha",       &ret.mAlpha) ||
                !base::JsonReadSafe(json, "linewidth",   &ret.mLineWidth) ||
                !base::JsonReadSafe(json, "renderpass",  &ret.mRenderPass) ||
                !base::JsonReadSafe(json, "renderstyle", &ret.mRenderStyle))
                return std::nullopt;
            ret.mBitFlags.set_from_value(bitflags);
            return ret;
        }

        // class setters.
        void SetDrawableId(const std::string& klass)
        { mDrawableId = klass; }
        void SetMaterialId(const std::string& klass)
        { mMaterialId = klass; }
        void SetLayer(int layer)
        { mLayer = layer; }
        void ResetMaterial()
        { mMaterialId.clear(); }
        void ResetDrawable()
        { mDrawableId.clear(); }
        void SetFlag(Flags flag, bool on_off)
        { mBitFlags.set(flag, on_off); }
        void SetAlpha(float alpha)
        { mAlpha = math::clamp(0.0f, 1.0f, alpha); }
        void SetLineWidth(float width)
        { mLineWidth = width; }
        void SetRenderPass(RenderPass pass)
        { mRenderPass = pass; }
        void SetRenderStyle(RenderStyle style)
        { mRenderStyle = style; }

        // class getters.
        std::string GetDrawableId() const
        { return mDrawableId; }
        std::string GetMaterialId() const
        { return mMaterialId; }
        int GetLayer() const
        { return mLayer; }
        float GetAlpha() const
        {return mAlpha; }
        float GetLineWidth() const
        { return mLineWidth; }
        bool TestFlag(Flags flag) const
        { return mBitFlags.test(flag); }
        RenderPass GetRenderPass() const
        { return mRenderPass; }
        RenderStyle GetRenderStyle() const
        { return mRenderStyle; }

    private:
        // item's bit flags.
        base::bitflag<Flags> mBitFlags;
        // class id of the material.
        std::string mMaterialId;
        // class id of the drawable shape.
        std::string mDrawableId;
        // the layer in which this node should be drawn.
        int mLayer = 0;
        // override alpha value, 0.0f = fully transparent, 1.0f = fully opaque.
        // only works with materials that enable alpha blending (transparency)
        float mAlpha = 1.0f;
        // linewidth for rasterizing the shape with lines.
        float mLineWidth = 1.0f;
        RenderPass mRenderPass = RenderPass::Draw;
        RenderStyle  mRenderStyle = RenderStyle::Solid;
    };

    class DrawableItem
    {
    public:
        using Flags       = DrawableItemClass::Flags;
        using RenderPass  = DrawableItemClass::RenderPass;
        using RenderStyle = DrawableItemClass::RenderStyle ;

        DrawableItem(std::shared_ptr<const DrawableItemClass> klass)
          : mClass(klass)
        {
            mInstanceAlpha = mClass->GetAlpha();
        }
        std::string GetMaterialId() const
        { return mClass->GetMaterialId(); }
        std::string GetDrawableId() const
        { return mClass->GetDrawableId(); }
        int GetLayer() const
        { return mClass->GetLayer(); }
        float GetLineWidth() const
        { return mClass->GetLineWidth(); }
        RenderPass GetRenderPass() const
        { return mClass->GetRenderPass(); }
        RenderStyle GetRenderStyle() const
        { return mClass->GetRenderStyle(); }
        bool TestFlag(Flags flag) const
        { return mClass->TestFlag(flag); }
        float GetAlpha() const
        { return mInstanceAlpha; }

        void SetAlpha(float alpha)
        { mInstanceAlpha = alpha; }

        const DrawableItemClass& GetClass() const
        { return *mClass.get(); }
        const DrawableItemClass* operator->() const
        { return mClass.get(); }
    private:
        std::shared_ptr<const DrawableItemClass> mClass;

        float mInstanceAlpha = 1.0f;
    };

    class RigidBodyItem
    {
    public:
        using Simulation = RigidBodyItemClass::Simulation;
        using Flags      = RigidBodyItemClass::Flags;
        using CollisionShape = RigidBodyItemClass::CollisionShape;

        RigidBodyItem(std::shared_ptr<const RigidBodyItemClass> klass)
            : mClass(klass)
        {}

        Simulation GetSimulation() const
        { return mClass->GetSimulation(); }
        CollisionShape GetCollisionShape() const
        { return mClass->GetCollisionShape(); }
        float GetFriction() const
        { return mClass->GetFriction(); }
        float GetRestitution() const
        { return mClass->GetRestitution(); }
        float GetAngularDamping() const
        { return mClass->GetAngularDamping(); }
        float GetLinearDamping() const
        { return mClass->GetLinearDamping(); }
        float GetDensity() const
        { return mClass->GetDensity(); }
        bool TestFlag(Flags flag) const
        { return mClass->TestFlag(flag); }
        std::string GetPolygonShapeId() const
        { return mClass->GetPolygonShapeId(); }

        const RigidBodyItemClass& GetClass() const
        { return *mClass.get(); }
        const RigidBodyItemClass* operator->() const
        { return mClass.get(); }
    private:
        std::shared_ptr<const RigidBodyItemClass> mClass;
    };

    class SceneNodeClass
    {
    public:
        using DrawableItemType = DrawableItemClass;

        enum class Flags {
            // Only pertains to editor (todo: maybe this flag should be removed)
            VisibleInEditor,
            // node is visible in the game or not.
            // Even if this is true the node will still need to have some
            // renderable items attached to it such as a shape or
            // animation item.
            VisibleInGame
        };

        SceneNodeClass()
        {
            mClassId = base::RandomString(10);
            mBitFlags.set(Flags::VisibleInEditor, true);
            mBitFlags.set(Flags::VisibleInGame, true);
        }
        SceneNodeClass(const SceneNodeClass& other);
        SceneNodeClass(SceneNodeClass&& other);

        // Get the class id.
        std::string GetClassId() const
        { return mClassId; }
        std::string GetInstanceId() const
        { return mClassId; }
        // Get the human readable name for this class.
        std::string GetName() const
        { return mName; }
        // Get the hash value based on the class object properties.
        std::size_t GetHash() const;

        // Get the node's translation relative to its parent node.
        glm::vec2 GetTranslation() const
        { return mPosition; }
        // Get the node's scale factor. The scale factor applies to
        // whole hierarchy of nodes.
        glm::vec2 GetScale() const
        { return mScale; }
        // Get the node's box size.
        glm::vec2 GetSize() const
        { return mSize;}
        // Get node's rotation relative to its parent node.
        float GetRotation() const
        { return mRotation; }
        // Set the human readable node name.
        void SetName(const std::string& name)
        { mName = name; }
        // Set the node's scale. The scale applies to all of
        // the subsequent hierarchy, i.e. all the nodes that
        // are in the tree under this node.
        void SetScale(const glm::vec2& scale)
        { mScale = scale; }
        // Set the node's translation relative to the parent
        // of the this scene node.
        void SetTranslation(const glm::vec2& vec)
        { mPosition = vec; }
        // Set the node's containing box size.
        // The size is used to for example to figure out
        // the dimensions of rigid body collision shape (if any)
        // and to resize the drawable object.
        void SetSize(const glm::vec2& size)
        { mSize = size; }
        // Set the starting rotation in radians around the z axis.
        void SetRotation(float angle)
        { mRotation = angle;}
        void SetFlag(Flags flag, bool on_off)
        { mBitFlags.set(flag, on_off); }
        bool TestFlag(Flags flag) const
        { return mBitFlags.test(flag); }

        // Attach a rigid body to this node class.
        void SetRigidBody(const RigidBodyItemClass& body);
        // Attach a simple static drawable item to this node.
        void SetDrawable(const DrawableItemClass& drawable);

        void RemoveDrawable()
        { mDrawable.reset(); }
        void RemoveRigidBody()
        { mRigidBody.reset(); }

        // Get the rigid body shared class object if any.
        std::shared_ptr<const RigidBodyItemClass> GetSharedRigidBody() const
        { return mRigidBody; }
        // Get the drawable shared class object if any.
        std::shared_ptr<const DrawableItemClass> GetSharedDrawable() const
        { return mDrawable; }

        // Returns true if a rigid body has been set for this class.
        bool HasRigidBody() const
        { return !!mRigidBody; }
        // Returns true if a drawable object has been set for this class.
        bool HasDrawable() const
        { return !!mDrawable; }

        // Get the rigid body object if any. If no rigid body class object
        // has been set then returns nullptr.
        RigidBodyItemClass* GetRigidBody()
        { return mRigidBody.get(); }
        // Get the drawable shape object if any. If no drawable shape class object
        // has been set then returns nullptr.
        DrawableItemClass* GetDrawable()
        { return mDrawable.get(); }
        // Get the rigid body object if any. If no rigid body class object
        // has been set then returns nullptr.
        const RigidBodyItemClass* GetRigidBody() const
        { return mRigidBody.get(); }
        // Get the drawable shape object if any. If no drawable shape class object
        // has been set then returns nullptr.
        const DrawableItemClass* GetDrawable() const
        { return mDrawable.get(); }

        // Get the transform that applies to this node
        // and the subsequent hierarchy of nodes.
        glm::mat4 GetNodeTransform() const;
        // Get this drawable item's model transform that applies
        // to the node's box based items such as drawables
        // and rigid bodies.
        glm::mat4 GetModelTransform() const;

        void Update(float time, float dt);
        // Serialize the node into JSON.
        nlohmann::json ToJson() const;
        // Load the node's properties from the given JSON object.
        static std::optional<SceneNodeClass> FromJson(const nlohmann::json& json);
        // Make a new unique copy of this scene node class object
        // with all the same properties but with a different/unique ID.
        SceneNodeClass Clone() const;

        SceneNodeClass& operator=(const SceneNodeClass& other);
    private:
        // the resource id.
        std::string mClassId;
        // human readable name of the class.
        std::string mName;
        // translation of the node relative to its parent.
        glm::vec2 mPosition = {0.0f, 0.0f};
        // Nodes scaling factor. applies to all children.
        glm::vec2 mScale = {1.0f, 1.0f};
        // Size of the node's containing box.
        glm::vec2 mSize  = {1.0f, 1.0f};
        // Rotation around z axis in radians.
        float mRotation = 0.0f;
        // rigid body if any.
        std::shared_ptr<RigidBodyItemClass> mRigidBody;
        // drawable item if any.
        std::shared_ptr<DrawableItemClass> mDrawable;
        // bitflags that apply to node.
        base::bitflag<Flags> mBitFlags;
    };


    class SceneNode
    {
    public:
        using Flags = SceneNodeClass::Flags;
        using DrawableItemType = DrawableItem;

        SceneNode(std::shared_ptr<const SceneNodeClass> klass);
        SceneNode(const SceneNodeClass& klass);
        SceneNode(const SceneNode& other);
        SceneNode(SceneNode&& other);

        // instance setters.
        void SetScale(const glm::vec2& scale)
        { mScale = scale; }
        void SetSize(const glm::vec2& size)
        { mSize = size; }
        void SetTranslation(const glm::vec2& pos)
        { mPosition = pos; }
        void SetRotation(float rotation)
        { mRotation = rotation; }
        void SetName(const std::string& name)
        { mName = name; }
        void Translate(const glm::vec2& vec)
        { mPosition += vec; }
        void Translate(float dx, float dy)
        { mPosition += glm::vec2(dx, dy); }
        void Rotate(float dr)
        { mRotation += dr; }

        // instance getters.
        std::string GetInstanceId() const
        { return mInstId; }
        std::string GetInstanceName() const
        { return mName; }
        glm::vec2 GetTranslation() const
        { return mPosition; }
        glm::vec2 GetScale() const
        { return mScale; }
        glm::vec2 GetSize() const
        { return mSize; }
        float GetRotation() const
        { return mRotation; }
        bool TestFlag(Flags flags) const
        { return mClass->TestFlag(flags); }

        // Get the node's drawable item if any. If no drawable
        // item is set then returns nullptr.
        DrawableItem* GetDrawable();
        // Get the node's rigid body item if any. If no rigid body
        // item is set then returns nullptr.
        RigidBodyItem* GetRigidBody();
        // Get the node's drawable item if any. If now drawable
        // item is set then returns nullptr.
        const DrawableItem* GetDrawable() const;
        // Get the node's rigid body item if any. If no rigid body
        // item is set then returns nullptr.
        const RigidBodyItem* GetRigidBody() const;

        bool HasRigidBody() const
        { return !!mRigidBody; }
        bool HasDrawable() const
        { return !!mDrawable; }

        // shortcut for class getters.
        std::string GetClassId() const
        { return mClass->GetClassId(); }
        std::string GetClassName() const
        { return mClass->GetName(); }

        // Reset node's state to initial class state.
        void Reset();
        // Get the transform that applies to this node
        // and the subsequent hierarchy of nodes.
        glm::mat4 GetNodeTransform() const;
        // Get this drawable item's model transform that applies
        // to the node's box based items such as drawables
        // and rigid bodies.
        glm::mat4 GetModelTransform() const;

        const SceneNodeClass& GetClass() const
        { return *mClass.get(); }
        const SceneNodeClass* operator->() const
        { return mClass.get(); }
    private:
        // the class object.
        std::shared_ptr<const SceneNodeClass> mClass;
        // the instance id.
        std::string mInstId;
        // the instance name.
        std::string mName;
        // translation of the node relative to its parent.
        glm::vec2 mPosition = {0.0f, 0.0f};
        // Nodes scaling factor. applies to this node
        // and all of its children.
        glm::vec2 mScale = {1.0f, 1.0f};
        // node's box size. used to generate collision shapes
        // and to resize the drawable shape (if any)
        glm::vec2 mSize = {1.0f, 1.0f};
        // Rotation around z axis in radians relative to parent.
        float mRotation = 0.0f;
        // rigid body if any.
        std::unique_ptr<RigidBodyItem> mRigidBody;
        // drawable if any.
        std::unique_ptr<DrawableItem> mDrawable;
    private:
        friend class Scene;
        bool mKilled = false;
    };


    class SceneClass
    {
    public:
        using RenderTree      = TreeNode<SceneNodeClass>;
        using RenderTreeNode  = TreeNode<SceneNodeClass>;
        using RenderTreeValue = SceneNodeClass;

        SceneClass()
        { mClassId = base::RandomString(10); }
        SceneClass(const SceneClass& other);

        // Add a new scene node to the scene.
        // Returns a pointer to the node that was added to the scene.
        SceneNodeClass* AddNode(const SceneNodeClass& node);
        // Add a new scene node to the scene.
        // Returns a pointer to the node that was added to the scene.
        SceneNodeClass* AddNode(SceneNodeClass&& node);
        // Add a new scene node to the scene.
        // Returns a pointer to the node that was added to the scene.
        SceneNodeClass* AddNode(std::unique_ptr<SceneNodeClass> node);
        // Get the scene node by index.
        // The index must be valid.
        SceneNodeClass& GetNode(size_t index);
        // Find scene node by name. Returns nullptr if
        // no such node could be found.
        SceneNodeClass* FindNodeByName(const std::string& name);
        // Find scene node by id. Returns nullptr if
        // no such node could be found.
        SceneNodeClass* FindNodeById(const std::string& id);
        // Get the scene node by index. The index must be valid.
        const SceneNodeClass& GetNode(size_t index) const;
        // Find scene node by name. Returns nullptr if
        // no such node could be found.
        const SceneNodeClass* FindNodeByName(const std::string& name) const;
        // Find scene node by id. Returns nullptr if
        // no such node could be found.
        const SceneNodeClass* FindNodeById(const std::string& id)const;

        // Delete a node by the given index.
        void DeleteNode(size_t index);
        // Delete a node by the given id. Returns tre if the node
        // was found and deleted otherwise false.
        bool DeleteNodeById(const std::string& id);
        // Delete a node by the given name. Returns true if the node
        // was found and deleted otherwise false.
        bool DeleteNodeByName(const std::string& name);

        // Perform coarse hit test to see if the given x,y point
        // intersects with any node's box in the scene.
        // The testing is coarse in the sense that it's done against the node's
        // size box only. The hit nodes are stored in the hits vector and the
        // positions with the nodes' hitboxes are (optionally) stored in the
        // hitbox_positions vector.
        void CoarseHitTest(float x, float y, std::vector<SceneNodeClass*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr);
        void CoarseHitTest(float x, float y, std::vector<const SceneNodeClass*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr) const;

        // Map coordinates in some SceneNode's (see SceneNode::GetNodeTransform) space
        // into scene coordinate space.
        glm::vec2 MapCoordsFromNode(float x, float y, const SceneNodeClass* node) const;
        // Map coordinates in scene coordinate space into some SceneNode's coordinate space.
        glm::vec2 MapCoordsToNode(float x, float y, const SceneNodeClass* node) const;

        // Compute the axis aligned bounding rectangle for the give scene node
        // at the current time of the scene.
        gfx::FRect GetBoundingRect(const SceneNodeClass* node) const;
        // Compute the axis aligned bounding rectangle for the whole scene.
        // i.e. including all the nodes at the current time of scene.
        // This is a shortcut for getting the union of all the bounding rectangles
        // of all the scene nodes.
        gfx::FRect GetBoundingRect() const;

        RenderTree& GetRenderTree()
        { return mRenderTree; }
        const RenderTree& GetRenderTree() const
        { return mRenderTree; }

        std::size_t GetHash() const;
        std::size_t GetNumNodes() const
        { return mNodes.size(); }
        std::string GetId() const
        { return mClassId; }

        std::shared_ptr<const SceneNodeClass> GetSharedSceneNodeClass(size_t index) const
        { return mNodes[index]; }

        // Lookup a SceneNode based on the serialized ID in the JSON.
        SceneNodeClass* TreeNodeFromJson(const nlohmann::json& json);

        // Serialize the scene into JSON.
        nlohmann::json ToJson() const;

        // Serialize an animation node contained in the RenderTree to JSON by doing
        // a shallow (id only) based serialization.
        // Later in TreeNodeFromJson when reading back the render tree we simply
        // look up the node based on the ID.
        static nlohmann::json TreeNodeToJson(const SceneNodeClass* node);

        static std::optional<SceneClass> FromJson(const nlohmann::json& json);

        SceneClass Clone() const;

        SceneClass& operator=(const SceneClass& other);
    private:
        // The class/resource id of this class.
        std::string mClassId;
        // the list of scene nodes that belong to this scene.
        std::vector<std::shared_ptr<SceneNodeClass>> mNodes;
        // The render tree for hierarchical traversal and
        // transformation of the scene and its nodes.
        RenderTree mRenderTree;
    };

    class Scene
    {
    public:
        using RenderTree      = TreeNode<SceneNode>;
        using RenderTreeNode  = TreeNode<SceneNode>;
        using RenderTreeValue = SceneNode;

        // Construct a new scene with the initial state based
        // on the scene class object's state. During the lifetime
        // of the scene objects (nodes) can be dynamically added
        // and removed.
        Scene(std::shared_ptr<const SceneClass> klass);
        Scene(const SceneClass& klass);
        Scene(const Scene& other) = delete;

        // Add a new node to the scene. Note that this doesn't yet insert the
        // node into the scene graph hierarchy. You can either use the render
        // tree directly to find a place where to insert the node or then
        // use some of the provided scene functions.
        // The return value is the pointer of the new node that exists in the scene
        // after the call returns.
        SceneNode* AddNode(const SceneNode& node);
        SceneNode* AddNode(SceneNode&& node);
        SceneNode* AddNode(std::unique_ptr<SceneNode> node);

        // Link the given child node with the parent.
        // The parent may be a nullptr in which case the child
        // is added to the root of the scene. The child node needs
        // to be a valid node and needs to point to node a node
        // that is not yet any part of the render tree and is a
        // node added to the scene. (see AddNode)
        void LinkChild(SceneNode* parent, SceneNode* child);

        // Get the scene node by index. The index must be valid.
        SceneNode& GetNode(size_t index);
        // Find scene node by class name. Returns nullptr if no such node could be found.
        // Note that there could be multiple nodes with the same name. In this
        // case it's undefined which of the nodes would be returned.
        SceneNode* FindNodeByClassName(const std::string& name);
        // Find scene node by class id. Returns nullptr if no such node could be found.
        // Note that there could be multiple nodes with the same class id. In this
        // case it's undefined which of the nodes would be returned.
        SceneNode* FindNodeByClassId(const std::string& id);
        // Find a scene node by node's instance id. Returns nullptr if no such node could be found.
        SceneNode* FindNodeByInstanceId(const std::string& id);
        // Find a scene node by it's instance name. Returns nullptr if no such node could be found.
        SceneNode* FindNodeByInstanceName(const std::string& name);
        // Get the scene node by index. The index must be valid.
        const SceneNode& GetNode(size_t index) const;
        // Find scene node by name. Returns nullptr if no such node could be found.
        // Note that there could be multiple nodes with the same name. In this
        // case it's undefined which of the nodes would be returned.
        const SceneNode* FindNodeByClassName(const std::string& name) const;
        // Find scene node by class id. Returns nullptr if no such node could be found.
        // Note that there could be multiple nodes with the same class id. In this
        // case it's undefined which of the nodes would be returned.
        const SceneNode* FindNodeByClassId(const std::string& id) const;
        // Find scene node by node's instance id. Returns nullptr if no such node could be found.
        const SceneNode* FindNodeByInstanceId(const std::string& id) const;
        // Find a scene node by it's instance name. Returns nullptr if no such node could be found.
        const SceneNode* FindNodeByInstanceName(const std::string& name) const;
        // Delete the node at the given index. This will also delete any
        // child nodes this node might have by recursing the render tree.
        void DeleteNode(size_t index);
        // Delete the node with the given instance id. This will also
        // delete any child nodes this node might have.
        // Returns true if any nodes were deleted otherwise false.
        bool DeleteNodeByInstanceId(const std::string& id);

        // Perform coarse hit test to see if the given x,y point
        // intersects with any node's box in the scene.
        // The testing is coarse in the sense that it's done against the node's
        // size box only. The hit nodes are stored in the hits vector and the
        // positions with the nodes' hitboxes are (optionally) stored in the
        // hitbox_positions vector.
        void CoarseHitTest(float x, float y, std::vector<SceneNode*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr);
        void CoarseHitTest(float x, float y, std::vector<const SceneNode*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr) const;

        // Map coordinates in some SceneNode's (see SceneNode::GetNodeTransform) space
        // into scene coordinate space.
        glm::vec2 MapCoordsFromNode(float x, float y, const SceneNode* node) const;
        // Map coordinates in scene coordinate space into some SceneNode's coordinate space.
        glm::vec2 MapCoordsToNode(float x, float y, const SceneNode* node) const;

        // Compute the axis aligned bounding rectangle for the give scene node
        // at the current time of the scene.
        gfx::FRect GetBoundingRect(const SceneNode* node) const;
        // Compute the axis aligned bounding rectangle for the whole scene.
        // i.e. including all the nodes at the current time of scene.
        // This is a shortcut for getting the union of all the bounding rectangles
        // of all the scene nodes.
        gfx::FRect GetBoundingRect() const;

        FBox GetBoundingBox(const SceneNode* node) const;

        size_t GetNumNodes() const
        { return mNodes.size(); }

        RenderTree& GetRenderTree()
        { return mRenderTree; }
        const RenderTree& GetRenderTree() const
        { return mRenderTree; }

        const SceneClass& GetClass() const
        { return *mClass.get(); }
        const SceneClass* operator->() const
        { return mClass.get(); }

        Scene& operator=(const Scene&) = delete;

        SceneNode* TreeNodeFromJson(const nlohmann::json& json) ;
    private:
        // the class object.
        std::shared_ptr<const SceneClass> mClass;
        // the list of nodes that are in the scene.
        std::vector<std::unique_ptr<SceneNode>> mNodes;
        // map for fast lookup based on the node's instance id.
        std::unordered_map<std::string, SceneNode*> mInstanceIdMap;
        // the render tree for hierarchical traversal and transformation
        // of the scene and its nodes.
        RenderTree mRenderTree;
        // Current scene time.
        double mCurrentTime = 0.0;
    };

    std::unique_ptr<Scene> CreateSceneInstance(std::shared_ptr<const SceneClass> klass);
    std::unique_ptr<Scene> CreateSceneInstance(const SceneClass& klass);
    std::unique_ptr<SceneNode> CreateSceneNodeInstance(std::shared_ptr<const SceneNodeClass> klass);

} // namespace
