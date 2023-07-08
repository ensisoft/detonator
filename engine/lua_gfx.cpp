// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#include "warnpush.h"
#  include <sol/sol.hpp>
#include "warnpop.h"

#include "engine/lua.h"
#include "graphics/material.h"

namespace engine
{

void BindGFX(sol::state& L)
{
    // todo: add more stuff here if/when needed.

    auto table = L["gfx"].get_or_create<sol::table>();

    auto material_class = table.new_usertype<gfx::MaterialClass>("MaterialClass");
    material_class["GetName"] = &gfx::MaterialClass::GetName;
    material_class["GetId"] = &gfx::MaterialClass::GetId;

    auto material = table.new_usertype<gfx::Material>("Material");
}

} // namespace