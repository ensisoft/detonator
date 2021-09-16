-- Entity 'RedMine' script.

-- This script will be called for every instance of 'RedMine'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.

require('common')

local velocity = -100

function Explode(redmine, mine_pos)
    Game:DebugPrint('KAPOW')
    Scene:KillEntity(redmine)
    local blast_radius = ClassLib:FindEntityClassByName('Shockwave')
    local args = game.EntityArgs:new()
    args.class = blast_radius
    args.name = 'blast radius'
    args.position = mine_pos
    args.rotation = util.Random(0.0, 3.1456*2.0)
    Scene:SpawnEntity(args, true)

    -- which enemy ships are close enough to be affected
    -- by the mine, i.e. are within the mine radius
    local num_entities = Scene:GetNumEntities()
    for i=0, num_entities-1, 1 do
        local enemy = Scene:GetEntity(i)
        if enemy:GetClassName() == 'Ship2' then
            local ship_node = enemy:FindNodeByClassName('Body')
            local ship_pos  = ship_node:GetTranslation()
            local distance = glm.length(mine_pos - ship_pos)
            if distance <= 250 then
                Scene:KillEntity(enemy)
                SpawnExplosion(ship_pos, 'BlueExplosion')
            end
        end
    end
    if State.play_effects then 
        Audio:PlaySoundEffect('Mine Explosion')
    end
end

-- Called when the game play begins for a scene.
function BeginPlay(redmine, scene)
    if State.play_effects then 
        Audio:PlaySoundEffect('Mine Launch')
    end
end

-- Called on every low frequency game tick.
function Tick(redmine, game_time, dt)

end

-- Called on every iteration of game loop.
function Update(redmine, game_time, dt)
    local mine_body = redmine:FindNodeByClassName('Body')
    local mine_pos  = mine_body:GetTranslation()
    if redmine:HasExpired() then
        Explode(redmine, mine_pos)
        return
    end

    local num_entities = Scene:GetNumEntities()
    for i=0, num_entities-1, 1 do
        local enemy = Scene:GetEntity(i)
        if enemy:GetClassName() == 'Ship2' then
            local ship_body = enemy:FindNodeByClassName('Body')
            local ship_pos  = ship_body:GetTranslation()
            local distance = glm.length(mine_pos - ship_pos)
            if distance <= 50 then
                Explode(redmine, mine_pos)
                return
            end
        end
    end

    mine_pos.y = mine_pos.y + dt * velocity;
    mine_body:SetTranslation(mine_pos)
    if mine_pos.y < -500 then
        Scene:KillEntity(redmine)
        return
    end
end

-- Called on collision events with other objects.
function OnBeginContact(redmine, node, other, other_node)
end

-- Called on collision events with other objects.
function OnEndContact(redmine, node, other, other_node)
end

