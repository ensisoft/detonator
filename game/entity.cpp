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

#include "config.h"

#include "warnpush.h"
#  include <glm/glm.hpp> // for glm::inverse
#include "warnpop.h"

#include <atomic>
#include <algorithm>
#include <set>
#include <unordered_map>

#include "base/logging.h"
#include "base/assert.h"
#include "base/utility.h"
#include "base/hash.h"
#include "base/trace.h"
#include "data/reader.h"
#include "data/writer.h"
#include "game/treeop.h"
#include "game/entity.h"
#include "game/entity_class.h"
#include "game/transform.h"
#include "game/timeline_animator.h"
#include "game/timeline_animation_trigger.h"
#include "game/timeline_property_animator.h"
#include "game/entity_node_linear_mover.h"
#include "game/entity_node_spline_mover.h"
#include "game/entity_node_rigid_body.h"
#include "game/entity_node_drawable_item.h"
#include "game/entity_node_text_item.h"
#include "game/entity_node_spatial_node.h"
#include "game/entity_node_fixture.h"
#include "game/entity_node_tilemap_node.h"

namespace game
{

Entity::Entity(std::shared_ptr<const EntityClass> klass)
  : mClass(std::move(klass))
  , mInstanceId(FastId(10))
  , mInstanceTag(mClass->GetTag())
  , mLifetime(mClass->GetLifetime())
  , mFlags(mClass->GetFlags())
  , mIdleTrackId(mClass->GetIdleTrackId())
{
    std::unordered_map<const EntityNodeClass*, EntityNode*> map;

    // important! don't allocate just make sure that the vector
    // can hold our nodes without allocation, thus the pointers
    // taken to elements in the vector will remain valid.
    mNodes.reserve(mClass->GetNumNodes());

    auto* allocator = mClass->GetAllocator();

    // build render tree, first create instances of all node classes
    // then build the render tree based on the node instances
    TRACE_BLOCK("Entity::Entity::BuildRenderTree",
        for (size_t i = 0; i < mClass->GetNumNodes(); ++i)
        {
            auto node_klass = mClass->GetSharedEntityNodeClass(i);
            EntityNode node(node_klass, allocator);
            node.SetEntity(this);
            mNodes.push_back(std::move(node));
            map[node_klass.get()] = &mNodes.back();
        }
        // build render tree by mapping class entity node class objects
        // to entity node instance objects
        mRenderTree.FromTree(mClass->GetRenderTree(),
            [&map](const EntityNodeClass* node) {
                return map[node];
            });
    );

    // assign the script variables.
    for (size_t i=0; i<mClass->GetNumScriptVars(); ++i)
    {
        auto var = mClass->GetSharedScriptVar(i);
        if (!var->IsReadOnly())
            mScriptVars.push_back(*var);
    }

    // create local joints by mapping the entity class joints from
    // entity class nodes to entity nodes in this entity instance.

    // make sure the joints stay valid.
    mJoints.reserve(mClass->GetNumJoints());

    for (size_t i=0; i<mClass->GetNumJoints(); ++i)
    {
        const auto& joint_klass = mClass->GetSharedJoint(i);
        const auto* klass_src_node = mClass->FindNodeById(joint_klass->src_node_id);
        const auto* klass_dst_node = mClass->FindNodeById(joint_klass->dst_node_id);
        ASSERT(klass_src_node && klass_dst_node);
        auto* inst_src_node = map[klass_src_node];
        auto* inst_dst_node = map[klass_dst_node];
        ASSERT(inst_src_node && inst_dst_node);
        PhysicsJoint joint(joint_klass,
                           FastId(10),
                           inst_src_node, inst_dst_node);
        mJoints.push_back(std::move(joint));

        auto* joint_ptr = &mJoints.back();
        if (auto* rigid_body = inst_src_node->GetRigidBody())
            rigid_body->AddJointConnection(joint_ptr);

        if (auto* rigid_body = inst_dst_node->GetRigidBody())
            rigid_body->AddJointConnection(joint_ptr);
    }

    if (mClass->GetNumAnimators())
    {
        mAnimator = EntityStateController(mClass->GetSharedEntityControllerClass(0));
    }
}

Entity::Entity(const EntityArgs& args) : Entity(args.klass)
{
    mInstanceName = args.name;
    mRenderLayer  = args.render_layer;
    if (!args.id.empty())
        mInstanceId = args.id;

    for (auto& node : mNodes)
    {
        if (mRenderTree.GetParent(&node))
            continue;
        // if this is a top level node (i.e. under the root node)
        // then bake the entity transform into this node.
        const auto& rotation    = node->GetRotation();
        const auto& translation = node->GetTranslation();
        const auto& scale       = node->GetScale();
        node.SetRotation(rotation + args.rotation);
        node.SetTranslation(translation + args.position);
        node.SetScale(scale * args.scale);
    }

    for (auto& pair : args.vars)
    {
        const auto& name  = pair.first;
        const auto& value = pair.second;
        if (auto* var = FindScriptVarByName(name))
        {
            if (var->IsArray()) {
                WARN("Setting array script variables on entity create is currently unsupported. [entity='%1', var='%2']", mInstanceName, name);
                continue;
            }

            const bool read_only = var->IsReadOnly();
            // a bit of a kludge here but we know that the read only variables
            // are shared between each entity instance, i.e. FindScriptVar returns
            // a pointer to the script var in the class object.
            // So create a mutable copy since we're actually writing to the
            // variable here.
            if (var->IsReadOnly())
            {
                mScriptVars.push_back(*var);
                var = &mScriptVars.back();
            }

            // todo: provide a non-const find for script variable.
            const_cast<ScriptVar*>(var)->SetReadOnly(false);

            const auto expected_type = var->GetType();
            if (expected_type == ScriptVar::Type::String && std::holds_alternative<std::string>(value))
                var->SetValue(std::get<std::string>(value));
            else if (expected_type == ScriptVar::Type::Integer && std::holds_alternative<int>(value))
                var->SetValue(std::get<int>(value));
            else if (expected_type == ScriptVar::Type::Float && std::holds_alternative<float>(value))
                var->SetValue(std::get<float>(value));
            else if (expected_type == ScriptVar::Type::Vec2 && std::holds_alternative<glm::vec2>(value))
                var->SetValue(std::get<glm::vec2>(value));
            else if (expected_type == ScriptVar::Type::Vec3 && std::holds_alternative<glm::vec3>(value))
                var->SetValue(std::get<glm::vec3>(value));
            else if (expected_type == ScriptVar::Type::Vec4 && std::holds_alternative<glm::vec4>(value))
                var->SetValue(std::get<glm::vec4>(value));
            else if (expected_type == ScriptVar::Type::Boolean && std::holds_alternative<bool>(value))
                var->SetValue(std::get<bool>(value));
            else if (expected_type == ScriptVar::Type::Color && std::holds_alternative<Color4f>(value))
                var->SetValue(std::get<Color4f>(value));
            else WARN("Unsupported entity script var type on entity create. [entity='%1', var='%2']", mInstanceName, name);

            const_cast<ScriptVar*>(var)->SetReadOnly(read_only);
        }
        else WARN("No such entity script variable. [entity='%1', var='%2']", mInstanceName, name);
    }

    mControlFlags.set(ControlFlags::EnableLogging, args.enable_logging);
}

Entity::Entity(const EntityClass& klass)
  : Entity(std::make_shared<EntityClass>(klass))
{}

Entity::~Entity()
{
    if (auto* allocator = mClass->GetAllocator())
    {
        for (auto& node: mNodes)
        {
            node.Release(allocator);
        }
    }
}

const std::string& Entity::GetClassId() const noexcept
{
    return mClass->GetId();
}

const std::string& Entity::GetClassName() const noexcept
{
    return mClass->GetName();
}

