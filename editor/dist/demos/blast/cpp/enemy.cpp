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

#include "base/logging.h"
#include "game/entity.h"
#include "game/entity_node.h"
#include "game/entity_node_text_item.h"
#include "game/scriptvar.h"
#include "engine/context.h"
#include "enemy.h"

namespace blast
{
void SpawnExplosion(const glm::vec2& pos, const std::string& klass)
{
    game::EntityArgs args;
    args.name     = "explosion";
    args.position = pos;
    args.render_layer = 1;
    args.async_spawn  = true;
    engine::SpawnEntity(args, klass);
}

void SpawnScore(const glm::vec2& pos, int score)
{
    game::EntityArgs args;
    args.name         = "score";
    args.position     = pos;
    args.render_layer = 1;
    args.async_spawn  = false;
    auto* entity = engine::SpawnEntity(args, "Score");

    auto& node = entity->GetNode(0);
    auto* text = node.GetTextItem();
    text->SetText(std::to_string(score));
}

void BroadcastDeath(int score)
{
    engine::GameEvent ev;
    ev.from    = "enemy";
    ev.message = "dead";
    ev.to      = "game";
    ev.values["value"] = score;
    engine::PostEvent(ev);
}

void EnemyShip::Update(game::Entity& entity, double game_time, double delta)
{
    const float dt = delta;

    auto& ship_body = entity.GetNode(0);
    auto ship_pos = ship_body.GetTranslation();
    auto ship_rot = ship_body.GetRotation();

    if (entity.IsDying())
    {
        if (ship_pos.y > 500.0f)
            return;

        const auto type  = entity.GetVar<std::string>("type");
        const auto score = entity.GetVar<int>("score");

        SpawnExplosion(ship_pos, "Enemy/Explosion/" + type);
        SpawnScore(ship_pos, score);
        BroadcastDeath(score);
        return;
    }

    // update x,y position
    auto velocity = entity.GetVar<glm::vec2>("velocity");
    auto rotation = entity.GetVar<float>("rotation");

    ship_pos += (velocity * dt);
    ship_rot += (rotation * dt);

    // change direction at the boundaries
    if (ship_pos.x >= 550.0f)
    {
        velocity.x *= -1.0f;
        ship_pos.x = 550.0f;
    }
    else if (ship_pos.x <= -550.0f)
    {
        velocity.x *= -1.0f;
        ship_pos.x = -550.0f;
    }

    entity.SetVar("velocity", velocity);

    ship_body.SetTranslation(ship_pos);
    ship_body.SetRotation(ship_rot);

    // if the ship reached the end of the space then kill it
    if (ship_pos.y > 500.0f)
    {
        entity.Die();
    }
}

} // namespace
