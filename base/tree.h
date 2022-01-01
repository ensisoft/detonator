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

#include <memory>
#include <vector>
#include <unordered_map>
#include <cmath>

#include "base/assert.h"
#include "base/memory.h"
#include "base/utility.h"
#include "base/types.h"

namespace base
{
    // Non-intrusive, non-owning tree structure for maintaining
    // parent-child relationships. This can be used to defined
    // things such as the scene's render hierarchy (i.e. the
    // scene graph)
    // The root of the tree can be denoted by using a special
    // pointer value, the nullptr.
    template<typename Element>
    class RenderTree
    {
    public:
        template<typename T>
        class TVisitor
        {
        public:
            virtual ~TVisitor() = default;
            // Called when the tree traversal algorithm enters a node.
            virtual void EnterNode(T* node) {}
            // Called when the tree traversal algorithm leaves a node.
            virtual void LeaveNode(T* node) {}
            // Called to check whether the tree traversal can finish early
            // without visiting the remainder of the nodes. When true is
            // returned the rest of the nodes are skipped and the algorithm
            // returns early. On false the tree traversal continues.
            virtual bool IsDone() const { return false; }
        private:
        };

        // Mutating visitor for visiting nodes in the tree.
        using Visitor = TVisitor<Element>;
        // Non-mutating visitor for visiting nodes in the tree.
        using ConstVisitor = TVisitor<const Element>;

        // Clear the tree. After this the tree is empty
        // and contains no nodes.
        void Clear()
        {
            mParents.clear();
            mChildren.clear();
        }

        void PreOrderTraverse(Visitor& visitor, Element* parent = nullptr)
        { PreOrderTraverse<Element>(visitor, parent); }

        void PreOrderTraverse(ConstVisitor& visitor, const Element* parent = nullptr) const
        { PreOrderTraverse<const Element>(visitor, parent); }

        template<typename Function>
        void PreOrderTraverseForEach(Function callback, Element* parent = nullptr)
        { PreOrderTraverseForEach<Element>(std::move(callback), parent); }

        template<typename Function>
        void PreOrderTraverseForEach(Function callback, const Element* parent = nullptr) const
        { PreOrderTraverseForEach<const Element>(std::move(callback), parent); }

        template<typename Function>
        void ForEachChild(Function callback, const Element* parent = nullptr) const
        { ForEachChild<const Element>(std::move(callback), parent); }

        template<typename Function>
        void ForEachChild(Function function, Element* parent = nullptr) const
        { ForEachChild<Element>(std::move(function), parent); }

        // Convenience operation for moving a child node to a new parent.
        void ReparentChild(const Element* parent, const Element* child)
        {
            BreakChild(child);
            LinkChild(parent, child);
        }
        // Delete the node and all of its ascendants from the tree.
        // If the child doesn't exist in the tree then nothing is done.
        void DeleteNode(const Element* child)
        {
            auto it = mParents.find(child);
            if (it == mParents.end())
                return;

            auto& children = mChildren[it->second];
            for (auto it = children.begin(); it != children.end(); ++it)
            {
                const Element* value = *it;
                if (value == child)
                {
                    DeleteChildren(value);
                    children.erase(it);
                    break;
                }
            }
            mParents.erase(child);
        }

        // Delete all the children of a parent.
        // If the parent doesn't exist in the tree nothing is done.
        void DeleteChildren(const Element* parent)
        {
            auto it = mChildren.find(parent);
            if (it == mChildren.end())
                return;
            auto& children = it->second;
            for (auto* child : children)
            {
                DeleteChildren(child);
                mParents.erase(child);
            }
            children.clear();
        }

        // Link a child node to a parent node.
        // The child must not be linked to any parent. It'd be
        // illegal to have a child with two parents nodes.
        // If the child should be moved from one parent to another
        // use ReparentChild or BreakChild followed by LinkChild.
        void LinkChild(const Element* parent, const Element* child)
        {
            ASSERT(mParents.find(child) == mParents.end());
            mChildren[parent].push_back(child);
            mParents[child] = parent;
        }
        // Break a child node away from its parent. The descendants
        // of child are still retained as node's children.
        // If the child node is currently not linked to any parent
        // nothing is done.
        void BreakChild(const Element* child)
        {
            auto it = mParents.find(child);
            if (it == mParents.end())
                return;

            const auto* parent = it->second;
            auto& children = mChildren[parent];
            for (auto it = children.begin(); it != children.end(); ++it)
            {
                const Element* value = *it;
                if (value == child)
                {
                    children.erase(it);
                    break;
                }
            }
            mParents.erase(it);
        }

