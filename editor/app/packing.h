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


