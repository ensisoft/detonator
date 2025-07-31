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
#  include <glm/mat4x4.hpp>
#  include <glm/vec2.hpp>
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <unordered_map>
#include <optional>
#include <variant>
#include <queue>

#include "base/allocator.h"
#include "base/bitflag.h"
#include "base/utility.h"
#include "base/math.h"
#include "base/hash.h"
#include "data/fwd.h"
#include "game/tree.h"
#include "game/types.h"
#include "game/enum.h"
#include "game/animation.h"
#include "game/color.h"
#include "game/scriptvar.h"
#include "game/entity_node.h"
#include "game/entity_node_rigid_body_joint.h"
#include "game/entity_state_controller.h"

namespace game
{
    class Scene;

    class EntityClass
    {
    public:
        using RenderTree      = game::RenderTree<EntityNodeClass>;
        using RenderTreeNode  = EntityNodeClass;
        using RenderTreeValue = EntityNodeClass;

        using PhysicsJoint = RigidBodyJointClass;
        using PhysicsJointType = RigidBodyJointClass::JointType;
        using PhysicsJointParams = RigidBodyJointClass::JointParams;
        using RevoluteJointParams = RigidBodyJointClass::RevoluteJointParams;
        using DistanceJointParams = RigidBodyJointClass::DistanceJointParams;
        using WeldJointParams = RigidBodyJointClass::WeldJointParams;
        using MotorJointParams = RigidBodyJointClass::MotorJointParams;
        using PrismaticJointParams = RigidBodyJointClass::PrismaticJointParams;
        using PulleyJointParams = RigidBodyJointClass::PulleyJointParams;

        enum class Flags {
            // Only pertains to editor (todo: maybe this flag should be removed)
            VisibleInEditor,
            // node is visible in the game or not.
            // Even if this is true the node will still need to have some
            // renderable items attached to it such as a shape or
            // animation item.
            VisibleInGame,
            // Limit the lifetime to some maximum amount
            // after which the entity is killed.
            LimitLifetime,
            // Whether to automatically kill entity when it reaches
            // its end of lifetime.
            KillAtLifetime,
            // Whether to automatically kill entity when it reaches (goes past)
            // the border of the scene
            KillAtBoundary,
            // Invoke the tick function on the entity
            TickEntity,
            // Invoke the update function on the entity
            UpdateEntity,
            // Invoke the node update function on the entity
            UpdateNodes,
            // Invoke the post update function on the entity.
            PostUpdate,
            // Whether to pass keyboard events to the entity or not
            WantsKeyEvents,
            // Whether to pass mouse events to the entity or not.
            WantsMouseEvents,
        };

        // for testing purposes make it easier to create an entity class
        // with a known ID.
        explicit EntityClass(std::string id);
        EntityClass();
        EntityClass(const EntityClass& other);

        // Add a new node to the entity.
        // Returns a pointer to the node that was added to the entity.
        // The returned EntityNodeClass object pointer is only guaranteed
        // to be valid until the next call to PlaceEntity/Delete. It is not
        // safe for the caller to hold on to it long term. Instead, the
        // assumed use of the pointer is to simplify for example calling
        // LinkChild for linking the node to the entity's render tree.
        EntityNodeClass* AddNode(const EntityNodeClass& node);
        EntityNodeClass* AddNode(EntityNodeClass&& node);
        EntityNodeClass* AddNode(std::unique_ptr<EntityNodeClass> node);

        void MoveNode(size_t src_index, size_t dst_index);

        // PhysicsJoint lifetimes.
        // For any API function returning a PhysicsJoint* (or const PhysicsJoint*) it's
        // safe to assume that the returned object pointer is valid only
        // until the next call to modify the list of joints. I.e. any call to
        // AddJoint or DeleteJoint can invalidate any previously returned PhysicsJoint*
        // pointer. The caller should not hold on to these pointers long term.

