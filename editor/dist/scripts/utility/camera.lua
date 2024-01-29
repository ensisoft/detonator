
local shaking    = false
local viewport   = nil
local shake_time = 0
local shake_duration = 0
local shake_strength = glm.vec2:new(0.0, 0.0)
local shake_iterations = 0.0

Camera = {}

Camera.SetViewport = function(vp)
  viewport = vp
end

Camera.Shake = function(strength, duration, iterations)
   if shaking then
    return
   end
   shake_time     = 0
   shake_duration = duration
   shake_strength = strength
   shake_iterations = iterations
   shaking = true
end


Camera.Update = function(dt)
    if shaking == false then
        return viewport
    end
    shake_time = shake_time + dt

    -- first level of shake
    local t = base.clamp(0.0, 1.0, shake_time / shake_duration)
    local x = math.sin(t * math.pi*2.0 * shake_iterations)

    local v = viewport:Copy()
    v:Translate(shake_strength.x * x, shake_strength.y * x)
    shake_strength = shake_strength * 0.97
    shaking = shake_time < shake_duration
    return v
end