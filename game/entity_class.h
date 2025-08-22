// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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

#include "base/bitflag.h"
#include "game/enum.h"
#include "game/tree.h"
#include "game/entity_node.h"
#include "game/entity_node_rigid_body.h"
#include "game/entity_node_rigid_body_joint.h"

namespace game
{
    class AnimationClass;
    class EntityStateControllerClass;
    class ScriptVar;

    // Any API function that has the form Object* AddXYZ will return
    // the object that was added to the entity. But the lifetime of
    // the returned object is guaranteed to be valid only until the
    // next call to Add or Delete an object. It is not considered
    // safe for the caller to hold on to the returned object long
    // term. Instead the intended uee case is to simplify calling
    // some other function that requires the object immediately
    // after having added it to the entity. For example LinkChild
    // for linking entity nodes to the render tree.

    // Any API function that uses an index such as GetNode, GetJoint,
    // DeleteNode, etc. expects the index to be valid. Using an
    // invalid index is a violation of the API contract and will
    // abort the program.

    // Any API function that Finds an object such as FindNode, FindJoint
    // etc. can return a nullptr to indicate that no such object
    // was found. In case of multiple objects matching the search
    // criteria it's unspecified which one is returned (but typically
    // it will the first object.)

    class EntityClass
    {
    public:
        using RenderTree           = game::RenderTree<EntityNodeClass>;
        using RenderTreeNode       = EntityNodeClass;
        using RenderTreeValue      = EntityNodeClass;
        using PhysicsJoint         = RigidBodyJointClass;
        using PhysicsJointType     = RigidBodyJointClass::JointType;
        using PhysicsJointParams   = RigidBodyJointClass::JointParams;
        using RevoluteJointParams  = RigidBodyJointClass::RevoluteJointParams;
        using DistanceJointParams  = RigidBodyJointClass::DistanceJointParams;
        using WeldJointParams      = RigidBodyJointClass::WeldJointParams;
        using MotorJointParams     = RigidBodyJointClass::MotorJointParams;
        using PrismaticJointParams = RigidBodyJointClass::PrismaticJointParams;
        using PulleyJointParams    = RigidBodyJointClass::PulleyJointParams;

        using Flags = EntityFlags;

        // for testing purposes make it easier to create an entity class
        // with a known ID.
        explicit EntityClass(std::string id);
        EntityClass();
        EntityClass(const EntityClass& other);
        ~EntityClass();

        // Add new entity node to the entity. Note that the entity node
        // is not automatically added to the render tree and thus will
        // not render. In order to add the entity node to render tree
        // call LinkChild with the expected parent.
        EntityNodeClass* AddNode(const EntityNodeClass& node);
        EntityNodeClass* AddNode(EntityNodeClass&& node);
        EntityNodeClass* AddNode(std::unique_ptr<EntityNodeClass> node);

        EntityNodeClass& GetNode(size_t index);
        EntityNodeClass* FindNodeByName(const std::string& name);
        EntityNodeClass* FindNodeById(const std::string& id);
        EntityNodeClass* FindNodeParent(const EntityNodeClass* node);

        const EntityNodeClass& GetNode(size_t index) const;
        const EntityNodeClass* FindNodeByName(const std::string& name) const;
        const EntityNodeClass* FindNodeById(const std::string& id) const;
        const EntityNodeClass* FindNodeParent(const EntityNodeClass* node) const;

        // PhysicsJoint connects between two rigid bodies and specifies a constraint
        // to the physics engine to alter their movement/motion relative to each other.
        // The joints must refer to valid nodes.

        PhysicsJoint* AddJoint(const PhysicsJoint& joint);
        PhysicsJoint* AddJoint(PhysicsJoint&& joint);

        // Respecify the details of a joint at the given index.
        void SetJoint(size_t index, const PhysicsJoint& joint);
        void SetJoint(size_t index, PhysicsJoint&& joint);

