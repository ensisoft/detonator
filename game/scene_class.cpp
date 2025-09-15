// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#include <unordered_map>

#include "base/assert.h"
#include "base/hash.h"
#include "base/logging.h"
#include "data/reader.h"
#include "data/writer.h"
#include "game/treeop.h"
#include "game/scene_class.h"
#include "game/entity_class.h"

namespace game
{

SceneClass::SceneClass(std::string id)
  : mClassId(std::move(id))
{}

SceneClass::SceneClass() : SceneClass(base::RandomString(10))
{}

SceneClass::SceneClass(const SceneClass& other)
{
    std::unordered_map<const EntityPlacement*, const EntityPlacement*> map;

    mClassId                 = other.mClassId;
    mName                    = other.mName;
    mTilemap                 = other.mTilemap;
    mScriptFile              = other.mScriptFile;
    mScriptVars              = other.mScriptVars;
    mDynamicSpatialIndex     = other.mDynamicSpatialIndex;
    mDynamicSpatialIndexArgs = other.mDynamicSpatialIndexArgs;
    mLeftBoundary            = other.mLeftBoundary;
    mRightBoundary           = other.mRightBoundary;
    mTopBoundary             = other.mTopBoundary;
    mBottomBoundary          = other.mBottomBoundary;
    mRenderingArgs           = other.mRenderingArgs;

    for (const auto& node : other.mNodes)
    {
        auto copy = std::make_unique<EntityPlacement>(*node);
        map[node.get()] = copy.get();
        mNodes.push_back(std::move(copy));
    }
    mRenderTree.FromTree(other.mRenderTree, [&map](const EntityPlacement* node) {
        return map[node];
    });
}
EntityPlacement* SceneClass::PlaceEntity(const EntityPlacement& placement)
{
    mNodes.emplace_back(new EntityPlacement(placement));
    return mNodes.back().get();
}
EntityPlacement* SceneClass::PlaceEntity(EntityPlacement&& placement)
{
    mNodes.emplace_back(new EntityPlacement(std::move(placement)));
    return mNodes.back().get();
}
EntityPlacement* SceneClass::PlaceEntity(std::unique_ptr<EntityPlacement> placement)
{
    mNodes.push_back(std::move(placement));
    return mNodes.back().get();
}
EntityPlacement& SceneClass::GetPlacement(size_t index)
{
    ASSERT(index < mNodes.size());
    return *mNodes[index].get();
}
EntityPlacement* SceneClass::FindPlacementByName(const std::string& name)
{
    for (const auto& node : mNodes)
        if (node->GetName() == name)
            return node.get();
    return nullptr;
}
EntityPlacement* SceneClass::FindPlacementById(const std::string& id)
{
    for (const auto& node : mNodes)
        if (node->GetId() == id)
            return node.get();
    return nullptr;
}
const EntityPlacement& SceneClass::GetPlacement(size_t index) const
{
    ASSERT(index < mNodes.size());
    return *mNodes[index].get();
}
const EntityPlacement* SceneClass::FindPlacementByName(const std::string& name) const
{
    for (const auto& node : mNodes)
        if (node->GetName() == name)
            return node.get();
    return nullptr;
}
const EntityPlacement* SceneClass::FindPlacementById(const std::string& id) const
{
    for (const auto& node : mNodes)
        if (node->GetId() == id)
            return node.get();
    return nullptr;
}

void SceneClass::LinkChild(EntityPlacement* parent, EntityPlacement* child)
{
    game::LinkChild(mRenderTree, parent, child);
}

void SceneClass::BreakChild(EntityPlacement* child, bool keep_world_transform)
{
    game::BreakChild(mRenderTree, child, keep_world_transform);
}

void SceneClass::ReparentChild(EntityPlacement* parent, EntityPlacement* child, bool keep_world_transform)
{
    game::ReparentChild(mRenderTree, parent, child, keep_world_transform);
}

void SceneClass::DeletePlacement(EntityPlacement* placement)
{
    game::DeleteNode(mRenderTree, placement, mNodes);
}

EntityPlacement* SceneClass::DuplicatePlacement(const EntityPlacement* placement)
{
    return game::DuplicateNode(mRenderTree, placement, &mNodes);
}

std::vector<SceneClass::ConstSceneNode> SceneClass::CollectNodes() const
{
    std::vector<SceneClass::ConstSceneNode> ret;

    // visit the entire render tree of the scene and transform every
    // EntityPlacement (which is basically a placement for an entity in the
    // scene) into world.
    // todo: this needs some kind of space partitioning which allows the
    // collection to only consider some nodes that lie within some area
    // of interest.

    class Visitor : public RenderTree::ConstVisitor {
    public:
        Visitor(std::vector<SceneClass::ConstSceneNode>& result) : mResult(result)
        {}
        virtual void EnterNode(const EntityPlacement* node) override
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
            entity.entity = node->GetEntityClass();
            entity.placement = node;
            mResult.push_back(std::move(entity));
        }
        virtual void LeaveNode(const EntityPlacement* node) override
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
        const EntityPlacement* GetParent() const
        {
            if (mParents.empty())
                return nullptr;
            return mParents.top();
        }
    private:
        std::stack<const EntityPlacement*> mParents;
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
    // EntityPlacement (which is basically a placement for an entity in the
    // scene) into world.
    // todo: this needs some kind of space partitioning which allows the
    // collection to only consider some nodes that lie within some area
    // of interest.

    class Visitor : public RenderTree::Visitor {
    public:
        Visitor(std::vector<SceneClass::SceneNode>& result) : mResult(result)
        {}
        virtual void EnterNode(EntityPlacement* node) override
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
            entity.entity = node->GetEntityClass();
            entity.placement = node;
            mResult.push_back(std::move(entity));
        }
        virtual void LeaveNode(EntityPlacement* node) override
        {
            if (!node)
                return;
            mTransform.Pop();
            mTransform.Pop();
            mParents.pop();
        }
    private:
        EntityPlacement* GetParent() const
        {
            if (mParents.empty())
                return nullptr;
            return mParents.top();
        }
    private:
        std::stack<EntityPlacement*> mParents;
        std::vector<SceneClass::SceneNode>& mResult;
        Transform mTransform;
    };
    Visitor visitor(ret);
    mRenderTree.PreOrderTraverse(visitor);
    return ret;
}

