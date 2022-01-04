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
#include <tuple>
#include <set>

#include "base/assert.h"
#include "base/types.h"
#include "base/utility.h"

namespace base
{
    template<typename Object>
    class DenseSpatialGrid
    {
    public:
        static constexpr auto DefaultRows = 10;
        static constexpr auto DefaultCols = 10;
        DenseSpatialGrid(const FRect& rect, unsigned rows=DefaultRows, unsigned cols=DefaultCols)
          : mRect(rect)
          , mRows(rows)
          , mCols(cols)
        { mGrid.resize(rows*cols); }
        DenseSpatialGrid(const FPoint& pos, const FSize& size, unsigned rows=DefaultRows, unsigned cols=DefaultCols)
          : mRect(pos, size)
          , mRows(rows)
          , mCols(cols)
        { mGrid.resize(rows*cols); }
        DenseSpatialGrid(const FSize& size, unsigned rows=DefaultRows, unsigned cols=DefaultCols)
          : mRect(0.0f, 0.0f, size)
          , mRows(rows)
          , mCols(cols)
        { mGrid.resize(rows*cols); }
        DenseSpatialGrid(float width, float height, unsigned rows=DefaultRows, unsigned cols=DefaultCols)
          : mRect(0.0f, 0.0f, width, height)
          , mRows(rows)
          , mCols(cols)
        { mGrid.resize(rows*cols); }
        bool Insert(const FRect& rect, Object object)
        {
            if (!Contains(mRect, rect))
                return false;

            const auto [row, col] = GetCornerCell(rect);
            for (unsigned y=row; y<mRows; ++y)
            {
                for (unsigned x=col; x<mCols; ++x)
                {
                    const auto& cell = GetCellRect(y, x);
                    const auto& intersection = Intersect(cell, rect);
                    if (intersection.IsEmpty())
                        break;
                    Item item;
                    item.rect   = intersection;
                    item.object = std::move(object);
                    mGrid[y * mCols + x].push_back(std::move(item));
                }
                if (y+1<mRows)
                {
                    const auto& cell = GetCellRect(y+1, col);
                    if (!DoesIntersect(cell, rect))
                        break;
                }
            }
            return true;
        }
        template<typename Predicate>
        void Erase(const Predicate& predicate)
        {
            for (auto& items : mGrid)
            {
                for (auto it=items.begin(); it != items.end();)
                {
                    auto& item = *it;
                    if (predicate(item.object, item.rect))
                        it = items.erase(it);
                    else ++it;
                }
            }
        }

        template<typename RetObject>
        inline void FindObjects(const FRect& rect, std::vector<RetObject>* result) const
        { find_objects_rect(rect, result); }
        template<typename RetObject>
        inline void FindObjects(const FRect& rect, std::set<RetObject>* result) const
        { find_objects_rect(rect, result); }
        template<typename RetObject>
        inline void FindObjects(const FRect& rect, std::unordered_set<Object*> result) const
        { find_objects_rect(rect, result); }
        template<typename RetObject>
        inline void FindObjects(const FPoint& point, std::vector<RetObject>* result) const
        { find_objects_point(point, result); }
        template<typename RetObject>
        inline void FindObjects(const FPoint& point, std::set<RetObject>* result) const
        { find_objects_point(point, result); }
        template<typename RetObject>
        inline void FindObjects(const FPoint& point, std::unordered_set<RetObject>* result) const
        { find_objects_point(point, result); }

        void Clear()
        {
            for (auto& list : mGrid)
                list.clear();
        }

        const FRect& GetItemRect(unsigned row, unsigned col, unsigned item) const
        {  return base::SafeIndex(base::SafeIndex(mGrid, row*mCols+col), item).rect; }
        Object& GetItemObject(unsigned row, unsigned col, unsigned item)
        { return base::SafeIndex(base::SafeIndex(mGrid, row*mCols+col), item).object; }
        const Object& GetItemObject(unsigned row, unsigned col, unsigned item) const
        { return base::SafeIndex(base::SafeIndex(mGrid, row*mCols+col), item).object; }
        unsigned GetNumCols() const
        { return mCols; }
        unsigned GetNumRows() const
        { return mRows; }
    private:
        std::tuple<unsigned, unsigned> GetCell(const FPoint& point) const
        {
            const auto& local = mRect.MapToLocal(point);
            const auto x = local.GetX();
            const auto y = local.GetY();
            const auto cell_width  = mRect.GetWidth() / mCols;
            const auto cell_height = mRect.GetHeight() / mRows;
            const unsigned row = y / cell_height;
            const unsigned col = x / cell_width;
            ASSERT(row < mRows && col < mCols);
            return std::make_tuple(row, col);
        }

        std::tuple<unsigned, unsigned> GetCornerCell(const FRect& rect) const
        {
            return GetCell(rect.GetPosition());
        }
        FRect GetCellRect(unsigned row, unsigned col) const
        {
            ASSERT(row < mRows && col < mCols);
            const auto width  = mRect.GetWidth();
            const auto height = mRect.GetHeight();
            const auto cell_width  = width / mCols;
            const auto cell_height = height / mRows;
            FRect rect;
            rect.Move(mRect.GetPosition());
            rect.Resize(cell_width, cell_height);
            rect.Translate(col * cell_width, row * cell_height);
            return rect;
        }
        template<typename Container>
        void find_objects_rect(const FRect& rect, Container* result) const
        {
            if (!Contains(mRect, rect))
                return;
            const auto [row, col] = GetCornerCell(rect);
            for (unsigned y=row; y<mRows; ++y)
            {
                for (unsigned x=col; x<mCols; ++x)
                {
                    const auto& cell = GetCellRect(y, x);
                    const auto& intersection = Intersect(cell, rect);
                    if (intersection.IsEmpty())
                        break;
                    auto& items = mGrid[y * mCols + x];
                    for (auto& item : items)
                    {
                        if (DoesIntersect(item.rect, intersection))
                            store_result(item.object, result);
                    }
                }
                if (y+1<mRows)
                {
                    const auto& cell = GetCellRect(y+1, col);
                    if (!DoesIntersect(cell, rect))
                        break;
                }
            }
        }
        template<typename Container>
        void find_objects_point(const FPoint& point, Container* result) const
        {
            if (!mRect.TestPoint(point))
                return;
            const auto [row, col] = GetCell(point);
            auto& items = mGrid[row * mCols + col];
            for (auto& item : items) {
                if (item.rect.TestPoint(point))
                    store_result(item.object, result);
            }
        }
        template<typename SrcObject, typename RetObject> inline
        void store_result(SrcObject object, std::vector<RetObject>* vector) const
        { vector->push_back(std::move(object)); }
        template<typename SrcObject, typename RetObject> inline
        void store_result(SrcObject object, std::set<RetObject>* set) const
        { set->insert(std::move(object)); }
        template<typename SrcObject, typename RetObject> inline
        void store_result(SrcObject object, std::unordered_set<RetObject>* set) const
        { set->insert(std::move(object)); }
    private:
        struct Item {
            base::FRect rect;
            Object object;
        };
        using ItemList = std::vector<Item>;
        FRect mRect;
        unsigned mRows = 0;
        unsigned mCols = 0;
        std::vector<ItemList> mGrid;
    };
} // namespace
