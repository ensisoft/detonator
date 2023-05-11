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
#include <type_traits>
#include <optional>
#include <cmath>

#include "base/assert.h"
#include "base/types.h"
#include "base/utility.h"

namespace base
{
    namespace detail {
        template<typename T>
        struct DenseGridItemTraits {
            static constexpr auto CacheRect = true;
        };

        template<typename T, bool CacheRect>
        struct DenseGridItem;

        template<typename T>
        struct DenseGridItem<T, true> {
            base::FRect rect;
            T object;
        };
        template<typename T>
        struct DenseGridItem<T, false> {
            T object;
        };
    } // namespace

    template<typename Object>
    class DenseSpatialGrid
    {
    public:
        using ItemTraits = detail::DenseGridItemTraits<Object>;
        using ItemType   = detail::DenseGridItem<Object, ItemTraits::CacheRect>;

        static constexpr auto DefaultRows = 10;
        static constexpr auto DefaultCols = 10;
        DenseSpatialGrid() = default;
        explicit DenseSpatialGrid(const FRect& rect, unsigned rows=DefaultRows, unsigned cols=DefaultCols)
          : mRect(rect)
          , mRows(rows)
          , mCols(cols)
        { mGrid.resize(rows*cols); }
        DenseSpatialGrid(const FPoint& pos, const FSize& size, unsigned rows=DefaultRows, unsigned cols=DefaultCols)
          : mRect(pos, size)
          , mRows(rows)
          , mCols(cols)
        { mGrid.resize(rows*cols); }
        explicit DenseSpatialGrid(const FSize& size, unsigned rows=DefaultRows, unsigned cols=DefaultCols)
          : mRect(0.0f, 0.0f, size)
          , mRows(rows)
          , mCols(cols)
        { mGrid.resize(rows*cols); }
        DenseSpatialGrid(float width, float height, unsigned rows=DefaultRows, unsigned cols=DefaultCols)
          : mRect(0.0f, 0.0f, width, height)
          , mRows(rows)
          , mCols(cols)
        { mGrid.resize(rows*cols); }

        void Reshape(const FRect& rect, unsigned rows, unsigned cols) noexcept
        {
            mGrid.clear();
            mGrid.resize(rows * cols);
            mRect = rect;
            mRows = rows;
            mCols = cols;
        }

        bool Insert(const FRect& rect, Object object) noexcept
        {
            if (!Contains(mRect, rect))
                return false;

            for_each_cell(Intersect(mRect, rect), [&rect, object](const auto& items) {
                ItemType item;
                if constexpr (ItemTraits::CacheRect)
                    item.rect = rect;

                item.object = std::move(object);
                const_cast<ItemList&>(items).push_back(std::move(item));
                return true;
            });
            return true;
        }
        template<typename Predicate>
        void Erase(Predicate predicate)
        {
            for (auto& items : mGrid)
            {
                for (auto it=items.begin(); it != items.end();)
                {
                    auto& item = *it;
                    if constexpr (ItemTraits::CacheRect) {
                        if (predicate(item.object, item.rect))
                            it = items.erase(it);
                        else ++it;
                    } else {
                        if (predicate(item.object))
                            it = items.erase(it);
                        else ++it;
                    }
                }
            }
        }
        // Erase objects whose rects intersect with the given rectangle.
        void Erase(const base::FRect& rect) noexcept
        {
            const auto sub_rect = Intersect(mRect, rect);
            if (sub_rect.IsEmpty())
                return;

            for_each_cell(sub_rect, [&sub_rect](const auto& c_items) {
                auto& items = const_cast<ItemList&>(c_items);
                for (auto it = items.begin(); it != items.end();)
                {
                    const auto& item = *it;
                    const auto& item_rect = GetItemRect(item);
                    if (DoesIntersect(item_rect, sub_rect))
                        it = items.erase(it);
                    else ++it;
                }
                return true;
            });
        }
        // Erase objects whose rects contain the given point.
        void Erase(const base::FPoint& point)  noexcept
        {
            if (!mRect.TestPoint(point))
                return;

            const auto map = MapPoint(point);
            const auto row = map.GetY();
            const auto col = map.GetX();
            auto& items = base::SafeIndex(mGrid, row * mCols + col);
            for (auto it = items.begin(); it != items.end(); )
            {
                const auto& item = *it;
                const auto& item_rect = GetItemRect(item);
                if (item_rect.TestPoint(point))
                    it = items.erase(it);
                else ++it;
            }
        }

        void Clear() noexcept
        {
            for (auto& list : mGrid)
                list.clear();
        }

