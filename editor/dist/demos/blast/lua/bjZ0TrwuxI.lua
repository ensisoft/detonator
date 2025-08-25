-- Entity 'Asteroid' script.
-- This script will be called for every instance of 'Asteroid'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.
function BulletHit(asteroid, bullet)
    bullet:Die()

    if bullet.player == false then
        return
    end

    if asteroid:HasScheduledDeath() then
        return
    end

    local damage = asteroid.damage
    damage = damage + 20.0
    if damage >= 100.0 then
        asteroid:DieLater(2.0)
        asteroid:PlayAnimation('Explode')
        return
    end
    asteroid.damage = damage
end

-- Called when the game play begins for a scene.
function BeginPlay(asteroid, scene, map)
    local node = asteroid:GetNode(0)
    local mover = node:GetLinearMover()
    mover:SetLinearVelocity(0.0, asteroid.speed)
end

-- Called on every low frequency game tick.
function Tick(asteroid, game_time, dt)

end

-- Called on every iteration of game loop.
function Update(asteroid, game_time, dt)
end

function PostUpdate(asteroid, game_time, dt)

    if asteroid:HasScheduledDeath() then
        return
    end

    local asteroid_node = asteroid:GetNode(0)
    local asteroid_pos = asteroid_node:GetTranslation()

    -- player's ship can get hurt if hit by an asteroid!
    local player = Scene:FindEntity('Player')
    if player == nil then
        return
    end
    local player_node = player:GetNode(0)
    local player_pos = player_node:GetTranslation()

    if util.DistanceIsLessOrEqual(asteroid_pos, player_pos, asteroid.radius) then
        CallMethod(player, 'AsteroidHit', asteroid)
    end
end

-- Called on collision events with other objects.
function OnBeginContact(asteroid, node, other, other_node)
end

-- Called on collision events with other objects.
function OnEndContact(asteroid, node, other, other_node)
end

