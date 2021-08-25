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
#  include <neargye/magic_enum.hpp>
#include "warnpop.h"

#include <unordered_set>
#include <stack>

#include "base/format.h"
#include "base/logging.h"
#include "base/hash.h"
#include "data/reader.h"
#include "data/writer.h"
#include "game/scene.h"
#include "game/entity.h"
#include "game/treeop.h"
#include "game/transform.h"

namespace game
{

std::size_t SceneNodeClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, mEntityId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mPosition);
    hash = base::hash_combine(hash, mScale);
    hash = base::hash_combine(hash, mRotation);
    hash = base::hash_combine(hash, mFlagValBits);
    hash = base::hash_combine(hash, mFlagSetBits);
    hash = base::hash_combine(hash, mLayer);
    hash = base::hash_combine(hash, mParentRenderTreeNodeId);
    hash = base::hash_combine(hash, mIdleAnimationId);
    if (mLifetime.has_value())
        hash = base::hash_combine(hash, mLifetime.value());
    return hash;
}

glm::mat4 SceneNodeClass::GetNodeTransform() const
{
    Transform transform;
    transform.Scale(mScale);
    transform.Rotate(mRotation);
    transform.Translate(mPosition);
    return transform.GetAsMatrix();
}

SceneNodeClass SceneNodeClass::Clone() const
{
    SceneNodeClass copy(*this);
    copy.mClassId = base::RandomString(10);
    return copy;
}

void SceneNodeClass::IntoJson(data::Writer& data) const
{
    data.Write("id",       mClassId);
    data.Write("entity",   mEntityId);
    data.Write("name",     mName);
    data.Write("position", mPosition);
    data.Write("scale",    mScale);
    data.Write("rotation", mRotation);
    data.Write("flag_val_bits", mFlagValBits);
    data.Write("flag_set_bits", mFlagSetBits);
    data.Write("layer",    mLayer);
    data.Write("parent_render_tree_node", mParentRenderTreeNodeId);
    data.Write("idle_animation_id", mIdleAnimationId);
    if (mLifetime.has_value())
        data.Write("lifetime", mLifetime.value());
}

// static
std::optional<SceneNodeClass> SceneNodeClass::FromJson(const data::Reader& data)
{
    SceneNodeClass ret;
    if (!data.Read("id",       &ret.mClassId)||
        !data.Read("entity",   &ret.mEntityId) ||
        !data.Read("name",     &ret.mName) ||
        !data.Read("position", &ret.mPosition) ||
        !data.Read("scale",    &ret.mScale) ||
        !data.Read("rotation", &ret.mRotation) ||
        !data.Read("flag_val_bits", &ret.mFlagValBits) ||
        !data.Read("flag_set_bits", &ret.mFlagSetBits) ||
        !data.Read("layer", &ret.mLayer) ||
        !data.Read("parent_render_tree_node", &ret.mParentRenderTreeNodeId) ||
        !data.Read("idle_animation_id", &ret.mIdleAnimationId))
        return std::nullopt;
    if (data.HasValue("lifetime"))
    {
        // todo: double
        float lifetime = 0.0;
        if (!data.Read("lifetime", &lifetime))
            return std::nullopt;
        ret.mLifetime = lifetime;
    }
    return ret;
}

