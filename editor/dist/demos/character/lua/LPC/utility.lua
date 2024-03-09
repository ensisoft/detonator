-- Get the entity world position. This assumes that the entity 
-- is directly under the scene root and has no other parent.
function GetPosition(entity)
    -- get the entity root node. It's position is expected to equal
    -- the scene position.
    local node = entity:GetNode(0)
    local pos = node:GetTranslation()
    return pos
end

-- Get the direct distance between two entities.
function GetDistance(first, second)
    local first_pos = GetPosition(first)
    local second_pos = GetPosition(second)
    return glm.length(first_pos - second_pos)
end

-- Get the manhattan (x,y) distance between two entities.
function GetManhattanDistance(first, second)
    local first_pos = GetPosition(first)
    local second_pos = GetPosition(second)
    local dist = first_pos - second_pos
    return glm.vec2:new(math.abs(dist.x), math.abs(dist.y))
end
