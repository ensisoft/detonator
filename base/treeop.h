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

#include <vector>
#include <set>
#include <unordered_set>
#include <optional>

#include "base/tree.h"
#include "base/math.h"
#include "base/types.h"

namespace base
{

template<typename Node>
void ListChildren(const RenderTree<Node>& tree, const Node* parent, std::vector<const Node*>* result)
{
    tree.ForEachChild([&result](const Node* child) {
        result->push_back(child);
    }, parent);
}

template<typename Node>
void ListChildren(RenderTree<Node>& tree, Node* parent, std::vector<Node*>* result)
{
    tree.ForEachChild([&result](Node* child) {
        result->push_back(child);
    }, parent);
}

template<typename Node>
void ListSiblings(const RenderTree<Node>& tree, const Node* sibling, std::vector<const Node*>* siblings)
{
    if (!tree.HasParent(sibling))
        return;
    const auto* parent = tree.GetParent(sibling);
    tree.ForEachChild([&siblings, &sibling](const Node* node) {
        if (node != sibling)
            siblings->push_back(node);
    }, parent);
}
template<typename Node>
void ListSiblings(RenderTree<Node>& tree, Node* sibling, std::vector<Node*>* siblings)
{
    if (!tree.HasParent(sibling))
        return;
    auto* parent = tree.GetParent(sibling);
    tree.ForEachChild([&siblings, &sibling](Node* node) {
        if (node != sibling)
            siblings->push_back(node);
    }, parent);
}

// Search the tree for a route from parent to assumed child node.
// When node is a descendant of parent returns true. Otherwise,
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

namespace detail {
    enum class QuadTreeFindMode {
        Closest, All
    };

    // use distinct types for the objects that are stored it the quad tree
    // vs the objects that are stored during query operation, so that simple
    // thing such as converting from T* to const T* work easily.

    template<typename SrcObject, typename RetObject> inline
    void StoreObject(SrcObject object, std::vector<RetObject>* vector)
    { vector->push_back(std::move(object)); }
    template<typename SrcObject, typename RetObject> inline
    void StoreObject(SrcObject object, std::set<RetObject>* set)
    { set->insert(std::move(object)); }
    template<typename SrcObject, typename RetObject> inline
    void StoreObject(SrcObject object, std::unordered_set<RetObject>* set)
    { set->insert(std::move(object)); }

    template<typename Object, typename Container>
    void QuadTreeFindAll(const base::FRect& area_of_interest, const QuadTreeNode<Object>& node, Container* result)
    {
        for (size_t i=0; i<node.GetNumItems(); ++i)
        {
            const auto& rect = node.GetItemRect(i);
            if (DoesIntersect(area_of_interest, rect))
                StoreObject(node.GetItemObject(i), result);
        }
        if (!node.HasChildren())
            return;
        for (size_t i=0; i<4; ++i)
        {
            const auto* quad = node.GetChildQuadrant(i);
            const auto& rect = quad->GetRect();
            const auto& area = base::Intersect(area_of_interest, rect);
            if (!area.IsEmpty())
                QuadTreeFindAll(area, *quad, result);
        }
    }

    template<typename Object, typename Container>
    void QuadTreeFindAll(const base::FPoint& point, const QuadTreeNode<Object>& node, Container* result)
    {
        for (size_t i=0; i<node.GetNumItems(); ++i)
        {
            const auto& rect = node.GetItemRect(i);
            if (rect.TestPoint(point))
                StoreObject(node.GetItemObject(i), result);
        }
        if (!node.HasChildren())
            return;
        for (size_t i=0; i<4; ++i)
        {
            const auto* quad = node.GetChildQuadrant(i);
            const auto& rect = quad->GetRect();
            if (rect.TestPoint(point))
                QuadTreeFindAll(point, *quad, result);
        }
    }

    template<typename Object, typename Container>
    void QuadTreeFindAll(const base::FCircle& circle, const QuadTreeNode<Object>& node, Container* result)
    {
        for (size_t i=0; i<node.GetNumItems(); ++i)
        {
            const auto& rect = node.GetItemRect(i);
            if (base::DoesIntersect(rect, circle))
                StoreObject(node.GetItemObject(i), result);
        }
        if (!node.HasChildren())
            return;
        for (size_t i=0; i<4; ++i)
        {
            const auto* quad = node.GetChildQuadrant(i);
            const auto& rect = quad->GetRect();
            if (base::DoesIntersect(rect, circle))
                QuadTreeFindAll(circle, *quad, result);
        }
    }

    template<typename Object>
    void QuadTreeFindClosest(const base::FPoint& point, const QuadTreeNode<Object>& node, float& best_distance, std::optional<Object>& best_found)
    {
        for (size_t i=0; i<node.GetNumItems(); ++i)
        {
            const auto& rect = node.GetItemRect(i);
            if (!rect.TestPoint(point))
                continue;

            const float dist = SquareDistance(point, rect.GetCenter());
            if (dist < best_distance) {
                best_distance = dist;
                best_found = node.GetItemObject(i);
            }
        }
        if (!node.HasChildren())
            return;

        for (size_t i=0; i<4; ++i)
        {
            if (const auto* quad = node.GetChildQuadrant(i))
            {
                const auto& rect = quad->GetRect();
                if (rect.TestPoint(point))
                    QuadTreeFindClosest(point, *quad, best_distance, best_found);
            }
        }
    }