SceneClass::SceneClass(const SceneClass& other)
{
    std::unordered_map<const SceneNodeClass*, const SceneNodeClass*> map;

    mClassId    = other.mClassId;
    mName       = other.mName;
    mScriptFile = other.mScriptFile;
    mScriptVars = other.mScriptVars;
    for (const auto& node : other.mNodes)
    {
        auto copy = std::make_unique<SceneNodeClass>(*node);
        map[node.get()] = copy.get();
        mNodes.push_back(std::move(copy));
    }
    mRenderTree.FromTree(other.mRenderTree, [&map](const SceneNodeClass* node) {
        return map[node];
    });
}
SceneNodeClass* SceneClass::AddNode(const SceneNodeClass& node)
{
    mNodes.emplace_back(new SceneNodeClass(node));
    return mNodes.back().get();
}
SceneNodeClass* SceneClass::AddNode(SceneNodeClass&& node)
{
    mNodes.emplace_back(new SceneNodeClass(std::move(node)));
    return mNodes.back().get();
}
SceneNodeClass* SceneClass::AddNode(std::unique_ptr<SceneNodeClass> node)
{
    mNodes.push_back(std::move(node));
    return mNodes.back().get();
}
SceneNodeClass& SceneClass::GetNode(size_t index)
{
    ASSERT(index < mNodes.size());
    return *mNodes[index].get();
}
SceneNodeClass* SceneClass::FindNodeByName(const std::string& name)
{
    for (const auto& node : mNodes)
        if (node->GetName() == name)
            return node.get();
    return nullptr;
}
SceneNodeClass* SceneClass::FindNodeById(const std::string& id)
{
    for (const auto& node : mNodes)
        if (node->GetId() == id)
            return node.get();
    return nullptr;
}
const SceneNodeClass& SceneClass::GetNode(size_t index) const
{
    ASSERT(index < mNodes.size());
    return *mNodes[index].get();
}
const SceneNodeClass* SceneClass::FindNodeByName(const std::string& name) const
{
    for (const auto& node : mNodes)
        if (node->GetName() == name)
            return node.get();
    return nullptr;
}
const SceneNodeClass* SceneClass::FindNodeById(const std::string& id) const
{
    for (const auto& node : mNodes)
        if (node->GetId() == id)
            return node.get();
    return nullptr;
}

void SceneClass::LinkChild(SceneNodeClass* parent, SceneNodeClass* child)
{
    game::LinkChild(mRenderTree, parent, child);
}

void SceneClass::BreakChild(SceneNodeClass* child, bool keep_world_transform)
{
    game::BreakChild(mRenderTree, child, keep_world_transform);
}

void SceneClass::ReparentChild(SceneNodeClass* parent, SceneNodeClass* child, bool keep_world_transform)
{
    game::ReparentChild(mRenderTree, parent, child, keep_world_transform);
}

void SceneClass::DeleteNode(SceneNodeClass* node)
{
    game::DeleteNode(mRenderTree, node, mNodes);
}

SceneNodeClass* SceneClass::DuplicateNode(const SceneNodeClass* node)
{
    return game::DuplicateNode(mRenderTree, node, &mNodes);
}

std::vector<SceneClass::ConstSceneNode> SceneClass::CollectNodes() const
{
    std::vector<SceneClass::ConstSceneNode> ret;

    // visit the entire render tree of the scene and transform every
    // SceneNodeClass (which is basically a placement for an entity in the
    // scene) into world.
    // todo: this needs some kind of space partitioning which allows the
    // collection to only consider some nodes that lie within some area
    // of interest.

    class Visitor : public RenderTree::ConstVisitor {
    public:
        Visitor(std::vector<SceneClass::ConstSceneNode>& result) : mResult(result)
        {}
        virtual void EnterNode(const SceneNodeClass* node) override
        {
            if (!node)
                return;

            // When entities are linked together the child entity refers
            // to a specific node in the parent entity. This node is the
            // parent node of the child entity's render tree.
            glm::mat4 parent_node_transform(1.0f);
            if (const auto* parent = GetParent())
            {
                const auto& klass = parent->GetEntityClass();
                const auto* parent_node = klass->FindNodeById(node->GetParentRenderTreeNodeId());
                parent_node_transform   = klass->FindNodeTransform(parent_node);
            }

            mParents.push(node);
            mTransform.Push(parent_node_transform);
            mTransform.Push(node->GetNodeTransform());
            ConstSceneNode entity;
            entity.node_to_scene = mTransform.GetAsMatrix();
            entity.entity        = node->GetEntityClass();
            entity.node          = node;
            mResult.push_back(std::move(entity));
        }
        virtual void LeaveNode(const SceneNodeClass* node) override
        {
            if (!node)
                return;
            // pop once the parent transform
            mTransform.Pop();
            // pop once the node transform.
            mTransform.Pop();
            mParents.pop();
        }
    private:
        const SceneNodeClass* GetParent() const
        {
            if (mParents.empty())
                return nullptr;
            return mParents.top();
        }
    private:
        std::stack<const SceneNodeClass*> mParents;
        std::vector<SceneClass::ConstSceneNode>& mResult;
        Transform mTransform;
    };
    Visitor visitor(ret);
    mRenderTree.PreOrderTraverse(visitor);
    return ret;
}

