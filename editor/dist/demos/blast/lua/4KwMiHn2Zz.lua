-- Entity 'Bullet' script.

-- This script will be called for every instance of 'Bullet'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.

require('common')


-- Called when the game play begins for a scene.
function BeginPlay(bullet, scene)
end

-- Called on every low frequency game tick.
function Tick(bullet, game_time, dt)
end

-- Called on every iteration of game loop.
function Update(bullet, game_time, dt)
    local bullet_node = bullet:FindNodeByClassName('Bullet')
    local bullet_pos  = bullet_node:GetTranslation()
    local velocity    = bullet.velocity
    bullet_pos.y =  bullet_pos.y + dt * velocity
    bullet_node:SetTranslation(bullet_pos)
    if bullet_pos.y < -500 or bullet_pos.y > 500 then
        Scene:KillEntity(bullet)
        return
    end
end

function PostUpdate(bullet, game_time)   
    -- check if this bullet is hitting anything
    local bullet_node = bullet:FindNodeByClassName('Bullet')
    local bullet_pos  = bullet_node:GetTranslation()
    local query_ret   = Scene:QuerySpatialNodes(bullet_pos)
    while query_ret:HasNext() do
        local node = query_ret:Get()
        local entity = node:GetEntity()
        local node_pos = node:GetTranslation()
        if entity:GetClassName() == 'Ship2' and bullet.red then
            Game:DebugPrint('Enemy was hit!')
            Scene:KillEntity(entity)
            Scene:KillEntity(bullet)
            SpawnExplosion(node_pos, 'BlueExplosion')
            SpawnScore(node_pos, entity.score)
            local score = Scene:FindEntityByInstanceName('GameScore')
            local node  = score:FindNodeByClassName('text')
            local text  = node:GetTextItem()
            Scene.score = Scene.score + entity.score
            text:SetText(tostring(Scene.score))
        elseif entity:GetClassName() == 'Asteroid' and bullet.red then
            Scene:KillEntity(bullet)
        elseif entity:GetClassName() == 'Player' and bullet.red == false then 
            Game:DebugPrint('You were hit!')
            Scene:KillEntity(entity)
            SpawnExplosion(node_pos, 'RedExplosion')
        end
        query_ret:Next()     
    end
end

-- Called on collision events with other objects.
function OnBeginContact(bullet, node, other, other_node)
end

-- Called on collision events with other objects.
function OnEndContact(bullet, node, other, other_node)
end

