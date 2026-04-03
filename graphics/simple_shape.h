// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#include <string>

#include "base/assert.h"
#include "base/utility.h"
#include "graphics/simple_shape_base.h"
#include "graphics/simple_shape_instance.h"
#include "graphics/simple_shapes.h"

namespace gfx
{
    inline SimpleShapeType GetSimpleShapeType(const Drawable& drawable)
    {
        if (const auto* ptr = dynamic_cast<const SimpleShapeInstance*>(&drawable))
            return ptr->GetShape();
        if (const auto* ptr = dynamic_cast<const SimpleShape*>(&drawable))
            return ptr->GetShape();
        BUG("Not a simple shape!");
    }

    inline float GetSimpleShapeAttribute(const Drawable& drawable, SimpleShapeAttribute attribute) noexcept
    {
        if (const auto* ptr = dynamic_cast<const SimpleShapeInstance*>(&drawable))
            return ptr->GetShapeAttribute(attribute);
        else if (const auto* ptr = dynamic_cast<const SimpleShape*>(&drawable))
            return ptr->GetShapeAttribute(attribute);
        BUG("Not a simple shape.");
    }

} // namespace
