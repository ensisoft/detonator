-- Entity 'User Ball' script.
-- This script will be called for every instance of 'User Ball'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.
local _curr_mouse_pos = nil

-- Called when the game play begins for an entity in the scene.
function BeginPlay(user_ball, scene, map)

end

-- Called when the game play ends for an entity in the scene.
function EndPlay(user_ball, scene, map)

end

-- Called on every low frequency game tick.
function Tick(user_ball, game_time, dt)

end

-- Called on every iteration of the game loop.
function Update(user_ball, game_time, dt)
    if _curr_mouse_pos == nil then
        return
    end

    -- basic idea for moving the ball based to the where
    -- the mouse pointer currently is, is to figure out the
    -- new/next ball velocity that will move the ball over the
    -- distance between the ball position and where it should be
    -- within a physics time step.
    -- so for example if current position is X game units away
    -- from where the mouse pointer currently is the ball velocity
    -- has to be such that the ball moves X game units over the
    -- next integration steps in the physics engine.

    local node = user_ball:FindNodeByClassName('Body')
    local body = node:GetRigidBody()
    -- the node is a root node, thus the position 
    -- is relative to the world basis vectors already!
    local world_pos = node:GetTranslation()

    -- the distance we need to have the ball move.
    -- velocity computation is done in physics units m/s
    local dist = glm.length(Physics:MapVectorFromGame(world_pos -
                                                          _curr_mouse_pos))

    -- if "close enough" then settle down and drop velocity to nothing
    if dist <= 0.1 then
        body:AdjustLinearVelocity(glm.vec2:new(0.0, 0.0))
        return
    end

    local step = Physics:GetTimeStep()
    local iter = Physics:GetNumPositionIterations()
    -- total time spent in the physics simulation will be
    -- the time step times number of position iterations
    local time = step * iter

    -- what speed is needed to travel dist in time?
    local velo = dist / time

    -- take the direction and scale by the speed
    local dir = glm.normalize(_curr_mouse_pos - world_pos)

    -- set the body velocity adjustment to apply on  next update.
    body:AdjustLinearVelocity(dir * velo)
end

-- Called on every iteration of the game loop game
-- after *all* entities have been updated.
function PostUpdate(user_ball, game_time)
end

-- Called on collision events with other objects.
function OnBeginContact(user_ball, node, other, other_node)
end

-- Called on collision events with other objects.
function OnEndContact(user_ball, node, other, other_node)
end

-- Called on key down events.
function OnKeyDown(user_ball, symbol, modifier_bits)
end

-- Called on key up events.
function OnKeyUp(user_ball, symbol, modifier_bits)
end

-- Called on mouse button press events.
function OnMousePress(user_ball, mouse)
end

-- Called on mouse button release events.
function OnMouseRelease(user_ball, mouse)
end

-- Called on mouse move events.
function OnMouseMove(user_ball, mouse)
    if not mouse.over_scene then
        return
    end
    _curr_mouse_pos = mouse.scene_coord
end

