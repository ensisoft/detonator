-- Entity 'Paddle' script.
-- This script will be called for every instance of 'Paddle'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.
-- Called when the game play begins for an entity in the scene.
local powerup_time = 0
local message_clear = 0

local paddle_move_time = 0

function SetMessage(message)
    local hud = Game:GetTopUI()
    hud.message:SetText(message)
    hud.message:SetVisible(true)
    message_clear = 3
end

function EndPowerup(paddle)
    local hud = Game:GetTopUI()
    local body_node = paddle:GetNode(0)
    local skin_node = paddle:GetNode(1)
    local skin_draw = skin_node:GetDrawable()

    body_node:SetScale(1.0, 1.0)
    body_node:SetScale(1.0, 1.0)
    skin_draw:SetActiveTextureMap('Blue')

    paddle.sticky = false

    hud.powerup:SetVisible(false)
    powerup_time = 0
end

function IdleAnimation(paddle)
    local time = util.GetSeconds()
    local idle = time - paddle_move_time
    if idle < 1.0 then
        return
    end

    if paddle:HasPendingAnimations() then
        return
    end

    if paddle:IsAnimating() then
        return
    end

    local t = math.sin(time * 15.0)
    local skin_node = paddle:GetNode(1)
    skin_node:SetTranslation(0.0, t * 2.0)
end

function Powerup(paddle)
    Audio:PlayMusic('Power Up')
    paddle:PlayAnimation('Powerup')

    paddle:EmitParticles('Power Up Emitter1')
    paddle:EmitParticles('Power Up Emitter2')

end

function BeginPlay(paddle, scene, map)

end

-- Called when the game play ends for an entity in the scene.
function EndPlay(paddle, scene, map)

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
    local body_node = paddle:GetNode(0)
    local skin_node = paddle:GetNode(1)
    local skin_draw = skin_node:GetDrawable()

    local position = body_node:GetTranslation()
    local distance = position.x - paddle.position
    local velocity = distance / dt
    paddle.velocity = velocity
    paddle.position = position.x

    local normalized_velocity = math.min(math.abs(velocity) / 1000.0, 1.0)
    local slow = base.Color4f:new(255, 165, 0, 0)
    local fast = base.Color4f:new(255, 0, 0, 255)

    local left_burn_zone_node = paddle:FindNode('Left Burn Zone')
    local right_burn_zone_node = paddle:FindNode('Right Burn Zone')
    local left_burn_zone_draw = left_burn_zone_node:GetDrawable()
    local right_burn_zone_draw = right_burn_zone_node:GetDrawable()

    if velocity > 0.0 then
        body_node:SetRotation(math.rad(0.5))
        right_burn_zone_draw:SetUniform('kBaseColor', util.lerp(slow, fast,
                                                                normalized_velocity))
        right_burn_zone_draw:SetVisible(true)
        left_burn_zone_draw:SetVisible(false)
    elseif velocity < 0.0 then
        body_node:SetRotation(math.rad(-0.5))
        left_burn_zone_draw:SetUniform('kBaseColor', util.lerp(slow, fast,
                                                               normalized_velocity))
        left_burn_zone_draw:SetVisible(true)
        right_burn_zone_draw:SetVisible(false)
    else
        IdleAnimation(paddle)
        left_burn_zone_draw:SetVisible(false)
        right_burn_zone_draw:SetVisible(false)
    end

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

    local body_node = paddle:GetNode(0)
    local skin_node = paddle:GetNode(1)
    local skin_draw = skin_node:GetDrawable()

    local message = nil

    if string.find(klass, 'Enlarge') then
        SetMessage('Large paddle!')
        EndPowerup(paddle)
        Powerup(paddle)

        body_node:SetScale(1.5, 1.0)
        powerup_time = 10
        message = 'Large paddle!'

    elseif string.find(klass, 'Shrink') then
        SetMessage('Oh no! Small paddle!')
        EndPowerup(paddle)

        skin_draw:SetActiveTextureMap('Red')

        body_node:SetScale(0.5, 1.0)
        powerup_time = 10
        -- todo better audio
        Audio:PlaySoundEffect('Power Up')

    elseif string.find(klass, 'Sticky') then
        SetMessage('Your paddle is sticky')
        EndPowerup(paddle)
        Powerup(paddle)

        skin_draw:SetActiveTextureMap('Green')

        paddle.sticky = true
        powerup_time = 10

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
        Powerup(paddle)
        return
    elseif string.find(klass, 'Heavy Ball') then
        SetMessage('Heavy Ball')
        local balls = Scene:ListEntitiesByClassName('Ball')
        balls:ForEach(function(ball)
            CallMethod(ball, 'SetBallWeight', 'Heavy')
        end)
        Powerup(paddle)
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
        local body_node = paddle:GetNode(0)
        local position = body_node:GetTranslation()
        position.x = mouse.scene_coord.x
        body_node:SetTranslation(position)

        paddle_move_time = util.GetSeconds()
    end
end

function OnGameEvent(paddle, event)
    if event.from == 'ball' then
        if event.message == 'died' then
            EndPowerup(paddle)
        end
    end
end