        // Add a new joint definition linking 2 entity nodes with rigid bodies
        // together using a physics joint. The joints must refer to valid nodes
        // that already exist (i.e. the src and dst node class IDst must be valid).
        PhysicsJoint* AddJoint(const PhysicsJoint& joint);
        PhysicsJoint* AddJoint(PhysicsJoint&& joint);
        // Respecify the details of a joint at the given index.
        void SetJoint(size_t index, const PhysicsJoint& joint);
        void SetJoint(size_t index, PhysicsJoint&& joint);
        // Get the joint at the given index. The index must be valid.
        PhysicsJoint& GetJoint(size_t index);
        // Find a joint by the given joint ID. Returns nullptr if no
        // such joint could be found.
        PhysicsJoint* FindJointById(const std::string& id);
        // Find a joint by a node id, i.e. when the joint specifies a
        // connection between either src or dst node having the given
        // node ID. In case of multiple joints connecting between the
        // same nodes the returned node is the first one found.
        PhysicsJoint* FindJointByNodeId(const std::string& id);
        // Get the joint at the given index. The index must be valid.
        const PhysicsJoint& GetJoint(size_t index) const;
        // Find a joint by the given joint ID. Returns nullptr if no
        // such joint could be found.
        const PhysicsJoint* FindJointById(const std::string& id) const;
        // Find a joint by a node id, i.e. when the joint specifies a
        // connection between either src or dst node having the given
        // node ID. In case of multiple joints connecting between the
        // same nodes the returned node is the first one found.
        const PhysicsJoint* FindJointByNodeId(const std::string& id) const;
        // Delete a joint by the given ID.
        void DeleteJointById(const std::string& id);
        // Delete a joint by the given index. The index must be valid.
        void DeleteJoint(std::size_t index);
        // todo:
        void DeleteInvalidJoints();
        // todo:
        void FindInvalidJoints(std::vector<PhysicsJoint*>* invalid);
        // todo:
        void DeleteInvalidFixtures();

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

        // Add a new animation class object. Returns a pointer to the animation
        // that was added to the entity class.
        AnimationClass* AddAnimation(AnimationClass&& track);
        // Add a new animation class object. Returns a pointer to the animation
        // that was added to the entity class.
        AnimationClass* AddAnimation(const AnimationClass& track);
        // Add a new animation class object. Returns a pointer to the animation
        // that was added to the entity class.
        AnimationClass* AddAnimation(std::unique_ptr<AnimationClass> track);
        // Delete an animation by the given index. The index must be valid.
        void DeleteAnimation(size_t index);
        // Delete an animation by the given name.
        // Returns true if a track was deleted, otherwise false.
        bool DeleteAnimationByName(const std::string& name);
        // Delete an animation by the given id.
        // Returns true if a track was deleted, otherwise false.
        bool DeleteAnimationById(const std::string& id);
        // Get the animation class object by index. The index must be valid.
        AnimationClass& GetAnimation(size_t index);
        // Find an animation class object by name.
        // Returns nullptr if no such animation could be found.
        // Returns the first animation object with a matching name.
        AnimationClass* FindAnimationByName(const std::string& name);
        // Get the animation class object by index. The index must be valid.
        const AnimationClass& GetAnimation(size_t index) const;
        // Find an animation class object by name.
        // Returns nullptr if no such animation could be found.
        // Returns the first animation object with a matching name.
        const AnimationClass* FindAnimationByName(const std::string& name) const;

        // Add a new animator class object. Returns a pointer to the animator
        // that was added to the entity class.
        EntityStateControllerClass* AddController(EntityStateControllerClass&& animator);
        EntityStateControllerClass* AddController(const EntityStateControllerClass& animator);
        EntityStateControllerClass* AddController(const std::shared_ptr<EntityStateControllerClass>& animator);

        void DeleteController(size_t index);
        bool DeleteControllerByName(const std::string& name);
        bool DeleteControllerById(const std::string& id);

        EntityStateControllerClass& GetController(size_t index);
        EntityStateControllerClass* FindControllerByName(const std::string& name);
        EntityStateControllerClass* FindControllerById(const std::string& id);

        const EntityStateControllerClass& GetController(size_t index) const;
        const EntityStateControllerClass* FindControllerByName(const std::string& name) const;
        const EntityStateControllerClass* FindControllerById(const std::string& id) const;

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
        // You can then either DeletePlacement to completely delete it or
        // LinkChild to insert it into another part of the render tree.
        void BreakChild(EntityNodeClass* child, bool keep_world_transform = true);

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

