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

namespace {
template<typename Result, typename Entity, typename Node>
class SceneEntityCollector : public game::RenderTree<Entity>::template TVisitor<Node> {
public:
    virtual void EnterNode(Node* node) override
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
        mTransform.Push(std::move(parent_node_transform));
        Result ret;
        ret.node_to_scene = mTransform.GetAsMatrix();
        ret.visual_entity = node;
        ret.entity_object = node;
        mResult.push_back(std::move(ret));
    }
    virtual void LeaveNode(Node* node) override
    {
        if (!node)
            return;
        mTransform.Pop();
        mParents.pop();
    }
    std::vector<Result> GetResult() &&
    { return std::move(mResult); }
private:
    Node* GetParent() const
    {
        if (mParents.empty())
            return nullptr;
        return mParents.top();
    }
private:
    std::stack<Node*> mParents;
    std::vector<Result> mResult;
    game::Transform mTransform;
};
} // namespace

namespace game
{

SceneNodeClass::ScriptVarValue* SceneNodeClass::FindScriptVarValueById(const std::string& id)
{
    for (auto& value : mScriptVarValues)
    {
        if (value.id == id)
            return &value;
    }
    return nullptr;
}
const SceneNodeClass::ScriptVarValue* SceneNodeClass::FindScriptVarValueById(const std::string& id) const
{
    for (auto& value : mScriptVarValues)
    {
        if (value.id == id)
            return &value;
    }
    return nullptr;
}

bool SceneNodeClass::DeleteScriptVarValueById(const std::string& id)
{
    for (auto it=mScriptVarValues.begin(); it != mScriptVarValues.end(); ++it)
    {
        const auto& val = *it;
        if (val.id == id)
        {
            mScriptVarValues.erase(it);
            return true;
        }
    }
    return false;
}

void SceneNodeClass::SetScriptVarValue(const ScriptVarValue& value)
{
    for (auto& val : mScriptVarValues)
    {
        if (val.id == value.id)
        {
            val.value = value.value;
            return;
        }
    }
    mScriptVarValues.push_back(value);
}

void SceneNodeClass::ClearStaleScriptValues(const EntityClass& klass)
{
    for (auto it=mScriptVarValues.begin(); it != mScriptVarValues.end();)
    {
        const auto& val = *it;
        const auto* var = klass.FindScriptVarById(val.id);
        if (var == nullptr) {
            it = mScriptVarValues.erase(it);
        } else if (ScriptVar::GetTypeFromVariant(val.value) != var->GetType()) {
            it = mScriptVarValues.erase(it);
        } else if(var->IsReadOnly()) {
            it = mScriptVarValues.erase(it);
        } else ++it;
    }
}

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
    for (const auto& value : mScriptVarValues) {
        hash = base::hash_combine(hash, value.id);
        hash = base::hash_combine(hash, value.value);
    }
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
    for (const auto& value : mScriptVarValues)
    {
        auto chunk = data.NewWriteChunk();
        chunk->Write("id", value.id);
        chunk->Write("value", value.value);
        data.AppendChunk("values", std::move(chunk));
    }
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
    for (unsigned i=0; i<data.GetNumChunks("values"); ++i)
    {
        const auto& chunk = data.GetReadChunk("values", i);
        ScriptVarValue value;
        chunk->Read("id", &value.id);
        chunk->Read("value", &value.value);
        ret.mScriptVarValues.push_back(std::move(value));
    }
    return ret;
}

SceneClass::SceneClass()
{
    mClassId = base::RandomString(10);
}

