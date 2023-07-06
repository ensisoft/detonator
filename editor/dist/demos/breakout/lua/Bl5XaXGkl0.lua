-- Entity 'Powerup Shrink' script.
-- This script will be called for every instance of 'Powerup Shrink'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.
-- Called when the game play begins for an entity in the scene.
function BeginPlay(powerup, scene, map)
    local node = powerup:GetNode(0)
    local body = node:GetRigidBody()
    body:AdjustLinearVelocity(0.0, 2.0)
end

-- Called when the game play ends for an entity in the scene.
function EndPlay(powerup_shrink, scene, map)

end

-- Called on every low frequency game tick.
function Tick(powerup_shrink, game_time, dt)

end

-- Called on every iteration of the game loop.
function Update(powerup_shrink, game_time, dt)

end

-- Called on every iteration of the game loop game
-- after *all* entities have been updated.
function PostUpdate(powerup_shrink, game_time)
end

-- Called on collision events with other objects.
function OnBeginContact(powerup_shrink, node, other, other_node)
end

-- Called on collision events with other objects.
function OnEndContact(powerup_shrink, node, other, other_node)
end

-- Called on key down events.
function OnKeyDown(powerup_shrink, symbol, modifier_bits)
end

-- Called on key up events.
function OnKeyUp(powerup_shrink, symbol, modifier_bits)
end

-- Called on mouse button press events.
function OnMousePress(powerup_shrink, mouse)
end

-- Called on mouse button release events.
function OnMouseRelease(powerup_shrink, mouse)
end

-- Called on mouse move events.
function OnMouseMove(powerup_shrink, mouse)
end

-- Called on game events.
function OnGameEvent(powerup_shrink, event)
end

-- Called on animation finished events.
function OnAnimationFinished(powerup_shrink, animation)
end

-- Called on timer events.
function OnTimer(powerup_shrink, timer, jitter)
end

-- Called on posted entity events.
function OnEvent(powerup_shrink, event)
end