        inline void CoarseHitTest(const glm::vec2& pos, std::vector<EntityNodeClass*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr)
        {
            return CoarseHitTest(pos.x, pos.y, hits, hitbox_positions);
        }
        inline void CoarseHitTest(const glm::vec2& pos, std::vector<const EntityNodeClass*>* hits,
                                  std::vector<glm::vec2>* hitbox_positions = nullptr) const
        {
            return CoarseHitTest(pos.x, pos.y, hits, hitbox_positions);
        }

        // Map coordinates in node's OOB space into entity coordinate space. The origin of
        // the OOB space is relative to the "TopLeft" corner of the OOB of the node.
        glm::vec2 MapCoordsFromNodeBox(float x, float y, const EntityNodeClass* node) const;
        glm::vec2 MapCoordsFromNodeBox(const glm::vec2& pos, const EntityNodeClass* node) const;
        // Map coordinates in entity coordinate space into node's OOB coordinate space.
        glm::vec2 MapCoordsToNodeBox(float x, float y, const EntityNodeClass* node) const;
        glm::vec2 MapCoordsToNodeBox(const glm::vec2& pos, const EntityNodeClass* node) const;

        // Compute the axis aligned bounding box (AABB) for the whole entity.
        FRect GetBoundingRect() const;
        // Compute the axis aligned bounding box (AABB) for the given entity node.
        FRect FindNodeBoundingRect(const EntityNodeClass* node) const;
        // Compute the oriented bounding box (OOB) for the given entity node.
        FBox FindNodeBoundingBox(const EntityNodeClass* node) const;

        size_t FindNodeIndex(const EntityNodeClass* node) const;

        // todo:
        glm::mat4 FindNodeTransform(const EntityNodeClass* node) const;
        glm::mat4 FindNodeModelTransform(const EntityNodeClass* node) const;

        // Add a new scripting variable to the list of variables.
        // No checks are made to whether a variable by that name
        // already exists.
        void AddScriptVar(const ScriptVar& var);
        void AddScriptVar(ScriptVar&& var);
        // Delete the scripting variable at the given index.
        // The index must be a valid index.
        void DeleteScriptVar(size_t index);
        // Set the properties (copy over) the scripting variable at the given index.
        // The index must be a valid index.
        void SetScriptVar(size_t index, const ScriptVar& var);
        void SetScriptVar(size_t index, ScriptVar&& var);
        // Get the scripting variable at the given index.
        // The index must be a valid index.
        ScriptVar& GetScriptVar(size_t index);
        // Find a scripting variable with the given name. If no such variable
        // exists then nullptr is returned.
        ScriptVar* FindScriptVarByName(const std::string& name);
        // Find a scripting variable with the given id. If no such variable
        // exists then nullptr is returned.
        ScriptVar* FindScriptVarById(const std::string& id);
        // Get the scripting variable at the given index.
        // The index must be a valid index.
        const ScriptVar& GetScriptVar(size_t index) const;
        // Find a scripting variable with the given name. If no such variable
        // exists then nullptr is returned.
        const ScriptVar* FindScriptVarByName(const std::string& name) const;
        // Find a scripting variable with the given id. If no such variable
        // exists then nullptr is returned.
        const ScriptVar* FindScriptVarById(const std::string& id) const;

        void SetLifetime(float value) noexcept
        { mLifetime = value;}
        void SetFlag(Flags flag, bool on_off) noexcept
        { mFlags.set(flag, on_off); }
        void SetName(const std::string& name)
        { mName = name; }
        void SetTag(const std::string& tag)
        { mTag = tag; }
        void SetIdleTrackId(const std::string& id)
        { mIdleTrackId = id; }
        void SetScriptFileId(const std::string& file)
        { mScriptFile = file; }
        void ResetIdleTrack() noexcept
        { mIdleTrackId.clear(); }
        void ResetScriptFile() noexcept
        { mScriptFile.clear(); }
        bool HasIdleTrack() const noexcept
        { return !mIdleTrackId.empty(); }
        bool HasScriptFile() const noexcept
        { return !mScriptFile.empty(); }
        bool TestFlag(Flags flag) const noexcept
        { return mFlags.test(flag); }
        int32_t GetRenderLayer() const noexcept /* stub*/
        { return 0; }