SceneClass::SceneClass(const SceneClass& other)
{
    std::unordered_map<const SceneNodeClass*, const SceneNodeClass*> map;

    mClassId    = other.mClassId;
    mName       = other.mName;
    mScriptFile = other.mScriptFile;
    mScriptVars = other.mScriptVars;
    mDynamicSpatialIndex = other.mDynamicSpatialIndex;
    mDynamicSpatialRect  = other.mDynamicSpatialRect;
    mDynamicSpatialIndexArgs = other.mDynamicSpatialIndexArgs;
    mLeftBoundary   = other.mLeftBoundary;
    mRightBoundary  = other.mRightBoundary;
    mTopBoundary    = other.mTopBoundary;
    mBottomBoundary = other.mBottomBoundary;

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
            entity.visual_entity = node->GetEntityClass();
            entity.entity_object = node;
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
            entity.visual_entity = node->GetEntityClass();
            entity.entity_object = node;
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
        if (!entity_node.visual_entity)
        {
            WARN("Node '%1' has no entity class object!", entity_node.entity_object->GetName());
            continue;
        }
        // transform the coordinate in the scene into the entity
        // coordinate space, then delegate the hit test to the
        // entity to see if we hit any of the entity nodes.
        auto scene_to_node = glm::inverse(entity_node.node_to_scene);
        auto node_hit_pos  = scene_to_node * glm::vec4(x, y, 1.0f, 1.0f);
        // perform entity hit test.
        std::vector<const EntityNodeClass*> nodes;
        entity_node.visual_entity->CoarseHitTest(node_hit_pos.x, node_hit_pos.y, &nodes);
        if (nodes.empty())
            continue;

        // hit some nodes so the entity as a whole is hit.
        hits->push_back(entity_node.entity_object);
        if (hitbox_positions)
            hitbox_positions->push_back(glm::vec2(node_hit_pos.x, node_hit_pos.y));
    }
}
void SceneClass::CoarseHitTest(const glm::vec2& pos, std::vector<SceneNodeClass*>* hits,
                               std::vector<glm::vec2>* hitbox_positions)
{
    CoarseHitTest(pos.x, pos.y, hits, hitbox_positions);
}

void SceneClass::CoarseHitTest(float x, float y, std::vector<const SceneNodeClass*>* hits,
                       std::vector<glm::vec2>* hitbox_positions) const
{
    const auto& entity_nodes = CollectNodes();
    for (const auto& entity_node : entity_nodes)
    {
        if (!entity_node.visual_entity)
        {
            WARN("Node '%1' has no entity class object!", entity_node.visual_entity->GetName());
            continue;
        }
        // transform the coordinate in the scene into the entity
        // coordinate space, then delegate the hit test to the
        // entity to see if we hit any of the entity nodes.
        auto scene_to_node = glm::inverse(entity_node.node_to_scene);
        auto node_hit_pos  = scene_to_node * glm::vec4(x, y, 1.0f, 1.0f);
        // perform entity hit test.
        std::vector<const EntityNodeClass*> nodes;
        entity_node.visual_entity->CoarseHitTest(node_hit_pos.x, node_hit_pos.y, &nodes);
        if (nodes.empty())
            continue;

        // hit some nodes so the entity as a whole is hit.
        hits->push_back(entity_node.entity_object);
        if (hitbox_positions)
            hitbox_positions->push_back(glm::vec2(node_hit_pos.x, node_hit_pos.y));
    }
}

void SceneClass::CoarseHitTest(const glm::vec2& pos, std::vector<const SceneNodeClass*>* hits,
                               std::vector<glm::vec2>* hitbox_positions) const
{
    CoarseHitTest(pos.x, pos.y, hits, hitbox_positions);
}

glm::vec2 SceneClass::MapCoordsFromNodeBox(float x, float y, const SceneNodeClass* node) const
{
    const auto& entity_nodes = CollectNodes();
    for (const auto& entity_node : entity_nodes)
    {
        if (entity_node.entity_object == node)
        {
            const auto ret = entity_node.node_to_scene * glm::vec4(x, y, 1.0f, 1.0f);
            return glm::vec2(ret.x, ret.y);
        }
    }
    // todo: should we return something else maybe ?
    return glm::vec2(0.0f, 0.0f);
}
glm::vec2 SceneClass::MapCoordsFromNodeBox(const glm::vec2& pos, const SceneNodeClass* node) const
{
    return MapCoordsFromNodeBox(pos.x, pos.y, node);
}

