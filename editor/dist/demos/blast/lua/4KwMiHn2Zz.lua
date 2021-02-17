-- Entity 'Bullet' script.

-- This script will be called for every instance of 'Bullet'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.

require('common')


-- Called when the game play begins for a scene.
function BeginPlay(bullet, scene)
end

-- Called on every low frequency game tick.
function Tick(bullet, wall_time, tick_time, dt)
end

-- Called on every iteration of game loop.
function Update(bullet, wall_time, game_time, dt)
    local bullet_node = bullet:FindNodeByClassName('Bullet')
    local bullet_pos  = bullet_node:GetTranslation()
    local velocity    = bullet.velocity
    bullet_pos.y =  bullet_pos.y + dt * velocity
    bullet_node:SetTranslation(bullet_pos)
    if bullet_pos.y < -500 or bullet_pos.y > 500 then
        Scene:KillEntity(bullet)
        return
    end

    -- check if this bullet is hitting anything
    local num_entities = Scene:GetNumEntities()
    for i=0, num_entities-1, 1 do
        local entity = Scene:GetEntity(i)
        if entity:GetClassName() == 'Ship2' and bullet.red then
            -- player's bullet hit invader ship
            local ship_mat = Scene:FindEntityNodeTransform(entity, entity:FindNodeByClassName('Body'))
            local ship_pos = ship_mat:GetTranslation()
            local distance = glm.length(ship_pos - bullet_pos)
            if (distance <= 50.0) then
                Game:DebugPrint('enemy was hit!')
                Scene:KillEntity(entity)
                Scene:KillEntity(bullet)
                SpawnExplosion(ship_pos, 'BlueExplosion')
            end
        elseif entity:GetClassName() == 'Player' and bullet.red == false then
            -- invader ship bullet hit player's ship
            local ship_mat = Scene:FindEntityNodeTransform(entity, entity:FindNodeByClassName('Body'))
            local ship_pos = ship_mat:GetTranslation()
            local distance = glm.length(ship_pos - bullet_pos)
            if (distance <= 50.0) then
                Game:DebugPrint('you were hit!')
                Scene:KillEntity(entity)
                SpawnExplosion(ship_pos, 'RedExplosion')
            end
        elseif entity:GetClassName() == 'Asteroid' and bullet.red then
            local rock_mat = Scene:FindEntityNodeTransform(entity, entity:FindNodeByClassName('Body'))
            local rock_pos = rock_mat:GetTranslation()
            local distance = glm.length(rock_pos -bullet_pos)
            local radius   = entity.radius
            if (distance <= radius) then
                Scene:KillEntity(bullet)
            end
        end
    end
end

-- Called on collision events with other objects.
function OnBeginContact(bullet, node, other, other_node)
end

-- Called on collision events with other objects.
function OnEndContact(bullet, node, other, other_node)
end

