-- Entity 'Player' script.
-- This script will be called for every instance of 'Player'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.
require('app://scripts/utility/keyboard.lua')

local vert_speed = 0
local hori_speed = 0
function ClipVelocity(velo, min, max)
    if velo > max then
        velo = max
    elseif velo < min then
        velo = min
    end
    return velo
end

-- Called when the game play begins for an entity in the scene.
function BeginPlay(player, scene)

end

-- Called when the game play ends for an entity in the scene.
function EndPlay(player, scene)

end

-- Called on every low frequency game tick.
function Tick(player, game_time, dt)

end

-- Called on every iteration of the game loop.
function Update(player, game_time, dt)
    if KB.TestKeyDown(KB.Keys.Left) then
        if hori_speed >= 0 then
            hori_speed = -200
        elseif hori_speed > -600 then
            hori_speed = hori_speed - 100
        end
    elseif KB.TestKeyDown(KB.Keys.Right) then
        if hori_speed <= 0 then
            hori_speed = 200
        elseif hori_speed < 600 then
            hori_speed = hori_speed + 100
        end
    else
        hori_speed = 0
    end

    if KB.TestKeyDown(KB.Keys.Up) then
        if vert_speed >= 0 then
            vert_speed = -200
        elseif vert_speed > -600 then
            vert_speed = vert_speed - 100
        end
    elseif KB.TestKeyDown(KB.Keys.Down) then
        if vert_speed <= 0 then
            vert_speed = 200
        elseif vert_speed < 600 then
            vert_speed = vert_speed + 100
        end
    else
        vert_speed = 0
    end

    hori_speed = ClipVelocity(hori_speed, -200, 200)
    vert_speed = ClipVelocity(vert_speed, -200, 200)

    local node = player:GetNode(0)
    node:Translate(dt * hori_speed, dt * vert_speed)

    local animator = player:GetAnimator()
    animator.velocity = glm.vec2:new(hori_speed, vert_speed)
end

-- Called on every iteration of the game loop game
-- after *all* entities have been updated.
function PostUpdate(player, game_time)
end

-- Called on collision events with other objects.
function OnBeginContact(player, node, other, other_node)
end

-- Called on collision events with other objects.
function OnEndContact(player, node, other, other_node)
end

-- Called on key down events.
function OnKeyDown(player, symbol, modifier_bits)
    KB.KeyDown(symbol, modifier_bits, KB.WASD)
    KB.KeyDown(symbol, modifiier_bits, KB.ARROW)
end

-- Called on key up events.
function OnKeyUp(player, symbol, modifier_bits)
    KB.KeyUp(symbol, modifier_bits, KB.WASD)
    KB.KeyUp(symbol, modifier_bits, KB.ARROW)
end

-- Called on mouse button press events.
function OnMousePress(player, mouse)
end

-- Called on mouse button release events.
function OnMouseRelease(player, mouse)
end

-- Called on mouse move events.
function OnMouseMove(player, mouse)
end

-- Called on game events.
function OnGameEvent(player, event)
end

-- Called on animation finished events.
function OnAnimationFinished(player, animation)
end

-- Called on timer events.
function OnTimer(player, timer, jitter)
end

-- Called on posted entity events.
function OnEvent(player, event)
end

