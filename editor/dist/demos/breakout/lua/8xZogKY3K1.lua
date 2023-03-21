-- Entity 'Ball' script.
-- This script will be called for every instance of 'Ball'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.
-- Called when the game play begins for an entity in the scene.
function TestAdjacent(scene, brick_pos)
    local bricks = scene:QuerySpatialNodes(brick_pos, 'First')
    if bricks:IsEmpty() then
        return false
    end
    return true
end

function LostBall(ball)
    Scene:KillEntity(ball)
    Audio:PlaySoundEffect('Fail')
    local ball_died = game.GameEvent:new()
    ball_died.from = 'ball'
    ball_died.to = 'game'
    ball_died.message = 'died'
    Game:PostEvent(ball_died)
end

function BeginPlay(ball, scene)
    if ball.demo_ball then
        ball.enabled = true
        ball.velocity = glm.normalize(glm.vec2:new(300, 500.0)) * 300
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
    if not ball.enabled or ball:HasBeenKilled() then
        return
    end

    local scene = ball:GetScene()
    local ball_body = ball:GetNode(0)
    local ball_position = ball_body:GetTranslation()
    local ball_velocity = ball.velocity
    local ball_direction = glm.normalize(ball_velocity)

    ball_position = ball_position + dt * ball_velocity

    if ball_position.x > 500.0 then
        ball_position.x = 500.0
        ball_velocity.x = -ball_velocity.x
    elseif ball_position.x < -500.0 then
        ball_position.x = -500.0
        ball_velocity.x = -ball_velocity.x
    end

    if ball_position.y > 400.0 then
        LostBall(ball)
        return
    elseif ball_position.y < -400.0 then
        ball_position.y = -400.0
        ball_velocity.y = -ball_velocity.y
    end

    local ball_radius = 10.0

    local bricks =
        scene:QuerySpatialNodes(ball_position, ball_radius, 'Closest')
    while bricks:HasNext() do

        local brick_node = bricks:GetNext()
        local brick_entity = brick_node:GetEntity()
        local brick_pos = brick_node:GetTranslation()
        local brick_size = brick_node:GetSize()
        local brick_width = brick_size.x
        local brick_height = brick_size.y

        local left_adjacent = TestAdjacent(scene, brick_pos -
                                               glm.vec2:new(brick_width, 0))
        local right_adjacent = TestAdjacent(scene, brick_pos +
                                                glm.vec2:new(brick_width, 0))
        local top_adjacent = TestAdjacent(scene, brick_pos -
                                              glm.vec2:new(0, brick_height))
        local bottom_adjacent = TestAdjacent(scene, brick_pos +
                                                 glm.vec2:new(0, brick_height))

        -- test the ball against each brick edge by checking the dot product
        -- of the balls direction vector and the brick edge's normal. 
        -- on a direct hit perpendicular to the brick edge the vectors are collinear
        -- and their dot product will be 0. Look for the edge with the smallest
        -- dot product and choose that as the brick edge to bounce off of. 
        -- Except that.. if the ball is  moving right it cannot hit a right edge, if it's
        -- moving left it cannot hit a left edge etc.
        -- If a brick has an adjacent brick right next to it it's not possible for the ball
        -- to hit the brick from that direction. I.e. if the brick has an adjacent
        -- brick on the left the left edge cannot be hit.

        local dots = {}

        -- moving right, check against left edge
        if ball_velocity.x > 0.0 and not left_adjacent then
            dots[0] = glm.dot(glm.vec2:new(-1.0, 0.0), ball_direction) -- left
        else
            dots[0] = 1.0
        end

        -- moving left, check against right edge
        if ball_velocity.x < 0.0 and not right_adjacent then
            dots[1] = glm.dot(glm.vec2:new(1.0, 0.0), ball_direction) -- right
        else
            dots[1] = 1.0
        end

        -- moving down, check against top edge
        if ball_velocity.y > 0.0 and not top_adjacent then
            dots[2] = glm.dot(glm.vec2:new(0.0, -1.0), ball_direction) -- top
        else
            dots[2] = 1.0
        end

        -- moving up, check against bottom edge
        if ball_velocity.y < 0.0 and not bottom_adjacent then
            dots[3] = glm.dot(glm.vec2:new(0.0, 1.0), ball_direction) -- bottom
        else
            dots[3] = 1.0
        end

        local dot_index = 0
        local dot_value = 1.0
        for i = 0, 3 do
            if dots[i] < dot_value then
                dot_index = i
                dot_value = dots[i]
            end
        end

        local brick_left = brick_pos.x - brick_width * 0.5
        local brick_right = brick_pos.x + brick_width * 0.5
        local brick_top = brick_pos.y - brick_height * 0.5
        local brick_bottom = brick_pos.y + brick_height * 0.5

        if dot_index == 0 then
            ball_velocity.x = -ball_velocity.x
            ball_position.x = brick_left - ball_radius
            -- Game:DebugPrint('Hit brick left')
        elseif dot_index == 1 then
            ball_velocity.x = -ball_velocity.x
            ball_position.x = brick_right + ball_radius
            -- Game:DebugPrint('Hit brick right')
        elseif dot_index == 2 then
            ball_velocity.y = -ball_velocity.y
            ball_position.y = brick_top - ball_radius
            -- Game:DebugPrint('Hit brick top')
        elseif dot_index == 3 then
            ball_velocity.y = -ball_velocity.y
            ball_position.y = brick_bottom + ball_radius
            -- Game:DebugPrint('Hit brick bottom')
        end

        -- useful for 'debugging' i.e. seeing how the ball gets adjusted after hitting something

        -- ball.enabled = false

        CallMethod(brick_entity, 'BallHit')
        break
    end

    --    ball.velocity = ball_velocity
    --    ball_body:SetTranslation(ball_position)

    local paddle = scene:FindEntityByInstanceName('Paddle')

    if paddle ~= nil then
        local paddle_body = paddle:GetNode(0)

        if ball.launched then
            local paddle_rect = scene:FindEntityNodeBoundingRect(paddle,
                                                                 paddle_body)

            if paddle_rect:TestPoint(ball_position) then
                ball_position.y = 350.0
                ball_velocity.y = -ball_velocity.y

                -- we're transferring the horizontal velocity from the paddle to the ball
                ball_velocity.x = paddle.velocity

                -- ball_velocity = glm.normalize(ball_velocity) * 500.0
            end
        else
            local paddle_position = paddle_body:GetTranslation()

            ball_position.x = paddle_position.x
        end
    end

    ball.velocity = ball_velocity
    ball_body:SetTranslation(ball_position)
end

-- Called on collision events with other objects.
function OnBeginContact(ball, node, other, other_node)
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
    if ball.demo_ball then
        return
    end

    if ball.launched then
        return
    end

    ball.launched = true
    ball.velocity = glm.vec2:new(0.0, -500.0)
end

-- Called on mouse button release events.
function OnMouseRelease(ball, mouse)
end

-- Called on mouse move events.
function OnMouseMove(ball, mouse)
end

