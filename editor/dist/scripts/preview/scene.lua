
-- This script is essentially an alternative version to a regular
-- game main script except that the purpose of this script is to set
-- things up for spawning a preview version of scenes while keeping
-- things as close as possible to the "real thing".

local up_velocity = 0.0
local down_velocity = 0.0
local left_velocity = 0.0
local right_velocity = 0.0
local up_acceleration = 0.0
local down_acceleration = 0.0
local left_acceleration = 0.0
local right_acceleration = 0.0

function StartGame()
    Game:DebugPrint('Start preview')

    Game:Play('_scene_preview_')

    Game:SetViewport(-SurfaceWidth*0.5, -SurfaceHeight*0.5, SurfaceWidth, SurfaceHeight)
    viewport_translate = glm.vec2:new(-SurfaceWidth*0.5, -SurfaceWidth*0.5)
end

function Update(game_time, dt)

    local velocity = glm.vec2:new(0.0, up_velocity) +
                     glm.vec2:new(0.0, down_velocity) +
                     glm.vec2:new(left_velocity, 0.0) +
                     glm.vec2:new(right_velocity, 0.0)
    if glm.length(velocity) < 0.001 then
        return
    end
    vector = glm.normalize(velocity)

    viewport_translate = viewport_translate + velocity * dt * 750.0

    Game:SetViewport(viewport_translate.x, viewport_translate.y, SurfaceWidth, SurfaceHeight)

    up_velocity = up_velocity + up_acceleration * dt
    if up_velocity > 0 then
        up_velocity = 0.0
        up_acceleration = 0.0
    end

    down_velocity = down_velocity + down_acceleration * dt;
    if down_velocity < 0 then
        down_velocity = 0.0
        down_acceleration = 0.0
    end

    left_velocity = left_velocity + left_acceleration * dt;
    if left_velocity > 0 then
        left_velocity = 0.0
        left_acceleration = 0.0
    end

    right_velocity = right_velocity + right_acceleration * dt;
    if right_velocity < 0 then
        right_velocity = 0.0
        right_acceleration = 0.0
    end
end

function OnKeyDown(symbol, modifier_bits)
    if symbol == wdk.Keys.Escape then
        Game:Play('_scene_preview_')
        Game:SetViewport(-SurfaceWidth*0.5, -SurfaceHeight*0.5, SurfaceWidth, SurfaceHeight)
        viewport_translate  = glm.vec2:new(-SurfaceWidth*0.5, -SurfaceHeight*0.5)
    elseif symbol == wdk.Keys.KeyW then
        up_velocity     = -1.0
        up_acceleration =  0.0
    elseif symbol == wdk.Keys.KeyS then
        down_velocity     = 1.0
        down_acceleration = 0.0
    elseif symbol == wdk.Keys.KeyA then
        left_velocity     = -1.0
        left_acceleration =  0.0
    elseif symbol == wdk.Keys.KeyD then
        right_velocity     = 1.0
        right_acceleration = 0.0
    end
end

function OnKeyUp(symbol, modifier_bits)
    if symbol == wdk.Keys.KeyW then
        up_acceleration = 1.4
    elseif symbol == wdk.Keys.KeyS then
        down_acceleration = -1.4
    elseif symbol == wdk.Keys.KeyA then
        left_acceleration = 1.4
    elseif symbol == wdk.Keys.KeyD then
        right_acceleration = -1.4
    end
end

-- Special function, only called in editing/preview mode.
function OnRenderingSurfaceResized(width, height)
    if viewport_translate == nil then
        Game:SetViewport(-width*0.5, -height*0.5, width, height)
        return
    end

    Game:SetViewport(viewport_translate.x, viewport_translate.y, width, height)
end