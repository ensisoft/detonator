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

#include "config.h"

#include "base/utility.h"
#include "base/hash.h"
#include "base/logging.h"
#include "data/reader.h"
#include "data/writer.h"
#include "game/util.h"
#include "game/treeop.h"
#include "game/timeline_animation.h"
#include "game/timeline_property_animator.h"
#include "game/scriptvar.h"
#include "game/entity_class.h"
#include "game/entity_state_controller.h"
#include "game/entity_node.h"
#include "game/entity_node_fixture.h"
#include "game/entity_node_linear_mover.h"
#include "game/entity_node_spline_mover.h"

namespace {
    struct ClassRuntime {
        bool needs_update = false;
        game::EntityNodeAllocator allocator;
    };
    std::unordered_map<std::string, std::unique_ptr<ClassRuntime>> runtimes;
}

namespace game
{

EntityClass::EntityClass(std::string id)
  : mClassId(std::move(id))
{
    mFlags.set(Flags::VisibleInEditor, true);
    mFlags.set(Flags::VisibleInGame,   true);
    mFlags.set(Flags::LimitLifetime,   false);
    mFlags.set(Flags::KillAtLifetime,  true);
    mFlags.set(Flags::KillAtBoundary,  true);
    mFlags.set(Flags::TickEntity,      true);
    mFlags.set(Flags::UpdateEntity,    true);
    mFlags.set(Flags::UpdateNodes,     false);
    mFlags.set(Flags::PostUpdate,      true);
    mFlags.set(Flags::WantsKeyEvents,  false);
    mFlags.set(Flags::WantsMouseEvents,false);
}
EntityClass::EntityClass() : EntityClass(base::RandomString(10))
{}

EntityClass::EntityClass(const EntityClass& other)
{
    mClassId     = other.mClassId;
    mName        = other.mName;
    mTag         = other.mTag;
    mScriptFile  = other.mScriptFile;
    mIdleTrackId = other.mIdleTrackId;
    mFlags       = other.mFlags;
    mLifetime    = other.mLifetime;

    std::unordered_map<const EntityNodeClass*, const EntityNodeClass*> map;

    // make a deep copy of the nodes.
    for (const auto& node : other.mNodes)
    {
        auto copy = std::make_unique<EntityNodeClass>(*node);
        map[node.get()] = copy.get();
        mNodes.push_back(std::move(copy));
    }

    // make a deep copy of the animation tracks
    for (const auto& track : other.mAnimations)
    {
        mAnimations.push_back(std::make_unique<AnimationClass>(*track));
    }

    // make a deep copy of the script variables
    for (const auto& var : other.mScriptVars)
    {
        mScriptVars.push_back(std::make_shared<ScriptVar>(*var));
    }

    // make a deep copy of the joints
    for (const auto& joint : other.mJoints)
    {
        mJoints.push_back(std::make_shared<PhysicsJoint>(*joint));
    }

    // Deepy copy animators
    if (other.mStateController)
    {
        mStateController = std::make_shared<EntityStateControllerClass>(*other.mStateController);
    }

    mRenderTree.FromTree(other.GetRenderTree(), [&map](const EntityNodeClass* node) {
        return map[node];
    });
}

EntityClass::~EntityClass()
{
    if (mInitRuntime)
    {
        runtimes.erase(mClassId);
    }
}

EntityNodeClass* EntityClass::AddNode(const EntityNodeClass& node)
{
    mNodes.emplace_back(new EntityNodeClass(node));
    return mNodes.back().get();
}
EntityNodeClass* EntityClass::AddNode(EntityNodeClass&& node)
{
    mNodes.emplace_back(new EntityNodeClass(std::move(node)));
    return mNodes.back().get();
}
EntityNodeClass* EntityClass::AddNode(std::unique_ptr<EntityNodeClass> node)
{
    mNodes.push_back(std::move(node));
    return mNodes.back().get();
}

void EntityClass::MoveNode(size_t src_index, size_t dst_index)
{
    ASSERT(src_index < mNodes.size());
    ASSERT(dst_index < mNodes.size());
    std::swap(mNodes[src_index], mNodes[dst_index]);
}

EntityClass::PhysicsJoint* EntityClass::AddJoint(const PhysicsJoint& joint)
{
    mJoints.push_back(std::make_shared<PhysicsJoint>(joint));
    return mJoints.back().get();
}

EntityClass::PhysicsJoint* EntityClass::AddJoint(PhysicsJoint&& joint)
{
    mJoints.push_back(std::make_shared<PhysicsJoint>(std::move(joint)));
    return mJoints.back().get();
}

void EntityClass::SetJoint(size_t index, const PhysicsJoint& joint)
{
    *base::SafeIndex(mJoints, index) = joint;
}
void EntityClass::SetJoint(size_t index, PhysicsJoint&& joint)
{
    *base::SafeIndex(mJoints, index) = std::move(joint);
}

EntityClass::PhysicsJoint& EntityClass::GetJoint(size_t index)
{
    return *base::SafeIndex(mJoints, index);
}
EntityClass::PhysicsJoint* EntityClass::FindJointById(const std::string& id)
{
    for (auto& joint : mJoints)
        if (joint->id == id) return joint.get();

    return nullptr;
}
EntityClass::PhysicsJoint* EntityClass::FindJointByNodeId(const std::string& id)
{
    for (auto& joint : mJoints)
    {
        if (joint->src_node_id == id ||
            joint->dst_node_id == id)
            return joint.get();
    }
    return nullptr;
}

const EntityClass::PhysicsJoint& EntityClass::GetJoint(size_t index) const
{
    return *base::SafeIndex(mJoints, index);
}
const EntityClass::PhysicsJoint* EntityClass::FindJointById(const std::string& id) const
{
    for (auto& joint : mJoints)
        if (joint->id == id) return joint.get();

    return nullptr;
}
const EntityClass::PhysicsJoint* EntityClass::FindJointByNodeId(const std::string& id) const
{
    for (auto& joint : mJoints)
    {
        if (joint->src_node_id == id ||
            joint->dst_node_id == id)
            return joint.get();
    }
    return nullptr;
}

void EntityClass::DeleteJointById(const std::string& id)
{
    for (auto it = mJoints.begin(); it != mJoints.end(); ++it)
    {
        if ((*it)->id == id)
        {
            mJoints.erase(it);
            return;
        }
    }
}

void EntityClass::DeleteJoint(std::size_t index)
{
    base::SafeErase(mJoints, index);
}

void EntityClass::DeleteInvalidJoints()
{
    mJoints.erase(std::remove_if(mJoints.begin(), mJoints.end(),
        [this](const auto& joint) {
            const auto* dst_node = FindNodeById(joint->dst_node_id);
            const auto* src_node = FindNodeById(joint->src_node_id);
            if (!dst_node || !src_node || (dst_node == src_node) ||
                !dst_node->HasRigidBody() ||
                !src_node->HasRigidBody())
                return true;
            return false;
    }), mJoints.end());
}

void EntityClass::FindInvalidJoints(std::vector<PhysicsJoint*>* invalid)
{
    // a joint is considered invalid when
    // - the src and dst nodes are the same.
    // - either dst or src node doesn't exist
    // - either dst or src node doesn't have rigid body
    for (auto& joint : mJoints)
    {
        const auto* dst_node = FindNodeById(joint->dst_node_id);
        const auto* src_node = FindNodeById(joint->src_node_id);
        if (!src_node || !dst_node || (dst_node == src_node) ||
            !dst_node->HasRigidBody() ||
            !src_node->HasRigidBody())
        {
            invalid->push_back(joint.get());
        }
    }
}

void EntityClass::DeleteInvalidFixtures()
{
    for (auto& node : mNodes)
    {
        if (auto* fixture = node->GetFixture())
        {
            if (!FindNodeById(fixture->GetRigidBodyNodeId()))
                node->RemoveFixture();
        }
    }
}

EntityNodeClass& EntityClass::GetNode(size_t index)
{
    ASSERT(index < mNodes.size());
    return *mNodes[index].get();
}
EntityNodeClass* EntityClass::FindNodeByName(const std::string& name)
{
    for (const auto& node : mNodes)
        if (node->GetName() == name)
            return node.get();
    return nullptr;
}
EntityNodeClass* EntityClass::FindNodeById(const std::string& id)
{
    for (const auto& node : mNodes)
        if (node->GetId() == id)
            return node.get();
    return nullptr;
}

EntityNodeClass* EntityClass::FindNodeParent(const EntityNodeClass* node)
{
    return mRenderTree.GetParent(node);
}

const EntityNodeClass& EntityClass::GetNode(size_t index) const
{
    ASSERT(index < mNodes.size());
    return *mNodes[index].get();
}
const EntityNodeClass* EntityClass::FindNodeByName(const std::string& name) const
{
    for (const auto& node : mNodes)
        if (node->GetName() == name)
            return node.get();
    return nullptr;
}
const EntityNodeClass* EntityClass::FindNodeById(const std::string& id) const
{
    for (const auto& node : mNodes)
        if (node->GetId() == id)
            return node.get();
    return nullptr;
}

const EntityNodeClass* EntityClass::FindNodeParent(const EntityNodeClass* node) const
{
    return mRenderTree.GetParent(node);
}

AnimationClass* EntityClass::AddAnimation(AnimationClass&& track)
{
    mAnimations.push_back(std::make_shared<AnimationClass>(std::move(track)));
    return mAnimations.back().get();
}
AnimationClass* EntityClass::AddAnimation(const AnimationClass& track)
{
    mAnimations.push_back(std::make_shared<AnimationClass>(track));
    return mAnimations.back().get();
}
AnimationClass* EntityClass::AddAnimation(std::unique_ptr<AnimationClass> track)
{
    mAnimations.push_back(std::move(track));
    return mAnimations.back().get();
}
void EntityClass::DeleteAnimation(size_t index)
{
    base::SafeErase(mAnimations, index);
}
bool EntityClass::DeleteAnimationByName(const std::string& name)
{
    return EraseByName(mAnimations, name);
}
bool EntityClass::DeleteAnimationById(const std::string& id)
{
    return EraseById(mAnimations, id);
}
AnimationClass& EntityClass::GetAnimation(size_t i)
{
    ASSERT(i < mAnimations.size());
    return *mAnimations[i].get();
}
AnimationClass* EntityClass::FindAnimationByName(const std::string& name)
{
    return FindByName(mAnimations, name);
}
const AnimationClass& EntityClass::GetAnimation(size_t i) const
{
    ASSERT(i < mAnimations.size());
    return *mAnimations[i].get();
}
const AnimationClass* EntityClass::FindAnimationByName(const std::string& name) const
{
    return FindByName(mAnimations, name);
}

EntityStateControllerClass* EntityClass::SetStateController(EntityStateControllerClass&& animator)
{
    mStateController = std::make_shared<EntityStateControllerClass>(std::move(animator));
    return mStateController.get();
}
EntityStateControllerClass* EntityClass::SetStateController(const EntityStateControllerClass& animator)
{
    mStateController = std::make_shared<EntityStateControllerClass>(animator);
    return mStateController.get();
}
EntityStateControllerClass* EntityClass::SetStateController(const std::shared_ptr<EntityStateControllerClass>& animator)
{
    mStateController = animator;
    return mStateController.get();
}

void EntityClass::DeleteStateController()
{
    mStateController.reset();
}

EntityStateControllerClass* EntityClass::GetStateController()
{
    return mStateController.get();
}

const EntityStateControllerClass* EntityClass::GetStateController() const
{
    return mStateController.get();
}

void EntityClass::LinkChild(EntityNodeClass* parent, EntityNodeClass* child)
{
    game::LinkChild(mRenderTree, parent, child);
}

void EntityClass::BreakChild(EntityNodeClass* child, bool keep_world_transform)
{
    game::BreakChild(mRenderTree, child, keep_world_transform);
}

void EntityClass::ReparentChild(EntityNodeClass* parent, EntityNodeClass* child, bool keep_world_transform)
{
    game::ReparentChild(mRenderTree, parent, child, keep_world_transform);
}

void EntityClass::DeleteNode(EntityNodeClass* node)
{
    // Erase joints that refer to this node in order to maintain
    // the invariant that the joints are always valid.
    mJoints.erase(std::remove_if(mJoints.begin(), mJoints.end(),
                                 [node](const auto& joint) {
        return joint->src_node_id == node->GetId() ||
               joint->dst_node_id == node->GetId();
    }), mJoints.end());
    game::DeleteNode(mRenderTree, node, mNodes);
}

EntityNodeClass* EntityClass::DuplicateNode(const EntityNodeClass* node)
{
    std::vector<std::unique_ptr<EntityNodeClass>> clones;

    auto* ret = game::DuplicateNode(mRenderTree, node, &clones);
    for (auto& clone : clones)
        mNodes.push_back(std::move(clone));
    return ret;

}

void EntityClass::CoarseHitTest(const Float2& point, std::vector<EntityNodeClass*>* hits, std::vector<glm::vec2>* hitbox_positions)
{
    game::CoarseHitTest(mRenderTree, point.x, point.y, hits, hitbox_positions);
}

void EntityClass::CoarseHitTest(const Float2& point, std::vector<const EntityNodeClass*>* hits, std::vector<glm::vec2>* hitbox_positions) const
{
    game::CoarseHitTest(mRenderTree, point.x, point.y, hits, hitbox_positions);
}

Float2 EntityClass::MapCoordsFromNodeBox(const Float2& coordinates, const EntityNodeClass* node) const
{
    return game::MapCoordsFromNodeBox(mRenderTree, coordinates.x, coordinates.y, node);
}

Float2 EntityClass::MapCoordsFromNode(const Float2& coordinates, const EntityNodeClass* node) const
{
    return game::MapCoordsFromNode(mRenderTree, coordinates.x, coordinates.y, node);
}

Float2 EntityClass::MapCoordsToNodeBox(const Float2& coordinates, const EntityNodeClass* node) const
{
    return game::MapCoordsToNodeBox(mRenderTree, coordinates.x, coordinates.y, node);
}

Float2 EntityClass::MapCoordsToNode(const Float2& coordinates, const EntityNodeClass* node) const
{
    return game::MapCoordsToNode(mRenderTree, coordinates.x, coordinates.y, node);
}


FRect EntityClass::FindNodeBoundingRect(const EntityNodeClass* node) const
{
    return game::FindBoundingRect(mRenderTree, node);
}
FRect EntityClass::GetBoundingRect() const
{
    return game::FindBoundingRect(mRenderTree);
}

FBox EntityClass::FindNodeBoundingBox(const EntityNodeClass* node) const
{
    return game::FindBoundingBox(mRenderTree, node);
}

size_t EntityClass::FindNodeIndex(const EntityNodeClass* node) const
{
    for (size_t i=0; i<mNodes.size(); ++i)
    {
        if (mNodes[i].get() == node)
            return i;
    }
    return mNodes.size();
}

glm::mat4 EntityClass::FindNodeTransform(const EntityNodeClass* node) const
{
    return game::FindNodeTransform(mRenderTree, node);
}

glm::mat4 EntityClass::FindNodeModelTransform(const EntityNodeClass* node) const
{
    return game::FindNodeModelTransform(mRenderTree, node);
}

void EntityClass::AddScriptVar(const ScriptVar& var)
{
    mScriptVars.push_back(std::make_shared<ScriptVar>(var));
}
void EntityClass::AddScriptVar(ScriptVar&& var)
{
    mScriptVars.push_back(std::make_shared<ScriptVar>(std::move(var)));
}
void EntityClass::DeleteScriptVar(size_t index)
{
    ASSERT(index <mScriptVars.size());
    auto it = mScriptVars.begin();
    mScriptVars.erase(it + index);
}
void EntityClass::SetScriptVar(size_t index, const ScriptVar& var)
{
    ASSERT(index <mScriptVars.size());
    *mScriptVars[index] = var;
}
void EntityClass::SetScriptVar(size_t index, ScriptVar&& var)
{
    ASSERT(index <mScriptVars.size());
    *mScriptVars[index] = std::move(var);
}
ScriptVar& EntityClass::GetScriptVar(size_t index)
{
    ASSERT(index <mScriptVars.size());
    return *mScriptVars[index];
}
ScriptVar* EntityClass::FindScriptVarByName(const std::string& name)
{
    for (auto& var : mScriptVars)
        if (var->GetName() == name)
            return var.get();
    return nullptr;
}
ScriptVar* EntityClass::FindScriptVarById(const std::string& id)
{
    for (auto& var : mScriptVars)
        if (var->GetId() == id)
            return var.get();
    return nullptr;
}

const ScriptVar& EntityClass::GetScriptVar(size_t index) const
{
    ASSERT(index <mScriptVars.size());
    return *mScriptVars[index];
}
const ScriptVar* EntityClass::FindScriptVarByName(const std::string& name) const
{
    for (auto& var : mScriptVars)
        if (var->GetName() == name)
            return var.get();
    return nullptr;
}
const ScriptVar* EntityClass::FindScriptVarById(const std::string& id) const
{
    for  (auto& var : mScriptVars)
        if (var->GetId() == id)
            return var.get();
    return nullptr;
}

std::size_t EntityClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mTag);
    hash = base::hash_combine(hash, mIdleTrackId);
    hash = base::hash_combine(hash, mScriptFile);
    hash = base::hash_combine(hash, mFlags);
    hash = base::hash_combine(hash, mLifetime);
    // include the node hashes in the animation hash
    // this covers both the node values and their traversal order
    mRenderTree.PreOrderTraverseForEach([&](const EntityNodeClass* node) {
        if (node == nullptr)
            return;
        hash = base::hash_combine(hash, node->GetHash());
    });

    for (const auto& track : mAnimations)
        hash = base::hash_combine(hash, track->GetHash());

    for (const auto& var : mScriptVars)
        hash = base::hash_combine(hash, var->GetHash());

    for (const auto& joint : mJoints)
    {
        hash = base::hash_combine(hash, joint->GetHash());
    }

    if (mStateController)
    {
        hash = base::hash_combine(hash, mStateController->GetHash());
    }

    return hash;
}

