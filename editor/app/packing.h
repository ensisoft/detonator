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

#pragma once

#include "config.h"

#include "warnpush.h"
#include "warnpop.h"

#include <any>
#include <vector>
#include <string>

namespace app
{
    // Abstract 2D object with a width and height and a name association
    // in order to establish a mapping to some other object.
    struct PackingRectangle {
        // x position of the named image in the container when packing is complete.
        unsigned xpos   = 0;
        // y position of the named image in the container when packing is complete
        unsigned ypos   = 0;
        // the width of the image
        unsigned width  = 0;
        // the height of the image
        unsigned height = 0;
        // success flag to indicate whether the object
        // was successfully packed or not.
        bool success    = false;
        // arbitrary user defined data.
        std::any data;
        // arbitrary user defined string.
        std::string cookie;
        // arbitrary user defined index.
        std::size_t index = 0;
    };

    struct RectanglePackSize {
        unsigned width  = 0;
        unsigned height = 0;
    };

    // Arrange the list of given rectangles so that they can all be laid out within
    // the 2 dimensional container.
    // the input list is mutated to so that each image is given the position
    // with the container by setting the x/ypos members.
    RectanglePackSize PackRectangles(std::vector<PackingRectangle>& list);

    bool PackRectangles(const RectanglePackSize& max, std::vector<PackingRectangle>& list,
        RectanglePackSize* ret_pack_size = nullptr);

} // namespce