        template<typename RetObject>
        inline void Find(const FRect& rect, std::vector<RetObject>* result) const
        { find_objects_by_rect(rect, result); }
        template<typename RetObject>
        inline void Find(const FRect& rect, std::set<RetObject>* result) const
        { find_objects_by_rect(rect, result); }
        template<typename RetObject>
        inline void Find(const FRect& rect, std::unordered_set<Object*> result) const
        { find_objects_by_rect(rect, result); }

        enum class FindMode {
            Closest, All, First
        };

        template<typename RetObject>
        inline void Find(const FPoint& point, std::vector<RetObject>* result, FindMode mode = FindMode::All) const
        { find_objects_by_point(point, result, mode); }
        template<typename RetObject>
        inline void Find(const FPoint& point, std::set<RetObject>* result, FindMode mode = FindMode::All) const
        { find_objects_by_point(point, result, mode); }
        template<typename RetObject>
        inline void Find(const FPoint& point, std::unordered_set<RetObject>* result, FindMode mode = FindMode::All) const
        { find_objects_by_point(point, result, mode); }

        template<typename RetObject>
        inline void Find(const FPoint& point, float radius, std::vector<RetObject>* result, FindMode mode = FindMode::All) const
        { find_objects_by_point_radius(point, radius, result, mode); }
        template<typename RetObject>
        inline void Find(const FPoint& point, float radius, std::set<RetObject>* result, FindMode mode = FindMode::All) const
        { find_objects_by_point_radius(point, radius, result, mode); }
        template<typename RetObject>
        inline void Find(const FPoint& point, float radius, std::unordered_set<RetObject>* result, FindMode mode = FindMode::All) const
        { find_objects_by_point_radius(point, radius, result, mode); }

        template<typename RetObject>
        inline void Find(const FPoint& a, const FPoint& b, std::vector<RetObject>* result, FindMode mode = FindMode::All) const
        { find_objects_by_line(a, b, result, mode); }
        template<typename RetObject>
        inline void Find(const FPoint& a, const FPoint& b, std::set<RetObject>* result, FindMode mode = FindMode::All) const
        { find_objects_by_line(a, b, result, mode); }
        template<typename RetObject>
        inline void Find(const FPoint& a, const FPoint& b, std::unordered_set<RetObject>* result, FindMode mode = FindMode::All) const
        { find_objects_by_line(a, b, result, mode); }

        Object& GetObject(unsigned row, unsigned col, unsigned item) noexcept
        { return base::SafeIndex(base::SafeIndex(mGrid, row*mCols+col), item).object; }
        const Object& GetObject(unsigned row, unsigned col, unsigned item) const noexcept
        { return base::SafeIndex(base::SafeIndex(mGrid, row*mCols+col), item).object; }
        unsigned GetNumItems(unsigned row, unsigned col) const noexcept
        { return base::SafeIndex(mGrid, row*mCols+col).size(); }
        unsigned GetNumCols() const noexcept
        { return mCols; }
        unsigned GetNumRows() const noexcept
        { return mRows; }
        unsigned GetNumItems() const noexcept
        {
            unsigned ret = 0;
            for (auto& items : mGrid)
                ret += items.size();

            return ret;
        }
        base::FRect GetRect() const noexcept
        { return mRect; }
        base::FRect GetRect(unsigned row, unsigned  col) noexcept
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
        // Map a space rect to a grid cell rect. The incoming rect must
        // be fully within the current space rect.
        base::URect MapRect(const FRect& rect) const noexcept
        {
            const auto cell_width = mRect.GetWidth() / mCols;
            const auto cell_height = mRect.GetHeight() / mRows;
            const auto xoffset = mRect.GetX();
            const auto yoffset = mRect.GetY();
            const auto top_xpos = rect.GetX() - xoffset;
            const auto top_ypos = rect.GetY() - yoffset;
            const auto bottom_xpos = top_xpos + rect.GetWidth();
            const auto bottom_ypos = top_ypos + rect.GetHeight();
            const auto top_xpos_cell = static_cast<unsigned>(top_xpos / cell_width);
            const auto top_ypos_cell = static_cast<unsigned>(top_ypos / cell_height);

            // this "obvious" computation is actually incorrect.
            // mapping the rect from floating point units to discrete cell units
            // requires rounding the width/height up to whole cells.
            // For example if rect that covers 2 cells horizontally could have
            // width of a 1 cell but the mapping in cell rect should have width of 2.
            //const auto width_cell  = static_cast<unsigned>(std::ceil(rect.GetWidth() / cell_width));
            //const auto height_cell = static_cast<unsigned>(std::ceil(rect.GetHeight() / cell_height));

            const auto bottom_xpos_cell = math::clamp(0u, mCols, static_cast<unsigned>(std::ceil(bottom_xpos / cell_width)));
            const auto bottom_ypos_cell = math::clamp(0u, mRows, static_cast<unsigned>(std::ceil(bottom_ypos / cell_height)));

            const auto width_cell  = bottom_xpos_cell - top_xpos_cell;
            const auto height_cell = bottom_ypos_cell - top_ypos_cell;
            // sanity
            ASSERT(top_xpos_cell + width_cell <= mCols);
            ASSERT(top_ypos_cell + height_cell <= mRows);

            return {top_xpos_cell, top_ypos_cell, width_cell, height_cell};
        }
        // Map a point in space to a grid cell. The point must be
        // within the current space rect.
        base::UPoint MapPoint(const FPoint& point) const noexcept
        {
            const auto cell_width = mRect.GetWidth() / mCols;
            const auto cell_height = mRect.GetHeight() / mRows;
            const auto xoffset = mRect.GetX();
            const auto yoffset = mRect.GetY();
            const auto xpos = point.GetX() - xoffset;
            const auto ypos = point.GetY() - yoffset;
            const auto top_xpos_cell = static_cast<unsigned>(xpos / cell_width);
            const auto top_ypos_cell = static_cast<unsigned>(ypos / cell_height);
            // sanity
            ASSERT(top_xpos_cell < mCols && top_ypos_cell < mRows);

            return {top_xpos_cell, top_ypos_cell};
        }