        RenderTree& GetRenderTree() noexcept
        { return mRenderTree; }
        const RenderTree& GetRenderTree() const noexcept
        { return mRenderTree; }

        std::size_t GetHash() const;
        std::size_t GetNumNodes() const noexcept
        { return mNodes.size(); }
        std::size_t GetNumAnimators() const noexcept
        { return mAnimators.size(); }
        std::size_t GetNumAnimations() const noexcept
        { return mAnimations.size(); }
        std::size_t GetNumScriptVars() const noexcept
        { return mScriptVars.size(); }
        std::size_t GetNumJoints() const noexcept
        { return mJoints.size(); }
        const std::string& GetId() const noexcept
        { return mClassId; }
        const std::string& GetIdleTrackId() const noexcept
        { return mIdleTrackId; }
        const std::string& GetName() const noexcept
        { return mName; }
        const std::string& GetClassName() const noexcept
        { return mName; }
        const std::string& GetTag() const noexcept
        { return mTag; }
        const std::string& GetScriptFileId() const noexcept
        { return mScriptFile; }
        float GetLifetime() const noexcept
        { return mLifetime; }
        const base::bitflag<Flags>& GetFlags() const noexcept
        { return mFlags; }

        std::shared_ptr<const EntityNodeClass> GetSharedEntityNodeClass(size_t index) const noexcept
        { return mNodes[index]; }
        std::shared_ptr<const AnimationClass> GetSharedAnimationClass(size_t index) const noexcept
        { return mAnimations[index]; }
        std::shared_ptr<const ScriptVar> GetSharedScriptVar(size_t index) const noexcept
        { return mScriptVars[index]; }
        std::shared_ptr<const PhysicsJoint> GetSharedJoint(size_t index) const noexcept
        { return mJoints[index]; }
        std::shared_ptr<const EntityStateControllerClass> GetSharedEntityControllerClass(size_t index) const noexcept
        { return mAnimators[index]; }

        EntityNodeAllocator& GetAllocator() const
        { return mAllocator; }

        // Serialize the entity into JSON.
        void IntoJson(data::Writer& data) const;

        bool FromJson(const data::Reader& data);

        EntityClass Clone() const;

        EntityClass& operator=(const EntityClass& other);
    private:
        // The class/resource id of this class.
        std::string mClassId;
        // the human-readable name of the class.
        std::string mName;
        // Arbitrary tag string.
        std::string mTag;
        // the track ID of the idle track that gets played when nothing
        // else is going on. can be empty in which case no animation plays.
        std::string mIdleTrackId;
        // the list of animation tracks that are pre-defined with this
        // type of animation.
        std::vector<std::shared_ptr<AnimationClass>> mAnimations;
        // the list of nodes that belong to this entity.
        std::vector<std::shared_ptr<EntityNodeClass>> mNodes;
        // the list of joints that belong to this entity.
        std::vector<std::shared_ptr<PhysicsJoint>> mJoints;
        // the list of animators
        std::vector<std::shared_ptr<EntityStateControllerClass>> mAnimators;
        // The render tree for hierarchical traversal and
        // transformation of the entity and its nodes.
        RenderTree mRenderTree;
        // Scripting variables. read-only variables are
        // shareable with each entity instance.
        std::vector<std::shared_ptr<ScriptVar>> mScriptVars;
        // the name of the associated script if any.
        std::string mScriptFile;
        // entity class flags.
        base::bitflag<Flags> mFlags;
        // maximum lifetime after which the entity is
        // deleted if LimitLifetime flag is set.
        float mLifetime = 0.0f;
    private:
        mutable EntityNodeAllocator mAllocator;
    };

