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
#include <set>

#include "base/assert.h"
#include "base/memory.h"
#include "base/tree.h"
#include "base/types.h"
#include "base/utility.h"

namespace game
{
    template<typename T>
    using RenderTree = base::RenderTree<T>;

    namespace detail {
        template<typename Object>
        class QuadTreeNode
        {
        public:
            QuadTreeNode(const base::FRect& rect)
              : mRect(rect)
            {}
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
        private:
            struct Item {
                base::FRect rect;
                Object object;
            };
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
        QuadTree(float x, float y, float width, float, float height, unsigned max_items = DefaultMaxItems, unsigned max_levels = DefaultMaxLevels)
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
        bool Insert(const base::FRect& rect, Object object)
        { return mRoot.Insert(rect, object, mPool, mMaxItems, mMaxLevels-1);  }
        void Clear()
        { mRoot.Clear(mPool); }
        const TreeNode& GetRoot() const 
        { return mRoot; }
        const TreeNode* operator->() const
        { return &mRoot; }
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

    namespace detail {
        template<typename Object> inline
        void StoreObject(std::vector<Object>* vector, Object object)
        { vector->push_back(object); }
        template<typename Object> inline
        void StoreObject(std::set<Object>* set, Object object)
        { set->insert(object); }

        template<typename Object, typename Container>
        void QueryQuadTree(const base::FRect& area_of_interest, const QuadTreeNode<Object>& node, Container* result)
        {
            for (size_t i=0; i<node.GetNumItems(); ++i)
            {
                const auto& rect = node.GetItemRect(i);
                const auto& test = base::Intersect(area_of_interest, rect);
                if (!test.IsEmpty())
                    StoreObject(result, node.GetItemObject(i));
            }
            if (!node.HasChildren())
                return;
            for (size_t i=0; i<4; ++i)
            {
                const auto* quadrant = node.GetChildQuadrant(i);
                const auto& rect = quadrant->GetRect();
                const auto& area = base::Intersect(area_of_interest, rect);
                if (!area.IsEmpty())
                    QueryQuadTree(area, *quadrant, result);
            }
        }
    } // namespace


    template<typename Object>
    void QueryQuadTree(const base::FRect& area_of_interest, const QuadTree<Object>& quadtree,  std::vector<Object>* result)
    { detail::QueryQuadTree(area_of_interest, quadtree.GetRoot(), result); }

    template<typename Object>
    void QueryQuadTree(const base::FRect& area_of_interest, const QuadTree<Object>& quadtree, std::set<Object>* result)
    { detail::QueryQuadTree(area_of_interest, quadtree.GetRoot(), result); }

} // namespace
