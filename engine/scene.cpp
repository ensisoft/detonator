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

#include "config.h"

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <unordered_set>
#include <stack>

#include "base/format.h"
#include "base/logging.h"
#include "engine/scene.h"
#include "engine/entity.h"
#include "engine/treeop.h"
#include "engine/transform.h"

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
    hash = base::hash_combine(hash, mFlags.value());
    hash = base::hash_combine(hash, mLayer);
    hash = base::hash_combine(hash, mParentRenderTreeNodeId);
    hash = base::hash_combine(hash, mIdleAnimationId);
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

nlohmann::json SceneNodeClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "id",       mClassId);
    base::JsonWrite(json, "entity",   mEntityId);
    base::JsonWrite(json, "name",     mName);
    base::JsonWrite(json, "position", mPosition);
    base::JsonWrite(json, "scale",    mScale);
    base::JsonWrite(json, "rotation", mRotation);
    base::JsonWrite(json, "flags",    mFlags);
    base::JsonWrite(json, "layer",    mLayer);
    base::JsonWrite(json, "parent_render_tree_node", mParentRenderTreeNodeId);
    base::JsonWrite(json, "idle_animation_id", mIdleAnimationId);
    return json;
}

// static
std::optional<SceneNodeClass> SceneNodeClass::FromJson(const nlohmann::json& json)
{
    SceneNodeClass ret;
    if (!base::JsonReadSafe(json, "id",       &ret.mClassId)||
        !base::JsonReadSafe(json, "entity",   &ret.mEntityId) ||
        !base::JsonReadSafe(json, "name",     &ret.mName) ||
        !base::JsonReadSafe(json, "position", &ret.mPosition) ||
        !base::JsonReadSafe(json, "scale",    &ret.mScale) ||
        !base::JsonReadSafe(json, "rotation", &ret.mRotation) ||
        !base::JsonReadSafe(json, "flags",    &ret.mFlags) ||
        !base::JsonReadSafe(json, "layer",    &ret.mLayer) ||
        !base::JsonReadSafe(json, "parent_render_tree_node", &ret.mParentRenderTreeNodeId) ||
        !base::JsonReadSafe(json, "idle_animation_id", &ret.mIdleAnimationId))
        return std::nullopt;
    return ret;
}

SceneClass::SceneClass(const SceneClass& other)
{
    std::unordered_map<const SceneNodeClass*, const SceneNodeClass*> map;

    mClassId    = other.mClassId;
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
                parent_node_transform   = klass->GetNodeTransform(parent_node);
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
                parent_node_transform   = klass->GetNodeTransform(parent_node);
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

glm::vec2 SceneClass::MapCoordsFromNode(float x, float y, const SceneNodeClass* node) const
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
glm::vec2 SceneClass::MapCoordsToNode(float x, float y, const SceneNodeClass* node) const
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

glm::mat4 SceneClass::GetNodeTransform(const SceneNodeClass* node) const
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

nlohmann::json SceneClass::ToJson() const
{
    nlohmann::json json;
    base::JsonWrite(json, "id", mClassId);
    for (const auto& node : mNodes)
    {
        json["nodes"].push_back(node->ToJson());
    }
    for (const auto& var : mScriptVars)
    {
        json["vars"].push_back(var.ToJson());
    }
    json["render_tree"] = mRenderTree.ToJson(&game::TreeNodeToJson<SceneNodeClass>);
    return json;
}

// static
std::optional<SceneClass> SceneClass::FromJson(const nlohmann::json& json)
{
    SceneClass ret;
    if (!base::JsonReadSafe(json, "id", &ret.mClassId))
        return std::nullopt;
    if (json.contains("nodes"))
    {
        for (const auto& json : json["nodes"].items())
        {
            std::optional<SceneNodeClass> node = SceneNodeClass::FromJson(json.value());
            if (!node.has_value())
                return std::nullopt;
            ret.mNodes.push_back(std::make_unique<SceneNodeClass>(std::move(node.value())));
        }
    }
    if (json.contains("vars"))
    {
        for (const auto& js : json["vars"].items())
        {
            std::optional<ScriptVar> var = ScriptVar::FromJson(js.value());
            if (!var.has_value())
                return std::nullopt;
            ret.mScriptVars.push_back(std::move(var.value()));
        }
    }

    ret.mRenderTree.FromJson(json["render_tree"], game::TreeNodeFromJson(ret.mNodes));
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
        // override entity instance flags with the flag values from the
        // placement scene node class.
        entity->SetFlag(Entity::Flags::VisibleInGame, node.TestFlag(SceneNodeClass::Flags::VisibleInGame));
        entity->SetParentNodeClassId(node.GetParentRenderTreeNodeId());
        entity->SetIdleTrackId(node.GetIdleAnimationId());
        entity->SetLayer(node.GetLayer());

        map[&node] = entity.get();
        mEntityMap[entity->GetId()] = entity.get();
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
    auto it = mEntityMap.find(id);
    if (it == mEntityMap.end())
        return nullptr;
    return it->second;
}
Entity* Scene::FindEntityByInstanceName(const std::string& name)
{
    for (auto& e : mEntities)
        if (e->GetName() == name)
            return e.get();
    return nullptr;
}

const Entity& Scene::GetEntity(size_t index) const
{
    ASSERT(index < mEntities.size());
    return *mEntities[index];
}
const Entity* Scene::FindEntityByInstanceId(const std::string& id) const
{
    auto it = mEntityMap.find(id);
    if (it == mEntityMap.end())
        return nullptr;
    return it->second;
}
const Entity* Scene::FindEntityByInstanceName(const std::string& name) const
{
    for (const auto& e : mEntities)
        if (e->GetName() == name)
            return e.get();
    return nullptr;
}

void Scene::DeleteEntity(Entity* entity)
{
    std::unordered_set<const Entity*> graveyard;

    // traverse the tree starting from the node to be deleted
    // and capture the ids of the scene nodes that are part
    // of this hierarchy.
    mRenderTree.PreOrderTraverseForEach([&graveyard](const Entity* value) {
        graveyard.insert(value);
    }, entity);

    for (const auto* entity : graveyard)
    {
        DEBUG("Deleting entity '%1'", entity->GetName());
        mEntityMap.erase(entity->GetId());
    }

    // delete from the tree.
    mRenderTree.DeleteNode(entity);

    // delete from the container.
    mEntities.erase(std::remove_if(mEntities.begin(), mEntities.end(), [&graveyard](const auto& node) {
        return graveyard.find(node.get()) != graveyard.end();
    }), mEntities.end());
}

void Scene::KillEntity(Entity* entity)
{
    // either set this flag here or then keep separate
    // kill set. The flag has the benefit that the entity can
    // easily proclaim it's status to the world if needed
    entity->SetFlag(Entity::ControlFlags::Killed, true);
}

void Scene::PruneEntities()
{
    // remove the entities that have been killed.
    // this may propagate to children when a parent
    // entity is killed. if this is not desired then one
    // should have unlinked the children first.
    for (auto& entity : mEntities)
    {
        if (!entity->TestFlag(Entity::ControlFlags::Killed))
            continue;
        mRenderTree.PreOrderTraverseForEach([](Entity* entity) {
            entity->SetFlag(Entity::ControlFlags::Killed, true);
        }, entity.get());
        DEBUG("Deleting entity '%1'", entity->GetName());
        mRenderTree.DeleteNode(entity.get());
    }
    // delete from the container.
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
                parent_node_transform   = parent->GetNodeTransform(parent_node);
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
                parent_node_transform   = parent->GetNodeTransform(parent_node);
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
