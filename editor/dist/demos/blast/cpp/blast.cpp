// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-20215 Ensisoft http://www.ensisoft.com
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

#include "engine/game.h"
#include "enemy.h"

namespace engine
{

void GetEntityScripts(std::vector<EntityScriptRegistration>* out)
{
    // todo: currently adding native scripts is a manual process
    // and has to be done by hand. The editor doesn't know how to
    // add c++ code to the game engine build.
    {
        EntityScriptRegistration reg;
        reg.classId = "pNT0bA0bpw";
        reg.script  = std::make_unique<blast::EnemyShip>();
        out->push_back(std::move(reg));
    }

    {
        EntityScriptRegistration reg;
        reg.classId = "XSYiC5gMBy";
        reg.script  = std::make_unique<blast::EnemyShip>();
        out->push_back(std::move(reg));
    }

    {
        EntityScriptRegistration reg;
        reg.classId = "41Od2TRXue";
        reg.script = std::make_unique<blast::EnemyShip>();
        out->push_back(std::move(reg));
    }
}

} // namespace