        // Get the parent node of a child node.
        // The child node must exist in the tree.
        Element* GetParent(const Element* child)
        {
            auto it = mParents.find(child);
            ASSERT(it != mParents.end());
            return const_cast<Element*>(it->second);
        }
        const Element* GetParent(const Element* child) const
        {
            auto it = mParents.find(child);
            ASSERT(it != mParents.end());
            return it->second;
        }

        // Returns true if this node exists in this tree.
        bool HasNode(const Element* node) const
        {
            // all nodes have a parent, thus if the node exists
            // in the tree it also exists in the child->Parent map
            return mParents.find(node) != mParents.end();
        }

        // Returns true if the node has a parent. All nodes except
        // for the *Root* node have a parent.
        bool HasParent(const Element* node) const
        {
            return mParents.find(node) != mParents.end();
        }

        // Build an equivalent tree (in terms of topology) based on the
        // source tree while remapping nodes from one instance to another
        // through the map function.
        template<typename T, typename MapFunction>
        void FromTree(const RenderTree<T>& tree, const MapFunction& map_node)
        {
            for (const auto& pair : tree.mChildren)
            {
                const auto* parent   = pair.first;
                const auto& children = pair.second;
                for (const auto* child : children)
                    LinkChild(map_node(parent), map_node(child));
            }
        }
    private:
        template<typename T>
        void PreOrderTraverse(TVisitor<T>& visitor, T* parent = nullptr) const
        {
            visitor.EnterNode(parent);
            auto it = mChildren.find(parent);
            if (it != mChildren.end())
            {
                const auto& children = it->second;
                for (auto *child : children)
                {
                    PreOrderTraverse<T>(visitor, const_cast<Element*>(child));
                    if (visitor.IsDone())
                        break;
                }
            }
            visitor.LeaveNode(parent);
        }
        template<typename T, typename Function>
        void PreOrderTraverseForEach(Function callback, T* parent = nullptr) const
        {
            class PrivateVisitor : public TVisitor<T> {
            public:
                PrivateVisitor(Function callback) : mCallback(std::move(callback))
                {}
                virtual void EnterNode(T* node) override
                { mCallback(node); }
            private:
                Function mCallback;
            };
            PrivateVisitor visitor(std::move(callback));
            PreOrderTraverse(visitor, parent);
        }
        template<typename T, typename Function>
        void ForEachChild(Function callback, T* parent = nullptr) const
        {
            auto it = mChildren.find(parent);
            if (it == mChildren.end())
                return;
            const auto& children = it->second;
            for (auto* child : children)
                callback(child);
        }
    private:
        using ChildList = std::vector<const Element*>;
        // lookup table for mapping parents to their children
        std::unordered_map<const Element*, ChildList> mChildren;
        // lookup table for mapping children to their parents
        std::unordered_map<const Element*, const Element*> mParents;

        template<typename T> friend class RenderTree;
    };