glm::vec2 SceneClass::MapCoordsToNodeBox(float x, float y, const SceneNodeClass* node) const
{
    const auto& entity_nodes = CollectNodes();
    for (const auto& entity_node : entity_nodes)
    {
        if (entity_node.entity_object == node)
        {
            const auto ret = glm::inverse(entity_node.node_to_scene) * glm::vec4(x, y, 1.0f, 1.0f);
            return glm::vec2(ret.x, ret.y);
        }
    }
    // todo: should we return something else maybe ?
    return glm::vec2(0.0f, 0.0f);
}
glm::vec2 SceneClass::MapCoordsToNodeBox(const glm::vec2& pos, const SceneNodeClass* node) const
{
    return MapCoordsToNodeBox(pos.x, pos.y, node);
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
ScriptVar* SceneClass::FindScriptVarByName(const std::string& name)
{
    for (auto& var : mScriptVars)
        if (var.GetName() == name)
            return &var;
    return nullptr;
}
ScriptVar* SceneClass::FindScriptVarById(const std::string& id)
{
    for (auto& var : mScriptVars)
        if (var.GetId() == id)
            return &var;
    return nullptr;
}
const ScriptVar& SceneClass::GetScriptVar(size_t index) const
{
    ASSERT(index <mScriptVars.size());
    return mScriptVars[index];
}
const ScriptVar* SceneClass::FindScriptVarByName(const std::string& name) const
{
    for (auto& var : mScriptVars)
        if (var.GetName() == name)
            return &var;
    return nullptr;
}
const ScriptVar* SceneClass::FindScriptVarById(const std::string& id) const
{
    for (auto& var : mScriptVars)
        if (var.GetId() == id)
            return &var;
    return nullptr;
}

size_t SceneClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mScriptFile);
    hash = base::hash_combine(hash, mDynamicSpatialIndex);
    hash = base::hash_combine(hash, mDynamicSpatialRect.has_value());
    hash = base::hash_combine(hash, mDynamicSpatialIndexArgs.has_value());
    if (const auto* ptr = GetDynamicSpatialRect())
    {
        hash = base::hash_combine(hash, *ptr);
    }
    if (const auto* ptr = GetQuadTreeArgs())
    {
        hash = base::hash_combine(hash, ptr->max_levels);
        hash = base::hash_combine(hash, ptr->max_items);
    }
    if (const auto* ptr = GetDenseGridArgs())
    {
        hash = base::hash_combine(hash, ptr->num_rows);
        hash = base::hash_combine(hash, ptr->num_cols);
    }
    hash = base::hash_combine(hash, mLeftBoundary);
    hash = base::hash_combine(hash, mRightBoundary);
    hash = base::hash_combine(hash, mTopBoundary);
    hash = base::hash_combine(hash, mBottomBoundary);

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

const FRect* SceneClass::GetDynamicSpatialRect() const
{
    if (!mDynamicSpatialRect.has_value())
        return nullptr;
    return &mDynamicSpatialRect.value();
}
const SceneClass::QuadTreeArgs* SceneClass::GetQuadTreeArgs() const
{
    if (!mDynamicSpatialIndexArgs.has_value())
        return nullptr;
    return std::get_if<QuadTreeArgs>(&mDynamicSpatialIndexArgs.value());
}
const SceneClass::DenseGridArgs* SceneClass::GetDenseGridArgs() const
{
    if (!mDynamicSpatialIndexArgs.has_value())
        return nullptr;
    return std::get_if<DenseGridArgs>(&mDynamicSpatialIndexArgs.value());
}

