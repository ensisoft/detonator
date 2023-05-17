--
-- Entity animator script
-- This script will be called for every entity animator instance that
-- has this particular script assigned.
-- This script allows you to write the logic for performing some
-- particular actions when entering/leaving animation states and
-- when transitioning from one state to another.
--
-- You're free to delete functions you don't need.
--
-- 
-- This table is used to map the characters current direction to one
-- of the 8 possible diagonal material animations. Each material is
-- associated with a direction vector and for entity state update we
-- check the entity's current direction against each of these vectors
-- and choose the one that is closest. Then the final material name for
-- the next character animation will be concatenated from the state 
-- and the direction name, for example Dummy/Idle/North
--
-- LuaFormatter off
Directions = {}
Directions[0] = {anim='North',     dir=glm.vec2:new( 0.0, -1.0) }
Directions[1] = {anim='NorthEast', dir=glm.vec2:new( 1.0, -1.0) }
Directions[2] = {anim='East',      dir=glm.vec2:new( 1.0,  0.0) }
Directions[3] = {anim='SouthEast', dir=glm.vec2:new( 1.0,  1.0) }
Directions[4] = {anim='South',     dir=glm.vec2:new( 0.0,  1.0) }
Directions[5] = {anim='SouthWest', dir=glm.vec2:new(-1.0,  1.0) }
Directions[6] = {anim='West',      dir=glm.vec2:new(-1.0,  0.0) }
Directions[7] = {anim='NorthWest', dir=glm.vec2:new(-1.0, -1.0) }

-- LuaFormatter on

function ChooseDirection(entity, velo, state)
    local best_dist = 10.0
    local best_anim = nil
    local best_dir = nil
    for i = 0, 7 do
        local dist = glm.length(Directions[i].dir - velo)
        if dist < best_dist then
            best_dist = dist
            best_anim = Directions[i].anim
            best_dir = Directions[i].dir
        end
    end
    local cosine = glm.dot(glm.normalize(best_dir), game.X)
    local angle = math.acos(cosine)

    if best_dir.y < 0 then
        angle = -angle
    end

    local body_node = entity:GetNode(0)
    local arrow_node = entity:FindNodeByClassName('Pointer')
    local body_drawable = body_node:GetDrawable()
    -- concatenate the material name with the dummy prefix + state + direction name
    body_drawable:SetMaterial('8-dir-char-template/Dummy/' .. state)
    body_drawable:SetActiveTextureMap(best_anim)
    arrow_node:SetRotation(angle)
end

local last_velocity = nil

-- Called once when the entity enters a new animation state at the end
-- of a transition.
function EnterState(animator, state, entity)
    Game:DebugPrint('Player is ' .. state)

    if state == 'Idle' and last_velocity ~= nil then
        ChooseDirection(entity, last_velocity, "Idle")
    end
end

-- Called once when the entity is leaving an animation state at the start
-- of a transition.
function LeaveState(animator, state, entity)

end

-- Called continuously on the current state. This is the place where you can
-- realize changes to the current input when in some particular state.
function UpdateState(animator, state, time, dt, entity)
    if state == 'Running' then
        local len = glm.length(animator.velocity)
        if len < 0.1 then
            return
        end
        last_velocity = glm.normalize(animator.velocity)
        ChooseDirection(entity, last_velocity, 'Run')
    end
end

-- Evaluate the condition to trigger a transition from one animation state
-- to another. Return true to take the transition or false to reject it.
function EvalTransition(animator, from, to, entity)
    local speed = glm.length(animator.velocity)

    if from == 'Idle' and to == 'Running' then
        if speed > 0.1 then
            return true
        end
    end

    if from == 'Running' and to == 'Idle' then
        if speed > 0.1 then
            return false
        end
        return true
    end

    return false
end

-- Called once when a transition is started from one state to another.
function StartTransition(animator, from, to, duration, entity)

end

-- Called once when the transition from one state to another is finished.
function FinishTransition(animator, from, to, entity)

end

-- Called continuously on a transition while it's in progress.
function UpdateTransition(animator, from, to, duration, time, dt, entity)

end