    // Collection of arguments for creating a new entity
    // with some initial state. The immutable arguments must
    // go here (i.e. the ones that cannot be changed after
    // the entity has been created).
    struct EntityArgs {
        // the class object that defines the type of the entity.
        std::shared_ptr<const EntityClass> klass;
        // A known entity ID. Used with entities that are spawned
        // in the scene based on scene's entity placement done during
        // the scene design.
        std::string id;
        // the entity instance name that is to be used. This is
        // designed to let the game programmer use human readable
        // names for interesting entities such as "Player" and then
        // easily lookup/find those entities during game play.
        std::string name;
        // the transformation parents are relative to the parent
        // of the entity.
        // the instance scale to be used. note that if the entity
        // has rigid body changing the scale dynamically later on
        // after the physics simulation object has been created may
        // not work correctly. Therefore, it's important to use the
        // scaling factor here to set the scale when creating a new 
        // entity.
        glm::vec2  scale    = {1.0f, 1.0f};
        // the entity position relative to parent.
        glm::vec2  position = {0.0f, 0.0f};
        // the entity rotation relative to parent
        float rotation = 0.0f;
        // flag to indicate whether to log events
        // pertaining to this entity or not.
        bool enable_logging = true;
        // flag to control spawning on the background.
        bool async_spawn = false;
        // The scene render layer for the entity.
        std::int32_t render_layer = 0;
        // delay before spawning/placing the entity into the scene.
        float delay = 0.0f;

        // nerfed version of ScriptVar data, missing support
        // for vectors and object references.
        using ScriptVarValue = std::variant<bool, float, int,
                std::string,
                glm::vec2, glm::vec3, glm::vec4,
                Color4f>;
        // The set of Script vars to set on the entity
        // immediately when it's spawned.
        std::unordered_map<std::string, ScriptVarValue> vars;
    };

    class Entity
    {
    public:
        // Runtime management flags
        enum class ControlFlags {
            // The entity has been killed and will be deleted
            // at the end of the current game loop iteration.
            Killed,
            // The entity has been just spawned through a
            // call to Scene::SpawnEntity. The flag will be
            // cleared at the end of the main loop iteration
            // and is thus on only from spawn until the end of
            // loop iteration.
            Spawned,
            // Flag to enable logging related to the entity.
            // Turning off logging is useful when spawning a large
            // number of entities continuously and doing that would
            // create excessive logging.
            EnableLogging,
            // The entity wants to die and be killed and removed
            // from the scene.
            WantsToDie
        };
        using Flags = EntityClass::Flags;
        using RenderTree      = game::RenderTree<EntityNode>;
        using RenderTreeNode  = EntityNode;
        using RenderTreeValue = EntityNode;

        // Construct a new entity with the initial state based
        // on the entity class object's state.
        explicit Entity(const EntityArgs& args);
        explicit Entity(std::shared_ptr<const EntityClass> klass);
        explicit Entity(const EntityClass& klass);
        Entity(const Entity& other) = delete;

        ~Entity();
        
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
        // Find a entity node by its instance name. Returns nullptr if no such node could be found.
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
        // Find a entity node by its instance name. Returns nullptr if no such node could be found.
        const EntityNode* FindNodeByInstanceName(const std::string& name) const;

        // Perform coarse hit test to see if the given x,y point
        // intersects with any node's box in the entity.
        // The testing is coarse in the sense that it's done against the node's
        // size box only. The hit nodes are stored in the hits vector and the
        // positions with the nodes' hitboxes are (optionally) stored in the
        // hitbox_positions vector.
        void CoarseHitTest(float x, float y, std::vector<EntityNode*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr);
        void CoarseHitTest(const glm::vec2& pos, std::vector<EntityNode*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr);
        void CoarseHitTest(float x, float y, std::vector<const EntityNode*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr) const;
        void CoarseHitTest(const glm::vec2& pos, std::vector<const EntityNode*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr) const;

        // Map coordinates in node's OOB space into entity coordinate space. The origin of
        // the OOB space is relative to the "TopLeft" corner of the OOB of the node.
        glm::vec2 MapCoordsFromNodeBox(float x, float y, const EntityNode* node) const;
        glm::vec2 MapCoordsFromNodeBox(const glm::vec2& pos, const EntityNode* node) const;
        // Map coordinates in entity coordinate space into node's OOB coordinate space.
        glm::vec2 MapCoordsToNodeBox(float x, float y, const EntityNode* node) const;
        glm::vec2 MapCoordsToNodeBox(const glm::vec2& pos, const EntityNode* node) const;

