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

// todo refactor the include away
#include "graphics/transform.h"

namespace game
{
    // provide type aliases for now for these types so that we can
    // use them as if they weren't in graphics where they shouldn't
    // be for most of the use in engine code. (i.e. not related to
    // graphics in any way)
    // todo: eventually should refactor them out of graphics/ into base/
    using Transform = gfx::Transform;

} // game
