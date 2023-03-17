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
#include "warnpop.h"

#include <memory>
#include <set>
#include <vector>
#include <cstddef>

#include "base/grid.h"
#include "game/tree.h"
#include "game/treeop.h"
#include "game/types.h"
#include "game/entity.h"

namespace game
{
    template<typename T>
    class SpatialIndex
    {
    public:
        struct Item {
            T* object;
            FRect rect;
        };

        virtual ~SpatialIndex() = default;
        virtual void Insert(const FRect& rect, const std::vector<Item>& items) = 0;
        virtual void Erase(const std::set<T*>& killset) = 0;

        // Query interface functions for specific query parameters
        // and result container types.
        inline void Query(const FRect& area, std::set<T*>* result)
        { query(area, result); }
        inline void Query(const FRect& area, std::set<const T*>*result) const
        { query(area, result); }
        inline void Query(const FRect& area, std::vector<T*>* result)
        { query(area, result); }
        inline void Query(const FRect& area, std::vector<const T*>*result) const
        { query(area, result); }
        inline void Query(const FPoint& point, std::set<T*>* result)
        { query(point, result); }
        inline void Query(const FPoint& point, std::set<const T*>* result) const
        { query(point, result); }
        inline void Query(const FPoint& point, std::vector<T*>* result)
        { query(point, result); }
        inline void Query(const FPoint& point, std::vector<const T*>* result) const
        { query(point, result); }
    protected:
        class SpatialQuery {
        public:
            virtual ~SpatialQuery() = default;
            virtual void Execute(const base::QuadTree<T*>& tree) = 0;
            virtual void Execute(const base::DenseSpatialGrid<T*>& grid) = 0;
        private:
        };
        template<typename Predicate, typename ResultContainer>
        class QueryTemplate : public SpatialQuery {
        public:
            QueryTemplate(const Predicate& predicate, ResultContainer* result)
               : mPredicate(predicate)
               , mResult(result)
            {}
            virtual void Execute(const base::QuadTree<T*>& tree) override
            { QueryQuadTree(mPredicate, tree, mResult); }
            virtual void Execute(const base::DenseSpatialGrid<T*>& grid) override
            { grid.Find(mPredicate, mResult); }
        private:
            const Predicate mPredicate;
            ResultContainer* mResult = nullptr;
        };
    protected:
        virtual void ExecuteQuery(SpatialQuery& query) const = 0;
    private:
        template<typename Predicate, typename ResultContainer>
        void query(const Predicate& predicate, ResultContainer* container) const
        {
            QueryTemplate<Predicate, ResultContainer> q(predicate, container);
            ExecuteQuery(q);
        }
    };

    template<typename T>
    class QuadTreeIndex final : public SpatialIndex<T>
    {
    public:
        using Item = typename SpatialIndex<T>::Item;

        QuadTreeIndex(unsigned max_items, unsigned max_levels)
          : mMaxItems(max_items)
          , mMaxLevels(max_levels)
        {}
        virtual void Insert(const FRect& rect, const std::vector<Item>& items) override
        {
            // todo: re-compute the tree parameters ?
            // todo: find some way to balance the tree ?
            mTree.Clear();
            mTree.Reshape(rect, mMaxItems, mMaxLevels);
            for (const auto& item : items)
                mTree.Insert(item.rect, item.object);
        }

        virtual void Erase(const std::set<T*>& killset) override
        {
            mTree.Erase([&killset](T* object, const base::FRect&) {
                return base::Contains(killset, object);
            });
        }
    protected:
        using SpatialQuery = typename SpatialIndex<T>::SpatialQuery;
        virtual void ExecuteQuery(SpatialQuery& query) const override
        { query.Execute(mTree); }
    private:
        const unsigned mMaxItems = 0;
        const unsigned mMaxLevels = 0;
        QuadTree<T*> mTree;
    };

    template<typename T>
    class DenseGridIndex final : public SpatialIndex<T>
    {
    public:
        using Item = typename SpatialIndex<T>::Item;

        DenseGridIndex(unsigned rows, unsigned cols)
          : mNumRows(rows)
          , mNumCols(cols)
        {}
        virtual void Insert(const FRect& rect, const std::vector<Item>& items) override
        {
            // todo: re-compute the grid parameters ?
            // todo: find some way to balance the grid ?
            mGrid.Clear();
            mGrid.Reshape(rect, mNumRows, mNumCols);
            for (const auto& item : items)
                mGrid.Insert(item.rect, item.object);
        }

        virtual void Erase(const std::set<T*>& killset) override
        {
            mGrid.Erase([&killset](T* object, const base::FRect& rect) {
                return base::Contains(killset, object);
            });
        }
    protected:
        using SpatialQuery = typename SpatialIndex<T>::SpatialQuery;
        virtual void ExecuteQuery(SpatialQuery& query) const override
        { query.Execute(mGrid); }
    private:
        const unsigned mNumRows = 0;
        const unsigned mNumCols = 0;
        base::DenseSpatialGrid<T*> mGrid;
    };

} // namespace
