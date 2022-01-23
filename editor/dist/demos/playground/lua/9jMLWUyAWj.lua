-- Entity 'Ball' script.

-- This script will be called for every instance of 'Ball'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.

-- keep the ball's "speed" constant but allow changes 
-- in direction.
-- Setting the velocity directly can cause problems 
-- in the physics simulation. An alternative way 
-- is to apply impulses as appropriate.

-- *physics world* units per second, i.e. in m/s
local _ball_speed = 10

local _adjust_velocity = false

-- Called when the game play begins for an entity in the scene.
function BeginPlay(ball, scene)
    local node = ball:FindNodeByClassName('Body')
    local body = node:GetRigidBody()
    local forward = scene:MapVectorFromEntityNode(ball, node, game.X)
    Game:DebugPrint(tostring(forward))
    body:AdjustLinearVelocity(forward * _ball_speed)
end

-- Called when the game play ends for an entity in the scene.
function EndPlay(ball, scene)

end

-- Called on every low frequency game tick.
function Tick(ball, game_time, dt)

end

-- Called on every iteration of the game loop.
function Update(ball, game_time, dt)
    if not _adjust_velocity then 
        return
    end
    local node = ball:FindNodeByClassName('Body')
    local body = node:GetRigidBody()
    local velo = glm.normalize(body:GetLinearVelocity())
    body:AdjustLinearVelocity(velo * _ball_speed)
end

-- Called on every iteration of the game loop game
-- after *all* entities have been updated.
function PostUpdate(ball, game_time)
end

-- Called on collision events with other objects.
function OnBeginContact(ball, node, other, other_node)
    _adjust_velocity = false
end

-- Called on collision events with other objects.
function OnEndContact(ball, node, other, other_node)
    _adjust_velocity = true
end

-- Called on key down events.
function OnKeyDown(ball, symbol, modifier_bits)
end

-- Called on key up events.
function OnKeyUp(ball, symbol, modifier_bits)
end

-- Called on mouse button press events.
function OnMousePress(ball, mouse)
end

-- Called on mouse button release events.
function OnMouseRelease(ball, mouse)
end

-- Called on mouse move events.
function OnMouseMove(ball, mouse)
end