const float* SceneClass::GetLeftBoundary() const
{
    if (!mLeftBoundary.has_value())
        return nullptr;
    return &mLeftBoundary.value();
}
const float* SceneClass::GetRightBoundary() const
{
    if (!mRightBoundary.has_value())
        return nullptr;
    return &mRightBoundary.value();
}
const float* SceneClass::GetTopBoundary() const
{
    if (!mTopBoundary.has_value())
        return nullptr;
    return &mTopBoundary.value();
}
const float* SceneClass::GetBottomBoundary() const
{
    if (!mBottomBoundary.has_value())
        return nullptr;
    return &mBottomBoundary.value();
}

void SceneClass::SetDynamicSpatialIndex(SpatialIndex index)
{
    if (mDynamicSpatialIndex == index)
        return;

    if (index == SpatialIndex::Disabled)
    {
        mDynamicSpatialRect.reset();
        mDynamicSpatialIndexArgs.reset();
        mDynamicSpatialIndex = index;
        return;
    }

    if (index == SpatialIndex::QuadTree)
    {
        if (!GetQuadTreeArgs())
            mDynamicSpatialIndexArgs = QuadTreeArgs{};
    }
    else if (index == SpatialIndex::DenseGrid)
    {
        if (!GetDenseGridArgs())
            mDynamicSpatialIndexArgs = DenseGridArgs{};
    }
    if (!GetDynamicSpatialRect())
        mDynamicSpatialRect = FRect();

    mDynamicSpatialIndex = index;
}
void SceneClass::SetDynamicSpatialIndexArgs(const DenseGridArgs& args)
{
    ASSERT(mDynamicSpatialIndex == SpatialIndex::DenseGrid);
    mDynamicSpatialIndexArgs = args;
}
void SceneClass::SetDynamicSpatialIndexArgs(const QuadTreeArgs& args)
{
    ASSERT(mDynamicSpatialIndex == SpatialIndex::QuadTree);
    mDynamicSpatialIndexArgs = args;
}
void SceneClass::SetDynamicSpatialRect(const FRect& rect)
{
    ASSERT(mDynamicSpatialIndex != SpatialIndex::Disabled);
    mDynamicSpatialRect = rect;
}

void SceneClass::IntoJson(data::Writer& data) const
{
    data.Write("id", mClassId);
    data.Write("name", mName);
    data.Write("script_file", mScriptFile);
    data.Write("dynamic_spatial_index", mDynamicSpatialIndex);
    if (const auto* ptr = GetDynamicSpatialRect())
    {
        data.Write("dynamic_spatial_rect", *ptr);
    }
    if (const auto* ptr = GetQuadTreeArgs())
    {
        data.Write("quadtree_max_items", ptr->max_items);
        data.Write("quadtree_max_levels", ptr->max_levels);
    }
    if (const auto* ptr = GetDenseGridArgs())
    {
        data.Write("dense_grid_rows", ptr->num_rows);
        data.Write("dense_grid_cols", ptr->num_cols);
    }
    data.Write("left_boundary",   mLeftBoundary);
    data.Write("right_boundary",  mRightBoundary);
    data.Write("top_boundary",    mTopBoundary);
    data.Write("bottom_boundary", mBottomBoundary);

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
    RenderTreeIntoJson(mRenderTree, &game::TreeNodeToJson<SceneNodeClass>, *chunk);
    data.Write("render_tree", std::move(chunk));
}

