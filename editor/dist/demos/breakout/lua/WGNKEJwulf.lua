-- Scene 'Level 0' script.

-- This script will be called for every instance of 'Level 0'
-- during gameplay.
-- You're free to delete functions you don't need.

-- Called when the scene begins play.
function BeginPlay(level)
end

-- Called when the scene ends play.
function EndPlay(level)
end

-- Called when a new entity has been spawned in the scene.
function SpawnEntity(level, entity)

end

-- Called when an entity has been killed from the scene.
function KillEntity(level, carcass)
    local num_bricks   = 0
    local num_entities = level:GetNumEntities()
    for i=0, num_entities-1, 1 do
        local entity = level:GetEntity(i)
        local klass  = entity:GetClassName()
        if string.match(klass, "Brick") then
            if entity:HasBeenKilled() == false and entity.immortal == false then
                num_bricks = num_bricks + 1
            end
        end
    end
    Game:DebugPrint("Bricks" .. tostring(num_bricks))
    if num_bricks ~= 0 then
        return
    end
    Game:DebugPrint("Level clear!")
    Audio:PlaySoundEffect('Level Clear')
end

-- Called on every low frequency game tick.
function Tick(level, game_time, dt)
end

-- Called on every iteration of game loop.
function Update(level, game_time, dt)
end

-- Called on collision events with other objects.
function OnBeginContact(level, entity, entity_node, other, other_node)
end

-- Called on collision events with other objects.
function OnEndContact(level, entity, entity_node, other, other_node)
end

-- Called on key down events.
function OnKeyDown(level, symbol, modifier_bits)
end

-- Called on key up events.
function OnKeyUp(level, symbol, modifier_bits)
end

-- Called on mouse button press events.
function OnMousePress(level, mouse)
end

-- Called on mouse button release events.
function OnMouseRelease(level, mouse)
end

-- Called on mouse move events.
function OnMouseMove(level, mouse)
end

