-- Entity 'Paddle' script.
-- This script will be called for every instance of 'Paddle'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.
-- Called when the game play begins for an entity in the scene.
local powerup_time = 0
local powerup_name = ''
local message_clear = 0

function EndPowerup(paddle)
    local hud = Game:GetTopUI()
    local body = paddle:GetNode(0)

    if powerup_name == 'Enlarge' then
        body:SetSize(100.0, 20.0)
    elseif powerup_name == 'Shrink' then
        body:SetSize(100.0, 20.0)
    end

    hud.powerup:SetVisible(false)
    powerup_time = 0
end

function BeginPlay(paddle, scene)

end

-- Called when the game play ends for an entity in the scene.
function EndPlay(paddle, scene)

end

-- Called on every low frequency game tick.
function Tick(paddle, game_time, dt)

    if message_clear == 0 then
        return
    end

    message_clear = message_clear - 1
    if message_clear == 0 then
        local hud = Game:GetTopUI()
        hud.message:SetText('')
    end
end

-- Called on every iteration of game loop.
function Update(paddle, game_time, dt)
    -- figure out the current paddle velocity based on the distance
    -- traveled since last update.
    local body = paddle:FindNodeByClassName('Body')
    local pos = body:GetTranslation()
    local dist = pos.x - paddle.position
    paddle.velocity = dist / dt
    paddle.position = pos.x

    if powerup_time <= 0 then
        return
    end

    local hud = Game:GetTopUI()

    powerup_time = powerup_time - dt
    if powerup_time > 0 then
        hud.powerup:SetValue(powerup_time / 10.0)
        return
    end

    EndPowerup(paddle)

end

-- Called on collision events with other objects.
function OnBeginContact(paddle, node, other, other_node)

end

-- Called on collision events with other objects.
function OnEndContact(paddle, node, other, other_node)

    local klass = other:GetClassName()

    other:Die()

    local message

    if string.find(klass, 'Enlarge') then

        -- grow the paddle to a larger size.
        local body = paddle:GetNode(0)
        body:SetSize(150.0, 20.0)
        powerup_name = 'Enlarge'
        powerup_time = 10
        message = 'Large paddle!'

    elseif string.find(klass, 'Shrink') then
        -- shrink the paddle to a smaller size
        local body = paddle:GetNode(0)
        body:SetSize(50.0, 20.0)
        powerup_name = 'Shrink'
        powerup_time = 10
        message = 'Oh no! Small paddle!'

    elseif string.find(klass, 'Points') then
        message = tostring(other.points) .. ' points!'
        local points = game.GameEvent:new()
        points.from = 'paddle'
        points.message = 'extra-points'
        points.to = 'game'
        points.value = other.points
        Game:PostEvent(points)
        Audio:PlaySoundEffect('Collect Points')
    end

    local hud = Game:GetTopUI()
    message_clear = 3
    hud.message:SetText(message)

    if string.find(klass, 'Powerup') then
        hud.powerup:SetValue(1.0)
        hud.powerup:SetVisible(true)
        Audio:PlaySoundEffect('Power Up')
    end
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
        local pos = node:GetTranslation()
        pos.x = mouse.scene_coord.x
        node:SetTranslation(pos)
    end
end

