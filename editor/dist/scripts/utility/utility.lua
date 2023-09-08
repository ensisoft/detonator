

util.RandomVec2 = function(min, max)
    local x = util.Random(min, max)
    local y = util.Random(min, max)
    return glm.vec2:new(x, y)
end

