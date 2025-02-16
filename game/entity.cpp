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

#include "base/logging.h"
#include "base/assert.h"
#include "base/utility.h"
#include "base/hash.h"
#include "base/trace.h"
#include "data/reader.h"
#include "data/writer.h"
#include "game/treeop.h"
#include "game/entity.h"
#include "game/transform.h"
#include "game/animator.h"
#include "game/property_animator.h"
#include "game/entity_node_transformer.h"
#include "game/entity_node_rigid_body.h"
#include "game/entity_node_drawable_item.h"
#include "game/entity_node_text_item.h"
#include "game/entity_node_spatial_node.h"
#include "game/entity_node_fixture.h"
#include "game/entity_node_tilemap_node.h"

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
    for (const auto& animator : other.mAnimators)
    {
        mAnimators.push_back(std::make_shared<EntityStateControllerClass>(*animator));
    }

    mRenderTree.FromTree(other.GetRenderTree(), [&map](const EntityNodeClass* node) {
        return map[node];
    });
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

EntityStateControllerClass* EntityClass::AddController(EntityStateControllerClass&& animator)
{
    mAnimators.push_back(std::make_shared<EntityStateControllerClass>(std::move(animator)));
    return mAnimators.back().get();
}
EntityStateControllerClass* EntityClass::AddController(const EntityStateControllerClass& animator)
{
    mAnimators.push_back(std::make_shared<EntityStateControllerClass>(animator));
    return mAnimators.back().get();
}
EntityStateControllerClass* EntityClass::AddController(const std::shared_ptr<EntityStateControllerClass>& animator)
{
    mAnimators.push_back(animator);
    return mAnimators.back().get();
}

void EntityClass::DeleteController(size_t index)
{
    base::SafeErase(mAnimators, index);
}
bool EntityClass::DeleteControllerByName(const std::string& name)
{
    return EraseByName(mAnimators, name);
}

bool EntityClass::DeleteControllerById(const std::string& id)
{
    return EraseById(mAnimators, id);
}

EntityStateControllerClass& EntityClass::GetController(size_t index)
{
    return *base::SafeIndex(mAnimators, index);
}
EntityStateControllerClass* EntityClass::FindControllerByName(const std::string& name)
{
    return FindByName(mAnimators, name);
}
EntityStateControllerClass* EntityClass::FindControllerById(const std::string& id)
{
    return FindById(mAnimators, id);
}

