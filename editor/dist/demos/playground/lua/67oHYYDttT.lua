-- Entity 'Kinematic Constant Speed Ball' script.
-- This script will be called for every instance of 'Kinematic Constant Speed Ball'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.
-- Called when the game play begins for an entity in the scene.
local random = util.RandomEngine:new()
random:Seed(7272)

function BeginPlay(ball, scene, map)
    local x = random:Random(-10.0, 10.0)
    local y = random:Random(-10.0, 10.0)

    local body_node = ball:GetNode(0)
    local rigid_body = body_node:GetRigidBody()

    rigid_body:AdjustLinearVelocity(glm.vec2:new(x, y))
end

-- Called when the game play ends for an entity in the scene.
function EndPlay(kinematic_constant_speed_ball, scene, map)

end

-- Called on every low frequency game tick.
function Tick(kinematic_constant_speed_ball, game_time, dt)

end

-- Called on every iteration of the game loop.
function Update(ball, game_time, dt)

    local body_node = ball:GetNode(0)
    local body_pos = body_node:GetTranslation()
    local rigid_body = body_node:GetRigidBody()

    local velocity = rigid_body:GetLinearVelocity()

    if body_pos.x <= -500.0 then
        body_pos.x = -500
        velocity.x = -velocity.x
    elseif body_pos.x >= 500.0 then
        body_pos.x = 500
        velocity.x = -velocity.x
    end

    if body_pos.y <= -400.0 then
        body_pos.x = -400.0
        velocity.y = -velocity.y
    elseif body_pos.y >= 400.0 then
        body_pos.y = 400.0
        velocity.y = -velocity.y
    end
    body_node:SetTranslation(body_pos)
    rigid_body:AdjustLinearVelocity(velocity)

end

-- Called on every iteration of the game loop game
-- after *all* entities have been updated.
function PostUpdate(kinematic_constant_speed_ball, game_time)
end

-- Called on collision events with other objects.
function OnBeginContact(kinematic_constant_speed_ball, node, other, other_node)
end

-- Called on collision events with other objects.
function OnEndContact(kinematic_constant_speed_ball, node, other, other_node)
end

-- Called on key down events.
function OnKeyDown(kinematic_constant_speed_ball, symbol, modifier_bits)
end

-- Called on key up events.
function OnKeyUp(kinematic_constant_speed_ball, symbol, modifier_bits)
end

-- Called on mouse button press events.
function OnMousePress(kinematic_constant_speed_ball, mouse)
end

-- Called on mouse button release events.
function OnMouseRelease(kinematic_constant_speed_ball, mouse)
end

-- Called on mouse move events.
function OnMouseMove(kinematic_constant_speed_ball, mouse)
end

-- Called on game events.
function OnGameEvent(kinematic_constant_speed_ball, event)
end

-- Called on animation finished events.
function OnAnimationFinished(kinematic_constant_speed_ball, animation)
end

-- Called on timer events.
function OnTimer(kinematic_constant_speed_ball, timer, jitter)
end

-- Called on posted entity events.
function OnEvent(kinematic_constant_speed_ball, event)
end