EntityNodeAllocator* EntityClass::GetAllocator() const
{
    if (auto* runtime = base::SafeFind(runtimes, mClassId))
        return &runtime->allocator;

    return nullptr;
}

void EntityClass::InitClassGameRuntime() const
{
    auto rt = std::make_unique<ClassRuntime>();

    bool ok = true;

    for (const auto& node : mNodes)
    {
        if (node->HasLinearMover() || node->HasSplineMover())
            rt->needs_update = true;

        if (auto* mover = node->GetSplineMover())
        {
            ok &= mover->InitClassRuntime();
        }

        for (const auto& animation : mAnimations)
        {
            for (size_t i=0; i<animation->GetNumAnimators(); ++i)
            {
                const auto& animator = animation->GetAnimatorClass(i);
                ok &= animator.InitClassRuntime();
            }
        }
    }

    if (!ok)
    {
        WARN("Entity class runtime failed to initialize completely. [name='%1']", mName);
    }

    runtimes.insert( { mClassId, std::move(rt) });
    mInitRuntime = true;
    DEBUG("Initialized class runtime. [class='%1']", mName);
}

void EntityClass::IntoJson(data::Writer& data) const
{
    data.Write("id",          mClassId);
    data.Write("name",        mName);
    data.Write("tag",         mTag);
    data.Write("idle_track",  mIdleTrackId);
    data.Write("script_file", mScriptFile);
    data.Write("flags",       mFlags);
    data.Write("lifetime",    mLifetime);

    for (const auto& node : mNodes)
    {
        auto chunk = data.NewWriteChunk();
        node->IntoJson(*chunk);
        data.AppendChunk("nodes", std::move(chunk));
    }

    for (const auto& track : mAnimations)
    {
        auto chunk = data.NewWriteChunk();
        track->IntoJson(*chunk);
        data.AppendChunk("tracks", std::move(chunk));
    }

    for (const auto& var : mScriptVars)
    {
        auto chunk = data.NewWriteChunk();
        var->IntoJson(*chunk);
        data.AppendChunk("vars", std::move(chunk));
    }

    for (const auto& joint : mJoints)
    {
        auto chunk = data.NewWriteChunk();
        joint->IntoJson(*chunk);
        data.AppendChunk("joints", std::move(chunk));
    }

    if (mStateController)
    {
        auto chunk = data.NewWriteChunk();
        mStateController->IntoJson(*chunk);
        data.Write("state-controller", std::move(chunk));
    }

    auto chunk = data.NewWriteChunk();
    RenderTreeIntoJson(mRenderTree, game::TreeNodeToJson<EntityNodeClass>, *chunk);
    data.Write("render_tree", std::move(chunk));
}


