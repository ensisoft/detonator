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
-- LuaFormatter off
Directions = {}
Directions[0] = {anim='North', dir=glm.vec2:new( 0.0, -1.0) }
Directions[1] = {anim='East',  dir=glm.vec2:new( 1.0,  0.0) }
Directions[2] = {anim='South', dir=glm.vec2:new( 0.0,  1.0) }
Directions[3] = {anim='West',  dir=glm.vec2:new(-1.0,  0.0) }
-- LuaFormatter on

function ChooseDirection(entity)
    local direction = entity.direction
    local best_dist = 10.0
    local best_anim = nil
    local best_dir = nil
    for i = 0, 3 do
        local dist = glm.length(Directions[i].dir - direction)
        if dist < best_dist then
            best_dist = dist
            best_anim = Directions[i].anim
            best_dir = Directions[i].dir
        end
    end

    assert(best_dir ~= nil and best_anim ~= nil)
    -- for debugging
    if best_anim == nil then
        Game:DebugPrint(tostring(entity.direction))
        Game:DebugPause(true)
        return
    end

    for i = 0, entity:GetNumNodes() - 1 do
        local node = entity:GetNode(i)
        local tag = node:GetTag()
        if tag ~= '#prop' then
            local draw = node:GetDrawable()
            if draw ~= nil and draw:IsVisible() then
                draw:SetActiveTextureMap(best_anim)
            end
        end
    end
end

function ResetTimes(entity)
    for i = 0, entity:GetNumNodes() - 1 do
        local node = entity:GetNode(i)
        local draw = node:GetDrawable()
        if draw ~= nil then
            draw:AdjustMaterialTime(0.0)
        end
    end
end

function SetBodyState(entity, state, update_flag)
    local node = entity:FindNodeByClassName('Body')
    local draw = node:GetDrawable()
    local name = 'LPC/Character/' .. entity.race .. '/' .. state

    draw:SetMaterial(name)
    draw:SetFlag('UpdateMaterial', update_flag)
    draw:ClearUniforms()
    draw:SetActiveTextureMap('South')
end

function SetGarbState(entity, state, update_flag)
    local node = entity:FindNodeByClassName('Garb')
    if node == nil then
        return
    end
    local draw = node:GetDrawable()

    draw:SetMaterial('LPC/Clothing/' .. entity.garb .. '/' .. state)
    draw:SetFlag('UpdateMaterial', update_flag)
    draw:ClearUniforms()
    draw:SetActiveTextureMap('South')
end

function SetWeaponState(entity, state)
    local node = entity:FindNodeByClassName('Weapon')
    if node == nil then
        return
    end
    local draw = node:GetDrawable()

    if state == nil then
        draw:SetFlag('UpdateMaterial', false)
        draw:SetFlag('VisibleInGame', false)
        return
    end

    draw:SetMaterial('LPC/Weapon/' .. entity.weapon .. '/' .. state)
    draw:SetFlag('UpdateMaterial', true)
    draw:SetFlag('VisibleInGame', true)
    draw:ClearUniforms()
    draw:SetActiveTextureMap('South')

end

-- Called once when the animator is first created.
-- This is the place where you can set the initial entity and animator
-- state to a known/desired first state.
function Init(animator, entity)
    Game:DebugPrint('Init animator')

    SetBodyState(entity, 'Walk', false)
    SetGarbState(entity, 'Walk', false)
    SetWeaponState(entity, nil)

    ChooseDirection(entity)
end

-- Called once when the entity enters a new animation state at the end
-- of a transition.
function EnterState(animator, state, entity)
    Game:DebugPrint('Enter State:' .. state)

    if state == 'Walking' then
        SetBodyState(entity, 'Walk', true)
        SetGarbState(entity, 'Walk', true)
        SetWeaponState(entity, nil)

    elseif state == 'Idle' then
        SetBodyState(entity, 'Walk', false)
        SetGarbState(entity, 'Walk', false)
        SetWeaponState(entity, nil)
        ResetTimes(entity)

    elseif state == 'Attack' then
        ResetTimes(entity)
        if entity.weapon == 'Spear' then
            SetBodyState(entity, 'Thrust', true)
            SetGarbState(entity, 'Thrust', true)
            SetWeaponState(entity, 'Thrust')
        elseif entity.weapon == 'Bow+Arrow' then
            SetBodyState(entity, 'Shoot', true)
            SetGarbState(entity, 'Shoot', true)
            SetWeaponState(entity, 'Shoot')
        elseif entity.weapon == 'Dagger' then
            SetBodyState(entity, 'Slash', true)
            SetGarbState(entity, 'Slash', true)
            SetWeaponState(entity, 'Slash')
        end

    elseif state == 'Dead' then
        ResetTimes(entity)
        SetBodyState(entity, 'Die', true)
        SetGarbState(entity, 'Die', true)
        SetWeaponState(entity, nil)
    end

    ChooseDirection(entity)
end

-- Called once when the entity is leaving an animation state at the start
-- of a transition.
function LeaveState(animator, state, entity)

end

-- Called continuously on the current state. This is the place where you can
-- realize changes to the current input when in some particular state.
function UpdateState(animator, state, time, dt, entity)
    if state == 'Walking' then
        local len = glm.length(entity.velocity)
        if len < 0.1 then
            return
        end
        ChooseDirection(entity)
    end
end

-- Evaluate the condition to trigger a transition from one animation state
-- to another. Return true to take the transition or false to reject it.
function EvalTransition(animator, from, to, entity)
    local speed = glm.length(entity.velocity)

    if from == 'Idle' and to == 'Walking' then
        if speed > 0.1 then
            return true
        end
    end

    if from == 'Idle' and to == 'Attack' then
        if animator.attack == true then
            animator.attack = false
            return true
        end
    end

    if from == 'Walking' and to == 'Idle' then
        if speed > 0.1 then
            return false
        end
        return true
    end

    if from == 'Attack' and to == 'Idle' then
        if animator:GetTime() >= 0.8 then
            entity:PostEvent('strike', 'animator', entity.weapon)
            return true
        end
    end

    -- any state can go to Dead state, so don't bother
    -- checking the from state.
    if to == 'Dead' then
        if entity.health <= 0.0 then
            return true
        end
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
