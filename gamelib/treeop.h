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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <glm/glm.hpp> // for glm::inverse
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <stack>
#include <vector>
#include <algorithm> // for min/max
#include <unordered_set>

#include "base/assert.h"
#include "base/format.h"
#include "graphics/types.h"
#include "graphics/transform.h"
#include "gamelib/tree.h"
#include "gamelib/types.h"

// collection of algorithms that operate on a render tree.

namespace game
{

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
    const Node* operator()(const nlohmann::json& json) const
    {
        if (!json.contains("id")) // root node has no id
            return nullptr;
        const std::string& id = json["id"];
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
nlohmann::json TreeNodeToJson(const Node* node)
{
    // do only shallow serialization of the render tree node,
    // i.e. only record the id so that we can restore the node
    // later on load based on the ID.
    nlohmann::json ret;
    if (node)
        ret["id"] = node->GetId();
    return ret;
}

// Search the tree for a route from parent to assumed child node.
// When node is a descendant of parent returns true. Otherwise
// false is returned and there's no path from parent to node.
// Optionally provide the path from root to the child.
template<typename Node>
bool SearchChild(const RenderTree<Node>& tree, const Node* node, const Node* parent = nullptr,
                 std::vector<const Node*>* path = nullptr)
{
    class ConstVisitor : public RenderTree<Node>::ConstVisitor {
    public:
        ConstVisitor(const Node* node,std::vector<const Node*>* path)
            : mNode(node)
            , mPath(path)
        {}
        virtual void EnterNode(const Node* node) override
        {
            if (!mFound && mPath)
                mPath->push_back(node);

            if (mNode == node)
                mFound = true;
        }
        virtual void LeaveNode(const Node* node) override
        {
            if (!mFound && mPath)
                mPath->pop_back();
        }
        virtual bool IsDone() const override
        { return mFound; }
        bool GetResult() const
        { return mFound; }
    private:
        const Node* mNode = nullptr;
        std::vector<const Node*>* mPath = nullptr;
        bool mFound = false;
    };
    ConstVisitor visitor(node, path);
    tree.PreOrderTraverse(visitor, parent);
    return visitor.GetResult();
}

template<typename Node>
bool SearchParent(const RenderTree<Node>& tree, const Node* node, const Node* parent = nullptr,
                  std::vector<const Node*>* path = nullptr)
{
    if (path)
        path->push_back(node);
    if (node == parent)
        return true;
    while (tree.HasParent(node))
    {
        const Node* p = tree.GetParent(node);
        if (path)
            path->push_back(p);
        if (p == parent)
            return true;
        node = p;
    }
    if (path)
        path->clear();
    return false;
}

template<typename Node>
glm::mat4 FindUnscaledNodeModelTransform(const RenderTree<Node>& tree, const Node* node)
{
    constexpr Node* root = nullptr;
    std::vector<const Node*> path;
    SearchParent(tree, node, root, &path);

    gfx::Transform transform;
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

    gfx::Transform transform;
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

    gfx::Transform transform;
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
void BreakChild(RenderTree<Node>& tree, const Node* child)
{
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
        child->SetTranslation(box.GetPosition());
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



template<typename Node>
void CoarseHitTest(RenderTree<Node>& tree, float x, float y,
                   std::vector<Node*>* hits,
                   std::vector<glm::vec2>* hitbox_positions)
{
    class Visitor : public RenderTree<Node>::Visitor {
    public:
        Visitor(const glm::vec4& point, std::vector<Node*>* hits, std::vector<glm::vec2>* boxes)
                : mHitPoint(point)
                , mHits(hits)
                , mBoxes(boxes)
        {}
        virtual void EnterNode(Node* node) override
        {
            if (!node)
                return;

            mTransform.Push(node->GetNodeTransform());
            // using the model transform will put the coordinates in the drawable coordinate
            // space, i.e. normalized coordinates.
            mTransform.Push(node->GetModelTransform());

            const auto& animation_to_node = glm::inverse(mTransform.GetAsMatrix());
            const auto& hitpoint_in_node = animation_to_node * mHitPoint;

            if (hitpoint_in_node.x >= 0.0f && hitpoint_in_node.x < 1.0f &&
                hitpoint_in_node.y >= 0.0f && hitpoint_in_node.y < 1.0f)
            {
                mHits->push_back(node);
                if (mBoxes)
                {
                    const auto& size = node->GetSize();
                    mBoxes->push_back(glm::vec2(hitpoint_in_node.x * size.x, hitpoint_in_node.y * size.y));
                }
            }
            mTransform.Pop();
        }
        virtual void LeaveNode(Node* node) override
        {
            if (!node)
                return;

            mTransform.Pop();
        }
    private:
        const glm::vec4 mHitPoint;
        gfx::Transform mTransform;
        std::vector<Node*>* mHits = nullptr;
        std::vector<glm::vec2>* mBoxes = nullptr;
    };

    Visitor visitor(glm::vec4(x, y, 1.0f, 1.0f), hits, hitbox_positions);
    tree.PreOrderTraverse(visitor);
}
template<typename Node>
void CoarseHitTest(const RenderTree<Node>& tree, float x, float y,
                   std::vector<const Node*>* hits,
                   std::vector<glm::vec2>* hitbox_positions)
{
    class Visitor : public RenderTree<Node>::ConstVisitor {
    public:
        Visitor(const glm::vec4& point, std::vector<const Node*>* hits, std::vector<glm::vec2>* boxes)
                : mHitPoint(point)
                , mHits(hits)
                , mBoxes(boxes)
        {}
        virtual void EnterNode(const Node* node) override
        {
            if (!node)
                return;

            mTransform.Push(node->GetNodeTransform());
            // using the model transform will put the coordinates in the drawable coordinate
            // space, i.e. normalized coordinates.
            mTransform.Push(node->GetModelTransform());

            const auto& animation_to_node = glm::inverse(mTransform.GetAsMatrix());
            const auto& hitpoint_in_node = animation_to_node * mHitPoint;

            if (hitpoint_in_node.x >= 0.0f && hitpoint_in_node.x < 1.0f &&
                hitpoint_in_node.y >= 0.0f && hitpoint_in_node.y < 1.0f)
            {
                mHits->push_back(node);
                if (mBoxes)
                {
                    const auto& size = node->GetSize();
                    mBoxes->push_back(glm::vec2(hitpoint_in_node.x * size.x, hitpoint_in_node.y * size.y));
                }
            }
            mTransform.Pop();
        }
        virtual void LeaveNode(const Node* node) override
        {
            if (!node)
                return;

            mTransform.Pop();
        }
    private:
        const glm::vec4 mHitPoint;
        gfx::Transform mTransform;
        std::vector<const Node*>* mHits = nullptr;
        std::vector<glm::vec2>* mBoxes = nullptr;
    };
    Visitor visitor(glm::vec4(x, y, 1.0f, 1.0f), hits, hitbox_positions);
    tree.PreOrderTraverse(visitor);
}

template<typename Node>
glm::vec2 MapCoordsFromNode(const RenderTree<Node>& tree, float x, float y, const Node* node)
{
    const auto& mat = FindUnscaledNodeModelTransform(tree, node);
    const auto& ret = mat * glm::vec4(x, y, 1.0f, 1.0f);
    return glm::vec2(ret.x, ret.y);
}

template<typename Node>
glm::vec2 MapCoordsToNode(const RenderTree<Node>& tree, float x, float y, const Node* node)
{
    const auto& mat = glm::inverse(FindUnscaledNodeModelTransform(tree, node));
    const auto& ret = mat * glm::vec4(x, y, 1.0f, 1.0f);
    return glm::vec2(ret.x, ret.y);
}

template<typename Node>
FBox GetBoundingBox(const RenderTree<Node>& tree, const Node* node)
{
    return FBox(FindNodeModelTransform(tree, node));
}

template<typename Node>
gfx::FRect GetBoundingRect(const RenderTree<Node>& tree, const Node* node)
{
    const auto& mat = FindNodeModelTransform(tree, node);
    // for each corner of a bounding volume compute new positions per
    // the transformation matrix and then choose the min/max on each axis.
    const auto& top_left  = mat * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    const auto& top_right = mat * glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
    const auto& bot_left  = mat * glm::vec4(0.0f, 1.0f, 1.0f, 1.0f);
    const auto& bot_right = mat * glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

    // choose min/max on each axis.
    const auto left = std::min(std::min(top_left.x, top_right.x),
                               std::min(bot_left.x, bot_right.x));
    const auto right = std::max(std::max(top_left.x, top_right.x),
                                std::max(bot_left.x, bot_right.x));
    const auto top = std::min(std::min(top_left.y, top_right.y),
                              std::min(bot_left.y, bot_right.y));
    const auto bottom = std::max(std::max(top_left.y, top_right.y),
                                 std::max(bot_left.y, bot_right.y));
    return gfx::FRect(left, top, right - left, bottom - top);
}

template<typename Node>
gfx::FRect GetBoundingRect(const RenderTree<Node>& tree)
{
    class Visitor : public RenderTree<Node>::ConstVisitor {
    public:
        virtual void EnterNode(const Node* node) override
        {
            if (!node)
                return;
            mTransform.Push(node->GetNodeTransform());
            mTransform.Push(node->GetModelTransform());
            // node to animation matrix.
            // for each corner of a bounding volume compute new positions per
            // the transformation matrix and then choose the min/max on each axis.
            const auto& mat = mTransform.GetAsMatrix();
            const auto& top_left  = mat * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
            const auto& top_right = mat * glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
            const auto& bot_left  = mat * glm::vec4(0.0f, 1.0f, 1.0f, 1.0f);
            const auto& bot_right = mat * glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

            // choose min/max on each axis.
            const auto left = std::min(std::min(top_left.x, top_right.x),
                                       std::min(bot_left.x, bot_right.x));
            const auto right = std::max(std::max(top_left.x, top_right.x),
                                        std::max(bot_left.x, bot_right.x));
            const auto top = std::min(std::min(top_left.y, top_right.y),
                                      std::min(bot_left.y, bot_right.y));
            const auto bottom = std::max(std::max(top_left.y, top_right.y),
                                         std::max(bot_left.y, bot_right.y));
            const auto box = gfx::FRect(left, top, right - left, bottom - top);
            if (mResult.IsEmpty())
                mResult = box;
            else mResult = Union(mResult, box);

            mTransform.Pop();
        }
        virtual void LeaveNode(const Node* node) override
        {
            if (!node)
                return;
            mTransform.Pop();
        }
        gfx::FRect GetResult() const
        { return mResult; }
    private:
        gfx::FRect mResult;
        gfx::Transform mTransform;
    };

    Visitor visitor;
    tree.PreOrderTraverse(visitor);
    return visitor.GetResult();
}

} // namespace
