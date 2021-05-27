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
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <vector>
#include <memory>
#include <variant>

#include "base/assert.h"
#include "uikit/types.h"

// layout system for things such as a game menu.

namespace uik
{
    class Widget;

    // GridLayout divides space into columns and rows.
    // Each grid cell can then either contain a widget or another
    // layout further subdividing the space.
    class GridLayout
    {
    public:
        struct EmptyCell {};

        // Each item in the layout's layout grid is either
        // another layout, a cell data item (client defined) or an nothing
        // i.e. an empty cell.
        using LayoutItem = std::variant<EmptyCell, GridLayout, Widget*>;

        GridLayout() = default;
        GridLayout(unsigned rows, unsigned cols);

        void Resize(unsigned rows, unsigned cols);

        void Arrange(const FSize& size);
        void Arrange(const FPoint& origin, const FSize& size);

        void SetItem(unsigned row, unsigned col, const GridLayout& layout)
        {
            GetItem(row, col) = layout;
        }
        void SetItem(unsigned row, unsigned col, GridLayout&& layout)
        {
            GetItem(row, col) = std::move(layout);
        }
        void SetItem(unsigned row, unsigned col, Widget* widget)
        {
            GetItem(row, col) = widget;
        }
        void ClearCell(unsigned row, unsigned  col)
        {
            GetItem(row, col) = EmptyCell{};
        }
        // Returns true if the layout cell holds a layout.
        bool HoldsLayout(unsigned row, unsigned col) const
        {
            const auto& item = GetItem(row, col);
            return std::holds_alternative<GridLayout>(item);
        }
        // Returns true if the layout cell holds an item.
        bool HoldsWidget(unsigned row, unsigned col) const
        {
            const auto& item = GetItem(row, col);
            return std::holds_alternative<Widget*>(item);
        }
        bool HoldsNothing(unsigned row, unsigned col) const
        {
            const auto& item = GetItem(row, col);
            return std::holds_alternative<EmptyCell>(item);
        }
        bool IsCellEmpty(unsigned row, unsigned col) const
        {
            const auto& item = GetItem(row, col);
            return std::holds_alternative<EmptyCell>(item);
        }

        LayoutItem& GetItem(unsigned row, unsigned col)
        {
            ASSERT(row < mRows && col < mCols);
            return mItems[row * mCols + col];
        }
        LayoutItem& GetItem(std::size_t index)
        {
            ASSERT(index < mItems.size());
            return mItems[index];
        }
        const LayoutItem& GetItem(unsigned row, unsigned col) const
        {
            ASSERT(row < mRows && col < mCols);
            return mItems[row * mCols + col];
        }
        const LayoutItem& GetItem(std::size_t index) const
        {
            ASSERT(index < mItems.size());
            return mItems[index];
        }

        Widget* GetWidget(unsigned row, unsigned col)
        {
            ASSERT(HoldsWidget(row, col));
            auto& item = GetItem(row, col);
            return std::get<Widget*>(item);
        }
        const Widget* GetWidget(unsigned row, unsigned col) const
        {
            ASSERT(HoldsWidget(row, col));
            const auto& item = GetItem(row, col);
            return std::get<Widget*>(item);
        }

        size_t GetNumItems() const
        { return mItems.size(); }
        unsigned GetNumRows() const
        { return mRows; }
        unsigned GetNumCols() const
        { return mCols; }
        bool IsEmpty() const
        { return mRows == 0 && mCols == 0; }

    private:
        unsigned mRows = 0;
        unsigned mCols = 0;
        std::vector<LayoutItem> mItems;
    };

} // namespace