const EntityStateControllerClass& EntityClass::GetController(size_t index) const
{
    return *base::SafeIndex(mAnimators, index);
}
const EntityStateControllerClass* EntityClass::FindControllerByName(const std::string& name) const
{
    return FindByName(mAnimators, name);
}
const EntityStateControllerClass* EntityClass::FindControllerById(const std::string& id) const
{
    return FindById(mAnimators, id);
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

void EntityClass::CoarseHitTest(float x, float y, std::vector<EntityNodeClass*>* hits, std::vector<glm::vec2>* hitbox_positions)
{
    game::CoarseHitTest(mRenderTree, x, y, hits, hitbox_positions);
}

void EntityClass::CoarseHitTest(float x, float y, std::vector<const EntityNodeClass*>* hits, std::vector<glm::vec2>* hitbox_positions) const
{
    game::CoarseHitTest(mRenderTree, x, y, hits, hitbox_positions);
}

glm::vec2 EntityClass::MapCoordsFromNodeBox(float x, float y, const EntityNodeClass* node) const
{
    return game::MapCoordsFromNodeBox(mRenderTree, x, y, node);
}
glm::vec2 EntityClass::MapCoordsFromNodeBox(const glm::vec2& pos, const EntityNodeClass* node) const
{
    return game::MapCoordsFromNodeBox(mRenderTree, pos.x, pos.y, node);
}
glm::vec2 EntityClass::MapCoordsToNodeBox(float x, float y, const EntityNodeClass* node) const
{
    return game::MapCoordsToNodeBox(mRenderTree, x, y, node);
}
glm::vec2 EntityClass::MapCoordsToNodeBox(const glm::vec2& pos, const EntityNodeClass* node) const
{
    return game::MapCoordsToNodeBox(mRenderTree, pos.x, pos.y, node);
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

    for (const auto& animator : mAnimators)
    {
        hash = base::hash_combine(hash, animator->GetHash());
    }

    return hash;
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

    for (const auto& animator : mAnimators)
    {
        auto chunk = data.NewWriteChunk();
        animator->IntoJson(*chunk);
        data.AppendChunk("animators", std::move(chunk));
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

    for (unsigned i=0; i<data.GetNumChunks("animators"); ++i)
    {
        const auto& chunk = data.GetReadChunk("animators", i);
        auto animator = std::make_shared<EntityStateControllerClass>();
        ok &= animator->FromJson(*chunk);
        mAnimators.push_back(std::move(animator));
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

    for (const auto& animator : mAnimators)
    {
        auto clone = std::make_shared<EntityStateControllerClass>(animator->Clone());
        ret.mAnimators.push_back(std::move(clone));
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
    mAnimators       = std::move(tmp.mAnimators);
    mFlags           = tmp.mFlags;
    mLifetime        = tmp.mLifetime;
    return *this;
}

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

    auto& allocator = mClass->GetAllocator();

    // build render tree, first create instances of all node classes
    // then build the render tree based on the node instances
    TRACE_BLOCK("Entity::Entity::BuildRenderTree",
        for (size_t i = 0; i < mClass->GetNumNodes(); ++i)
        {
            auto node_klass = mClass->GetSharedEntityNodeClass(i);
            EntityNode node(node_klass, &allocator);
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
    mLayer        = args.layer;
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
    auto& allocator = mClass->GetAllocator();

    for (auto& node: mNodes)
    {
        node.Release(&allocator);
    }
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

void Entity::CoarseHitTest(float x, float y, std::vector<EntityNode*>* hits, std::vector<glm::vec2>* hitbox_positions)
{
    game::CoarseHitTest(mRenderTree, x, y, hits, hitbox_positions);
}

void Entity::CoarseHitTest(const glm::vec2& pos, std::vector<EntityNode*>* hits, std::vector<glm::vec2>* hitbox_positions)
{
    game::CoarseHitTest(mRenderTree, pos.x, pos.y, hits, hitbox_positions);
}

void Entity::CoarseHitTest(float x, float y, std::vector<const EntityNode*>* hits, std::vector<glm::vec2>* hitbox_positions) const
{
    game::CoarseHitTest(mRenderTree, x, y, hits, hitbox_positions);
}
void Entity::CoarseHitTest(const glm::vec2& pos, std::vector<const EntityNode*>* hits, std::vector<glm::vec2>* hitbox_positions) const
{
    game::CoarseHitTest(mRenderTree, pos.x, pos.y, hits, hitbox_positions);
}

glm::vec2 Entity::MapCoordsFromNodeBox(float x, float y, const EntityNode* node) const
{
    return game::MapCoordsFromNodeBox(mRenderTree, x, y, node);
}
glm::vec2 Entity::MapCoordsFromNodeBox(const glm::vec2& pos, const EntityNode* node) const
{
    return game::MapCoordsFromNodeBox(mRenderTree, pos.x, pos.y, node);
}

glm::vec2 Entity::MapCoordsToNodeBox(float x, float y, const EntityNode* node) const
{
    return game::MapCoordsToNodeBox(mRenderTree, x, y, node);
}

glm::vec2 Entity::MapCoordsToNodeBox(const glm::vec2& pos, const EntityNode* node) const
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


    for (auto& node : mNodes)
    {
        auto* transformer = node.GetTransformer();
        if (!transformer || !transformer->IsEnabled())
            continue;

        const auto integrator = transformer->GetIntegrator();
        if (integrator == NodeTransformerClass::Integrator::Euler)
        {
            {
                float acceleration = transformer->GetAngularAcceleration();
                float velocity = transformer->GetAngularVelocity();
                velocity += acceleration * dt;
                node.Rotate(velocity * dt);
                transformer->SetAngularVelocity(velocity);
            }

            {
                auto acceleration = transformer->GetLinearAcceleration();
                auto velocity = transformer->GetLinearVelocity();
                velocity += acceleration * dt;
                node.Translate(velocity * dt);
                transformer->SetLinearVelocity(velocity);
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