        bool IsValid() const noexcept
        { return mRows && mCols; }
    private:
        constexpr static decltype(auto) GetItemRect(const ItemType& item) noexcept
        {
            if constexpr (ItemTraits::CacheRect)
                return item.rect;
            else return GetODenseGridbjectRect(item.object);
        }
        template<typename Container>
        void find_objects_by_line(const FPoint& a, const FPoint& b, Container* result, FindMode mode) const
        {
            const FLine line(a, b);

            const auto& sub_rect = Intersect(mRect, line.Inscribe());
            if (sub_rect.IsEmpty())
                return;

            if (mode == FindMode::All)
            {
                for_each_cell(sub_rect, [&line, result](const auto& items) {
                    for (const auto& item : items)
                    {
                        const auto& item_rect = GetItemRect(item);
                        if (DoesIntersect(item_rect, line))
                            store_result(item.object, result);
                    }
                    return true;
                });
            }
            else if (mode == FindMode::First)
            {
                for_each_cell(sub_rect, [&line, result](const auto& items) {
                    for (const auto& item : items)
                    {
                        const auto& item_rect = GetItemRect(item);
                        if (!DoesIntersect(item_rect, line))
                            continue;

                        store_result(item.object, result);
                        return false;
                    }
                    return true;
                });
            }
            else if (mode == FindMode::Closest)
            {
                float best_dist = std::numeric_limits<float>::max();
                std::optional<Object> best_found;

                for_each_cell(sub_rect, [&line, &best_found, &best_dist](const auto& items) {
                    for (const auto& item : items)
                    {
                        const auto& item_rect = GetItemRect(item);
                        if (!DoesIntersect(item_rect, line))
                            continue;
                        const float dist = SquareDistance(line.GetPointA(), item_rect.GetCenter());
                        if (dist < best_dist)
                        {
                            best_found = item.object;
                            best_dist  = dist;
                        }
                    }
                    return true;
                });
                if (best_found)
                    store_result(best_found.value(), result);

            } else BUG("Missing FindMode implementation.");
        }

