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
#  include <nlohmann/json_fwd.hpp>
#include "warnpop.h"

#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <unordered_map>

#include "base/bitflag.h"
#include "base/utility.h"
#include "base/math.h"
#include "graphics/types.h"
#include "graphics/drawable.h"
#include "gamelib/tree.h"
#include "gamelib/types.h"
#include "gamelib/enum.h"
#include "gamelib/animation.h"

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
            Enabled,
            // Whether the rigid body can go to sleep (i.e. simulation stops)
            // when the body comes to a halt
            CanSleep,
            // Discard rotational component of physics simulation for this body.
            // Useful for things such as player character that should stay "upright
            DiscardRotation
        };
        RigidBodyItemClass()
        {
            mBitFlags.set(Flags::Enabled, true);
            mBitFlags.set(Flags::CanSleep, true);
        }

        std::size_t GetHash() const;

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

        nlohmann::json ToJson() const;

        static std::optional<RigidBodyItemClass> FromJson(const nlohmann::json& json);
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

    // Drawable item defines a drawable item and its material and
    // properties that affect the rendering of the entity node
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

        std::size_t GetHash() const;

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

        nlohmann::json ToJson() const;

        static std::optional<DrawableItemClass> FromJson(const nlohmann::json& json);
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

    class EntityNodeClass
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

        EntityNodeClass()
        {
            mClassId = base::RandomString(10);
            mBitFlags.set(Flags::VisibleInEditor, true);
            mBitFlags.set(Flags::VisibleInGame, true);
        }
        EntityNodeClass(const EntityNodeClass& other);
        EntityNodeClass(EntityNodeClass&& other);

        // Get the class id.
        std::string GetId() const
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
        // of the this node.
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

        int GetLayer() const
        { return mDrawable ? mDrawable->GetLayer() : 0; }

        void Update(float time, float dt);
        // Serialize the node into JSON.
        nlohmann::json ToJson() const;
        // Load the node's properties from the given JSON object.
        static std::optional<EntityNodeClass> FromJson(const nlohmann::json& json);
        // Make a new unique copy of this node class object
        // with all the same properties but with a different/unique ID.
        EntityNodeClass Clone() const;

        EntityNodeClass& operator=(const EntityNodeClass& other);
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


    class EntityNode
    {
    public:
        using Flags = EntityNodeClass::Flags;
        using DrawableItemType = DrawableItem;

        EntityNode(std::shared_ptr<const EntityNodeClass> klass);
        EntityNode(const EntityNodeClass& klass);
        EntityNode(const EntityNode& other);
        EntityNode(EntityNode&& other);

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
        std::string GetId() const
        { return mInstId; }
        std::string GetName() const
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
        { return mClass->GetId(); }
        std::string GetClassName() const
        { return mClass->GetName(); }
        int GetLayer() const
        { return mClass->GetLayer(); }

        // Reset node's state to initial class state.
        void Reset();
        // Get the transform that applies to this node
        // and the subsequent hierarchy of nodes.
        glm::mat4 GetNodeTransform() const;
        // Get this drawable item's model transform that applies
        // to the node's box based items such as drawables
        // and rigid bodies.
        glm::mat4 GetModelTransform() const;

        const EntityNodeClass& GetClass() const
        { return *mClass.get(); }
        const EntityNodeClass* operator->() const
        { return mClass.get(); }
    private:
        // the class object.
        std::shared_ptr<const EntityNodeClass> mClass;
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
    };

    class EntityClass
    {
    public:
        using RenderTree      = game::RenderTree<EntityNodeClass>;
        using RenderTreeNode  = EntityNodeClass;
        using RenderTreeValue = EntityNodeClass;

        EntityClass()
        { mClassId = base::RandomString(10); }
        EntityClass(const EntityClass& other);

        // Add a new node to the entity.
        // Returns a pointer to the node that was added to the entity.
        EntityNodeClass* AddNode(const EntityNodeClass& node);
        EntityNodeClass* AddNode(EntityNodeClass&& node);
        EntityNodeClass* AddNode(std::unique_ptr<EntityNodeClass> node);

        // Get the node by index. The index must be valid.
        EntityNodeClass& GetNode(size_t index);
        // Find entity node by name. Returns nullptr if
        // no such node could be found.
        EntityNodeClass* FindNodeByName(const std::string& name);
        // Find entity node by id. Returns nullptr if
        // no such node could be found.
        EntityNodeClass* FindNodeById(const std::string& id);
        // Get the entity node by index. The index must be valid.
        const EntityNodeClass& GetNode(size_t index) const;
        // Find entity node by name. Returns nullptr if
        // no such node could be found.
        const EntityNodeClass* FindNodeByName(const std::string& name) const;
        // Find entity node by id. Returns nullptr if
        // no such node could be found.
        const EntityNodeClass* FindNodeById(const std::string& id) const;

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


        // Link the given child node with the parent.
        // The parent may be a nullptr in which case the child
        // is added to the root of the entity. The child node needs
        // to be a valid node and needs to point to node that is not
        // yet any part of the render tree and is a node that belongs
        // to this entity.
        void LinkChild(EntityNodeClass* parent, EntityNodeClass* child);

        // Break a child node away from its parent. The child node needs
        // to be a valid node and needs to point to a node that is added
        // to the render tree and belongs to this entity class object.
        // The child (and all of its children) that has been broken still
        // exists in the entity but is removed from the render tree.
        // You can then either DeleteNode to completely delete it or
        // LinkChild to insert it into another part of the render tree.
        void BreakChild(EntityNodeClass* child);

        // Re-parent a child node from its current parent to another parent.
        // Both the child node and the parent node to be a valid nodes and
        // need to point to nodes that are part of the render tree and belong
        // to this entity class object. This will move the whole hierarchy of
        // nodes starting from child under the new parent.
        // If keep_world_transform is true the child will be transformed such
        // that it's current world transformation remains the same. I.e  it's
        // position and rotation in the world don't change.
        void ReparentChild(EntityNodeClass* parent, EntityNodeClass* child, bool keep_world_transform = true);

        // Delete a node from the entity. The given node and all of its
        // children will be removed from the entity render tree and then deleted.
        void DeleteNode(EntityNodeClass* node);

        // Duplicate an entire node hierarchy starting at the given node
        // and add the resulting hierarchy to node's parent.
        // Returns the root node of the new node hierarchy.
        EntityNodeClass* DuplicateNode(const EntityNodeClass* node);

        // Perform coarse hit test to see if the given x,y point
        // intersects with any node's box in the entity.
        // The testing is coarse in the sense that it's done against the node's
        // size box only. The hit nodes are stored in the hits vector and the
        // positions with the nodes' hitboxes are (optionally) stored in the
        // hitbox_positions vector.
        void CoarseHitTest(float x, float y, std::vector<EntityNodeClass*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr);
        void CoarseHitTest(float x, float y, std::vector<const EntityNodeClass*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr) const;

        // Map coordinates in some node's (see EntityNode::GetNodeTransform) space
        // into entity coordinate space.
        glm::vec2 MapCoordsFromNode(float x, float y, const EntityNodeClass* node) const;
        // Map coordinates in entity coordinate space into some node's coordinate space.
        glm::vec2 MapCoordsToNode(float x, float y, const EntityNodeClass* node) const;

        // Compute the axis aligned bounding rectangle for the given node
        // at the current time.
        gfx::FRect GetBoundingRect(const EntityNodeClass* node) const;
        // Compute the axis aligned bounding rectangle for the whole entity.
        // i.e. including all the nodes at the current time.
        // This is a shortcut for getting the union of all the bounding rectangles
        // of all the entity nodes.
        gfx::FRect GetBoundingRect() const;

        FBox GetBoundingBox(const EntityNodeClass* node) const;

        void AddScriptVar(const ScriptVar& var);
        void AddScriptVar(ScriptVar&& var);
        void DeleteScriptVar(size_t index);
        void SetScriptVar(size_t index, const ScriptVar& var);
        void SetScriptVar(size_t index, ScriptVar&& var);
        ScriptVar& GetScriptVar(size_t index);
        ScriptVar* FindScriptVar(const std::string& name);
        const ScriptVar& GetScriptVar(size_t index) const;
        const ScriptVar* FindScriptVar(const std::string& name) const;

        RenderTree& GetRenderTree()
        { return mRenderTree; }
        const RenderTree& GetRenderTree() const
        { return mRenderTree; }

        std::size_t GetHash() const;
        std::size_t GetNumNodes() const
        { return mNodes.size(); }
        std::size_t GetNumTracks() const
        { return mAnimationTracks.size(); }
        std::size_t GetNumScriptVars() const
        { return mScriptVars.size(); }
        std::string GetId() const
        { return mClassId; }

        std::shared_ptr<const EntityNodeClass> GetSharedEntityNodeClass(size_t index) const
        { return mNodes[index]; }
        std::shared_ptr<const AnimationTrackClass> GetSharedAnimationTrackClass(size_t index) const
        { return mAnimationTracks[index]; }
        std::shared_ptr<const ScriptVar> GetSharedScriptVar(size_t index) const
        { return mScriptVars[index]; }

        // Serialize the entity into JSON.
        nlohmann::json ToJson() const;

        static std::optional<EntityClass> FromJson(const nlohmann::json& json);

        EntityClass Clone() const;

        EntityClass& operator=(const EntityClass& other);
    private:
        // The class/resource id of this class.
        std::string mClassId;
        // the list of animation tracks that are pre-defined with this
        // type of animation.
        std::vector<std::shared_ptr<AnimationTrackClass>> mAnimationTracks;
        // the list of nodes that belong to this entity.
        std::vector<std::shared_ptr<EntityNodeClass>> mNodes;
        // The render tree for hierarchical traversal and
        // transformation of the entity and its nodes.
        RenderTree mRenderTree;
        // Scripting variables. read-only variables are
        // shareable with each entity instance.
        std::vector<std::shared_ptr<ScriptVar>> mScriptVars;
    };

    // Collection of arguments for creating a new entity
    // with some initial state. 
    struct EntityArgs {
        // the class object that defines the type of the entity.
        std::shared_ptr<const EntityClass> klass;
        // the entity instance id that is to be used.
        std::string id;
        // the entity instance name that is to be used.
        std::string name;
        // the transformation parents are relative to the parent
        // of the entity.
        // the instance scale to be used. note that if the entity
        // has rigid body changing the scale dynamically later on
        // after the physics simulation object has been created may
        // not work correctly. therefore it's important to use the 
        // scaling factor here to set the scale when creating a new 
        // entity.
        glm::vec2  scale    = {1.0f, 1.0f};
        // the entity position relative to parent.
        glm::vec2  position = {0.0f, 0.0f};
        // the entity rotation relative to parent
        float      rotation = 0.0f;
        // the render layer index.
        int        layer = 0;
        EntityArgs() {
            id = base::RandomString(10);
        }
    };

    class Entity
    {
    public:
        enum class Flags {
            // Only pertains to editor (todo: maybe this flag should be removed)
            VisibleInEditor,
            // node is visible in the game or not.
            // Even if this is true the node will still need to have some
            // renderable items attached to it such as a shape or
            // animation item.
            VisibleInGame
        };
        using RenderTree      = game::RenderTree<EntityNode>;
        using RenderTreeNode  = EntityNode;
        using RenderTreeValue = EntityNode;

        // Construct a new entity with the initial state based
        // on the entity class object's state.
        Entity(const EntityArgs& args);
        Entity(std::shared_ptr<const EntityClass> klass);
        Entity(const EntityClass& klass);
        Entity(const Entity& other) = delete;

        // Add a new node to the entity. Note that this doesn't yet insert the
        // node into the render tree. You can either use the render tree directly
        // to find a place where to insert the node or then use some of the
        // provided functions such as LinkChild.
        // The return value is the pointer of the new node that exists in the entity
        // after the call returns.
        EntityNode* AddNode(const EntityNode& node);
        EntityNode* AddNode(EntityNode&& node);
        EntityNode* AddNode(std::unique_ptr<EntityNode> node);

        // Link the given child node with the parent.
        // The parent may be a nullptr in which case the child
        // is added to the root of the entity. The child node needs
        // to be a valid node and needs to point to node that is not
        // yet any part of the render tree and is a node that belongs
        // to this entity.
        void LinkChild(EntityNode* parent, EntityNode* child);

        // Get the entity node by index. The index must be valid.
        EntityNode& GetNode(size_t index);
        // Find entity node by class name. Returns nullptr if no such node could be found.
        // Note that there could be multiple nodes with the same name. In this
        // case it's undefined which of the nodes would be returned.
        EntityNode* FindNodeByClassName(const std::string& name);
        // Find entity node by class id. Returns nullptr if no such node could be found.
        // Note that there could be multiple nodes with the same class id. In this
        // case it's undefined which of the nodes would be returned.
        EntityNode* FindNodeByClassId(const std::string& id);
        // Find a entity node by node's instance id. Returns nullptr if no such node could be found.
        EntityNode* FindNodeByInstanceId(const std::string& id);
        // Find a entity node by it's instance name. Returns nullptr if no such node could be found.
        EntityNode* FindNodeByInstanceName(const std::string& name);
        // Get the entity node by index. The index must be valid.
        const EntityNode& GetNode(size_t index) const;
        // Find entity node by name. Returns nullptr if no such node could be found.
        // Note that there could be multiple nodes with the same name. In this
        // case it's undefined which of the nodes would be returned.
        const EntityNode* FindNodeByClassName(const std::string& name) const;
        // Find entity node by class id. Returns nullptr if no such node could be found.
        // Note that there could be multiple nodes with the same class id. In this
        // case it's undefined which of the nodes would be returned.
        const EntityNode* FindNodeByClassId(const std::string& id) const;
        // Find entity node by node's instance id. Returns nullptr if no such node could be found.
        const EntityNode* FindNodeByInstanceId(const std::string& id) const;
        // Find a entity node by it's instance name. Returns nullptr if no such node could be found.
        const EntityNode* FindNodeByInstanceName(const std::string& name) const;
        // Delete the node at the given index. This will also delete any
        // child nodes this node might have by recursing the render tree.
        void DeleteNode(EntityNode* node);

        // Perform coarse hit test to see if the given x,y point
        // intersects with any node's box in the entity.
        // The testing is coarse in the sense that it's done against the node's
        // size box only. The hit nodes are stored in the hits vector and the
        // positions with the nodes' hitboxes are (optionally) stored in the
        // hitbox_positions vector.
        void CoarseHitTest(float x, float y, std::vector<EntityNode*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr);
        void CoarseHitTest(float x, float y, std::vector<const EntityNode*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr) const;

        // Map coordinates in some EntityNode's (see EntityNode::GetNodeTransform) space
        // into entity coordinate space.
        glm::vec2 MapCoordsFromNode(float x, float y, const EntityNode* node) const;
        // Map coordinates in entity coordinate space into some EntityNode's coordinate space.
        glm::vec2 MapCoordsToNode(float x, float y, const EntityNode* node) const;

        // Get entity's transform (relative to its parent) expressed as
        // transformation matrix. (Called node transform because it makes
        // generic code easier in other parts of the system)
        glm::mat4 GetNodeTransform() const;

        // Compute the axis aligned bounding rectangle for the give entity node
        // at the current time of the entity.
        gfx::FRect GetBoundingRect(const EntityNode* node) const;
        // Compute the axis aligned bounding rectangle for the whole entity
        // i.e. including all the nodes at the current time of entity.
        // This is a shortcut for getting the union of all the bounding rectangles
        // of all the entity nodes.
        gfx::FRect GetBoundingRect() const;

        FBox GetBoundingBox(const EntityNode* node) const;

        void Update(float dt);

        // Play the given animation track.
        void Play(std::unique_ptr<AnimationTrack> track);
        void Play(const AnimationTrack& track)
        { Play(std::make_unique<AnimationTrack>(track)); }
        void Play(AnimationTrack&& track)
        { Play(std::make_unique<AnimationTrack>(std::move(track))); }
        // Play a previously recorded (stored in the animation class object)
        // animation track identified by name. Note that there could be
        // ambiguity between the names, i.e. multiple tracks with the same name.
        void PlayAnimationByName(const std::string& name);
        // Play a previously recorded (stored in the animation class object)
        // animation track identified by its track id.
        void PlayAnimationById(const std::string& id);
        // Returns true if an animation track is still playing.
        bool IsPlaying() const;

        // Find a scripting variable for read-only access.
        // Returns nullptr if there was no variable by this name.
        // Note that the const here only implies that the object
        // may not change in terms of c++ semantics. The actual *value*
        // can still be changed as long as the variable is not read only.
        const ScriptVar* FindScriptVar(const std::string& name) const;

        void SetTranslation(const glm::vec2& position)
        { mPosition = position; }
        void SetRotation(float angle)
        { mRotation = angle; }
        void SetFlag(Flags flag, bool on_off)
        { mFlags.set(flag, on_off); }
        void SetScale(const glm::vec2& scale);

        // Get the current track if any. (when IsPlaying is true)
        AnimationTrack* GetCurrentTrack()
        { return mAnimationTrack.get(); }
        const AnimationTrack* GetCurrentTrack() const
        { return mAnimationTrack.get(); }

        std::string GetClassId() const
        { return mClass->GetId(); }
        std::size_t GetNumNodes() const
        { return mNodes.size(); }
        std::string GetName() const
        { return mInstanceName; }
        std::string GetId() const
        { return mInstanceId; }
        glm::vec2 GetTranslation() const
        { return mPosition; }
        glm::vec2 GetScale() const
        { return mScale; }
        int GetLayer() const
        { return mLayer; }
        float GetRotation() const
        { return mRotation; }
        bool TestFlag(Flags flag) const
        { return mFlags.test(flag); }
        RenderTree& GetRenderTree()
        { return mRenderTree; }
        const RenderTree& GetRenderTree() const
        { return mRenderTree; }
        const EntityClass& GetClass() const
        { return *mClass.get(); }
        const EntityClass* operator->() const
        { return mClass.get(); }
        Entity& operator=(const Entity&) = delete;
    private:
        // the class object.
        std::shared_ptr<const EntityClass> mClass;
        // The entity instance id.
        std::string mInstanceId;
        // the entity instance name (if any)
        std::string mInstanceName;
        // The current animation track if any.
        std::unique_ptr<AnimationTrack> mAnimationTrack;
        // the list of nodes that are in the entity.
        std::vector<std::unique_ptr<EntityNode>> mNodes;
        // the list of script variables. read-only ones can
        // be shared between all instances and the EntityClass.
        std::vector<ScriptVar> mScriptVars;
        // the render tree for hierarchical traversal and transformation
        // of the entity and its nodes.
        RenderTree mRenderTree;
        // Current entity time.
        double mCurrentTime = 0.0;
        // Current entity position in it's parent frame.
        glm::vec2 mPosition = {0.0f, 0.0f};
        // Current entity scaling factor that applies to
        // all of its nodes.
        glm::vec2 mScale = {1.0f, 1.0f};
        // Current entity rotation angle relative to its parent.
        float mRotation = 0.0f;
        // the render layer index.
        int mLayer = 0;
        // entity bit flags
        base::bitflag<Flags> mFlags;
    };

    std::unique_ptr<Entity> CreateEntityInstance(std::shared_ptr<const EntityClass> klass);
    std::unique_ptr<Entity> CreateEntityInstance(const EntityClass& klass);
    std::unique_ptr<Entity> CreateEntityInstance(const EntityArgs& args);
    std::unique_ptr<EntityNode> CreateEntityNodeInstance(std::shared_ptr<const EntityNodeClass> klass);

} // namespace
