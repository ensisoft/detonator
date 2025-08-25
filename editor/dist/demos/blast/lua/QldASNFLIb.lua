-- Entity 'RedMine' script.
-- This script will be called for every instance of 'RedMine'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.
require('common')

local velocity = -100

function Explode(redmine, mine_pos)
    trace.event('rocket explode')

    Game:DebugPrint('KAPOW')

    local args = game.EntityArgs:new()
    args.class = ClassLib:FindEntityClassByName('Rocket Explosion')
    args.name = 'blast radius'
    args.position = mine_pos
    args.layer = 4
    args.async = true
    Scene:SpawnEntity(args, true)

    -- which enemy ships are close enough to be affected
    -- by the mine, i.e. are within the mine radius
    local num_entities = Scene:GetNumEntities()
    for i = 0, num_entities - 1, 1 do
        local enemy = Scene:GetEntity(i)
        if enemy:GetTag() == '#enemy' then
            local ship_node = enemy:GetNode(0)
            local ship_pos = ship_node:GetTranslation()
            if (util.DistanceIsLessOrEqual(ship_pos, mine_pos, 550.0)) then
                CallMethod(enemy, 'RocketBlast', redmine)
            end
        end
    end

    Audio:PlaySoundEffect('Rocket Explosion')

    redmine:Die()

    local event = game.GameEvent:new()
    event.from = 'rocket'
    event.message = 'explosion'
    event.to = 'game'
    Game:PostEvent(event)
end

-- Called when the game play begins for a scene.
function BeginPlay(redmine, scene, map)
    if State.play_effects then
        Audio:PlaySoundEffect('Rocket Launch')
    end
end

-- Called on every low frequency game tick.
function Tick(redmine, game_time, dt)

end

-- Called on every iteration of game loop.
function Update(rocket, game_time, dt)
    if rocket:IsDying() then
        return
    end

    local rocket_body = rocket:GetNode(0)
    local rocket_pos = rocket_body:GetTranslation()
    if rocket:HasExpired() then
        -- Explode(rocket, rocket_pos)
        rocket:Die()
        return
    end

    local num_entities = Scene:GetNumEntities()
    for i = 0, num_entities - 1, 1 do
        local enemy = Scene:GetEntity(i)
        if enemy:GetTag() == '#enemy' then
            local ship_body = enemy:GetNode(0)
            local ship_pos = ship_body:GetTranslation()
            if (util.DistanceIsLessOrEqual(ship_pos, rocket_pos, 50.0)) then
                Explode(rocket, rocket_pos)
                return
            end
        end
    end

    --    rocket_pos.y = rocket_pos.y + dt * velocity;
    --    rocket_body:SetTranslation(rocket_pos)
    if rocket_pos.y < -500 then
        Scene:KillEntity(rocket)
        return
    end
end

-- Called on collision events with other objects.
function OnBeginContact(redmine, node, other, other_node)
end

-- Called on collision events with other objects.
function OnEndContact(redmine, node, other, other_node)
end