        template<typename Container>
        void find_objects_by_rect(const FRect& rect, Container* result) const
        {
            const auto& sub_rect = Intersect(mRect, rect);
            if (sub_rect.IsEmpty())
                return;

            for_each_cell(sub_rect, [&sub_rect, result](const auto& items) {
                for (auto& item : items)
                {
                    const auto& item_rect = GetItemRect(item);
                    if (DoesIntersect(item_rect, sub_rect))
                        store_result(item.object, result);
                }
                return true;
            });
        }
        template<typename Container>
        void find_objects_by_point(const FPoint& point, Container* result, FindMode mode) const
        {
            if (!mRect.TestPoint(point))
                return;

            const auto map = MapPoint(point);
            const auto col = map.GetX();
            const auto row = map.GetY();
            const auto& items = mGrid[row * mCols + col];
            if (items.empty())
                return;

            if (mode == FindMode::All)
            {
                for (const auto& item: items)
                {
                    const auto& item_rect = GetItemRect(item);
                    if (item_rect.TestPoint(point))
                        store_result(item.object, result);
                }
            }
            else if (mode == FindMode::Closest)
            {
                float best_dist = std::numeric_limits<float>::max();
                std::optional<Object> best_object;

                for (const auto& item : items)
                {
                    const auto& item_rect = GetItemRect(item);
                    if (!item_rect.TestPoint(point))
                        continue;

                    const float dist = SquareDistance(point, item_rect.GetCenter());
                    if (dist < best_dist)
                    {
                        best_object = item.object;
                        best_dist   = dist;
                    }
                }
                if (best_object)
                    store_result(best_object.value(), result);

            }
            else if (mode == FindMode::First)
            {
                for (const auto& item : items)
                {
                    const auto& item_rect = GetItemRect(item);
                    if (!item_rect.TestPoint(point))
                        continue;

                    store_result(item.object, result);
                    return;
                }
            }
            else BUG("Missing FindMode implementation");
        }

        template<typename Container>
        void find_objects_by_point_radius(const FPoint& point, float radius, Container* result, FindMode mode) const
        {
            const FCircle circle(point, radius);

            // todo: this is sub-optimal since we're going to check all the cells
            // that are within the rectangle that contains the line segment completely.
            // this means that for a diagonal line we're going to end up checking cells
            // that are not actually intersecting with the  line proper.
            // Better algorithm would consider less grid cells in the first place.
            const auto& sub_rect = Intersect(mRect, circle.Inscribe());
            if (sub_rect.IsEmpty())
                return;

            if (mode == FindMode::All)
            {
                for_each_cell(sub_rect, [&circle, result](const auto& items) {
                    for (const auto& item: items)
                    {
                        const auto& item_rect = GetItemRect(item);
                        if (DoesIntersect(item_rect, circle))
                            store_result(item.object, result);
                    }
                    return true;
                });
            }
            else if (mode == FindMode::First)
            {
                for_each_cell(sub_rect, [&circle, result](const auto& items) {
                    for (const auto& item : items)
                    {
                        const auto& item_rect = GetItemRect(item);
                        if (!DoesIntersect(item_rect, circle))
                            continue;

                        store_result(item.object, result);
                        return false;
                    }
                    return true;
                });
            }
            else if (mode == FindMode::Closest)
            {
                float best_dist = std::numeric_limits<float>::max();
                std::optional<Object> best_found;
                for_each_cell(sub_rect, [&circle, &best_found, &best_dist](const auto& items) {
                    for (const auto& item : items)
                    {
                        const auto& item_rect = GetItemRect(item);
                        if (!DoesIntersect(item_rect, circle))
                            continue;

                        const float dist = SquareDistance(circle.GetCenter(), item_rect.GetCenter());
                        if (dist < best_dist)
                        {
                            best_found = item.object;
                            best_dist  = dist;
                        }
                    }
                    return true;
                });
                if (best_found)
                    store_result(best_found.value(), result);

            } else BUG("Missing FindMode implementation");
        }
        template<typename Callback>
        void for_each_cell(const FRect& sub_rect, Callback callback) const
        {
            const auto map_rect = MapRect(sub_rect);
            const auto start_x = map_rect.GetX();
            const auto start_y = map_rect.GetY();
            const auto end_x = start_x + map_rect.GetWidth();
            const auto end_y = start_y + map_rect.GetHeight();

            for (unsigned row=start_y; row<end_y; ++row)
            {
                for (unsigned col=start_x; col<end_x; ++col)
                {
                    ASSERT(row * mCols + col < mGrid.size());
                    const auto& items = mGrid[row * mCols + col];
                    if (!callback(items))
                        return;
                }
            }
        }

        template<typename SrcObject, typename RetObject> inline
        static void store_result(SrcObject object, std::vector<RetObject>* vector)
        { vector->push_back(std::move(object)); }
        template<typename SrcObject, typename RetObject> inline
        static void store_result(SrcObject object, std::set<RetObject>* set)
        { set->insert(std::move(object)); }
        template<typename SrcObject, typename RetObject> inline
        static void store_result(SrcObject object, std::unordered_set<RetObject>* set)
        { set->insert(std::move(object)); }
    private:
        using ItemList = std::vector<ItemType>;
        FRect mRect;
        unsigned mRows = 0;
        unsigned mCols = 0;
        std::vector<ItemList> mGrid;
    };
} // namespace
