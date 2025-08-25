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
#include "base/math.h"
#include "game/scene.h"
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

void BasicEnemyMovement(game::Entity& entity, double game_time, double delta)
{
    const float dt = delta;
    auto& ship_body = entity.GetNode(0);
    auto ship_pos = ship_body.GetTranslation();
    auto ship_rot = ship_body.GetRotation();

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

void IntermediateEnemyMovement(game::Entity& entity, double game_time, double delta)
{
    const auto dt = static_cast<float>(delta);

    auto* scene = entity.GetScene();

    glm::vec2 target_pos = {0.0f, 600.0f};

    const auto& entities = scene->ListEntitiesByClassName("Player");
    if (!entities.empty())
    {
        auto* player = entities[0];
        const auto& player_node = player->GetNode(0);
        const auto& player_pos = player_node.GetTranslation();
        // move laterally to face the player
        target_pos = player_pos;
    }

    auto& entity_node = entity.GetNode(0);
    auto entity_pos = entity_node.GetTranslation();

    // see if there's a bullet we need to dodge
    const auto& bullets = scene->ListEntitiesByClassName("Bullet/Player");
    for (const auto& bullet : bullets)
    {
        const auto& bullet_node = bullet->GetNode(0);
        const auto& bullet_pos = bullet_node.GetTranslation();
        // if it's behind us, ignore it
        if (bullet_pos.y <= (entity_pos.y + 50.0f))
            continue;

        // if the bullet is going past us ignore it
        const auto safety_margin = std::abs(bullet_pos.x - entity_pos.x);
        if (safety_margin > 100.0f)
            continue;

        // take evasive action.
        const auto x = bullet_pos.x - entity_pos.x;
        if (x < 0.0f)
            target_pos += glm::vec2 { 100.0f, 0.0f };
        else target_pos += glm::vec2 { -100.0f, 0.0f };

        break;
    }

    // this clever trick here is called
    // "frame rate independent exponential smoothing"
    constexpr auto tracking_speed = 5.0f; // larger = snappier
    const float t = 1.0f - std::exp(-tracking_speed * dt);
    entity_pos.x = math::lerp(entity_pos.x, target_pos.x, t);

    constexpr float base_speed = 50.0f;   // downward speed (units/sec)
    constexpr float amplitude  = 200.0f;    // height of sine wave
    constexpr float frequency  = 2.0f;     // oscillations per second

    entity_pos.y += base_speed * dt;
    entity_pos.y += std::sin(game_time * frequency) * amplitude * dt;
    entity_node.SetTranslation(entity_pos);

    if (entity_pos.y > 500.0)
    {
        entity.Die();
    }
}

void EnemyShip::Update(game::Entity& entity, double game_time, double delta)
{

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

    const auto& name = entity.GetClassName();
    if (name == "Enemy/Basic")
        BasicEnemyMovement(entity, game_time, delta);
    else if (name == "Enemy/Intermediate")
        IntermediateEnemyMovement(entity, game_time, delta);
}

} // namespace
