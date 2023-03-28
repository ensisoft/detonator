-- Entity 'Paddle' script.
-- This script will be called for every instance of 'Paddle'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.
-- Called when the game play begins for an entity in the scene.
local powerup_time = 0
local message_clear = 0

function SetMessage(message)
    local hud = Game:GetTopUI()
    hud.message:SetText(message)
    hud.message:SetVisible(true)
    message_clear = 3
end

function EndPowerup(paddle)
    local hud = Game:GetTopUI()
    local body = paddle:GetNode(0)

    body:SetScale(1.0, 1.0)
    body:SetScale(1.0, 1.0)
    paddle:PlayAnimationByName('Restore')
    paddle.sticky = false

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
    local body = paddle:GetNode(0)
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
function OnEndContact(paddle, node_, other, other_node)
    if other:HasBeenKilled() or other:IsDying() then
        return
    end

    local klass = other:GetClassName()

    other:Die()

    local node = paddle:GetNode(0)
    local message = nil

    if string.find(klass, 'Enlarge') then
        SetMessage('Large paddle!')
        EndPowerup(paddle)
        paddle:PlayAnimationByName('Enlarge')
        node:SetScale(1.5, 1.0)
        powerup_time = 10
        message = 'Large paddle!'
        Audio:PlaySoundEffect('Power Up')

    elseif string.find(klass, 'Shrink') then
        SetMessage('Oh no! Small paddle!')
        EndPowerup(paddle)
        paddle:PlayAnimationByName('Shrink')
        node:SetScale(0.5, 1.0)
        powerup_time = 10
        Audio:PlaySoundEffect('Power Up')

    elseif string.find(klass, 'Sticky') then
        SetMessage('Your paddle is sticky')
        EndPowerup(paddle)
        paddle:PlayAnimationByName('Make Sticky')
        paddle.sticky = true
        powerup_time = 10
        Audio:PlayMusic('Power Up')

    elseif string.find(klass, 'Points') then
        SetMessage(tostring(other.points) .. ' points!')

        local event = game.GameEvent:new()
        event.from = 'paddle'
        event.message = 'extra-points'
        event.to = 'game'
        event.value = other.points
        Game:PostEvent(event)
        Audio:PlaySoundEffect('Collect Points')

    elseif string.find(klass, 'Double Ball') then
        SetMessage('Double Ball!')
        local balls = Scene:ListEntitiesByClassName('Ball')
        balls:ForEach(function(ball)
            CallMethod(ball, 'SplitBall', ball)
        end)
        Audio:PlaySoundEffect('Power Up')
        return
    elseif string.find(klass, 'Heavy Ball') then
        SetMessage('Heavy Ball')
        local balls = Scene:ListEntitiesByClassName('Ball')
        balls:ForEach(function(ball)
            CallMethod(ball, 'SetBallWeight', 'Heavy')
        end)
        Audio:PlaySoundEffect('Power Up')
        return
    end

    if string.find(klass, 'Powerup') then
        local hud = Game:GetTopUI()
        hud.powerup:SetValue(1.0)
        hud.powerup:SetVisible(true)
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

function OnGameEvent(paddle, event)
    if event.from == 'ball' then
        if event.message == 'died' then
            EndPowerup(paddle)
        end
    end
end