bool EntityClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",          &mClassId);
    ok &= data.Read("name",        &mName);
    ok &= data.Read("tag",         &mTag);
    ok &= data.Read("idle_track",  &mIdleTrackId);
    ok &= data.Read("script_file", &mScriptFile);
    ok &= data.Read("flags",       &mFlags);
    ok &= data.Read("lifetime",    &mLifetime);

    for (unsigned i=0; i<data.GetNumChunks("nodes"); ++i)
    {
        const auto& chunk = data.GetReadChunk("nodes", i);
        auto klass = std::make_shared<EntityNodeClass>();
        mNodes.push_back(klass);
        if (!klass->FromJson(*chunk)) {
            WARN("Failed to load entity class node completely. [entity=%1', node='%1']", mName, klass->GetName());
            ok = false;
        }
    }
    for (unsigned i=0; i<data.GetNumChunks("tracks"); ++i)
    {
        const auto& chunk = data.GetReadChunk("tracks", i);
        auto klass = std::make_shared<AnimationClass>();
        mAnimations.push_back(klass);
        if (!klass->FromJson(*chunk)) {
            WARN("Failed to load entity animation track completely. [entity='%1', animation='%2']", mName, klass->GetName());
            ok = false;
        }
    }
    for (unsigned i=0; i<data.GetNumChunks("vars"); ++i)
    {
        const auto& chunk = data.GetReadChunk("vars", i);
        auto var = std::make_shared<ScriptVar>();
        if (!var->FromJson(*chunk)) {
            WARN("Failed to load entity script variable completely. [entity='%1', var='%2']", mName, var->GetName());
            ok = false;
        } else mScriptVars.push_back(var);
    }
    for (unsigned i=0; i<data.GetNumChunks("joints"); ++i)
    {
        const auto& chunk = data.GetReadChunk("joints", i);
        auto joint = std::make_shared<PhysicsJoint>();
        if (!joint->FromJson(*chunk)) {
            WARN("Failed to load entity physics joint completely. [entity='%1', joint='%2']", mName, joint->GetName());
            ok = false;
        }
        mJoints.push_back(std::move(joint));
    }

    // migration, animators array becomes a single state-controller
    if (const auto& chunk = data.GetReadChunk("state-controller"))
    {
        mStateController = std::make_shared<EntityStateControllerClass>();
        ok &= mStateController->FromJson(*chunk);
    }
    else if (data.GetNumChunks("animators"))
    {
        const auto& chunk = data.GetReadChunk("animators", 0);
        mStateController = std::make_shared<EntityStateControllerClass>();
        ok &= mStateController->FromJson(*chunk);
    }

    const auto& chunk = data.GetReadChunk("render_tree");
    if (!chunk)
        return false;
    RenderTreeFromJson(mRenderTree, game::TreeNodeFromJson(mNodes), *chunk);
    return ok;
}