    namespace detail {
        template<typename Object>
        class QuadTreeNode
        {
        public:
            QuadTreeNode(const QuadTreeNode&) = delete;
            QuadTreeNode(const base::FRect& rect)
                : mRect(rect)
            {}
            ~QuadTreeNode()
            {
                ASSERT(mQuadrants[0] == nullptr &&
                       mQuadrants[1] == nullptr &&
                       mQuadrants[2] == nullptr &&
                       mQuadrants[3] == nullptr);
            }
            bool Insert(const base::FRect& rect, Object object, mem::IFixedAllocator& alloc,
                        unsigned max_items, unsigned level)
            {
                // if the object is not completely within this node's rect
                // then fail
                if (!base::Contains(mRect, rect))
                    return false;

                // if this node holds less than max capacity items store here.
                if (!HasChildren() && mItems.size() < max_items)
                {
                    Item item;
                    item.rect = rect;
                    item.object = object;
                    mItems.push_back(item);
                    return true;
                }
                // if we're reaching the maximum levels then store in this node
                // and stop further sub-divisions of space.
                if (level == 0)
                {
                    Item item;
                    item.rect = rect;
                    item.object = object;
                    mItems.push_back(item);
                    return true;
                }

                // split the rect into 4 smaller quadrants
                const auto [q0, q1, q2, q3] = mRect.Quadrants();
                const base::FRect quadrant_rects[4] = {
                        q0, q1, q2, q3
                };

                // create quadrants if not yet created.
                if (!HasChildren())
                {
                    // create the quadrants
                    for (int i = 0; i < 4; ++i)
                    {
                        void* mem = alloc.Allocate();
                        ASSERT(mem);
                        mQuadrants[i] = new (mem) QuadTreeNode(quadrant_rects[i]);
                    }
                    // insert the items into the quadrants.
                    for (const auto& item : mItems)
                    {
                        for (int i = 0; i < 4; ++i)
                        {
                            const auto& quadrant_rect = quadrant_rects[i];
                            const auto& intersection = base::Intersect(quadrant_rect, item.rect);
                            if (intersection.IsEmpty())
                                continue;
                            auto& quadrant = mQuadrants[i];
                            ASSERT(quadrant->Insert(intersection, item.object, alloc, max_items, level-1));
                        }
                    }
                    mItems.clear();
                }

                // insert the object into tree, into every quadrant
                // that intersects with the object's rectangle.
                for (int i = 0; i < 4; ++i)
                {
                    const auto& quadrant_rect = quadrant_rects[i];
                    const auto& intersection = base::Intersect(quadrant_rect, rect);
                    if (intersection.IsEmpty())
                        continue;
                    auto& quadrant = mQuadrants[i];
                    ASSERT(quadrant->Insert(intersection, object, alloc, max_items, level-1));
                }
                return true;
            }
            void Clear(mem::IFixedAllocator& alloc)
            {
                mItems.clear();
                for (int i = 0; i < 4; ++i)
                {
                    if (!mQuadrants[i])
                        continue;

                    mQuadrants[i]->Clear(alloc);
                    mQuadrants[i]->~QuadTreeNode();
                    alloc.Free((void*)mQuadrants[i]);
                    mQuadrants[i] = nullptr;
                }
            }
            template<typename Predicate>
            void Erase(const Predicate& predicate, mem::IFixedAllocator& alloc, unsigned max_items)
            {
                for (auto it = mItems.begin(); it != mItems.end();)
                {
                    auto& item = *it;
                    if (predicate(item.object, item.rect))
                        it = mItems.erase(it);
                    else ++it;
                }
                if (!HasChildren())
                    return;

                // so ideally an object that was split between different quadrants
                // would be combined back into a single object/rectangle. but the
                // problem is that there's no simple generic way to realize object
                // identity right now. I.e. there's no knowledge connecting an object
                // in one quadrant to another object in an adjacent quadrant. doing this
                // would require some kind of split-table/link or place a requirement
                // on the type T to be uniquely identifiable.
                size_t items = 0;
                for (int i=0; i<4; ++i)
                {
                    mQuadrants[i]->Erase(predicate, alloc, max_items);
                    items += mQuadrants[i]->GetNumItems();
                }
                if (items > max_items)
                    return;

                for (int i=0; i<4; ++i)
                {
                    mQuadrants[i]->MoveItems(mItems);
                    mQuadrants[i]->Clear(alloc);
                    mQuadrants[i]->~QuadTreeNode();
                    alloc.Free((void*)mQuadrants[i]);
                    mQuadrants[i] = nullptr;
                }
            }