std::vector<SceneClass::SceneNode> SceneClass::CollectNodes()
{
    std::vector<SceneClass::SceneNode> ret;

    // visit the entire render tree of the scene and transform every
    // SceneNodeClass (which is basically a placement for an entity in the
    // scene) into world.
    // todo: this needs some kind of space partitioning which allows the
    // collection to only consider some nodes that lie within some area
    // of interest.

    class Visitor : public RenderTree::Visitor {
    public:
        Visitor(std::vector<SceneClass::SceneNode>& result) : mResult(result)
        {}
        virtual void EnterNode(SceneNodeClass* node) override
        {
            if (!node)
                return;

            // When entities are linked together the child entity refers
            // to a specific node in the parent entity. This node is the
            // parent node of the child entity's render tree.
            glm::mat4 parent_node_transform(1.0f);
            if (const auto* parent = GetParent())
            {
                const auto& klass = parent->GetEntityClass();
                const auto* parent_node = klass->FindNodeById(node->GetParentRenderTreeNodeId());
                parent_node_transform   = klass->FindNodeTransform(parent_node);
            }
            mParents.push(node);
            mTransform.Push(parent_node_transform);
            mTransform.Push(node->GetNodeTransform());
            SceneNode entity;
            entity.node_to_scene = mTransform.GetAsMatrix();
            entity.entity        = node->GetEntityClass();
            entity.node          = node;
            mResult.push_back(std::move(entity));
        }
        virtual void LeaveNode(SceneNodeClass* node) override
        {
            if (!node)
                return;
            mTransform.Pop();
            mTransform.Pop();
            mParents.pop();
        }
    private:
        SceneNodeClass* GetParent() const
        {
            if (mParents.empty())
                return nullptr;
            return mParents.top();
        }
    private:
        std::stack<SceneNodeClass*> mParents;
        std::vector<SceneClass::SceneNode>& mResult;
        Transform mTransform;
    };
    Visitor visitor(ret);
    mRenderTree.PreOrderTraverse(visitor);
    return ret;
}

void SceneClass::CoarseHitTest(float x, float y, std::vector<SceneNodeClass*>* hits,
                   std::vector<glm::vec2>* hitbox_positions)
{
    const auto& entity_nodes = CollectNodes();
    for (const auto& entity_node : entity_nodes)
    {
        if (!entity_node.entity)
        {
            WARN("Node '%1' has no entity class object!", entity_node.node->GetName());
            continue;
        }
        // transform the coordinate in the scene into the entity
        // coordinate space, then delegate the hit test to the
        // entity to see if we hit any of the entity nodes.
        auto scene_to_node = glm::inverse(entity_node.node_to_scene);
        auto node_hit_pos  = scene_to_node * glm::vec4(x, y, 1.0f, 1.0f);
        // perform entity hit test.
        std::vector<const EntityNodeClass*> nodes;
        entity_node.entity->CoarseHitTest(node_hit_pos.x, node_hit_pos.y, &nodes);
        if (nodes.empty())
            continue;

        // hit some nodes so the entity as a whole is hit.
        hits->push_back(entity_node.node);
        if (hitbox_positions)
            hitbox_positions->push_back(glm::vec2(node_hit_pos.x, node_hit_pos.y));
    }
}
void SceneClass::CoarseHitTest(float x, float y, std::vector<const SceneNodeClass*>* hits,
                       std::vector<glm::vec2>* hitbox_positions) const
{
    const auto& entity_nodes = CollectNodes();
    for (const auto& entity_node : entity_nodes)
    {
        if (!entity_node.entity)
        {
            WARN("Node '%1' has no entity class object!", entity_node.node->GetName());
            continue;
        }
        // transform the coordinate in the scene into the entity
        // coordinate space, then delegate the hit test to the
        // entity to see if we hit any of the entity nodes.
        auto scene_to_node = glm::inverse(entity_node.node_to_scene);
        auto node_hit_pos  = scene_to_node * glm::vec4(x, y, 1.0f, 1.0f);
        // perform entity hit test.
        std::vector<const EntityNodeClass*> nodes;
        entity_node.entity->CoarseHitTest(node_hit_pos.x, node_hit_pos.y, &nodes);
        if (nodes.empty())
            continue;

        // hit some nodes so the entity as a whole is hit.
        hits->push_back(entity_node.node);
        if (hitbox_positions)
            hitbox_positions->push_back(glm::vec2(node_hit_pos.x, node_hit_pos.y));
    }
}