void SceneClass::CoarseHitTest(const Float2& point, std::vector<EntityPlacement*>* hits,
                   std::vector<glm::vec2>* hitbox_positions)
{
    const auto& entity_nodes = CollectNodes();
    for (const auto& entity_node : entity_nodes)
    {
        if (!entity_node.entity)
        {
            WARN("Node '%1' has no entity class object!", entity_node.placement->GetName());
            continue;
        }
        // transform the coordinate in the scene into the entity
        // coordinate space, then delegate the hit test to the
        // entity to see if we hit any of the entity nodes.
        auto scene_to_node = glm::inverse(entity_node.node_to_scene);
        auto node_hit_pos  = scene_to_node * glm::vec4(point.x, point.y, 1.0f, 1.0f);
        // perform entity hit test.
        std::vector<const EntityNodeClass*> nodes;
        entity_node.entity->CoarseHitTest(node_hit_pos, &nodes);
        if (nodes.empty())
            continue;

        // hit some nodes so the entity as a whole is hit.
        hits->push_back(entity_node.placement);
        if (hitbox_positions)
            hitbox_positions->push_back(glm::vec2(node_hit_pos.x, node_hit_pos.y));
    }
}

void SceneClass::CoarseHitTest(const Float2& point, std::vector<const EntityPlacement*>* hits,
                       std::vector<glm::vec2>* hitbox_positions) const
{
    const auto& entity_nodes = CollectNodes();
    for (const auto& entity_node : entity_nodes)
    {
        if (!entity_node.entity)
        {
            WARN("Node '%1' has no entity class object!", entity_node.entity->GetName());
            continue;
        }
        // transform the coordinate in the scene into the entity
        // coordinate space, then delegate the hit test to the
        // entity to see if we hit any of the entity nodes.
        auto scene_to_node = glm::inverse(entity_node.node_to_scene);
        auto node_hit_pos  = scene_to_node * glm::vec4(point.x, point.y, 1.0f, 1.0f);
        // perform entity hit test.
        std::vector<const EntityNodeClass*> nodes;
        entity_node.entity->CoarseHitTest(node_hit_pos, &nodes);
        if (nodes.empty())
            continue;

        // hit some nodes so the entity as a whole is hit.
        hits->push_back(entity_node.placement);
        if (hitbox_positions)
            hitbox_positions->push_back(glm::vec2(node_hit_pos.x, node_hit_pos.y));
    }
}

