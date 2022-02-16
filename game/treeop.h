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
#  include <glm/glm.hpp> // for glm::inverse
#include "warnpop.h"

#include <stack>
#include <vector>
#include <unordered_set>

#include "base/assert.h"
#include "base/format.h"
#include "base/treeop.h"
#include "data/reader.h"
#include "data/writer.h"
#include "game/types.h"
#include "game/transform.h"
#include "game/tree.h"
#include "game/types.h"
#include "game/util.h"

// collection of algorithms that operate on a render tree.

namespace game
{

using base::SearchChild;
using base::SearchParent;
using base::QueryQuadTree;

template<typename Node>
class TreeNodeFromJson
{
public:
    using UniqueNodeList = std::vector<std::unique_ptr<Node>>;
    using SharedNodeList = std::vector<std::shared_ptr<Node>>;
    TreeNodeFromJson(const UniqueNodeList& nodes)
    {
        for (const auto& node : nodes)
            mNodeMap[node->GetId()] = node.get();
    }
    TreeNodeFromJson(const SharedNodeList& nodes)
    {
        for (const auto& node : nodes)
            mNodeMap[node->GetId()] = node.get();
    }
    const Node* operator()(const data::Reader& data) const
    {
        if (!data.HasValue("id"))
            return nullptr; // root node has no id
        std::string id;
        data.Read("id", &id);
        auto it = mNodeMap.find(id);
        if (it == mNodeMap.end())
        {
            // todo: fully implement the error handling when
            // trying to read broken JSON
            mError = true;
            return nullptr;
        }
        return it->second;
    }
private:
    std::unordered_map<std::string, Node*> mNodeMap;
    mutable bool mError = false;
};

template<typename Node>
void TreeNodeToJson(data::Writer& writer, const Node* node)
{
    // do only shallow serialization of the render tree node,
    // i.e. only record the id so that we can restore the node
    // later on load based on the ID.
    if (node)
        writer.Write("id", node->GetId());
}


template<typename Serializer, typename Node>
void RenderTreeIntoJson(const RenderTree<Node>& tree, const Serializer& to_json, data::Writer& data, const Node* node = nullptr)
{
    auto chunk = data.NewWriteChunk();
    to_json(*chunk, node);
    data.Write("node", *chunk);
    tree.ForEachChild([&tree, &to_json, &data](const Node* child) {
        auto child_chunk = data.NewWriteChunk();
        RenderTreeIntoJson(tree, to_json, *child_chunk, child);
        data.AppendChunk("children", std::move(child_chunk));
    }, node);
}

template<typename Serializer, typename Node>
const Node* RenderTreeFromJson(RenderTree<Node>& tree, const Serializer& from_json, const data::Reader& data)
{
    const auto& chunk = data.GetReadChunk("node");
    const auto* node  = chunk ? from_json(*chunk) : nullptr;
    for (unsigned i=0; i<data.GetNumChunks("children"); ++i)
    {
        const auto& chunk = data.GetReadChunk("children", i);
        const auto* child = RenderTreeFromJson(tree, from_json, *chunk);
        tree.LinkChild(node, child);
    }
    return node;
}

template<typename Node>
glm::mat4 FindUnscaledNodeModelTransform(const RenderTree<Node>& tree, const Node* node)
{
    constexpr Node* root = nullptr;
    std::vector<const Node*> path;
    SearchParent(tree, node, root, &path);

    Transform transform;
    for (auto it = path.rbegin(); it != path.rend(); ++it)
    {
        const auto* node = *it;
        if (node == nullptr)
            continue;
        transform.Push(node->GetNodeTransform());
    }

    transform.Push();
    // offset the drawable size, don't use the scale operation
    // because then the input would need to be in the model space (i.e. [0.0f, 1.0f])
    const auto& size = node->GetSize();
    transform.Translate(-size.x*0.5f, -size.y*0.5f);

    // transform stack cleanup (pop) is not done
    // because it's meaningless.
    return transform.GetAsMatrix();
}

template<typename Node>
glm::mat4 FindNodeModelTransform(const RenderTree<Node>& tree, const Node* node)
{
    constexpr Node* root = nullptr;
    std::vector<const Node*> path;
    SearchParent(tree, node, root, &path);

    Transform transform;
    for (auto it = path.rbegin(); it != path.rend(); ++it)
    {
        const auto* node = *it;
        if (node == nullptr)
            continue;
        transform.Push(node->GetNodeTransform());
    }
    transform.Push(node->GetModelTransform());
    return transform.GetAsMatrix();
}

template<typename Node>
glm::mat4 FindNodeTransform(const RenderTree<Node>& tree, const Node* node)
{
    constexpr Node* root = nullptr;
    std::vector<const Node*> path;
    SearchParent(tree, node, root, &path);

    Transform transform;
    for (auto it = path.rbegin(); it != path.rend(); ++it)
    {
        const auto* node = *it;
        if (node == nullptr)
            continue;
        transform.Push(node->GetNodeTransform());
    }
    return transform.GetAsMatrix();
}

template<typename Node> inline
void LinkChild(RenderTree<Node>& tree, const Node* parent, const Node* child)
{
    tree.LinkChild(parent, child);
}

template<typename Node> inline
void BreakChild(RenderTree<Node>& tree, Node* child, bool retain_world_transform = true)
{
    if (retain_world_transform)
    {
        const auto& child_to_world = FindNodeTransform(tree, child);
        FBox box;
        box.Transform(child_to_world);
        child->SetTranslation(box.GetCenter());
        child->SetRotation(box.GetRotation());
    }
    tree.BreakChild(child);
}

template<typename Node> inline
void ReparentChild(RenderTree<Node>& tree, const Node* parent, Node* child, bool retain_world_transform = true)
{
    // compute a new node transform that expresses the node's
    // current world transform relative to its new parent.
    // i.e figure out which transform gives the node the same
    // world position/rotation related to the new parent.
    if (retain_world_transform)
    {
        const auto& child_to_world  = FindNodeTransform(tree, child);
        const auto& parent_to_world = FindNodeTransform(tree, parent);
        FBox box;
        box.Transform(child_to_world);
        box.Transform(glm::inverse(parent_to_world));
        child->SetTranslation(box.GetCenter());
        child->SetRotation(box.GetRotation());
    }

    tree.ReparentChild(parent, child);
}

template<typename Node, typename Container> inline
void DeleteNode(RenderTree<Node>& tree, const Node* node, Container& nodes)
{
    std::unordered_set<const Node*> graveyard;

    // traverse the tree starting from the node to be deleted
    // and capture the ids of the scene nodes that are part
    // of this hierarchy.
    tree.PreOrderTraverseForEach([&graveyard](const Node* value) {
        graveyard.insert(value);
    }, node);
    // delete from the tree.
    tree.DeleteNode(node);

    // delete from the container.
    nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [&graveyard](const auto& node) {
        return graveyard.find(node.get()) != graveyard.end();
    }), nodes.end());
}

