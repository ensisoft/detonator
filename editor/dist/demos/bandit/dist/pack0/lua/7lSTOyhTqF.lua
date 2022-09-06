-- Entity 'Coin' script.

-- This script will be called for every instance of 'Coin'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.

-- Called when the game play begins for an entity in the scene.
function BeginPlay(coin, scene)

end

-- Called when the game play ends for an entity in the scene.
function EndPlay(coin, scene)

end

-- Called on every low frequency game tick.
function Tick(coin, game_time, dt)

end

-- Called on every iteration of the game loop.
function Update(coin, game_time, dt)

end

-- Called on every iteration of the game loop game
-- after *all* entities have been updated.
function PostUpdate(coin, game_time)
end

-- Called on collision events with other objects.
function OnBeginContact(coin, node, other, other_node)
end

-- Called on collision events with other objects.
function OnEndContact(coin, node, other, other_node)
end

-- Called on key down events.
function OnKeyDown(coin, symbol, modifier_bits)
end

-- Called on key up events.
function OnKeyUp(coin, symbol, modifier_bits)
end

-- Called on mouse button press events.
function OnMousePress(coin, mouse)
end

-- Called on mouse button release events.
function OnMouseRelease(coin, mouse)
end

-- Called on mouse move events.
function OnMouseMove(coin, mouse)
end

-- Called on game events.
function OnGameEvent(coin, event)
end

-- Called on animation finished events.
function OnAnimationFinished(coin, animation)
    coin:Die()
end