        PhysicsJoint& GetJoint(size_t index);
        PhysicsJoint* FindJointById(const std::string& id);
        PhysicsJoint* FindJointByNodeId(const std::string& id);

        const PhysicsJoint& GetJoint(size_t index) const;
        const PhysicsJoint* FindJointById(const std::string& id) const;
        const PhysicsJoint* FindJointByNodeId(const std::string& id) const;

        void DeleteJoint(std::size_t index);
        void DeleteJointById(const std::string& id);

        // Cleanup and delete joints that have had their connected nodes
        // deleted. Use this at design time to cleanup joints after removing
        // nodes from the entity class to make sure that all joints are
        // valid and refer to nodes that actually exist.
        void DeleteInvalidJoints();

        // Delete  invalid fixtures that refer to nodes that are not found
        // or that no longer have a rigid body to which the fixture can
        // attach to.
        void DeleteInvalidFixtures();

        // Discover and list currently invalid joints.
        void FindInvalidJoints(std::vector<PhysicsJoint*>* invalid);

        // Add a new entity animation track. Animations change the entity
        // node properties over time in order to produce motion or visual
        // change in the entity instance.

        AnimationClass* AddAnimation(AnimationClass&& track);
        AnimationClass* AddAnimation(const AnimationClass& track);
        AnimationClass* AddAnimation(std::unique_ptr<AnimationClass> track);

        AnimationClass& GetAnimation(size_t index);
        AnimationClass* FindAnimationByName(const std::string& name);

        const AnimationClass& GetAnimation(size_t index) const;
        const AnimationClass* FindAnimationByName(const std::string& name) const;

        void DeleteAnimation(size_t index);
        bool DeleteAnimationByName(const std::string& name);
        bool DeleteAnimationById(const std::string& id);
        void DeleteAnimations();

        // Add a new animator class object.
        EntityStateControllerClass* SetStateController(EntityStateControllerClass&& animator);
        EntityStateControllerClass* SetStateController(const EntityStateControllerClass& animator);
        EntityStateControllerClass* SetStateController(const std::shared_ptr<EntityStateControllerClass>& animator);
        EntityStateControllerClass* GetStateController();
        const EntityStateControllerClass* GetStateController() const;
        void DeleteStateController();

        // Scripting variables provide variable properties/data that the game
        // can define and add to the entity class type. For example in a space
        // game a "SpaceShip" entity class can have a vector2 variable "velocity"
        // that the game uses to perform ship motion over time.
        // The variable is easily available in the Lua scripting environment
        // with the dot notation 'my_ship.velocity'.

        void AddScriptVar(const ScriptVar& var);
        void AddScriptVar(ScriptVar&& var);
        void DeleteScriptVar(size_t index);

        // Set the properties (copy over) the scripting variable at the given index.
        // The index must be a valid index.
        void SetScriptVar(size_t index, const ScriptVar& var);
        void SetScriptVar(size_t index, ScriptVar&& var);

        ScriptVar& GetScriptVar(size_t index);
        ScriptVar* FindScriptVarByName(const std::string& name);
        ScriptVar* FindScriptVarById(const std::string& id);

        const ScriptVar& GetScriptVar(size_t index) const;
        const ScriptVar* FindScriptVarByName(const std::string& name) const;
        const ScriptVar* FindScriptVarById(const std::string& id) const;

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

        // Move node around in the node array. Both src and dst indices
        // must valid indices.
        void MoveNode(size_t src_index, size_t dst_index);

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
        void CoarseHitTest(const Float2& point, std::vector<EntityNodeClass*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr);
        void CoarseHitTest(const Float2& point, std::vector<const EntityNodeClass*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr) const;

        // Map coordinates in node's OOB space into entity coordinate space.
        // The origin of the OOB space is relative to the "TopLeft" corner of the OOB of the node.
        Float2 MapCoordsFromNodeBox(const Float2& coordinates, const EntityNodeClass* node) const;

