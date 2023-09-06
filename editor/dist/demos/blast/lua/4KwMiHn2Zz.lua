-- Entity 'Bullet' script.
-- This script will be called for every instance of 'Bullet'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.
require('common')

-- Called when the game play begins for a scene.
function BeginPlay(bullet, scene, map)
end

-- Called on every low frequency game tick.
function Tick(bullet, game_time, dt)
end

-- Called on every iteration of game loop.
function Update(bullet, game_time, dt)
    local bullet_node = bullet:GetNode(0)
    local bullet_pos = bullet_node:GetTranslation()
    local velocity = bullet.velocity
    bullet_pos.y = bullet_pos.y + dt * velocity
    bullet_node:SetTranslation(bullet_pos)
end

function PostUpdate(bullet, game_time)
    if bullet:IsDying() then
        return
    end

    -- check if this bullet is hitting anything
    local bullet_node = bullet:GetNode(0)
    local bullet_pos = bullet_node:GetTranslation()

    if bullet_pos.y > 400.0 then
        return
    end
    if bullet_pos.y < -400.0 then
        return
    end

    local query_ret = Scene:QuerySpatialNodes(bullet_pos, 'Closest')

    -- invoke BulletHit on every entity we have hit
    while query_ret:HasNext() do
        local node = query_ret:Get()
        local entity = node:GetEntity()
        CallMethod(entity, 'BulletHit', bullet)
        query_ret:Next()
    end
end

-- Called on collision events with other objects.
function OnBeginContact(bullet, node, other, other_node)
end

-- Called on collision events with other objects.
function OnEndContact(bullet, node, other, other_node)
end

