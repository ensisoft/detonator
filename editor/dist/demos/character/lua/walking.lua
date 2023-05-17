require('utility')

-- Check if the entity is currently walking or not.
function IsWalking(entity)
    local walk_distance = entity.walk_distance
    if walk_distance.x > 0.0 or walk_distance.y > 0.0 then
        return true
    end

    return false
end

function WalkingDistance(entity, destination)
    return GetManhattanDistance(entity, destination)
end

-- Start walking towards the given position (in scene coordinates)
-- If the entity is already close enough to the destination then
-- no walking is actually started.
function StartWalking(entity, destination)
    -- get the entity root node. It's position is expected to equal
    -- the scene position. In other words the entity root is the scene root.
    local node = entity:GetNode(0)

    -- we know the entity has no parent in the world/scene thus the
    -- body node's translation is the position in the world
    local position = node:GetTranslation()
    -- the direction where we need to walk
    local walk_vector = destination - position
    -- the distance that need to be covered both vertically and horizontally
    local walk_distance = glm.vec2:new(math.abs(walk_vector.x),
                                       math.abs(walk_vector.y))

    -- if walk distance is very short we're clamping it to zero
    if walk_distance.x < 10.0 then
        walk_distance.x = 0.0
    end
    if walk_distance.y < 10.0 then
        walk_distance.y = 0.0
    end

    entity.walk_vector = glm.normalize(walk_vector)
    entity.walk_distance = walk_distance
end

-- Walk/advance the entity by taking steps vertically and/or
-- horizontally based on the entity's walking vector. The walking
-- vector is essentially the "manhattan distance" that needs to be
-- consumed/walked in order to reach the target position.
-- Returns true if walking steps were taken or false to indicate that
-- no advance was made (already close enough, i.e. walk vector is ~zero)
function Walk(entity, dt, threshold)
    local walk_distance = entity.walk_distance
    local walk_vector = entity.walk_vector
    local walk_speed = entity.walk_speed
    local hori_velocity = 0.0
    local vert_velocity = 0.0

    if walk_distance.x > threshold then
        if walk_vector.x > 0.0 then
            hori_velocity = walk_speed
        else
            hori_velocity = -walk_speed
        end
    elseif walk_distance.y > threshold then
        if walk_vector.y > 0.0 then
            vert_velocity = walk_speed
        else
            vert_velocity = -walk_speed
        end
    else
        entity.walk_distance = glm.vec2:new(0.0, 0.0)
        entity.velocity = glm.vec2:new(0.0, 0.0)
        return false
    end

    local hori_step = hori_velocity * dt
    local vert_step = vert_velocity * dt

    local body_node = entity:GetNode(0)
    body_node:Translate(hori_step, vert_step)

    -- reduce the walk distance by taking steps on both axis.
    walk_distance.x = walk_distance.x - math.abs(hori_step)
    walk_distance.y = walk_distance.y - math.abs(vert_step)

    local velocity = glm.vec2:new(hori_velocity, vert_velocity)

    -- only update the direction vector if we have a non-zero velocity
    -- vector. remember, normalizing a zero vector is bad (division by zero)
    -- this check also makes sure that we always have a direction vector
    -- pointing the way the entity is facing.
    if math.abs(hori_velocity) > 0.0 or math.abs(vert_velocity) > 0.0 then
        entity.direction = glm.normalize(velocity)
    end
    entity.walk_distance = walk_distance
    entity.velocity = velocity
    return true
end