EntityClass EntityClass::Clone() const
{
    EntityClass ret;
    ret.mName = mName;
    ret.mFlags = mFlags;
    ret.mLifetime = mLifetime;
    ret.mScriptFile = mScriptFile;

    std::unordered_map<const EntityNodeClass*, const EntityNodeClass*> map;

    // make a deep clone of the nodes.
    for (const auto& node : mNodes)
    {
        auto clone = std::make_unique<EntityNodeClass>(node->Clone());
        map[node.get()] = clone.get();
        ret.mNodes.push_back(std::move(clone));
    }

    // make a deep clone of the animation tracks
    for (const auto& track : mAnimations)
    {
        auto clone = std::make_unique<AnimationClass>(track->Clone());
        if (track->GetId() == mIdleTrackId)
            ret.mIdleTrackId = clone->GetId();
        ret.mAnimations.push_back(std::move(clone));
    }
    // remap the animator node ids.
    for (auto& track : ret.mAnimations)
    {
        for (size_t i=0; i< track->GetNumAnimators(); ++i)
        {
            auto& animator = track->GetAnimatorClass(i);
            const auto* source_node = FindNodeById(animator.GetNodeId());
            if (source_node == nullptr)
                continue;
            const auto* cloned_node = map[source_node];
            animator.SetNodeId(cloned_node->GetId());
        }
    }
    // make a deep copy of the scripting variables.
    for (const auto& var : mScriptVars)
    {
        // remap entity node references.
        if (var->GetType() == ScriptVar::Type::EntityNodeReference)
        {
            std::vector<ScriptVar::EntityNodeReference> refs;
            const auto& src_arr = var->GetArray<ScriptVar::EntityNodeReference>();
            for (const auto& src_ref : src_arr)
            {
                const auto* src_node = FindNodeById(src_ref.id);
                const auto* dst_node = map[src_node];
                ScriptVar::EntityNodeReference ref;
                ref.id = dst_node ? dst_node->GetId() : "";
                refs.push_back(std::move(ref));
            }
            auto clone = std::make_shared<ScriptVar>();
            clone->SetName(var->GetName());
            clone->SetReadOnly(var->IsReadOnly());
            clone->SetArray(var->IsArray());
            clone->SetNewArrayType(std::move(refs));
            ret.mScriptVars.push_back(clone);
        }
        else ret.mScriptVars.push_back(std::make_shared<ScriptVar>(*var));
    }

    std::unordered_map<std::string, std::string> joint_mapping;

    // make a deep clone of the joints.
    for (const auto& joint : mJoints)
    {
        auto clone   = std::make_shared<PhysicsJoint>(*joint);
        clone->id = base::RandomString(10);

        joint_mapping[joint->id] = clone->id;

        // map the src and dst node IDs.
        const auto* old_src_node = FindNodeById(joint->src_node_id);
        const auto* old_dst_node = FindNodeById(joint->dst_node_id);
        ASSERT(old_src_node && old_dst_node);
        clone->src_node_id = map[old_src_node]->GetId();
        clone->dst_node_id = map[old_dst_node]->GetId();
        ret.mJoints.push_back(std::move(clone));
    }
    // remap property animator joint ids
    for (auto& animation : ret.mAnimations)
    {
        for (size_t i=0; i<animation->GetNumAnimators(); ++i)
        {
            auto& animator = animation->GetAnimatorClass(i);
            if (auto* ptr = AsPropertyAnimatorClass(&animator))
            {
                // map old joint ID to a new joint ID, empty string maps to an empty string.
                ptr->SetJointId(joint_mapping[ptr->GetJointId()]);
            }
            else if (auto* ptr = AsBooleanPropertyAnimatorClass(&animator))
            {
                ptr->SetJointId(joint_mapping[ptr->GetJointId()]);
            }
        }
    }

    if (mStateController)
    {
        ret.mStateController = std::make_shared<EntityStateControllerClass>(mStateController->Clone());
    }

    ret.mRenderTree.FromTree(mRenderTree, [&map](const EntityNodeClass* node) {
        return map[node];
    });
    return ret;
}