glm::vec2 SceneClass::MapCoordsFromNodeModel(float x, float y, const SceneNodeClass* node) const
{
    const auto& entity_nodes = CollectNodes();
    for (const auto& entity_node : entity_nodes)
    {
        if (entity_node.node == node)
        {
            const auto ret = entity_node.node_to_scene * glm::vec4(x, y, 1.0f, 1.0f);
            return glm::vec2(ret.x, ret.y);
        }
    }
    // todo: should we return something else maybe ?
    return glm::vec2(0.0f, 0.0f);
}
glm::vec2 SceneClass::MapCoordsToNodeModel(float x, float y, const SceneNodeClass* node) const
{
    const auto& entity_nodes = CollectNodes();
    for (const auto& entity_node : entity_nodes)
    {
        if (entity_node.node == node)
        {
            const auto ret = glm::inverse(entity_node.node_to_scene) * glm::vec4(x, y, 1.0f, 1.0f);
            return glm::vec2(ret.x, ret.y);
        }
    }
    // todo: should we return something else maybe ?
    return glm::vec2(0.0f, 0.0f);
}

glm::mat4 SceneClass::FindNodeTransform(const SceneNodeClass* node) const
{
    return game::FindNodeTransform(mRenderTree, node);
}

void SceneClass::AddScriptVar(const ScriptVar& var)
{
    mScriptVars.push_back(var);
}
void SceneClass::AddScriptVar(ScriptVar&& var)
{
    mScriptVars.push_back(var);
}
void SceneClass::DeleteScriptVar(size_t index)
{
    ASSERT(index <mScriptVars.size());
    auto it = mScriptVars.begin();
    mScriptVars.erase(it + index);
}
void SceneClass::SetScriptVar(size_t index, const ScriptVar& var)
{
    ASSERT(index <mScriptVars.size());
    mScriptVars[index] = var;
}
void SceneClass::SetScriptVar(size_t index, ScriptVar&& var)
{
    ASSERT(index <mScriptVars.size());
    mScriptVars[index] = std::move(var);
}
ScriptVar& SceneClass::GetScriptVar(size_t index)
{
    ASSERT(index <mScriptVars.size());
    return mScriptVars[index];
}
ScriptVar* SceneClass::FindScriptVar(const std::string& name)
{
    for (auto& var : mScriptVars)
        if (var.GetName() == name)
            return &var;
    return nullptr;
}
const ScriptVar& SceneClass::GetScriptVar(size_t index) const
{
    ASSERT(index <mScriptVars.size());
    return mScriptVars[index];
}
const ScriptVar* SceneClass::FindScriptVar(const std::string& name) const
{
    for (auto& var : mScriptVars)
        if (var.GetName() == name)
            return &var;
    return nullptr;
}

size_t SceneClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mScriptFile);
    // include the node hashes in the animation hash
    // this covers both the node values and their traversal order
    mRenderTree.PreOrderTraverseForEach([&](const SceneNodeClass* node) {
        if (node == nullptr)
            return;
        hash = base::hash_combine(hash, node->GetHash());
    });
    for (const auto& var : mScriptVars)
        hash = base::hash_combine(hash, var.GetHash());
    return hash;
}