    template<typename Object>
    void QuadTreeFindClosest(const base::FCircle& circle, const QuadTreeNode<Object>& node, float& best_distance, std::optional<Object>& best_found)
    {
        for (size_t i=0; i<node.GetNumItems(); ++i)
        {
            const auto& rect = node.GetItemRect(i);
            if (!DoesIntersect(circle, rect))
                continue;
            const float dist = SquareDistance(circle.GetCenter(), rect.GetCenter());
            if (dist < best_distance) {
                best_distance = dist;
                best_found = node.GetItemObject(i);
            }
        }
        if(!node.HasChildren())
            return;

        for (size_t i=0; i<4; ++i)
        {
            if (const auto* quad = node.GetChildQuadrant(i))
            {
                const auto& rect = quad->GetRect();
                if (DoesIntersect(rect, circle))
                    QuadTreeFindClosest(circle, *quad, best_distance, best_found);
            }
        }
    }

    template<typename Predicate, typename Object, typename Container>
    void QuadTreeFind(const Predicate& pred, const QuadTreeNode<Object>& node, Container* result, QuadTreeFindMode mode)
    {
        if (mode == QuadTreeFindMode::All)
        {
            QuadTreeFindAll(pred, node, result);
        }
        else if (mode == QuadTreeFindMode::Closest)
        {
            float best_distance = std::numeric_limits<float>::max();
            std::optional<Object> best_object;
            detail::QuadTreeFindClosest(pred, node, best_distance, best_object);
            if (best_object.has_value())
                detail::StoreObject(best_object.value(), result);
        } else BUG("Missing quad tree find mode.");
    }

} // namespace detail

using QuadTreeQueryMode = detail::QuadTreeFindMode;

template<typename SrcObject, typename RetObject> inline
void QueryQuadTree(const base::FRect& area_of_interest, const QuadTree<SrcObject>& quadtree,  std::vector<RetObject>* result)
{ detail::QuadTreeFindAll(area_of_interest, quadtree.GetRoot(), result); }

template<typename SrcObject, typename RetObject> inline
void QueryQuadTree(const base::FRect& area_of_interest, const QuadTree<SrcObject>& quadtree, std::set<RetObject>* result)
{ detail::QuadTreeFindAll(area_of_interest, quadtree.GetRoot(), result); }

template<typename SrcObject, typename RetObject> inline
void QueryQuadTree(const base::FRect& area_of_interest, const QuadTree<SrcObject>& quadtree, std::unordered_set<RetObject>* result)
{ detail::QuadTreeFindAll(area_of_interest, quadtree.GetRoot(), result); }

template<typename SrcObject, typename RetObject> inline
void QueryQuadTree(const base::FPoint& point, const QuadTree<SrcObject>& quadtree,  std::vector<RetObject>* result, QuadTreeQueryMode mode = QuadTreeQueryMode::All)
{ detail::QuadTreeFind(point, quadtree.GetRoot(), result, mode); }

template<typename SrcObject, typename RetObject> inline
void QueryQuadTree(const base::FPoint& point, const QuadTree<SrcObject>& quadtree, std::set<RetObject>* result, QuadTreeQueryMode mode = QuadTreeQueryMode::All)
{ detail::QuadTreeFind(point, quadtree.GetRoot(), result, mode); }

template<typename SrcObject, typename RetObject> inline
void QueryQuadTree(const base::FPoint& point, const QuadTree<SrcObject>& quadtree, std::unordered_set<RetObject>* result, QuadTreeQueryMode mode = QuadTreeQueryMode::All)
{ detail::QuadTreeFind(point, quadtree.GetRoot(), result, mode); }

template<typename SrcObject, typename RetObject> inline
void QueryQuadTree(const base::FPoint& point, float radius, const QuadTree<SrcObject>& quadtree,  std::vector<RetObject>* result, QuadTreeQueryMode mode = QuadTreeQueryMode::All)
{ detail::QuadTreeFind(FCircle(point, radius), quadtree.GetRoot(), result, mode); }

template<typename SrcObject, typename RetObject> inline
void QueryQuadTree(const base::FPoint& point, float radius, const QuadTree<SrcObject>& quadtree, std::set<RetObject>* result, QuadTreeQueryMode mode = QuadTreeQueryMode::All)
{ detail::QuadTreeFind(FCircle(point, radius), quadtree.GetRoot(), result, mode); }

template<typename SrcObject, typename RetObject> inline
void QueryQuadTree(const base::FPoint& point, float radius, const QuadTree<SrcObject>& quadtree, std::unordered_set<RetObject>* result, QuadTreeQueryMode mode = QuadTreeQueryMode::All)
{ detail::QuadTreeFind(FCircle(point, radius), quadtree.GetRoot(), result, mode); }

} // namespace
