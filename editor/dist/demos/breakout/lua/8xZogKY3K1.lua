-- Entity 'Ball' script.

-- This script will be called for every instance of 'Ball'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.

-- Called when the game play begins for an entity in the scene.
function BeginPlay(ball, scene)
    if ball.demo_ball then 
        local node = ball:FindNodeByClassName('Ball')
        local body = node:GetRigidBody()
        body:SetFlag('Enabled', true)
    end
end

-- Called when the game play ends for an entity in the scene.
function EndPlay(ball, scene)

end

-- Called on every low frequency game tick.
function Tick(ball, game_time, dt)
end

-- Called on every iteration of game loop.
function Update(ball, game_time, dt)
    -- clip the ball's maximum velocity to something more reasonable
    local node = ball:FindNodeByClassName('Ball')
    local body = node:GetRigidBody()
    local velo = glm.length(body:GetLinearVelocity())
    if velo > 15 then
        local vec = glm.normalize(body:GetLinearVelocity())
        Physics:SetLinearVelocity(node, vec * 15.0)
    end
end

-- Called on collision events with other objects.
function OnBeginContact(ball, node, other, other_node)
    if other:GetClassName() == 'Floor' then 
        Scene:KillEntity(ball)
        Audio:PlaySoundEffect('Fail')
        local ball_died   = game.GameEvent:new()
        ball_died.from    = 'ball'
        ball_died.to      = 'game'
        ball_died.message = 'died'
        Game:PostEvent(ball_died)
    end
end

-- Called on collision events with other objects.
function OnEndContact(ball, node, other, other_node)
end

-- Called on key down events.
function OnKeyDown(ball, symbol, modifier_bits)
end

-- Called on key up events.
function OnKeyUp(ball, symbol, modifier_bits)
end

-- Called on mouse button press events.
function OnMousePress(ball, mouse)
    local node = ball:FindNodeByClassName('Ball')
    local body = node:GetRigidBody()
    body:SetFlag('Enabled', true)
end

-- Called on mouse button release events.
function OnMouseRelease(ball, mouse)
end

-- Called on mouse move events.
function OnMouseMove(ball, mouse)
end