        // Compute the axis aligned bounding box (AABB) for the whole entity.
        FRect GetBoundingRect() const;
        // Compute the axis aligned bounding box (AABB) for the given entity node.
        FRect FindNodeBoundingRect(const EntityNode* node) const;
        // Compute the oriented bounding box (OOB) for the given entity node.
        FBox FindNodeBoundingBox(const EntityNode* node) const;

        // todo:
        glm::mat4 FindNodeTransform(const EntityNode* node) const;
        glm::mat4 FindNodeModelTransform(const EntityNode* node) const;
        glm::mat4 FindRelativeTransform(const EntityNode* parent, const EntityNode* child) const;

        void Die();
        void DieIn(float seconds);

        using PostedEventValue = std::variant<
            bool, int, float,
            std::string,
            glm::vec2, glm::vec3, glm::vec4>;
        struct PostedEvent {
            std::string message;
            std::string sender;
            PostedEventValue value;
        };

        struct TimerEvent {
            std::string name;
            float jitter = 0.0f;
        };
        using Event = std::variant<TimerEvent, PostedEvent>;

        void Update(float dt, std::vector<Event>* events = nullptr);

        using EntityStateUpdate = game::EntityStateController::StateUpdate;
        using EntityState = game::EntityState;
        using EntityStateTransition = game::EntityStateTransition;

        // update the entity state controller and receive back a bunch of
        // state updates indicating what is happening. All the updates are
        // notifications, except for the EvalTransition which the caller
        // must handle in order to progress the state machine from one state
        // to another state.
        void UpdateStateController(float dt, std::vector<EntityStateUpdate>* updates);

        // Request the entity state controller to begin a chosen state
        // transition to the given next state.
        // Returns true if the transition was started or false to
        // indicate that the transition was not started. At any given
        // time a maximum of one transition is allowed.
        bool TransitionStateController(const EntityStateTransition* transition, const EntityState* next);

        const EntityState* GetCurrentEntityState() noexcept;
        const EntityStateTransition* GetCurrentEntityStateTransition() noexcept;

        // Play the given animation immediately. If there's any
        // current animation it is replaced.
        Animation* PlayAnimation(std::unique_ptr<Animation> animation);
        // Create a copy of the given animation and play it immediately.
        Animation* PlayAnimation(const Animation& animation);
        // Create a copy of the given animation and play it immediately.
        Animation* PlayAnimation(Animation&& animation);
        // Play a previously recorded animation identified by name.
        // Returns a pointer to the animation instance or nullptr if
        // no such animation could be found.
        Animation* PlayAnimationByName(const std::string& name);
        // Play a previously recorded animation identified by its ID.
        // Returns a pointer to the animation instance or nullptr if
        // no such animation could be found.
        Animation* PlayAnimationById(const std::string& id);

        Animation* QueueAnimation(std::unique_ptr<Animation> animation);

        Animation* QueueAnimationByName(const std::string& name);

        // Play the designated idle animation if there is a designated
        // idle animation and if no other animation is currently playing.
        // Returns a pointer to the animation instance or nullptr if
        // idle animation was started.
        Animation* PlayIdle();
        // Returns true if someone killed the entity.
        bool IsDying() const;
        // Returns true if an animation is currently playing.
        bool IsAnimating() const;
        // Returns true if there are pending (queued) animations
        bool HasPendingAnimations() const;
        // Returns true if the entity lifetime has been exceeded.
        bool HasExpired() const;
        // Returns true if the kill control flag has been set.
        bool HasBeenKilled() const;
        // Returns true the spawn control flag has been set.
        bool HasBeenSpawned() const;
        // Returns true if entity contains entity nodes that have rigid bodies.
        bool HasRigidBodies() const;
        // Returns true if the entity contains entity nodes that have spatial nodes.
        bool HasSpatialNodes() const;
        // Returns true if the entity should be killed at the scene boundary.
        bool KillAtBoundary() const;
        // Returns true if the entity contains nodes that have rendering
        // attachments, i.d. drawables or text items.
        bool HasRenderableItems() const;

        bool HasLights() const;

        using PhysicsJointClass = EntityClass::PhysicsJoint;
        using PhysicsJointType  = EntityClass::PhysicsJointType;
        using PhysicsJointParams = EntityClass::PhysicsJointParams;
        using PhysicsJoint = RigidBodyJoint;

