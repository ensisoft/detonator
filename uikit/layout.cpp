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

#include "config.h"

#include "base/assert.h"
#include "uikit/widget.h"
#include "uikit/layout.h"

namespace uik
{

GridLayout::GridLayout(unsigned rows, unsigned cols)
{
    Resize(rows, cols);
}

void GridLayout::Resize(unsigned rows, unsigned cols)
{
    mItems.resize(rows * cols);
    mRows = rows;
    mCols = cols;
}

void GridLayout::Arrange(const FSize& size)
{
    Arrange(FPoint(0.0f, 0.0f), size);
}

void GridLayout::Arrange(const FPoint& origin, const FSize& size)
{
    const auto cell_width  = size.GetWidth() / mCols;
    const auto cell_height = size.GetHeight() / mRows;
    for (unsigned row = 0; row < mRows; ++row)
    {
        for (unsigned  col=0; col < mCols; ++col)
        {
            const auto index  = row * mCols + col;
            const auto& cell_origin = FPoint(origin.GetX() + col * cell_width,
                                             origin.GetY() + row * cell_height);
            const auto& cell_size = FSize(cell_width, cell_height);
            auto& item  = mItems[index];
            if (auto* ptr = std::get_if<Widget*>(&item))
            {
                (*ptr)->SetSize(cell_size);
                (*ptr)->SetPosition(cell_origin);
            }
            else if (auto* ptr = std::get_if<GridLayout>(&item))
            {
                // recurse into the layout if the given cell contains another
                // layout further subdividing this cell into smaller cells.
                ptr->Arrange(cell_origin, cell_size);
            }
        }
    }
}

} // namespace