template<typename Node>
Node* DuplicateNode(RenderTree<Node>& tree, const Node* node, std::vector<std::unique_ptr<Node>>* clones)
{
    // mark the index of the item that will be the first dupe we create
    // we'll return this later since it's the root of the new hierarchy.
    const size_t first = clones->size();
    // do a deep copy of a hierarchy of nodes starting from
    // the selected node and add the new hierarchy as a new
    // child of the selected node's parent
    if (tree.HasNode(node))
    {
        const auto* parent = tree.GetParent(node);
        class ConstVisitor : public RenderTree<Node>::ConstVisitor {
        public:
            ConstVisitor(const Node* parent, std::vector<std::unique_ptr<Node>>& clones)
                : mClones(clones)
            {
                mParents.push(parent);
            }
            virtual void EnterNode(const Node* node) override
            {
                const auto* parent = mParents.top();

                auto clone = std::make_unique<Node>(node->Clone());
                clone->SetName(base::FormatString("Copy of %1", node->GetName()));
                mParents.push(clone.get());
                mLinks[clone.get()] = parent;
                mClones.push_back(std::move(clone));
            }
            virtual void LeaveNode(const Node* node) override
            {
                mParents.pop();
            }
            void LinkChildren(RenderTree<Node>& tree)
            {
                for (const auto& p : mLinks)
                {
                    const Node* child  = p.first;
                    const Node* parent = p.second;
                    tree.LinkChild(parent, child);
                }
            }
        private:
            std::stack<const Node*> mParents;
            std::unordered_map<const Node*, const Node*> mLinks;
            std::vector<std::unique_ptr<Node>>& mClones;
        };
        ConstVisitor visitor(parent, *clones);
        tree.PreOrderTraverse(visitor, node);
        visitor.LinkChildren(tree);
    }
    else
    {
        clones->emplace_back(new Node(node->Clone()));
    }
    return (*clones)[first].get();
}

