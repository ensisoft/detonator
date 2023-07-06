-- Entity 'StoneBlock' script.
-- This script will be called for every instance of 'StoneBlock'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.
-- Called when the game play begins for an entity in the scene.
function BeginPlay(stoneblock, scene, map)

end

-- Called when the game play ends for an entity in the scene.
function EndPlay(stoneblock, scene, map)

end

-- Called on every low frequency game tick.
function Tick(stoneblock, game_time, dt)

end

-- Called on every iteration of the game loop.
function Update(stoneblock, game_time, dt)

end

-- Called on every iteration of the game loop game
-- after *all* entities have been updated.
function PostUpdate(stoneblock, game_time)
end

-- Called on collision events with other objects.
function OnBeginContact(stoneblock, node, other, other_node)
    if other:GetClassName() == 'Player' then
        local body = node:GetRigidBody()
        stoneblock:PlayAnimationByName('Fall Down')
    end
end

-- Called on collision events with other objects.
function OnEndContact(stoneblock, node, other, other_node)
end

-- Called on key down events.
function OnKeyDown(stoneblock, symbol, modifier_bits)
end

-- Called on key up events.
function OnKeyUp(stoneblock, symbol, modifier_bits)
end

-- Called on mouse button press events.
function OnMousePress(stoneblock, mouse)
end

-- Called on mouse button release events.
function OnMouseRelease(stoneblock, mouse)
end

-- Called on mouse move events.
function OnMouseMove(stoneblock, mouse)
end

-- Called on game events.
function OnGameEvent(stoneblock, event)
end