void SceneClass::IntoJson(data::Writer& data) const
{
    data.Write("id", mClassId);
    data.Write("name", mName);
    data.Write("script_file", mScriptFile);
    for (const auto& node : mNodes)
    {
        auto chunk = data.NewWriteChunk();
        node->IntoJson(*chunk);
        data.AppendChunk("nodes", std::move(chunk));
    }
    for (const auto& var : mScriptVars)
    {
        auto chunk = data.NewWriteChunk();
        var.IntoJson(*chunk);
        data.AppendChunk("vars", std::move(chunk));
    }
    auto chunk = data.NewWriteChunk();
    mRenderTree.IntoJson(&game::TreeNodeToJson<SceneNodeClass>, *chunk);
    data.Write("render_tree", std::move(chunk));
}

// static
std::optional<SceneClass> SceneClass::FromJson(const data::Reader& data)
{
    SceneClass ret;
    if (!data.Read("id", &ret.mClassId) ||
        !data.Read("name", &ret.mName) ||
        !data.Read("script_file", &ret.mScriptFile))
        return std::nullopt;
    for (unsigned i=0; i<data.GetNumChunks("nodes"); ++i)
    {
        const auto& chunk = data.GetReadChunk("nodes", i);
        std::optional<SceneNodeClass> node = SceneNodeClass::FromJson(*chunk);
        if (!node.has_value())
            return std::nullopt;
        ret.mNodes.push_back(std::make_unique<SceneNodeClass>(std::move(node.value())));

    }
    for (unsigned i=0; i<data.GetNumChunks("vars"); ++i)
    {
        const auto& chunk = data.GetReadChunk("vars", i);
        std::optional<ScriptVar> var = ScriptVar::FromJson(*chunk);
        if (!var.has_value())
            return std::nullopt;
        ret.mScriptVars.push_back(std::move(var.value()));
    }
    const auto& chunk = data.GetReadChunk("render_tree");
    if (!chunk)
        return std::nullopt;
    ret.mRenderTree.FromJson(*chunk, game::TreeNodeFromJson(ret.mNodes));
    return ret;
}
SceneClass SceneClass::Clone() const
{
    SceneClass ret;

    std::unordered_map<const SceneNodeClass*, const SceneNodeClass*> map;

    // make a deep copy of the nodes.
    for (const auto& node : mNodes)
    {
        auto clone = std::make_unique<SceneNodeClass>(*node);
        map[node.get()] = clone.get();
        ret.mNodes.push_back(std::move(clone));
    }
    ret.mScriptVars = mScriptVars;
    ret.mScriptFile = mScriptFile;
    ret.mName       = mName;
    ret.mRenderTree.FromTree(mRenderTree, [&map](const SceneNodeClass* node) {
        return map[node];
    });
    return ret;
}

SceneClass& SceneClass::operator=(const SceneClass& other)
{
    if (this == &other)
        return *this;

    SceneClass tmp(other);
    mClassId    = std::move(tmp.mClassId);
    mScriptFile = std::move(tmp.mScriptFile);
    mName       = std::move(tmp.mName);
    mNodes      = std::move(tmp.mNodes);
    mScriptVars = std::move(tmp.mScriptVars);
    mRenderTree = tmp.mRenderTree;
    return *this;
}

