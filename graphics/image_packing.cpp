// Copyright (c) 2010-2019 Sami Väisänen, Ensisoft
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

#include <algorithm>
#include <memory>

#include "base/assert.h"
#include "image_packing.h"

namespace gfx {
namespace pack {

class Rectangle
{
public:
    Rectangle(unsigned x, unsigned y, unsigned width, unsigned height)
      : mXPos(x)
      , mYPos(y)
      , mWidth(width)
      , mHeight(height)
    {}

    bool Pack(NamedImage& img)
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
                    mRight = std::make_unique<Rectangle>(mXPos + w, mYPos, mWidth - w, h);
                if (!mBelow)
                    mBelow = std::make_unique<Rectangle>(mXPos, mYPos + h, mWidth, mHeight - h);
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
    void AccomodateBelow(std::unique_ptr<Rectangle> old_root)
    {
        ASSERT(mWidth > old_root->mWidth);
        ASSERT(mHeight == old_root->mHeight);
        mUsed  = true;
        const auto width = mWidth - old_root->mWidth;
        mRight = std::make_unique<Rectangle>(mXPos + old_root->mWidth, mYPos, width, mHeight);
        mBelow = std::move(old_root);
        mBelow->mXPos = mXPos;
        mBelow->mYPos = mYPos;
    }

    // grow height and place the old root to the right
    void AccomodateRight(std::unique_ptr<Rectangle> old_root)
    {
        ASSERT(mWidth  == old_root->mWidth);
        ASSERT(mHeight > old_root->mHeight);
        mUsed  = true;
        const auto height = mHeight - old_root->mHeight;
        mBelow = std::make_unique<Rectangle>(mXPos, mYPos + old_root->mHeight, mWidth, height);
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
    NamedImage* mImage = nullptr;
    bool mUsed = false;

    std::unique_ptr<Rectangle> mRight;
    std::unique_ptr<Rectangle> mBelow;
};

// Binary tree bin packing algorithm
// https://codeincomplete.com/posts/bin-packing/

Container PackImages(std::vector<NamedImage>& images)
{
    Container out;
    if (images.size() == 0)
        return out;
    else if (images.size() == 1)
    {
        out.height = images[0].height;
        out.width  = images[0].width;
        images[0].xpos = 0;
        images[0].ypos = 0;
        return out;
    }

    std::sort(std::begin(images), std::end(images),
        [](const NamedImage& first, const NamedImage& second) {
            const auto max0 = std::max(first.width, first.height);
            const auto max1 = std::max(second.width, second.height);
            return max0 < max1;
        });
    std::reverse(std::begin(images), std::end(images));

    auto root = std::make_unique<Rectangle>(0, 0, images[0].width, images[0].height);
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

        std::unique_ptr<Rectangle> new_root;

        if (should_grow_width)
        {
            const auto new_width  = root_width + img.width;
            const auto new_height = root_height;
            new_root = std::make_unique<Rectangle>(0, 0, new_width, new_height);
            new_root->AccomodateBelow(std::move(root));
        }
        else if (should_grow_height)
        {
            const auto new_width  = root_width;
            const auto new_height = root_height + img.height;
            new_root = std::make_unique<Rectangle>(0, 0, new_width, new_height);
            new_root->AccomodateRight(std::move(root));
        }
        else if (can_grow_height)
        {
            const auto new_width  = root_width;
            const auto new_height = root_height + img.height;
            new_root = std::make_unique<Rectangle>(0, 0, new_width, new_height);
            new_root->AccomodateRight(std::move(root));
        }
        else if (can_grow_width)
        {
            const auto new_width  = root_width + img.width;
            const auto new_height = root_height;
            new_root = std::make_unique<Rectangle>(0, 0, new_width, new_height);
            new_root->AccomodateBelow(std::move(root));
        }

        root = std::move(new_root);
        root->Pack(img);
    }
    root->Finalize();
    out.height = root->GetHeight();
    out.width  = root->GetWidth();
    return out;
}

} // namespace
} // namespace
