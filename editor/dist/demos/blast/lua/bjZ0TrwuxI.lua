-- Entity 'Asteroid' script.

-- This script will be called for every instance of 'Asteroid'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.

require('common')

-- Called when the game play begins for a scene.
function BeginPlay(asteroid, scene)

end

-- Called on every low frequency game tick.
function Tick(asteroid, wall_time, tick_time, dt)

end

-- Called on every iteration of game loop.
function Update(asteroid, wall_time, game_time, dt)
    local speed = 300
    local rock_body  = asteroid:FindNodeByClassName('Body')
    local rock_pos   = rock_body:GetTranslation()
    rock_pos.y = rock_pos.y + dt * speed
    if rock_pos.y > 500 then
        Scene:KillEntity(asteroid)
    end
    rock_body:SetTranslation(rock_pos)

    -- player's ship can get hurt if hit by an asteroid!
    local ship = Scene:FindEntityByInstanceName('Player')
    if ship == nil then
        return
    end
    local ship_mat = Scene:FindEntityNodeTransform(ship, ship:FindNodeByClassName('Body'))
    local ship_pos = ship_mat:GetTranslation()
    local distance = glm.length(ship_pos - rock_pos)
    local radius   = asteroid.radius
    if distance <= radius then
        Game:DebugPrint('you were  killed by an asteroid!')
        Scene:KillEntity(asteroid)
        Scene:KillEntity(ship)
        SpawnExplosion(ship_pos, 'RedExplosion')
    end
end

-- Called on collision events with other objects.
function OnBeginContact(asteroid, node, other, other_node)
end

-- Called on collision events with other objects.
function OnEndContact(asteroid, node, other, other_node)
end