Scene::Scene(std::shared_ptr<const SceneClass> klass)
  : mClass(klass)
{
    std::unordered_map<const SceneNodeClass*, const Entity*> map;

    // spawn an entity instance for each scene node class
    // in the scene class
    for (size_t i=0; i<klass->GetNumNodes(); ++i)
    {
        const auto& node = klass->GetNode(i);
        EntityArgs args;
        args.klass    = node.GetEntityClass();
        args.rotation = node.GetRotation();
        args.position = node.GetTranslation();
        args.scale    = node.GetScale();
        args.name     = node.GetName();
        args.id       = node.GetId();
        ASSERT(args.klass);
        auto entity   = CreateEntityInstance(args);

        // these need always be set for each entity spawned from scene
        // placement node.
        entity->SetParentNodeClassId(node.GetParentRenderTreeNodeId());
        entity->SetLayer(node.GetLayer());

        // optionally set instance settings, if these are not set then
        // entity class defaults apply.
        if (node.HasIdleAnimationSetting())
            entity->SetIdleTrackId(node.GetIdleAnimationId());
        if (node.HasLifetimeSetting())
            entity->SetLifetime(node.GetLifetime());

        // check which flags the scene node has set and set those on the
        // entity instance. for any flag setting that is not set entity class
        // default will apply.
        for (const auto& flag : magic_enum::enum_values<Entity::Flags>())
        {
            if (node.HasFlagSetting(flag))
                entity->SetFlag(flag, node.TestFlag(flag));
        }

        map[&node] = entity.get();
        mIdMap[entity->GetId()] = entity.get();
        mNameMap[entity->GetName()] = entity.get();
        mEntities.push_back(std::move(entity));
    }
    mRenderTree.FromTree(mClass->GetRenderTree(), [&map](const SceneNodeClass* node) {
        return map[node];
    });

    // make copies of mutable script variables.
    for (size_t i=0; i<klass->GetNumScriptVars(); ++i)
    {
        auto var = klass->GetScriptVar(i);
        if (!var.IsReadOnly())
            mScriptVars.push_back(std::move(var));
    }
}

Scene::Scene(const SceneClass& klass) : Scene(std::make_shared<SceneClass>(klass))
{}

Entity& Scene::GetEntity(size_t index)
{
    ASSERT(index < mEntities.size());
    return *mEntities[index];
}
Entity* Scene::FindEntityByInstanceId(const std::string& id)
{
    auto it = mIdMap.find(id);
    if (it == mIdMap.end())
        return nullptr;
    return it->second;
}
Entity* Scene::FindEntityByInstanceName(const std::string& name)
{
    auto it = mNameMap.find(name);
    if (it == mNameMap.end())
        return nullptr;
    return it->second;
}

const Entity& Scene::GetEntity(size_t index) const
{
    ASSERT(index < mEntities.size());
    return *mEntities[index];
}
const Entity* Scene::FindEntityByInstanceId(const std::string& id) const
{
    auto it = mIdMap.find(id);
    if (it == mIdMap.end())
        return nullptr;
    return it->second;
}
const Entity* Scene::FindEntityByInstanceName(const std::string& name) const
{
    auto it = mNameMap.find(name);
    if (it == mNameMap.end())
        return nullptr;
    return it->second;
}

void Scene::KillEntity(Entity* entity)
{
    mKillList.push_back(entity);
}

Entity* Scene::SpawnEntity(const EntityArgs& args, bool link_to_root)
{
    // we must have the klass of the entity and an id.
    // the invariant that must hold is that entity IDs are
    // always going to be unique.
    ASSERT(args.klass);
    ASSERT(args.id.empty() == false);
    ASSERT(mIdMap.find(args.id) == mIdMap.end());

    mSpawnList.push_back(CreateEntityInstance(args));
    DEBUG("New entity '%1/%2'", args.klass->GetName(), args.name);
    return mSpawnList.back().get();
}

void Scene::BeginLoop()
{
    // turn on the kill flag for entities that were killed
    // during the last iteration of the game play.
    for (auto* entity : mKillList)
    {
        // Set entity kill flag to indicate that it's been killed from the scene.
        // Note that this isn't the same as the c++ lifetime of the object!
        entity->SetFlag(Entity::ControlFlags::Killed, true);
        // propagate the kill flag to children when a parent
        // entity is killed. if this is not desired then one
        // should have unlinked the children first.
        mRenderTree.PreOrderTraverseForEach([](Entity* entity) {
            entity->SetFlag(Entity::ControlFlags::Killed, true);
            DEBUG("Entity '%1/%2' was killed", entity->GetClassName(), entity->GetName());
        }, entity);
    }
    for (auto& entity : mSpawnList)
    {
        DEBUG("Entity '%1/%2' was spawned.", entity->GetClassName(), entity->GetName());
        entity->SetFlag(Entity::ControlFlags::Spawned, true);
        mIdMap[entity->GetId()]     = entity.get();
        mNameMap[entity->GetName()] = entity.get();
        mRenderTree.LinkChild(nullptr, entity.get());
        mEntities.push_back(std::move(entity));
    }
    mKillList.clear();
    mSpawnList.clear();
}

