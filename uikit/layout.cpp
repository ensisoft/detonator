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