Float2 SceneClass::MapCoordsFromNodeBox(const Float2& coordinates, const EntityPlacement* node) const
{
    const auto& entity_nodes = CollectNodes();
    for (const auto& entity_node : entity_nodes)
    {
        if (entity_node.placement == node)
        {
            const auto ret = entity_node.node_to_scene * glm::vec4(coordinates.x, coordinates.y, 1.0f, 1.0f);
            return glm::vec2(ret.x, ret.y);
        }
    }
    // todo: should we return something else maybe ?
    return {0.0f, 0.0f};
}

Float2 SceneClass::MapCoordsToNodeBox(const Float2& coordinates, const EntityPlacement* node) const
{
    const auto& entity_nodes = CollectNodes();
    for (const auto& entity_node : entity_nodes)
    {
        if (entity_node.placement == node)
        {
            const auto ret = glm::inverse(entity_node.node_to_scene) * glm::vec4(coordinates.x, coordinates.y, 1.0f, 1.0f);
            return glm::vec2(ret.x, ret.y);
        }
    }
    // todo: should we return something else maybe ?
    return {0.0f, 0.0f};
}

glm::mat4 SceneClass::FindEntityTransform(const EntityPlacement* placement) const
{
    return game::FindNodeTransform(mRenderTree, placement);
}