void Scene::EndLoop()
{
    for (auto& entity : mEntities)
    {
        // turn off spawn flags.
        entity->SetFlag(Entity::ControlFlags::Spawned, false);

        if (!entity->TestFlag(Entity::ControlFlags::Killed))
            continue;
        DEBUG("Delete entity '%1/%2'.", entity->GetClassName(), entity->GetName());
        mRenderTree.DeleteNode(entity.get());
        mIdMap.erase(entity->GetId());
        mNameMap.erase(entity->GetName());
    }
    // delete from the container the ones that were killed.
    mEntities.erase(std::remove_if(mEntities.begin(), mEntities.end(), [](const auto& entity) {
        return entity->TestFlag(Entity::ControlFlags::Killed);
    }), mEntities.end());
}

std::vector<Scene::ConstSceneNode> Scene::CollectNodes() const
{
    std::vector<Scene::ConstSceneNode> ret;

    class Visitor : public RenderTree::ConstVisitor {
    public:
        Visitor(std::vector<Scene::ConstSceneNode>& result) : mResult(result)
        {}
        virtual void EnterNode(const Entity* node) override
        {
            if (!node)
                return;

            glm::mat4 parent_node_transform(1.0f);
            if (const auto* parent = GetParent())
            {
                const auto* parent_node = parent->FindNodeByClassId(node->GetParentNodeClassId());
                parent_node_transform   = parent->FindNodeTransform(parent_node);
            }
            mParents.push(node);
            mTransform.Push(parent_node_transform);
            ConstSceneNode entity;
            entity.node_to_scene = mTransform.GetAsMatrix();
            entity.entity        = node;
            entity.node          = node;
            mResult.push_back(std::move(entity));
        }
        virtual void LeaveNode(const Entity* node) override
        {
            if (!node)
                return;
            mTransform.Pop();
            mParents.pop();
        }
    private:
        const Entity* GetParent() const
        {
            if (mParents.empty())
                return nullptr;
            return mParents.top();
        }
    private:
        std::stack<const Entity*> mParents;
        std::vector<Scene::ConstSceneNode>& mResult;
        Transform mTransform;
    };
    Visitor visitor(ret);
    mRenderTree.PreOrderTraverse(visitor);
    return ret;
}

std::vector<Scene::SceneNode> Scene::CollectNodes()
{
    std::vector<Scene::SceneNode> ret;

    class Visitor : public RenderTree::Visitor {
    public:
        Visitor(std::vector<Scene::SceneNode>& result) : mResult(result)
        {}
        virtual void EnterNode(Entity* node) override
        {
            if (!node)
                return;

            glm::mat4 parent_node_transform(1.0f);
            if (const auto* parent = GetParent())
            {
                const auto* parent_node = parent->FindNodeByClassId(node->GetParentNodeClassId());
                parent_node_transform   = parent->FindNodeTransform(parent_node);
            }
            mParents.push(node);
            mTransform.Push(parent_node_transform);
            SceneNode entity;
            entity.node_to_scene = mTransform.GetAsMatrix();
            entity.entity        = node;
            entity.node          = node;
            mResult.push_back(std::move(entity));
        }
        virtual void LeaveNode(Entity* node) override
        {
            if (!node)
                return;
            mTransform.Pop();
            mParents.pop();
        }
    private:
        Entity* GetParent() const
        {
            if (mParents.empty())
                return nullptr;
            return mParents.top();
        }
    private:
        std::stack<Entity*> mParents;
        std::vector<Scene::SceneNode>& mResult;
        Transform mTransform;
    };
    Visitor visitor(ret);
    mRenderTree.PreOrderTraverse(visitor);
    return ret;
}