// static
std::optional<SceneClass> SceneClass::FromJson(const data::Reader& data)
{
    SceneClass ret;
    if (!data.Read("id", &ret.mClassId) ||
        !data.Read("name", &ret.mName) ||
        !data.Read("script_file", &ret.mScriptFile) ||
        !data.Read("dynamic_spatial_index", &ret.mDynamicSpatialIndex))
        return std::nullopt;
    if (data.HasValue("dynamic_spatial_rect"))
    {
        base::FRect rect;
        if (!data.Read("dynamic_spatial_rect", &rect))
            return std::nullopt;
        ret.mDynamicSpatialRect = rect;
    }
    if (ret.mDynamicSpatialIndex == SpatialIndex::QuadTree)
    {
        QuadTreeArgs quadtree_args;
        if (data.Read("quadtree_max_items", &quadtree_args.max_items) &&
            data.Read("quadtree_max_levels", &quadtree_args.max_levels))
            ret.mDynamicSpatialIndexArgs = quadtree_args;
    }

    if (ret.mDynamicSpatialIndex == SpatialIndex::DenseGrid)
    {
        DenseGridArgs densegrid_args;
        if (data.Read("dense_grid_rows", &densegrid_args.num_rows) &&
            data.Read("dense_grid_cols", &densegrid_args.num_cols))
            ret.mDynamicSpatialIndexArgs = densegrid_args;
    }
    data.Read("left_boundary",   &ret.mLeftBoundary);
    data.Read("right_boundary",  &ret.mRightBoundary);
    data.Read("top_boundary",    &ret.mTopBoundary);
    data.Read("bottom_boundary", &ret.mBottomBoundary);

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
    RenderTreeFromJson(ret.mRenderTree, game::TreeNodeFromJson(ret.mNodes), *chunk);
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
    ret.mDynamicSpatialRect = mDynamicSpatialRect;
    ret.mDynamicSpatialIndex = mDynamicSpatialIndex;
    ret.mDynamicSpatialIndexArgs = mDynamicSpatialIndexArgs;
    ret.mLeftBoundary = mLeftBoundary;
    ret.mRightBoundary = mRightBoundary;
    ret.mTopBoundary = mTopBoundary;
    ret.mBottomBoundary = mBottomBoundary;
    return ret;
}

SceneClass& SceneClass::operator=(const SceneClass& other)
{
    if (this == &other)
        return *this;

    SceneClass tmp(other);
    mClassId                 = std::move(tmp.mClassId);
    mScriptFile              = std::move(tmp.mScriptFile);
    mName                    = std::move(tmp.mName);
    mNodes                   = std::move(tmp.mNodes);
    mScriptVars              = std::move(tmp.mScriptVars);
    mRenderTree              = std::move(tmp.mRenderTree);
    mDynamicSpatialIndex     = std::move(tmp.mDynamicSpatialIndex);
    mDynamicSpatialRect      = std::move(tmp.mDynamicSpatialRect);
    mDynamicSpatialIndexArgs = std::move(tmp.mDynamicSpatialIndexArgs);
    mLeftBoundary            = std::move(tmp.mLeftBoundary);
    mRightBoundary           = std::move(tmp.mRightBoundary);
    mTopBoundary             = std::move(tmp.mTopBoundary);
    mBottomBoundary          = std::move(tmp.mBottomBoundary);
    return *this;
}