FRect SceneClass::FindEntityBoundingRect(const EntityPlacement* placement) const
{
    FRect  ret;
    Transform transform(FindEntityTransform(placement));

    const auto& entity = placement->GetEntityClass();
    ASSERT(entity);
    for (size_t i=0; i<entity->GetNumNodes(); ++i)
    {
        const auto& node = entity->GetNode(i);
        transform.Push(entity->FindNodeTransform(&node));
          transform.Push(node.GetModelTransform());
          ret = Union(ret, ComputeBoundingRect(transform));
          transform.Pop();
       transform.Pop();
    }
    return ret;
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
    hash = base::hash_combine(hash, mTilemap);
    hash = base::hash_combine(hash, mDynamicSpatialIndex);
    hash = base::hash_combine(hash, mDynamicSpatialIndexArgs);

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
    hash = base::hash_combine(hash, mRenderingArgs.projection);
    hash = base::hash_combine(hash, mRenderingArgs.shading);
    hash = base::hash_combine(hash, mRenderingArgs.bloom);
    hash = base::hash_combine(hash, mRenderingArgs.fog);

    // include the node hashes in the animation hash
    // this covers both the node values and their traversal order
    mRenderTree.PreOrderTraverseForEach([&](const EntityPlacement* node) {
        if (node == nullptr)
            return;
        hash = base::hash_combine(hash, node->GetHash());
    });
    for (const auto& var : mScriptVars)
        hash = base::hash_combine(hash, var.GetHash());
    return hash;
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
void SceneClass::IntoJson(data::Writer& data) const
{
    data.Write("id", mClassId);
    data.Write("name", mName);
    data.Write("script_file", mScriptFile);
    data.Write("tilemap", mTilemap);
    data.Write("dynamic_spatial_index", mDynamicSpatialIndex);
    data.Write("shading", mRenderingArgs.shading);
    data.Write("projection", mRenderingArgs.projection);
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
    if (const auto* bloom = GetBloom())
    {
        auto chunk = data.NewWriteChunk();
        chunk->Write("threshold", bloom->threshold);
        chunk->Write("red", bloom->red);
        chunk->Write("green", bloom->green);
        chunk->Write("blue",  bloom->blue);
        data.Write("bloom", std::move(chunk));
    }
    if (const auto* fog = GetFog())
    {
        auto chunk = data.NewWriteChunk();
        chunk->Write("mode", fog->mode);
        chunk->Write("start_distance", fog->start_dist);
        chunk->Write("end_distance", fog->end_dist);
        chunk->Write("density", fog->density);
        chunk->Write("color", fog->color);
        data.Write("fog", std::move(chunk));
    }

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
    RenderTreeIntoJson(mRenderTree, &game::TreeNodeToJson<EntityPlacement>, *chunk);
    data.Write("render_tree", std::move(chunk));
}

bool SceneClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",                    &mClassId);
    ok &= data.Read("name",                  &mName);
    ok &= data.Read("script_file",           &mScriptFile);
    ok &= data.Read("tilemap",               &mTilemap);
    ok &= data.Read("dynamic_spatial_index", &mDynamicSpatialIndex);
    ok &= data.Read("left_boundary",         &mLeftBoundary);
    ok &= data.Read("right_boundary",        &mRightBoundary);
    ok &= data.Read("top_boundary",          &mTopBoundary);
    ok &= data.Read("bottom_boundary",       &mBottomBoundary);
    ok &= data.Read("shading",               &mRenderingArgs.shading);
    ok &= data.Read("projection",            &mRenderingArgs.projection);

    if (data.HasValue("bloom"))
    {
        BloomFilter bloom;
        const auto& chunk = data.GetReadChunk("bloom");
        if (chunk->Read("threshold", &bloom.threshold) &&
            chunk->Read("red",       &bloom.red) &&
            chunk->Read("green",     &bloom.green) &&
            chunk->Read("blue",      &bloom.blue))
            mRenderingArgs.bloom = bloom;
        else WARN("Failed to load scene bloom filter properties. [scene='%1']", mName);
    }
    if (data.HasValue("fog"))
    {
        Fog fog;
        const auto& chunk = data.GetReadChunk("fog");
        if (chunk->Read("mode",           &fog.mode) &&
            chunk->Read("start_distance", &fog.start_dist) &&
            chunk->Read("end_distance",   &fog.end_dist) &&
            chunk->Read("density",        &fog.density) &&
            chunk->Read("color",          &fog.color))
            mRenderingArgs.fog = fog;
        else WARN("Failed to load scene fog properties. [scene='%1']", mName);
    }

    if (mDynamicSpatialIndex == SpatialIndex::QuadTree)
    {
        QuadTreeArgs quadtree_args;
        if (data.Read("quadtree_max_items", &quadtree_args.max_items) &&
            data.Read("quadtree_max_levels", &quadtree_args.max_levels))
            mDynamicSpatialIndexArgs = quadtree_args;
        else WARN("Failed to load scene quadtree property. [scene='%1']", mName);
    }
    if (mDynamicSpatialIndex == SpatialIndex::DenseGrid)
    {
        DenseGridArgs densegrid_args;
        if (data.Read("dense_grid_rows", &densegrid_args.num_rows) &&
            data.Read("dense_grid_cols", &densegrid_args.num_cols))
            mDynamicSpatialIndexArgs = densegrid_args;
        else WARN("Failed to load scene spatial dense grid property. [scene='%1']", mName);
    }

    for (unsigned i=0; i<data.GetNumChunks("nodes"); ++i)
    {
        const auto& chunk = data.GetReadChunk("nodes", i);
        auto node = std::make_unique<EntityPlacement>();
        if (!node->FromJson(*chunk))
            WARN("Failed to load scene node. [scene='%1', node='%2']", mName, node->GetName());

        mNodes.push_back(std::move(node));
    }
    for (unsigned i=0; i<data.GetNumChunks("vars"); ++i)
    {
        const auto& chunk = data.GetReadChunk("vars", i);
        ScriptVar var;
        if (var.FromJson(*chunk))
            mScriptVars.push_back(std::move(var));
        else WARN("Failed to load scene script variable. [scene='%1', var='%2']", mName, var.GetName());
    }
    const auto& chunk = data.GetReadChunk("render_tree");
    if (!chunk)
        return false;

    RenderTreeFromJson(mRenderTree, game::TreeNodeFromJson(mNodes), *chunk);
    return ok;
}
SceneClass SceneClass::Clone() const
{
    SceneClass ret;

    std::unordered_map<const EntityPlacement*, const EntityPlacement*> map;

    // make a deep copy of the nodes.
    for (const auto& node : mNodes)
    {
        auto clone = std::make_unique<EntityPlacement>(*node);
        map[node.get()] = clone.get();
        ret.mNodes.push_back(std::move(clone));
    }

    // remap entity reference variables
    for (const auto& var : mScriptVars)
    {
        if (var.GetType() == ScriptVar::Type::EntityReference)
        {
            std::vector<ScriptVar::EntityReference> refs;
            const auto& src_arr = var.GetArray<ScriptVar::EntityReference>();
            for (const auto& src_ref : src_arr)
            {
                const auto* src_node = FindPlacementById(src_ref.id);
                const auto* dst_node = map[src_node];
                ScriptVar::EntityReference ref;
                ref.id = dst_node ? dst_node->GetId() : "";
                refs.push_back(std::move(ref));

                ScriptVar v;
                v.SetName(var.GetName());
                v.SetReadOnly(var.IsReadOnly());
                v.SetArray(var.IsArray());
                v.SetNewArrayType(std::move(refs));
                ret.mScriptVars.push_back(std::move(v));
            }
        } else ret.mScriptVars.push_back(var);
    }

    ret.mScriptFile              = mScriptFile;
    ret.mTilemap                 = mTilemap;
    ret.mName                    = mName;
    ret.mDynamicSpatialIndex     = mDynamicSpatialIndex;
    ret.mDynamicSpatialIndexArgs = mDynamicSpatialIndexArgs;
    ret.mLeftBoundary            = mLeftBoundary;
    ret.mRightBoundary           = mRightBoundary;
    ret.mTopBoundary             = mTopBoundary;
    ret.mBottomBoundary          = mBottomBoundary;
    ret.mRenderingArgs           = mRenderingArgs;
    ret.mRenderTree.FromTree(mRenderTree, [&map](const EntityPlacement* node) {
        return map[node];
    });
    return ret;
}

SceneClass& SceneClass::operator=(const SceneClass& other)
{
    if (this == &other)
        return *this;

    SceneClass tmp(other);
    mClassId                 = std::move(tmp.mClassId);
    mScriptFile              = std::move(tmp.mScriptFile);
    mTilemap                 = std::move(tmp.mTilemap);
    mName                    = std::move(tmp.mName);
    mNodes                   = std::move(tmp.mNodes);
    mScriptVars              = std::move(tmp.mScriptVars);
    mRenderTree              = std::move(tmp.mRenderTree);
    mDynamicSpatialIndex     = std::move(tmp.mDynamicSpatialIndex);
    mDynamicSpatialIndexArgs = std::move(tmp.mDynamicSpatialIndexArgs);
    mLeftBoundary            = std::move(tmp.mLeftBoundary);
    mRightBoundary           = std::move(tmp.mRightBoundary);
    mTopBoundary             = std::move(tmp.mTopBoundary);
    mBottomBoundary          = std::move(tmp.mBottomBoundary);
    mRenderingArgs           = std::move(tmp.mRenderingArgs);
    return *this;
}


} // namespace