        PhysicsJoint* FindJointById(const std::string& id);
        PhysicsJoint* FindJointByClassId(const std::string& id);

        const PhysicsJoint& GetJoint(std::size_t index) const;
        const PhysicsJoint* FindJointById(const std::string& id) const;
        const PhysicsJoint* FindJointByNodeId(const std::string& id) const;
        const PhysicsJoint* FindJointByClassId(const std::string& id) const;

        // Find a scripting variable.
        // Returns nullptr if there was no variable by this name.
        // Note that the const here only implies that the object
        // may not change in terms of c++ semantics. The actual *value*
        // can still be changed as long as the variable is not read only.
        const ScriptVar* FindScriptVarByName(const std::string& name) const;
        const ScriptVar* FindScriptVarById(const std::string& id) const;

        template<typename T>
        T GetVar(const std::string& name) const
        {
            if (const auto* ptr = FindScriptVarByName(name))
                return ptr->GetValue<T>();
            return T {};
        }
        template<typename T>
        void SetVar(const std::string& name, const T& val)
        {
            if (const auto* ptr = FindScriptVarByName(name))
                ptr->SetValue(val);
        }

        void SetTag(const std::string& tag)
        { mInstanceTag = tag; }
        void SetFlag(ControlFlags flag, bool on_off) noexcept
        { mControlFlags.set(flag, on_off); }
        void SetFlag(Flags flag, bool on_off) noexcept
        { mFlags.set(flag, on_off); }
        void SetParentNodeClassId(const std::string& id)
        { mParentNodeId = id; }
        void SetIdleTrackId(const std::string& id)
        { mIdleTrackId = id; }
        void SetRenderLayer(int32_t layer) noexcept
        { mRenderLayer = layer; }
        void SetLifetime(double lifetime) noexcept
        { mLifetime = lifetime; }
        void SetScene(Scene* scene) noexcept
        { mScene = scene; }
        void SetVisible(bool on_off) noexcept
        { SetFlag(Flags::VisibleInGame, on_off); }
        void SetTimer(const std::string& name, double when)
        { mTimers.push_back({name, when}); }
        void PostEvent(const PostedEvent& event)
        { mEvents.push_back(event); }
        void PostEvent(PostedEvent&& event)
        { mEvents.push_back(std::move(event)); }


        // Get the current track if any. (when IsAnimating is true)
        Animation* GetCurrentAnimation(size_t index) noexcept
        { return mCurrentAnimations[index].get(); }
        // Get the current track if any. (when IsAnimating is true)
        const Animation* GetCurrentAnimation(size_t index) const noexcept
        { return mCurrentAnimations[index].get(); }

        inline size_t GetNumCurrentAnimations()  const noexcept
        { return mCurrentAnimations.size(); }