 const std::string& Entity::GetScriptFileId() const noexcept
 {
    return mClass->GetScriptFileId();
 }

std::string Entity::GetDebugName() const
{
    return mClass->GetName() + "/" + mInstanceName;
}
bool Entity::HasIdleTrack() const noexcept
{
    return !mIdleTrackId.empty() || mClass->HasIdleTrack();
}

const EntityClass& Entity::GetClass() const noexcept
{
    return *mClass;
}

const EntityClass* Entity::operator->() const noexcept
{
    return mClass.get();
}

EntityNode& Entity::GetNode(size_t index)
{
    ASSERT(index < mNodes.size());
    return mNodes[index];
}
EntityNode* Entity::FindNodeByClassName(const std::string& name)
{
    for (auto& node : mNodes)
        if (node.GetClassName() == name)
            return &node;
    return nullptr;
}
EntityNode* Entity::FindNodeByClassId(const std::string& id)
{
    for (auto& node : mNodes)
        if (node.GetClassId() == id)
            return &node;
    return nullptr;
}
EntityNode* Entity::FindNodeByInstanceId(const std::string& id)
{
    for (auto& node : mNodes)
        if (node.GetId() == id)
            return &node;
    return nullptr;
}
EntityNode* Entity::FindNodeByInstanceName(const std::string& name)
{
    for (auto& node : mNodes)
        if (node.GetName() == name)
            return &node;
    return nullptr;
}

const EntityNode& Entity::GetNode(size_t index) const
{
    ASSERT(index < mNodes.size());
    return mNodes[index];
}
const EntityNode* Entity::FindNodeByClassName(const std::string& name) const
{
    for (auto& node : mNodes)
        if (node.GetClassName() == name)
            return &node;
    return nullptr;
}
const EntityNode* Entity::FindNodeByClassId(const std::string& id) const
{
    for (auto& node : mNodes)
        if (node.GetClassId() == id)
            return &node;
    return nullptr;
}
const EntityNode* Entity::FindNodeByInstanceId(const std::string& id) const
{
    for (auto& node : mNodes)
        if (node.GetId() == id)
            return &node;
    return nullptr;
}
const EntityNode* Entity::FindNodeByInstanceName(const std::string& name) const
{
    for (const auto& node : mNodes)
        if (node.GetName() == name)
            return &node;
    return nullptr;
}

void Entity::CoarseHitTest(const Float2& pos, std::vector<EntityNode*>* hits, std::vector<glm::vec2>* hitbox_positions)
{
    game::CoarseHitTest(mRenderTree, pos.x, pos.y, hits, hitbox_positions);
}

void Entity::CoarseHitTest(const Float2& pos, std::vector<const EntityNode*>* hits, std::vector<glm::vec2>* hitbox_positions) const
{
    game::CoarseHitTest(mRenderTree, pos.x, pos.y, hits, hitbox_positions);
}

Float2 Entity::MapCoordsFromNodeBox(const Float2& pos, const EntityNode* node) const
{
    return game::MapCoordsFromNodeBox(mRenderTree, pos.x, pos.y, node);
}

Float2 Entity::MapCoordsToNodeBox(const Float2& pos, const EntityNode* node) const
{
    return game::MapCoordsToNodeBox(mRenderTree, pos.x, pos.y, node);
}

glm::mat4 Entity::FindNodeTransform(const EntityNode* node) const
{
    return game::FindNodeTransform(mRenderTree, node);
}
glm::mat4 Entity::FindNodeModelTransform(const EntityNode* node) const
{
    return game::FindNodeModelTransform(mRenderTree, node);
}

glm::mat4 Entity::FindRelativeTransform(const EntityNode* parent, const EntityNode* child) const
{
    const auto& parent_to_world = game::FindNodeTransform(mRenderTree, parent);
    const auto& child_to_world  = game::FindNodeTransform(mRenderTree, child);
    const auto& world_to_parent = glm::inverse(parent_to_world);
    return world_to_parent * child_to_world;
}

FRect Entity::FindNodeBoundingRect(const EntityNode* node) const
{
    return game::FindBoundingRect(mRenderTree, node);
}

FRect Entity::GetBoundingRect() const
{
    return game::FindBoundingRect(mRenderTree);
}

FBox Entity::FindNodeBoundingBox(const EntityNode* node) const
{
    return game::FindBoundingBox(mRenderTree, node);
}

void Entity::Die()
{
    SetFlag(ControlFlags::WantsToDie, true);
}
void Entity::DieIn(float seconds)
{
    mScheduledDeath = seconds;
}

void Entity::Update(float dt, std::vector<Event>* events)
{
    mCurrentTime += dt;

    if (mScheduledDeath.has_value())
    {
        float& val = mScheduledDeath.value();
        val -= dt;
        if (val <= 0.0f)
            SetFlag(ControlFlags::WantsToDie, true);
    }

    mFinishedAnimations.clear();

    for (auto it = mTimers.begin(); it != mTimers.end();)
    {
        auto& timer = *it;
        timer.when -= dt;
        if (timer.when < 0.0f)
        {
            if (events)
            {
                TimerEvent event;
                event.name   = timer.name;
                event.jitter = timer.when;
                events->push_back(std::move(event));
            }
            it = mTimers.erase(it);
        } else ++it;
    }

    if (events)
    {
        for (auto& event: mEvents)
        {
            events->push_back(std::move(event));
        }
    }
    mEvents.clear();

    // this path only exists here for the development/design time when
    // we don't have an actual entity class runtime service enabled.
    // in this case we'll do the mover based transformations here.
    if (!mClass->HaveRuntime())
    {
        for (auto& node: mNodes)
        {
            if (auto* mover = node.GetLinearMover())
            {
                mover->TransformObject(static_cast<float>(dt), node);
            }
            if (auto* mover = node.GetSplineMover())
            {
                mover->TransformObject(static_cast<float>(dt), node);
            }
        }
    }

    if (mCurrentAnimations.empty())
    {
        if (mAnimationQueue.empty())
            return;

        auto animation = std::move(mAnimationQueue.front());
        VERBOSE("Starting next queued entity animation. [entity='%1/%2', animation='%3']",
                mClass->GetName(), mInstanceName, animation->GetClassName());
        mCurrentAnimations.push_back(std::move(animation));
        mAnimationQueue.pop();
        return;
    }

    // Update the animation state.
    for (auto& animation : mCurrentAnimations)
    {
        animation->Update(dt);
    }

    // Apply animation state transforms/actions on the entity nodes.
    for (auto& node : mNodes)
    {
        for (auto& animation : mCurrentAnimations)
        {
            animation->Apply(node);
        }
    }

    for (auto& animation : mCurrentAnimations)
    {
        if (!animation->IsComplete())
            continue;

        if (animation->IsLooping())
        {
            animation->Restart();
            for (auto& node: mNodes)
            {
                if (!mRenderTree.GetParent(&node))
                    continue;

                const auto& klass = node.GetClass();
                const auto& rotation = klass.GetRotation();
                const auto& translation = klass.GetTranslation();
                const auto& scale = klass.GetScale();
                // if the node is a child node (i.e. has a parent) then reset the
                // node's transformation based on the node's transformation relative
                // to its parent in the entity class.
                node.SetTranslation(translation);
                node.SetRotation(rotation);
                node.SetScale(scale);
            }
            continue;
        }

        VERBOSE("Entity animation is complete. [entity='%1/%2', animation='%3']",
              mClass->GetName(), mInstanceName, animation->GetClassName());
        mFinishedAnimations.push_back(std::move(animation));
        animation.reset();
    }
    base::EraseRemove(mCurrentAnimations, [](const auto& animation) {
        return !animation;
    });
}

void Entity::UpdateStateController(float dt, std::vector<EntityStateUpdate>* updates)
{
    if (auto* animator = base::GetOpt(mAnimator))
    {
        animator->Update(dt, updates);
    }
}
bool Entity::TransitionStateController(const EntityStateTransition* transition, const EntityState* next)
{
    if (auto* animator = base::GetOpt(mAnimator))
    {
        return animator->BeginStateTransition(transition, next);
    }
    return false;
}

const EntityState* Entity::GetCurrentEntityState() noexcept
{
    if (auto* animator = base::GetOpt(mAnimator))
    {
        return animator->GetCurrentState();
    }
    return nullptr;
}

const EntityStateTransition* Entity::GetCurrentEntityStateTransition() noexcept
{
    if (auto* animator = base::GetOpt(mAnimator))
    {
        return animator->GetTransition();
    }
    return nullptr;
}

Animation* Entity::PlayAnimation(std::unique_ptr<Animation> animation)
{
    // todo: what to do if there's a previous track ?
    // possibilities: reset or queue?
    mCurrentAnimations.push_back(std::move(animation));
    return mCurrentAnimations.back().get();
}
Animation* Entity::PlayAnimation(const Animation& animation)
{
    return PlayAnimation(std::make_unique<Animation>(animation));
}

Animation* Entity::PlayAnimation(Animation&& animation)
{
    return PlayAnimation(std::make_unique<Animation>(std::move(animation)));
}

Animation* Entity::PlayAnimationByName(const std::string& name)
{
    for (size_t i=0; i< mClass->GetNumAnimations(); ++i)
    {
        const auto& klass = mClass->GetSharedAnimationClass(i);
        if (klass->GetName() != name)
            continue;
        auto track = std::make_unique<Animation>(klass);
        return PlayAnimation(std::move(track));
    }
    WARN("No such entity animation found. [entity='%1/%2', animation='%3']",
         mClass->GetClassName(), mInstanceName, name);
    return nullptr;
}
Animation* Entity::PlayAnimationById(const std::string& id)
{
    for (size_t i=0; i< mClass->GetNumAnimations(); ++i)
    {
        const auto& klass = mClass->GetSharedAnimationClass(i);
        if (klass->GetId() != id)
            continue;
        auto track = std::make_unique<Animation>(klass);
        return PlayAnimation(std::move(track));
    }
    WARN("No such entity animation found. [entity='%1/%2', animation='%3']",
         mClass->GetClassName(), mInstanceName, id);
    return nullptr;
}

Animation* Entity::QueueAnimation(std::unique_ptr<Animation> animation)
{
    mAnimationQueue.push(std::move(animation));
    auto* ret = mAnimationQueue.back().get();
    VERBOSE("Queued new entity animation. [entity='%1/%2', animation='%3']",
            mClass->GetClassName(), mInstanceName, ret->GetClassName());
    return ret;
}

Animation* Entity::QueueAnimationByName(const std::string& name)
{
    for (size_t i=0; i< mClass->GetNumAnimations(); ++i)
    {
        const auto& klass = mClass->GetSharedAnimationClass(i);
        if (klass->GetName() != name)
            continue;
        auto track = std::make_unique<Animation>(klass);
        return QueueAnimation(std::move(track));
    }
    WARN("No such entity animation found. [entity='%1/%2', animation='%3']",
         mClass->GetClassName(), mInstanceName, name);
    return nullptr;
}

Animation* Entity::PlayIdle()
{
    if (!mCurrentAnimations.empty())
        return nullptr;

    if (!mIdleTrackId.empty())
        return PlayAnimationById(mIdleTrackId);
    else if(mClass->HasIdleTrack())
        return PlayAnimationById(mClass->GetIdleTrackId());

    return nullptr;
}

bool Entity::IsDying() const
{
    return mControlFlags.test(ControlFlags::WantsToDie);
}

bool Entity::IsAnimating() const
{
    return !mCurrentAnimations.empty();
}

bool Entity::HasPendingAnimations() const
{
    return !mAnimationQueue.empty();
}

bool Entity::HasExpired() const
{
    if (!mFlags.test(EntityClass::Flags::LimitLifetime))
        return false;
    else if (mCurrentTime >= mLifetime)
        return true;
    return false;
}

bool Entity::HasBeenKilled() const
{ return TestFlag(ControlFlags::Killed); }

bool Entity::HasBeenSpawned() const
{ return TestFlag(ControlFlags::Spawned); }

bool Entity::HasRigidBodies() const
{
    for (auto& node : mNodes)
        if (node->HasRigidBody())
            return true;
    return false;
}

bool Entity::HasSpatialNodes() const
{
    for (auto& node : mNodes)
        if (node->HasSpatialNode())
            return true;
    return false;
}

bool Entity::KillAtBoundary() const
{ return TestFlag(Flags::KillAtBoundary); }


bool Entity::HasRenderableItems() const
{
    for (auto& node : mNodes)
        if (node->HasDrawable() || node->HasTextItem())
            return true;
    return false;
}

bool Entity::HasLights() const
{
    for (auto& node : mNodes)
    {
        if (node->HasBasicLight())
            return true;
    }
    return false;
}

Entity::PhysicsJoint* Entity::FindJointById(const std::string& id)
{
    for (auto& joint : mJoints)
    {
        if (joint.GetId() == id)
            return &joint;
    }
    return nullptr;
}

Entity::PhysicsJoint* Entity::FindJointByClassId(const std::string& id)
{
    for (auto& joint : mJoints)
    {
        if (joint.GetClassId() == id)
            return &joint;
    }
    return nullptr;
}

const Entity::PhysicsJoint& Entity::GetJoint(std::size_t index) const
{
    return base::SafeIndex(mJoints, index);
}

const Entity::PhysicsJoint* Entity::FindJointById(const std::string& id) const
{
    for (const auto& joint : mJoints)
    {
        if (joint.GetId() == id)
            return &joint;
    }
    return nullptr;
}

const Entity::PhysicsJoint* Entity::FindJointByNodeId(const std::string& id) const
{
    for (const auto& joint : mJoints)
    {
        if (joint.GetSrcId() == id ||
            joint.GetDstId() == id)
            return &joint;
    }
    return nullptr;
}

const Entity::PhysicsJoint* Entity::FindJointByClassId(const std::string& id) const
{
    for (const auto& joint : mJoints)
    {
        if (joint.GetClassId() == id)
            return &joint;
    }
    return nullptr;
}


const ScriptVar* Entity::FindScriptVarByName(const std::string& name) const
{
    // first check the mutable variables per this instance then check the class.
    for (const auto& var : mScriptVars)
    {
        if (var.GetName() == name)
            return &var;
    }
    return mClass->FindScriptVarByName(name);
}
const ScriptVar* Entity::FindScriptVarById(const std::string& id) const
{
    // first check the mutable variables per this instance then check the class.
    for (const auto& var : mScriptVars)
    {
        if (var.GetId() == id)
            return &var;
    }
    return mClass->FindScriptVarById(id);
}

std::unique_ptr<Entity> CreateEntityInstance(std::shared_ptr<const EntityClass> klass)
{ return std::make_unique<Entity>(std::move(klass)); }

std::unique_ptr<Entity> CreateEntityInstance(const EntityClass& klass)
{ return CreateEntityInstance(std::make_shared<const EntityClass>(klass)); }

std::unique_ptr<Entity> CreateEntityInstance(const EntityArgs& args)
{ return std::make_unique<Entity>(args); }

std::unique_ptr<EntityNode> CreateEntityNodeInstance(std::shared_ptr<const EntityNodeClass> klass)
{ return std::make_unique<EntityNode>(klass); }

} // namespace