        // Map coordinates in node coordinate space into entity coordinate space.
        // The origin of the coordinate space is in the middle of the node.
        Float2 MapCoordsFromNode(const Float2& coordinates, const EntityNodeClass* node) const;

        // Map coordinates in entity coordinate space into node's OOB coordinate space.
        Float2 MapCoordsToNodeBox(const Float2& coordinates, const EntityNodeClass* node) const;

        Float2 MapCoordsToNode(const Float2& coordinates, const EntityNodeClass* node) const;

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

        // Set the expected maximum lifetime for the entity to live after
        // it has been spawned into scene. Note that the entity is not
        // automatically killed unless also the LimitLifetime flag is set.
        inline void SetLifetime(float value) noexcept
        { mLifetime = value;}

        // Set an entity class flag to control the entity functionality
        // and runtime behaviour. See the flags for more details.
        inline void SetFlag(Flags flag, bool on_off) noexcept
        { mFlags.set(flag, on_off); }

        // Set the (human-readable) entity class name.
        inline void SetName(std::string name) noexcept
        { mName = std::move(name); }

        // Set the (human-readable) entity class tag string. Tags
        // are arbitrary strings that can be used to find entities
        // during game play by string tag matching. The tag string
        // format is arbitrary and up to the game.  The find function
        // is Scene::ListEntitiesByTag.
        inline void SetTag(std::string tag)
        { mTag = std::move(tag); }

        // Set the idle animation track ID. When set the entity plays
        // the designed "idle" animation whenever there are no other
        // animations taking place. This is convenient to make the
        // entity always return to some expected "idle" animation
        // loop, for example when the character is not being controlled
        // by the player etc.
        inline void SetIdleTrackId(std::string id)
        { mIdleTrackId = std::move(id); }

        // Set the Lua script file name/ID (resource URI) that is used
        // to identify the right Lua script file when running the game.
        inline void SetScriptFileId(std::string file)
        { mScriptFile = std::move(file); }

        // Reset the entity idle track to nothing.
        inline void ResetIdleTrack() noexcept
        { mIdleTrackId.clear(); }

        // Reset the entity script file ID to nothing.
        inline void ResetScriptFile() noexcept
        { mScriptFile.clear(); }

        // Returns whether the entity has a designated idle animation
        // track or not. (See SetIdleTrack for more details about
        // idle animation).
        inline bool HasIdleTrack() const noexcept
        { return !mIdleTrackId.empty(); }

        // Returns whether the entity has a Lua script file ID set or not.
        inline bool HasScriptFile() const noexcept
        { return !mScriptFile.empty(); }

        // Test an entity class flag. Returns true if the flag is set
        // and false if the flag is not set. See Flags for more details
        // about specific flags.
        inline bool TestFlag(Flags flag) const noexcept
        { return mFlags.test(flag); }

        // Returns whether the class runtime services have been initialized
        // or not. The runtime contains values that can be initialized once
        // per class object and then used to server all *instances* of the
        // specific entity type.
        inline bool HaveRuntime() const noexcept
        { return mInitRuntime; }

        // Get the entity render layer. This method is a stub and always
        // returns 0. When dealing with classes the render layer is stored
        // in the entity placement and during instantiation the placement
        // information is set in the entity instance. This method only
        // exists to provide uniform / generic API between entity class
        // and instance in order to simplify some stuff such as the renderer.
        inline int32_t GetRenderLayer() const noexcept /* stub*/
        { return 0; }

        // Get the entity render tree. The render tree manages the entity
        // node transformation order, i.e. the parent-child relationship.
        // Normally you would not use the render tree directly but rather
        // use the appropriate entity class methods to manipulate it
        // when for example adding entity nodes.
        inline auto& GetRenderTree() noexcept
        { return mRenderTree; }
        inline const auto& GetRenderTree() const noexcept
        { return mRenderTree; }

        // Get the current entity node count.
        inline auto GetNumNodes() const noexcept
        { return mNodes.size(); }

        //Get the current state controller count.
        inline bool HasStateController() const
        { return !!mStateController; }

