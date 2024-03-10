--
-- Entity animator script.
-- This script will be called for every entity animator instance that
-- has this particular script assigned.
-- This script allows you to write the logic for performing some
-- particular actions when entering/leaving animation states and
-- when transitioning from one state to another.
--
-- You're free to delete functions you don't need.
--
-- Called once when the animator is first created.
-- This is the place where you can set the initial entity and animator
-- state to a known/desired first state.
--
--
-- enable this to print animator state changes
local state_debug = false

function Init(animator, entity)

end

-- Called once when the entity enters a new animation state at the end
-- of a transition.
function EnterState(animator, state, entity)

    if state_debug then
        Game:DebugPrint('State: ' .. state)
    end

    -- updating the texture map in UpdateState
    if state == 'Active' then
        return
    end

    local skin_node = entity:GetNode(1)
    local skin_draw = skin_node:GetDrawable()

    -- for each animation state we have the corresponding texture map 
    -- in the material.
    -- when the animator state changes we simply use the name of the 
    -- state to change the active texture map in order to represent 
    -- the state visually.
    skin_draw:SetActiveTextureMap(state)
    skin_draw:AdjustMaterialTime(0.0)

end

-- Called once when the entity is leaving an animation state at the start
-- of a transition.
function LeaveState(animator, state, entity)
    if state == 'Active' then
        animator.action = ''
    end
end

-- Called continuously on the current state. This is the place where you can
-- realize changes to the current input when in some particular state.
function UpdateState(animator, state, time, dt, entity)
    if state ~= 'Active' then
        return
    end

    local body_node = entity:GetNode(0)
    local rigid_body = body_node:GetRigidBody()
    local velocity = rigid_body:GetLinearVelocity()

    local action = nil

    -- todo this isn't so simple the chracter might have 
    -- velocity while it's not really moving itself. for
    -- example if the chracter is standing on a moving platform
    -- then it has world velocity even though it's velocity
    -- relative to the platform is actually 0

    if math.abs(velocity.y) > 0.1 then
        if velocity.y < 0.1 then
            action = 'Jump'
        elseif velocity.y > 0.1 then
            action = 'Fall'
        end
    elseif math.abs(velocity.x) > 0.1 then
        action = 'Walk'
    end

    if animator.action == action or action == nil then
        return
    end

    local skin_node = entity:GetNode(1)
    local skin_draw = skin_node:GetDrawable()
    skin_draw:SetActiveTextureMap(action)
    skin_draw:AdjustMaterialTime(0.0)

    animator.action = action

    if state_debug then
        Game:DebugPrint('Action:  ' .. action)
    end

end

-- Evaluate the condition to trigger a transition from one animation state
-- to another. Return true to take the transition or false to reject it.
function EvalTransition(animator, from, to, entity)

    if from == 'Idle' and to == 'Attack' then
        if animator.attack == true then
            animator.attack = false
            return true
        end
    end

    if from == 'Attack' and to == 'Idle' then
        local time = animator:GetTime()
        if time >= 0.2 then
            return true
        end
    end

    local body_node = entity:GetNode(0)
    local rigid_body = body_node:GetRigidBody()

    -- if we have some velocity then enter 'active' state
    if from == 'Idle' and to == 'Active' then
        local velocity = rigid_body:GetLinearVelocity()
        if math.abs(velocity.x) > 0.1 or math.abs(velocity.y) > 0.1 then
            return true
        end
    end

    -- if we have some velocity then remain 'active' state
    -- otherwise return to 'idle'
    if from == 'Active' and to == 'Idle' then
        local velocity = rigid_body:GetLinearVelocity()
        if math.abs(velocity.x) > 0.01 or math.abs(velocity.y) > 0.01 then
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
