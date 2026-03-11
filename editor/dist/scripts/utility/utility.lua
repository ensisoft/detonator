

util.RandomVec2 = function(min, max)
    local x = util.rand(min, max)
    local y = util.rand(min, max)
    return glm.vec2:new(x, y)
end


util.lerp = function(y0, y1, t)
   return (1.0 - t) * y0 + y1 * t
end

util.ease = function(y0, y1, t, curve)
    local t = easing.ease(t, curve)
    return util.lerp(y0, y1, t)
end