Scene::Scene(std::shared_ptr<const SceneClass> klass)
  : mClass(klass)
{
    std::unordered_map<const SceneNodeClass*, const Entity*> map;

    bool spatial_nodes = false;

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

        for (size_t j=0; j<entity->GetNumNodes(); ++j)
        {
            const auto& node = entity->GetNode(j);
            if (node->HasSpatialNode())
                spatial_nodes = true;
        }

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

        // set the entity script variable values
        for (size_t i=0; i<node.GetNumScriptVarValues(); ++i)
        {
            const auto& val = node.GetScriptVarValue(i);
            const auto* var = entity->FindScriptVarById(val.id);
            // deal with potentially stale data in the scene node.
            if (var == nullptr)
            {
                WARN("SceneNode '%1' refers to entity script variable '%2' that no longer exists.", node.GetName(), val.id);
                continue;
            }
            else if (ScriptVar::GetTypeFromVariant(val.value) != var->GetType())
            {
                WARN("SceneNode '%1' refers to entity script variable '%2' with incorrect type.", node.GetName(), val.id);
                continue;
            }
            else if (var->IsReadOnly())
            {
                WARN("SceneNode '%1' tries to set a read only script variable '%1'.", node.GetName(), var->GetName());
                continue;
            }
            std::visit([var](const auto& variant_value) {
                var->SetValue(variant_value);
            }, val.value);
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

    const auto index = mClass->GetDynamicSpatialIndex();
    if (index == SceneClass::SpatialIndex::QuadTree)
    {
        const auto* rect = mClass->GetDynamicSpatialRect();
        const auto* args = mClass->GetQuadTreeArgs();
        ASSERT(args && rect);
        mSpatialIndex.reset(new QuadTreeIndex<EntityNode>(*rect, args->max_items, args->max_levels));
        DEBUG("Created scene spatial index. [type=%1, max_items=%2, max_levels=%3]",
              index, args->max_items, args->max_levels);
    }
    else if (index == SceneClass::SpatialIndex::DenseGrid)
    {
        const auto* rect = mClass->GetDynamicSpatialRect();
        const auto* args = mClass->GetDenseGridArgs();
        ASSERT(args && rect);
        mSpatialIndex.reset(new DenseGridIndex<EntityNode>(*rect, args->num_rows, args->num_cols));
        DEBUG("Created scene spatial index. [type=%1, rows=%2, cols=%3]",
              index, args->num_rows, args->num_cols);
    }
    if (spatial_nodes && !mSpatialIndex)
    {
        WARN("Scene entities have spatial nodes but scene has no spatial index set.\n"
             "Spatial indexing and spatial queries will not work.\n"
             "You can enable spatial indexing in the scene editor.");
    }
}

Scene::Scene(const SceneClass& klass) : Scene(std::make_shared<SceneClass>(klass))
{}

Scene::~Scene() = default;

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
    // if the entity still exists but it has been killed
    // it means it'll be deleted at the end of this loop iteration
    // in EndLoop. Make sure not to add it again in the
    // kill set for next iteration even if the application tries
    // to kill it again.
    if (entity->HasBeenKilled())
        return;

    mKillSet.insert(entity);
}

Entity* Scene::SpawnEntity(const EntityArgs& args, bool link_to_root)
{
    ASSERT(args.klass);
    // we must have the klass of the entity and an id.
    // the invariant that must hold is that entity IDs are
    // always going to be unique.
    auto instance = CreateEntityInstance(args);

    ASSERT(mIdMap.find(instance->GetId()) == mIdMap.end());

    mSpawnList.push_back(std::move(instance));
    if (args.enable_logging)
        DEBUG("New entity '%1/%2'", args.klass->GetName(), args.name);

    return mSpawnList.back().get();
}

void Scene::BeginLoop()
{
    // turn on the kill flag for entities that were killed
    // during the last iteration of the game play.
    for (auto* entity : mKillSet)
    {
        // Set entity kill flag to indicate that it's been killed from the scene.
        // Note that this isn't the same as the c++ lifetime of the object!
        entity->SetFlag(Entity::ControlFlags::Killed, true);
        // propagate the kill flag to children when a parent
        // entity is killed. if this is not desired then one
        // should have unlinked the children first.
        mRenderTree.PreOrderTraverseForEach([](Entity* entity) {
            entity->SetFlag(Entity::ControlFlags::Killed, true);
            if (entity->TestFlag(Entity::ControlFlags::EnableLogging))
                DEBUG("Entity '%1/%2' was killed", entity->GetClassName(), entity->GetName());
        }, entity);
    }
    for (auto& entity : mSpawnList)
    {
        if (entity->TestFlag(Entity::ControlFlags::EnableLogging))
            DEBUG("Entity '%1/%2' was spawned.", entity->GetClassName(), entity->GetName());
        entity->SetFlag(Entity::ControlFlags::Spawned, true);
        mIdMap[entity->GetId()]     = entity.get();
        mNameMap[entity->GetName()] = entity.get();
        mRenderTree.LinkChild(nullptr, entity.get());
        mEntities.push_back(std::move(entity));
    }
    mKillSet.clear();
    mSpawnList.clear();
}