namespace detail {
    template<typename Node, typename PtrT>
    class HitTestVisitor : public RenderTree<Node>::template TVisitor<PtrT> {
    public:
        HitTestVisitor(const glm::vec4& point, std::vector<PtrT*>* hits, std::vector<glm::vec2>* boxes)
            : mHitPoint(point)
            , mHits(hits)
            , mBoxes(boxes)
        {}
        virtual void EnterNode(PtrT* node) override
        {
            if (!node)
                return;
            mTransform.Push(node->GetNodeTransform());
            // using the model transform will put the coordinates in the drawable coordinate
            // space, i.e. normalized coordinates.
            mTransform.Push(node->GetModelTransform());

            const auto& world_to_node = glm::inverse(mTransform.GetAsMatrix());
            const auto& point_in_node = world_to_node * mHitPoint;
            if (point_in_node.x >= 0.0f && point_in_node.x < 1.0f &&
                point_in_node.y >= 0.0f && point_in_node.y < 1.0f)
            {
                mHits->push_back(node);
                if (mBoxes)
                {
                    const auto& size = node->GetSize();
                    mBoxes->push_back(glm::vec2(point_in_node.x * size.x,
                                                point_in_node.y * size.y));
                }
            }
            mTransform.Pop();
        }
        virtual void LeaveNode(PtrT* node) override
        {
            if (!node)
                return;
            mTransform.Pop();
        }
    private:
        const glm::vec4 mHitPoint;
        Transform mTransform;
        std::vector<PtrT*>* mHits = nullptr;
        std::vector<glm::vec2>* mBoxes = nullptr;
    };
} // detail

template<typename Node>
void CoarseHitTest(RenderTree<Node>& tree, float x, float y,
                   std::vector<Node*>* hit_nodes,
                   std::vector<glm::vec2>* hit_node_points)
{
    detail::HitTestVisitor<Node, Node> visitor(glm::vec4(x, y, 1.0f, 1.0), hit_nodes, hit_node_points);
    tree.PreOrderTraverse(visitor);
}
template<typename Node>
void CoarseHitTest(const RenderTree<Node>& tree, float x, float y,
                   std::vector<const Node*>* hit_nodes,
                   std::vector<glm::vec2>* hit_node_points)
{
    detail::HitTestVisitor<Node, const Node> visitor(glm::vec4(x, y, 1.0f, 1.0), hit_nodes, hit_node_points);
    tree.PreOrderTraverse(visitor);
}

template<typename Node>
glm::vec2 MapCoordsFromNodeBox(const RenderTree<Node>& tree, float x, float y, const Node* node)
{
    const auto& mat = FindUnscaledNodeModelTransform(tree, node);
    const auto& ret = mat * glm::vec4(x, y, 1.0f, 1.0f);
    return glm::vec2(ret.x, ret.y);
}

template<typename Node>
glm::vec2 MapCoordsToNodeBox(const RenderTree<Node>& tree, float x, float y, const Node* node)
{
    const auto& mat = glm::inverse(FindUnscaledNodeModelTransform(tree, node));
    const auto& ret = mat * glm::vec4(x, y, 1.0f, 1.0f);
    return glm::vec2(ret.x, ret.y);
}

template<typename Node>
FBox FindBoundingBox(const RenderTree<Node>& tree, const Node* node)
{
    return FBox(FindNodeModelTransform(tree, node));
}

template<typename Node>
FRect FindBoundingRect(const RenderTree<Node>& tree, const Node* node)
{
    const auto& mat = FindNodeModelTransform(tree, node);

    return ComputeBoundingRect(mat);
}

template<typename Node>
FRect FindBoundingRect(const RenderTree<Node>& tree)
{
    class Visitor : public RenderTree<Node>::ConstVisitor {
    public:
        virtual void EnterNode(const Node* node) override
        {
            if (!node)
                return;
            mTransform.Push(node->GetNodeTransform());
            mTransform.Push(node->GetModelTransform());

            const auto box = ComputeBoundingRect(mTransform.GetAsMatrix());
            mResult = Union(mResult, box);
            mTransform.Pop();
        }
        virtual void LeaveNode(const Node* node) override
        {
            if (!node)
                return;
            mTransform.Pop();
        }
        FRect GetResult() const
        { return mResult; }
    private:
        FRect mResult;
        Transform mTransform;
    };

    Visitor visitor;
    tree.PreOrderTraverse(visitor);
    return visitor.GetResult();
}

} // namespace