        // Get the previously completed animation track (if any).
        // This is a transient state that only exists for one
        // iteration of the game loop after the animation is done.
        const std::vector<Animation*> GetFinishedAnimations() const
        {
            std::vector<Animation*> ret;
            for (auto& anim : mFinishedAnimations)
                ret.push_back(anim.get());
            return ret;
        }
        // Get the current scene.
        const Scene* GetScene() const noexcept
        { return mScene; }
        Scene* GetScene() noexcept
        { return mScene; }
        double GetLifetime() const noexcept
        { return mLifetime; }
        double GetTime() const noexcept
        { return mCurrentTime; }
        const std::string& GetIdleTrackId() const noexcept
        { return mIdleTrackId; }
        const std::string& GetParentNodeClassId() const noexcept
        { return mParentNodeId; }
        const std::string& GetClassId() const noexcept
        { return mClass->GetId(); }
        const std::string& GetId() const noexcept
        { return mInstanceId; }
        const std::string& GetClassName() const noexcept
        { return mClass->GetName(); }
        const std::string& GetName() const noexcept
        { return mInstanceName; }
        const std::string& GetTag() const noexcept
        { return mInstanceTag; }
        const std::string& GetScriptFileId() const noexcept
        { return mClass->GetScriptFileId(); }
        std::size_t GetNumNodes() const noexcept
        { return mNodes.size(); }
        std::size_t GetNumJoints() const noexcept
        { return mJoints.size(); }
        std::string GetDebugName() const
        { return mClass->GetName() + "/" + mInstanceName; }
        auto GetRenderLayer() const noexcept
        { return mRenderLayer; }
        bool TestFlag(ControlFlags flag) const noexcept
        { return mControlFlags.test(flag); }
        bool TestFlag(Flags flag) const noexcept
        { return mFlags.test(flag); }
        bool IsVisible() const noexcept
        { return TestFlag(Flags::VisibleInGame); }
        bool HasIdleTrack() const noexcept
        { return !mIdleTrackId.empty() || mClass->HasIdleTrack(); }
        bool HasStateController() const noexcept
        { return mAnimator.has_value(); }
        const EntityStateController* GetStateController() const
        { return base::GetOpt(mAnimator); }
        EntityStateController* GetStateController()
        { return base::GetOpt(mAnimator); }
        RenderTree& GetRenderTree() noexcept
        { return mRenderTree; }
        const RenderTree& GetRenderTree() const noexcept
        { return mRenderTree; }
        const EntityClass& GetClass() const noexcept
        { return *mClass.get(); }
        const EntityClass* operator->() const noexcept
        { return mClass.get(); }
        Entity& operator=(const Entity&) = delete;
    private:
        // the class object.
        std::shared_ptr<const EntityClass> mClass;
        // The entity instance id.
        std::string mInstanceId;
        // the entity instance name (if any)
        std::string mInstanceName;
        // the entity instance tag
        std::string mInstanceTag;
        // When the entity is linked (parented)
        // to another entity this id is the node in the parent
        // entity's render tree that is to be used as the parent
        // of this entity's nodes.
        std::string mParentNodeId;
        std::optional<game::EntityStateController> mAnimator;
        // The current animation if any.
        std::vector<std::unique_ptr<Animation>> mCurrentAnimations;
        // the list of nodes that are in the entity.
        std::vector<EntityNode> mNodes;
        // the list of read-write script variables. read-only ones are
        // shared between all instances and the EntityClass.
        std::vector<ScriptVar> mScriptVars;
        // list of physics joints between nodes with rigid bodies.
        std::vector<PhysicsJoint> mJoints;
        // the render tree for hierarchical traversal and transformation
        // of the entity and its nodes.
        RenderTree mRenderTree;
        // the current scene.
        Scene* mScene = nullptr;
        // remaining time for the until scheduled death takes place
        std::optional<float> mScheduledDeath;
        // Current entity time.
        double mCurrentTime = 0.0;
        // Entity's max lifetime.
        double mLifetime = 0.0;
        // the render layer index.
        int32_t mRenderLayer = 0;
        // entity bit flags
        base::bitflag<Flags> mFlags;
        // id of the idle animation track.
        std::string mIdleTrackId;
        // control flags for the engine itself
        base::bitflag<ControlFlags> mControlFlags;
        // the previously finished animation track (if any)
        std::vector<std::unique_ptr<Animation>> mFinishedAnimations;
        // Currently queued animations
        std::queue<std::unique_ptr<Animation>> mAnimationQueue;

        struct Timer {
            std::string name;
            double when = 0.0f;
        };
        std::vector<Timer> mTimers;
        std::vector<PostedEvent> mEvents;

    };

    std::unique_ptr<Entity> CreateEntityInstance(std::shared_ptr<const EntityClass> klass);
    std::unique_ptr<Entity> CreateEntityInstance(const EntityClass& klass);
    std::unique_ptr<Entity> CreateEntityInstance(const EntityArgs& args);
    std::unique_ptr<EntityNode> CreateEntityNodeInstance(std::shared_ptr<const EntityNodeClass> klass);

} // namespace

namespace std {
    template<>
    struct iterator_traits<game::EntityNodeTransformSequence::iterator> {
        using value_type = game::EntityNodeTransform;
        using pointer    = game::EntityNodeTransform*;
        using reference  = game::EntityNodeTransform&;
        using size_type  = size_t;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;
    };

    template<>
    struct iterator_traits<game::EntityNodeDataSequence::iterator> {
        using value_type = game::EntityNodeData;
        using pointer    = game::EntityNodeData*;
        using reference  = game::EntityNodeData&;
        using size_type  = size_t;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;
    };

} // std
