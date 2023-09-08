
local shaking    = false
local viewport   = nil
local shake_time = 0
local shake_duration = 0
local shake_strenght = glm.vec2:new(0.0, 0.0)

Camera = {}

Camera.SetViewport = function(vp)
  viewport = vp
end

Camera.Shake = function(strength, duration)
   if shaking then
    return
   end
   shake_time     = 0
   shake_duration = duration
   shake_strength = strength
   shaking = true
end


Camera.Update = function(dt)
    if shaking == false then
        return viewport
    end
    shake_time = shake_time + dt

    -- first level of shake
    local t0 = base.clamp(0.0, 1.0, shake_time / shake_duration)
    local x0 = math.cos(t0 * math.pi)
    local y0 = math.sin(t0 * math.pi)

    -- second level shake, added to the first shake
    local t1 = base.clamp(0.0, 1.0, shake_time / (shake_duration * 0.3))
    local x1 = math.cos(t1 * math.pi)
    local y1 = math.sin(t1 * math.pi)

    local v = viewport:Copy()
    v:Translate(shake_strength.x * x0, shake_strength.y * y0)
    v:Translate(shake_strength.y * y1, shake_strength.x * x1)
    shaking = shake_time < shake_duration
    return v
end