-- Entity 'Paddle' script.

-- This script will be called for every instance of 'Paddle'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.

-- Called when the game play begins for an entity in the scene.
function BeginPlay(paddle, scene)

end

-- Called when the game play ends for an entity in the scene.
function EndPlay(paddle, scene)

end

-- Called on every low frequency game tick.
function Tick(paddle, game_time, dt)

end

-- Called on every iteration of game loop.
function Update(paddle, game_time, dt)
    -- figure out the current paddle velocity based on the distance
    -- traveled since last update.
    local body   = paddle:FindNodeByClassName('Body')
    local pos    = body:GetTranslation()
    local dist   = pos.x - paddle.position
    paddle.velocity = dist/dt
    paddle.position = pos.x
end

-- Called on collision events with other objects.
function OnBeginContact(paddle, node, other, other_node)
    if other:GetClassName() == 'Ball' then
        Physics:ApplyImpulseToCenter(other_node, glm.vec2:new(paddle.velocity*0.002, -0.6))
    end
end

-- Called on collision events with other objects.
function OnEndContact(paddle, node, other, other_node)
end

-- Called on key down events.
function OnKeyDown(paddle, symbol, modifier_bits)
end

-- Called on key up events.
function OnKeyUp(paddle, symbol, modifier_bits)
end

-- Called on mouse button press events.
function OnMousePress(paddle, mouse)
end

-- Called on mouse button release events.
function OnMouseRelease(paddle, mouse)
end

-- Called on mouse move events.
function OnMouseMove(paddle, mouse)
    if mouse.over_scene then 
        local node = paddle:FindNodeByClassName('Body')
        local pos  = node:GetTranslation()
        pos.x = mouse.scene_coord.x
        node:SetTranslation(pos)
    end
end