EntityClass& EntityClass::operator=(const EntityClass& other)
{
    if (this == &other)
        return *this;

    EntityClass tmp(other);
    mClassId         = std::move(tmp.mClassId);
    mName            = std::move(tmp.mName);
    mTag             = std::move(tmp.mTag);
    mIdleTrackId     = std::move(tmp.mIdleTrackId);
    mNodes           = std::move(tmp.mNodes);
    mJoints          = std::move(tmp.mJoints);
    mScriptVars      = std::move(tmp.mScriptVars);
    mScriptFile      = std::move(tmp.mScriptFile);
    mRenderTree      = std::move(tmp.mRenderTree);
    mAnimations      = std::move(tmp.mAnimations);
    mStateController = std::move(tmp.mStateController);
    mFlags           = tmp.mFlags;
    mLifetime        = tmp.mLifetime;
    return *this;
}

// static
void EntityClass::UpdateRuntimes(double game_time, double dt)
{
    for (auto& pair : runtimes)
    {
        auto* runtime = pair.second.get();
        if (!runtime->needs_update)
            continue;

        auto& allocator = runtime->allocator;

        std::unique_lock<std::mutex> allocator_lock(allocator.GetMutex());

        for (size_t i=0; i<allocator.GetHighIndex(); ++i)
        {
            auto* transform = allocator.GetObject<EntityNodeTransform>(i);
            auto* data = allocator.GetObject<EntityNodeData>(i);
            if (!transform || !data)
                continue;

            auto* node = data->GetNode();
            if (auto* mover = node->GetLinearMover())
            {
                mover->TransformObject(static_cast<float>(dt), *transform);
            }
            if (auto* mover = node->GetSplineMover())
            {
                mover->TransformObject(static_cast<float>(dt), *transform);
            }
        }
    }
}

} // namespace