        // Get the current animation track count.
        inline auto GetNumAnimations() const noexcept
        { return mAnimations.size(); }

        // Get the current scripting variable count.
        inline auto GetNumScriptVars() const noexcept
        { return mScriptVars.size(); }

        // Get the current physics joint count.
        inline auto GetNumJoints() const noexcept
        { return mJoints.size(); }

        // Get the unique class ID.
        inline const auto& GetId() const noexcept
        { return mClassId; }

        // Get the current idle tack ID that specifies the designated idle
        // track animation (if any). If there's no idle track then an empty
        // string is returned.
        inline const auto& GetIdleTrackId() const noexcept
        { return mIdleTrackId; }

        // Get the human-readable object name. Since *this* object is a
        // class object the returned value will be the same as GetClassName.
        inline const auto& GetName() const noexcept
        { return mName; }

        // Get the human-readable class name.
        inline const auto& GetClassName() const noexcept
        { return mName; }

        // Get the human-readable free form tag string if any.
        inline const auto& GetTag() const noexcept
        { return mTag; }

        // Get the associated Lua script file ID if any.
        inline const auto& GetScriptFileId() const noexcept
        { return mScriptFile; }

        // Get the lifetime value set on the entity. See SetLifetime for more details.
        inline float GetLifetime() const noexcept
        { return mLifetime; }

        // Get the class bitflag object.
        inline const auto& GetFlags() const noexcept
        { return mFlags; }

        // Get the shared class objects that are shared between all
        // instances of this entity class type.

        inline auto  GetSharedEntityNodeClass(size_t index) const noexcept
        { return std::shared_ptr<const EntityNodeClass>(mNodes[index]); }
        inline auto GetSharedAnimationClass(size_t index) const noexcept
        { return std::shared_ptr<const AnimationClass>(mAnimations[index]); }
        inline auto GetSharedScriptVar(size_t index) const noexcept
        { return std::shared_ptr<const ScriptVar>(mScriptVars[index]); }
        inline auto GetSharedJoint(size_t index) const noexcept
        { return std::shared_ptr<const PhysicsJoint>(mJoints[index]); }
        inline auto GetSharedEntityControllerClass() const noexcept
        { return std::shared_ptr<const EntityStateControllerClass>(mStateController); }

        // Get the allocator object used to allocate entity node data for entity instances
        // of *this* class type. The allocator only exists when using the class runtime
        // services, i.e. after someone has called InitClassGameRuntime. Once the
        // allocator exists it can be used to iterate over all entity nodes (and some
        // of their attachments) in a more efficient manner.
        EntityNodeAllocator* GetAllocator() const;

        // Init the entity class runtime services. The engine call this once at the
        // start of the game runtime in order to initialize class specific services
        // used to serve all instances of the class. Things that we can do here
        // include initializing memory, memory allocators, pre-compute expensive
        // immutable values etc.
        void InitClassGameRuntime() const;

        // Get the current class hash value hashed over the current class data
        // and properties.
        std::size_t GetHash() const;

        // Serialize the entity class into JSON.
        void IntoJson(data::Writer& data) const;

        // Deserialize / load the entity class from JSON. Returns true if the
        // class was loaded successfully or false to indicate that not all data
        // was loaded as expected. The class might still be useful so one
        // should not discard the class since that will prohibit the user from
        // fixing the problems in the class.
        bool FromJson(const data::Reader& data);

        // Create a bitwise clone of the class with all the same properties and
        // data expect for the class ID which is generated anew.
        EntityClass Clone() const;

        EntityClass& operator=(const EntityClass& other);

        // Service *all* the entity class runtimes that have had their class specific
        // runtimes initialized by InitClassGameRuntime.
        static void UpdateRuntimes(double game_time, double dt);
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
        // entity state controller if any.
        std::shared_ptr<EntityStateControllerClass> mStateController;
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
        mutable bool mInitRuntime = false;
    };

} // namespace