void Scene::EndLoop()
{
    std::set<EntityNode*> killed_spatial_nodes;

    for (auto& entity : mEntities)
    {
        // turn off spawn flags.
        entity->SetFlag(Entity::ControlFlags::Spawned, false);

        if (!entity->TestFlag(Entity::ControlFlags::Killed))
            continue;
        if (entity->TestFlag(Entity::ControlFlags::EnableLogging))
            DEBUG("Entity '%1/%2' was deleted.", entity->GetClassName(), entity->GetName());
        mRenderTree.DeleteNode(entity.get());
        mIdMap.erase(entity->GetId());
        mNameMap.erase(entity->GetName());

        if (mSpatialIndex)
        {
            for (size_t i = 0; i < entity->GetNumNodes(); ++i)
            {
                auto& node = entity->GetNode(i);
                if (node.HasSpatialNode())
                    killed_spatial_nodes.insert(&node);
            }
        }
    }
    if (mSpatialIndex)
        mSpatialIndex->Erase(killed_spatial_nodes);

    // delete the entities that were killed from the container
    mEntities.erase(std::remove_if(mEntities.begin(), mEntities.end(), [](const auto& entity) {
        return entity->TestFlag(Entity::ControlFlags::Killed);
    }), mEntities.end());
}

std::vector<Scene::ConstSceneNode> Scene::CollectNodes() const
{
    std::vector<ConstSceneNode> ret;
    for (auto& entity : mEntities)
    {
        ConstSceneNode node;
        node.node_to_scene = FindEntityTransform(entity.get());
        node.visual_entity = entity.get();
        ret.push_back(std::move(node));
    }
    return ret;
}

std::vector<Scene::SceneNode> Scene::CollectNodes()
{
    std::vector<SceneNode> ret;
    for (auto& entity : mEntities)
    {
        SceneNode node;
        node.node_to_scene = FindEntityTransform(entity.get());
        node.visual_entity = entity.get();
        ret.push_back(std::move(node));
    }
    return ret;
}