glm::mat4 Scene::FindEntityTransform(const Entity* entity) const
{
    // if the parent of this entity is the root node then the
    // the matrix will be simply identity
    if (!mRenderTree.HasNode(entity) || !mRenderTree.GetParent(entity))
        return glm::mat4(1.0f);

    // search the render tree until we find the entity.
    class Visitor : public RenderTree::ConstVisitor {
    public:
        Visitor(const Entity* entity) : mEntity(entity), mMatrix(1.0f)
        {}
        virtual void EnterNode(const Entity* entity) override
        {
            if (!entity)
                return;
            glm::mat4 parent_node_transform(1.0f);
            if (const auto* parent = GetParent())
            {
                const auto* parent_node = parent->FindNodeByClassId(entity->GetParentNodeClassId());
                parent_node_transform   = parent->FindNodeTransform(parent_node);
            }
            mParents.push(entity);
            mTransform.Push(parent_node_transform);
            if (entity == mEntity)
            {
                mMatrix = mTransform.GetAsMatrix();
                mDone = true;
            }
        }
        virtual void LeaveNode(const Entity* entity) override
        {
            if (!entity)
                return;
            mTransform.Pop();
            mParents.pop();
        }
        virtual bool IsDone() const override
        { return mDone; }

        glm::mat4 GetMatrix() const
        { return mMatrix; }
    private:
        const Entity* GetParent() const
        {
            if (mParents.empty())
                return nullptr;
            return mParents.top();
        }
    private:
        const Entity* mEntity = nullptr;
        std::stack<const Entity*> mParents;
        glm::mat4 mMatrix;
        Transform mTransform;
        bool mDone = false;
    };
    Visitor visitor(entity);
    mRenderTree.PreOrderTraverse(visitor);
    return visitor.GetMatrix();
}

glm::mat4 Scene::FindEntityNodeTransform(const Entity* entity, const EntityNode* node) const
{
    Transform transform(FindEntityTransform(entity));
    transform.Push(entity->FindNodeTransform(node));
    return transform.GetAsMatrix();
}

FRect Scene::FindEntityBoundingRect(const Entity* entity) const
{
    FRect ret;
    Transform transform(FindEntityTransform(entity));
    for (size_t i=0; i<entity->GetNumNodes(); ++i)
    {
        const auto& node = entity->GetNode(i);
        transform.Push(entity->FindNodeTransform(&node));
        transform.Push(node->GetModelTransform());
        ret = Union(ret, ComputeBoundingRect(transform.GetAsMatrix()));
        transform.Pop();
        transform.Pop();
    }
    return ret;
}
FRect Scene::FindEntityNodeBoundingRect(const Entity* entity, const EntityNode* node) const
{
    Transform transform(FindEntityNodeTransform(entity, node));
    transform.Push(node->GetModelTransform());
    return ComputeBoundingRect(transform.GetAsMatrix());
}

FBox Scene::FindEntityNodeBoundingBox(const Entity* entity, const EntityNode* node) const
{
    Transform transform(FindEntityNodeTransform(entity, node));
    transform.Push(node->GetModelTransform());
    return FBox(transform.GetAsMatrix());
}

const ScriptVar* Scene::FindScriptVar(const std::string& name) const
{
    // first check the mutable variables per this instance then check the class.
    for (const auto& var : mScriptVars)
    {
        if (var.GetName() == name)
            return &var;
    }
    return mClass->FindScriptVar(name);
}

void Scene::Update(float dt)
{
    mCurrentTime += dt;

    // todo: limit which entities are getting updated.
    for (auto& entity : mEntities)
    {
        entity->Update(dt);
        if (entity->HasExpired())
        {
            if (entity->TestFlag(Entity::Flags::KillAtLifetime))
                entity->SetFlag(Entity::ControlFlags::Killed , true);
            continue;
        }
        if (entity->IsPlaying())
            continue;
        if (entity->HasIdleTrack())
            entity->PlayIdle();
    }
}

std::unique_ptr<Scene> CreateSceneInstance(std::shared_ptr<const SceneClass> klass)
{
    return std::make_unique<Scene>(klass);
}

std::unique_ptr<Scene> CreateSceneInstance(const SceneClass& klass)
{
    return std::make_unique<Scene>(klass);
}

} // namespace
