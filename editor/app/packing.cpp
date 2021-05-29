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

#define LOGTAG "workspace"

#include "config.h"

#include "warnpush.h"
#include "warnpop.h"

#include <algorithm>
#include <memory>

#include "editor/app/packing.h"
#include "base/assert.h"

namespace app
{

class SpacePartition
{
public:
    SpacePartition(unsigned x, unsigned y, unsigned width, unsigned height)
      : mXPos(x)
      , mYPos(y)
      , mWidth(width)
      , mHeight(height)
    {}

    bool Pack(PackingRectangle& img)
    {
        const auto w = img.width;
        const auto h = img.height;
        if (!mUsed)
        {
            const auto available_height = mBelow ? mHeight - mBelow->GetHeight() : mHeight;
            const auto available_width  = mRight ? mWidth - mRight->GetWidth() : mWidth;
            if (w <= available_width && h <= available_height)
            {
                if (!mRight)
                    mRight = std::make_unique<SpacePartition>(mXPos + w, mYPos, mWidth - w, h);
                if (!mBelow)
                    mBelow = std::make_unique<SpacePartition>(mXPos, mYPos + h, mWidth, mHeight - h);
                mImage  = &img;
                mUsed   = true;
                return true;
            }
        }
        else if (mRight && mRight->Pack(img))
            return true;
        else if (mBelow && mBelow->Pack(img))
            return true;
        return false;
    }
    unsigned GetHeight() const
    { return mHeight; }
    unsigned GetWidth() const
    { return mWidth; }

    // grow width and place old root to below
    void AccomodateBelow(std::unique_ptr<SpacePartition> old_root)
    {
        ASSERT(mWidth > old_root->mWidth);
        ASSERT(mHeight == old_root->mHeight);
        mUsed  = true;
        const auto width = mWidth - old_root->mWidth;
        mRight = std::make_unique<SpacePartition>(mXPos + old_root->mWidth, mYPos, width, mHeight);
        mBelow = std::move(old_root);
        mBelow->mXPos = mXPos;
        mBelow->mYPos = mYPos;
    }

    // grow height and place the old root to the right
    void AccomodateRight(std::unique_ptr<SpacePartition> old_root)
    {
        ASSERT(mWidth  == old_root->mWidth);
        ASSERT(mHeight > old_root->mHeight);
        mUsed  = true;
        const auto height = mHeight - old_root->mHeight;
        mBelow = std::make_unique<SpacePartition>(mXPos, mYPos + old_root->mHeight, mWidth, height);
        mRight = std::move(old_root);
        mRight->mXPos = mXPos;
        mRight->mYPos = mYPos;
    }
    void Finalize()
    {
        if (mImage)
        {
            mImage->xpos = mXPos;
            mImage->ypos = mYPos;
            mImage->success = true;
        }
        if (mRight)
            mRight->Finalize();
        if (mBelow)
            mBelow->Finalize();
    }

private:
    unsigned mXPos = 0;
    unsigned mYPos = 0;
    unsigned mWidth  = 0;
    unsigned mHeight = 0;
    PackingRectangle* mImage = nullptr;
    bool mUsed = false;

    std::unique_ptr<SpacePartition> mRight;
    std::unique_ptr<SpacePartition> mBelow;
};

// Binary tree bin packing algorithm
// https://codeincomplete.com/posts/bin-packing/

RectanglePackSize PackRectangles(std::vector<PackingRectangle>& images)
{
    RectanglePackSize ret;
    if (images.size() == 0)
        return ret;
    else if (images.size() == 1)
    {
        ret.height = images[0].height;
        ret.width  = images[0].width;
        images[0].xpos = 0;
        images[0].ypos = 0;
        return ret;
    }

    std::sort(std::begin(images), std::end(images),
        [](const PackingRectangle& first, const PackingRectangle& second) {
            const auto max0 = std::max(first.width, first.height);
            const auto max1 = std::max(second.width, second.height);
            return max0 < max1;
        });
    std::reverse(std::begin(images), std::end(images));

    auto root = std::make_unique<SpacePartition>(0, 0, images[0].width, images[0].height);
    for (auto& img : images)
    {
        if (root->Pack(img))
            continue;

        const auto root_width  = root->GetWidth();
        const auto root_height = root->GetHeight();

        // do we have enough width in order to grow height ?
        const auto can_grow_height = root_width >= img.width;
        // do we have enough height in order to grow width ?
        const auto can_grow_width  = root_height >= img.height;

        // preferred grow dimension
        const auto should_grow_width  = can_grow_width && root_height >= (root_width + img.width);
        const auto should_grow_height = can_grow_height && root_width >= (root_height + img.height);

        std::unique_ptr<SpacePartition> new_root;

        if (should_grow_width)
        {
            const auto new_width  = root_width + img.width;
            const auto new_height = root_height;
            new_root = std::make_unique<SpacePartition>(0, 0, new_width, new_height);
            new_root->AccomodateBelow(std::move(root));
        }
        else if (should_grow_height)
        {
            const auto new_width  = root_width;
            const auto new_height = root_height + img.height;
            new_root = std::make_unique<SpacePartition>(0, 0, new_width, new_height);
            new_root->AccomodateRight(std::move(root));
        }
        else if (can_grow_height)
        {
            const auto new_width  = root_width;
            const auto new_height = root_height + img.height;
            new_root = std::make_unique<SpacePartition>(0, 0, new_width, new_height);
            new_root->AccomodateRight(std::move(root));
        }
        else if (can_grow_width)
        {
            const auto new_width  = root_width + img.width;
            const auto new_height = root_height;
            new_root = std::make_unique<SpacePartition>(0, 0, new_width, new_height);
            new_root->AccomodateBelow(std::move(root));
        }

        root = std::move(new_root);
        root->Pack(img);
    }
    root->Finalize();
    ret.height = root->GetHeight();
    ret.width  = root->GetWidth();
    return ret;
}

bool PackRectangles(const RectanglePackSize& max, std::vector<PackingRectangle>& list,  RectanglePackSize* ret_pack_size)
{
    if (list.empty())
        return true;

    // sort into descending order (biggest ones first)
    std::sort(std::begin(list), std::end(list),
        [](const auto& lhs, const auto& rhs) {
            const auto max0 = std::max(lhs.width, lhs.height);
            const auto max1 = std::max(rhs.width, rhs.height);
            return max0 > max1;
        });
    bool all_ok = true;
    auto root = std::make_unique<SpacePartition>(0, 0, max.width, max.height);
    for (auto& img : list)
    {
        if (!root->Pack(img))
            all_ok = false;
    }
    root->Finalize();
    if (ret_pack_size)
    {
        // compute the actual minimum box based on the content
        unsigned width  = 0;
        unsigned height = 0;
        for (const auto& item : list)
        {
            const auto right_edge = item.xpos + item.width;
            const auto bottom_edge = item.ypos + item.height;
            width = std::max(right_edge, width);
            height = std::max(bottom_edge, height);
        }
        ret_pack_size->height = height;
        ret_pack_size->width  = width;
    }
    return all_ok;
}

} // namespace
