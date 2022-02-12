-- Scene 'Game' script.

-- This script will be called for every instance of 'Game'
-- during gameplay.
-- You're free to delete functions you don't need.

-- Called when the scene begins play.
function BeginPlay(game)
    local player = game:FindEntityByInstanceName('Player')
    player:PlayAnimationByName('Fly In')
end

-- Called when the scene ends play.
function EndPlay(game)
end
            
-- Called when a new entity has been spawned in the scene.
function SpawnEntity(game, entity)

end

-- Called when an entity has been killed from the scene.
function KillEntity(game, entity)

end

-- Called on every low frequency game tick.
function Tick(game, game_time, dt)

end

-- Called on every iteration of game loop.
function Update(game, game_time, dt)

end

-- Called on collision events with other objects.
function OnBeginContact(game, entity, entity_node, other, other_node)
end

-- Called on collision events with other objects.
function OnEndContact(game, entity, entity_node, other, other_node)
end

-- Called on key down events.
function OnKeyDown(game, symbol, modifier_bits)
end

-- Called on key up events.
function OnKeyUp(game, symbol, modifier_bits)
end

-- Called on mouse button press events.
function OnMousePress(game, mouse)
end

-- Called on mouse button release events.
function OnMouseRelease(game, mouse)
end

-- Called on mouse move events.
function OnMouseMove(game, mouse)
end

-- Called on game events.
function OnGameEvent(game, event)
end

