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

#include "engine/entity_script.h"

namespace blast
{
    class EnemyShip : public engine::EntityScript
    {
    public:
        void Update(game::Entity& entity, double game_time, double dt) override;
    };
} // namespace
