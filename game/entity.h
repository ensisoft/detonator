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

#include "base/bitflag.h"
#include "base/utility.h"
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
    class EntityClass;

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
        using Flags = EntityFlags;
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
        void CoarseHitTest(const Float2& point, std::vector<EntityNode*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr);
        void CoarseHitTest(const Float2& point, std::vector<const EntityNode*>* hits,
                           std::vector<glm::vec2>* hitbox_positions = nullptr) const;

        // Map coordinates in node's OOB space into entity coordinate space. The origin of
        // the OOB space is relative to the "TopLeft" corner of the OOB of the node.
        Float2 MapCoordsFromNodeBox(const Float2& pos, const EntityNode* node) const;
        // Map coordinates in entity coordinate space into node's OOB coordinate space.
        Float2 MapCoordsToNodeBox(const Float2& pos, const EntityNode* node) const;

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

        using PhysicsJointClass  = RigidBodyJointClass;
        using PhysicsJointType   = RigidBodyJointClass::JointType;
        using PhysicsJointParams = RigidBodyJointClass::JointParams;
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

        const std::string& GetClassId() const noexcept;
        const std::string& GetClassName() const noexcept;
        const std::string& GetScriptFileId() const noexcept;

        const EntityClass& GetClass() const noexcept;
        const EntityClass* operator->() const noexcept;

        std::string GetDebugName() const;

        bool HasIdleTrack() const noexcept;

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
        const std::string& GetId() const noexcept
        { return mInstanceId; }

        const std::string& GetName() const noexcept
        { return mInstanceName; }
        const std::string& GetTag() const noexcept
        { return mInstanceTag; }

        std::size_t GetNumNodes() const noexcept
        { return mNodes.size(); }
        std::size_t GetNumJoints() const noexcept
        { return mJoints.size(); }
        auto GetRenderLayer() const noexcept
        { return mRenderLayer; }
        bool TestFlag(ControlFlags flag) const noexcept
        { return mControlFlags.test(flag); }
        bool TestFlag(Flags flag) const noexcept
        { return mFlags.test(flag); }
        bool IsVisible() const noexcept
        { return TestFlag(Flags::VisibleInGame); }

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