            inline bool HasChildren() const
            { return !!mQuadrants[0]; }
            inline bool HasItems() const
            { return !mItems.empty(); }
            const QuadTreeNode* GetChildQuadrant(size_t i) const
            { return mQuadrants[i]; }
            const base::FRect& GetRect() const
            { return mRect; }
            const base::FRect& GetItemRect(size_t index) const
            { return base::SafeIndex(mItems, index).rect; }
            Object GetItemObject(size_t index) const
            { return base::SafeIndex(mItems, index).object; }
            std::size_t GetNumItems() const
            { return mItems.size(); }
            std::size_t GetSize() const
            {
                size_t ret = mItems.size();
                for (int i=0; i<4; ++i)
                {
                    if (mQuadrants[i])
                        ret += mQuadrants[i]->GetSize();
                }
                return ret;
            }
            QuadTreeNode& operator=(const QuadTreeNode&) = delete;
        private:
            struct Item {
                base::FRect rect;
                Object object;
            };
            void MoveItems(std::vector<Item>& out) const
            { base::AppendVector(out, std::move(mItems)); }
        private:
            const base::FRect mRect;
            std::vector<Item> mItems;
            QuadTreeNode* mQuadrants[4] = { nullptr, nullptr,
                                            nullptr, nullptr };
        };
    } // namespace

    // Non-intrusive, non-owning space partitioning tree.
    // Maps points in space to objects.
    template<typename Object>
    class QuadTree
    {
    public:
        using TreeNode = detail::QuadTreeNode<Object>;
        using NodePool = mem::HeapMemoryPool<sizeof(TreeNode)>;
        static constexpr auto DefaultMaxItems  = 4;
        static constexpr auto DefaultMaxLevels = 3;

        QuadTree(const base::FRect& rect, unsigned max_items = DefaultMaxItems, unsigned max_levels = DefaultMaxLevels)
          : mMaxItems(max_items)
          , mMaxLevels(max_levels)
          , mRoot(rect)
          , mPool(FindMaxNumNodes(max_levels))
        {}
        QuadTree(float width, float height, unsigned max_items = DefaultMaxItems, unsigned max_levels = DefaultMaxLevels)
          : mMaxItems(max_items)
          , mMaxLevels(max_levels)
          , mRoot(base::FRect(0.0f, 0.0f, width, height))
          , mPool(FindMaxNumNodes(max_levels))
        {}
        QuadTree(float x, float y, float width, float height, unsigned max_items = DefaultMaxItems, unsigned max_levels = DefaultMaxLevels)
          : mMaxItems(max_items)
          , mMaxLevels(max_levels)
          , mRoot(base::FRect(x, y, width, height))
          , mPool(FindMaxNumNodes(max_levels))
        {}
        QuadTree(const base::FPoint& pos, const base::FSize& size, unsigned max_items = DefaultMaxItems, unsigned max_levels = DefaultMaxLevels)
          : mMaxItems(max_items)
          , mMaxLevels(max_levels)
          , mRoot(base::FRect(pos, size))
          , mPool(FindMaxNumNodes(max_levels))
        {}
        QuadTree(const base::FSize& size, unsigned max_items = DefaultMaxItems, unsigned max_levels = DefaultMaxLevels)
          : mMaxItems(max_items)
          , mMaxLevels(max_levels)
          , mRoot(base::FRect(0.0f, 0.0f, size))
          , mPool(FindMaxNumNodes(max_levels))
        {}
       ~QuadTree()
        {
            mRoot.Clear(mPool);
        }
        bool Insert(const base::FRect& rect, Object object)
        { return mRoot.Insert(rect, object, mPool, mMaxItems, mMaxLevels-1);  }
        void Clear()
        { mRoot.Clear(mPool); }
        template<typename Predicate>
        void Erase(const Predicate& predicate)
        { mRoot.Erase(predicate, mPool, mMaxItems); }

        const TreeNode& GetRoot() const
        { return mRoot; }
        const TreeNode* operator->() const
        { return &mRoot; }
        size_t GetSize() const
        { return mRoot.GetSize(); }

        static unsigned FindMaxNumNodes(unsigned levels)
        {
            unsigned nodes = 0;
            for (unsigned i = 0; i < levels; ++i)
            {
                nodes += std::pow(4, i);
            }
            return nodes;
        }
    private:
        const unsigned mMaxItems = 0;
        const unsigned mMaxLevels = 0;
        TreeNode mRoot;
        NodePool mPool;
    };

} // namespace