glm::mat4 Scene::FindEntityTransform(const Entity* entity) const
{
    // if the parent of this entity is the root node
    // then the matrix will be simply identity
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

glm::vec2 Scene::MapVectorFromEntityNode(const Entity* entity, const EntityNode* node, const glm::vec2& vector) const
{
    const auto& mat = FindEntityNodeTransform(entity, node);
    // we assume here that the vector represents a direction
    // thus translation is not desired. hence, _w = 0.0f.
    const auto& ret = mat * glm::vec4(vector, 1.0f, 0.0f);
    // return normalized result.
    return glm::normalize(glm::vec2(ret.x, ret.y));
}

FPoint Scene::MapPointFromEntityNode(const Entity* entity, const EntityNode* node, const FPoint& point) const
{
    const auto& mat = FindEntityNodeTransform(entity, node);
    const auto& ret = mat * glm::vec4(point.GetY(), point.GetY(), 1.0f, 1.0f);
    return FPoint(ret.x, ret.y);
}

const ScriptVar* Scene::FindScriptVarByName(const std::string& name) const
{
    // first check the mutable variables per this instance then check the class.
    for (const auto& var : mScriptVars)
    {
        if (var.GetName() == name)
            return &var;
    }
    return mClass->FindScriptVarByName(name);
}
const ScriptVar* Scene::FindScriptVarById(const std::string& id) const
{
    // first check the mutable variables per this instance then check the class.
    for (const auto& var : mScriptVars)
    {
        if (var.GetId() == id)
            return &var;
    }
    return mClass->FindScriptVarById(id);
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

void Scene::Rebuild()
{
    using SpatialIndex = Scene::SpatialIndex;

    const auto* left_boundary  = mClass->GetLeftBoundary();
    const auto* right_boundary = mClass->GetRightBoundary();
    const auto* top_boundary   = mClass->GetTopBoundary();
    const auto* bottom_boundary = mClass->GetBottomBoundary();

    if (!mSpatialIndex &&
        !left_boundary && !right_boundary &&
        !top_boundary && !bottom_boundary)
        return;

    if (mSpatialIndex)
        mSpatialIndex->BeginInsert();

    // iterate over the render tree and look for
    // entity nodes that have SpatialNode attachment.
    // for the nodes with spatial node compute the nodes
    // AABB and place the node into the spatial index.
    class Visitor : public RenderTree::Visitor {
    public:
        Visitor(SpatialIndex* index, Scene* scene, float left, float right, float top, float bottom)
            : mLeftBound(left)
            , mRightBound(right)
            , mTopBound(top)
            , mBottomBound(bottom)
            , mIndex(index)
            , mScene(scene)
        {}

        virtual void EnterNode(Entity* entity) override
        {
            if (!entity)
                return;

            if (const auto* parent = GetParent())
            {
                const auto* parent_node = parent->FindNodeByClassId(entity->GetParentNodeClassId());
                mTransform.Push(parent->FindNodeTransform(parent_node));
            }

            mParents.push(entity);

            // if the entity has no spatial nodes and is not expected
            // to be killed at the scene boundary then the rest of the
            // work can be skipped.
            if (!entity->HasSpatialNodes() &&
                (!entity->KillAtBoundary() || entity->HasBeenKilled()))
                return;

            FRect rect;
            for (size_t i=0; i<entity->GetNumNodes(); ++i)
            {
                const auto& node = entity->GetNode(i);
                mTransform.Push(entity->FindNodeModelTransform(&node));
                const auto& aabb = ComputeBoundingRect(mTransform.GetAsMatrix());
                if (const auto* spatial = node.GetSpatialNode())
                {
                    if (spatial->GetShape() == SpatialNode::Shape::AABB)
                    {
                        if (mIndex)
                            mIndex->Insert(aabb, const_cast<EntityNode*>(&node));
                    } else BUG("Unimplemented spatial shape insertion.");
                }
                rect = base::Union(rect, aabb);
                mTransform.Pop();
            }
            // if the entity has already been killed there's no point
            // to test whether it should be killed if it has gone
            // beyond the boundaries.
            if (entity->HasBeenKilled())
                return;
            // if the entity doesn't enable boundary killing skip boundary testing.
            if (!entity->KillAtBoundary())
                return;

            // check against the scene's boundary values.
            const double left   = double(rect.GetX());
            const double right  = double(rect.GetX()) + rect.GetWidth();
            const double top    = double(rect.GetY());
            const double bottom = double(rect.GetY()) + rect.GetHeight();
            if ((left > mRightBound) || (right < mLeftBound) ||
                (top > mBottomBound) || (bottom < mTopBound))
            {
                mScene->mKillSet.insert(entity);
            }
        }
        virtual void LeaveNode(Entity* entity) override
        {
            if (!entity)
                return;
            mParents.pop();
            if (const auto* parent = GetParent())
                mTransform.Pop();
        }
    private:
        Entity* GetParent() const
        {
            if (mParents.empty())
                return nullptr;
            return mParents.top();
        }
    private:
        const double mLeftBound = 0.0f;
        const double mRightBound = 0.0f;
        const double mTopBound   = 0.0f;
        const double mBottomBound = 0.0f;
        std::stack<Entity*> mParents;
        Transform mTransform;
        SpatialIndex* mIndex = nullptr;
        Scene* mScene = nullptr;
    };

    const auto left_boundary_value =  left_boundary
        ? *left_boundary : std::numeric_limits<float>::lowest();
    const auto right_boundary_value = right_boundary
        ? *right_boundary : std::numeric_limits<float>::max();
    const auto top_boundary_value = top_boundary
        ? *top_boundary : std::numeric_limits<float>::lowest();
    const auto bottom_boundary_value = bottom_boundary
        ? *bottom_boundary : std::numeric_limits<float>::max();

    Visitor visitor(mSpatialIndex.get(), this,
        left_boundary_value, right_boundary_value,
        top_boundary_value, bottom_boundary_value);
    mRenderTree.PreOrderTraverse(visitor);

    if (mSpatialIndex)
        mSpatialIndex->EndInsert();
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
