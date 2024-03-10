require('app://scripts/utility/keyboard.lua')

--
-- Entity 'bandit' script.
-- This script will be called for every instance of 'bandit' in the scene during gameplay.
-- You're free to delete functions you don't need.
local direction = 'right'

local jump_power = 5.0
local walk_power = 3.0

--
-- Called once when the game play begins for the entity in the scene.
function BeginPlay(bandit, scene, map)
    local animator = bandit:GetAnimator()
    animator.attack = false
    animator.jump = false
end

-- Called once when the game play ends for the entity in the scene.
function EndPlay(bandit, scene, map)
end

-- Called on every low frequency game tick. The tick frequency is
-- determined in the project settings. If you want to perform animation
-- such as move your game objects more smoothly then Update is the place
-- to do it. This function can be used to do thing such as evaluate AI or
-- path finding etc.
function Tick(bandit, game_time, dt)
end

-- Called on every iteration of the game loop. game_time is the current
-- game time so far in seconds not including the next time step dt.
-- allocator is an instance of game.EntityNodeAllocator that provides
-- the storage for the entity nodes. Keep in mind that this contains
-- *all* the nodes of any specific entity type. So the combination of
-- all the nodes across all entity instances 'klass' type.
-- Any component for any given node (at some index) may be nil so you
-- need to remember to check for nils before accessing.
function UpdateNodes(allocator, game_time, dt, klass)
end

function SetDirection(bandit, dir)
    if direction == dir then
        return
    end
    local skin_node = bandit:GetNode(1)
    local skin_draw = skin_node:GetDrawable()
    if dir == 'left' then
        skin_draw:SetFlag('FlipHorizontally', true)
        skin_node:Translate(20.0, 0.0)
    else
        skin_draw:SetFlag('FlipHorizontally', false)
        skin_node:Translate(-20.0, 0.0)
    end
    direction = dir
end

function ApplyWalkingImpulse(bandit, dir, walk_power)
    local animator = bandit:GetAnimator()
    local state = animator:GetStateName()
    local action = animator.action

    local body_node = bandit:GetNode(0)
    local rigid_body = body_node:GetRigidBody()

    -- note that when walking its possible that the physics
    -- engine has issues around edges of shapes, for example if
    -- the chracter is running on blocks and each block is a
    -- separate physics body sometimes the character may get
    -- stuck at the block edge.
    -- https://gamedev.stackexchange.com/questions/83585/box2d-bodies-that-are-really-close-together-are-getting-stuck
    -- the character has a physics body shape with slanted
    -- bottom to help mitigate the issue
    -- when there's a weird "bop" it's likely the physics engine
    -- applying upwards motionwhen the horizal impulse forces
    -- the object above the "sticking point"

    if direction == dir and state == 'Idle' then
        -- if we're idle and already facing that direction then 
        -- apply the initial impulse to start walking
        rigid_body:ApplyImpulse(walk_power, 0.0)

    elseif direction == dir and (action == 'Walk' or action == 'Jump') then
        -- if we're walking and already facing that direction then
        -- apply a throttled impulse to maintain walking without 
        -- picking up too much velocity.
        -- also if have jumped (positive Y velocity) then maintain
        -- that velocity too since using 0 for target vertical velocity
        -- will obviously kill the jump
        local velocity = rigid_body:GetLinearVelocity()
        local _, mass = Physics:FindMass(body_node)
        local impulse = util.FindImpulse(velocity,
                                         glm.vec2:new(walk_power, velocity.y),
                                         mass)
        rigid_body:ApplyImpulse(impulse)

    elseif direction ~= dir and action == 'Walk' then
        -- if we're walking and need to turn around then 'hit the brakes'
        -- by applying an impulse that will bring the current velocity to 0.0
        local _, mass = Physics:FindMass(body_node)
        local impulse = util.FindImpulse(rigid_body:GetLinearVelocity(),
                                         glm.vec2:new(0.0, 0.0), mass)
        rigid_body:ApplyImpulse(impulse)

        KB.ClearAll()
    end
end

function ApplyJumpingImpulse(bandit, jump)
    local animator = bandit:GetAnimator()
    local state = animator:GetStateName()
    local action = animator.action

    if state == 'Idle' or (state == 'Active' and action == 'Walk') then
        local body_node = bandit:GetNode(0)
        local rigid_body = body_node:GetRigidBody()
        rigid_body:AddImpulse(0.0, -jump)
    end
end

-- Called on every iteration of the game loop. game_time is the current
-- game time so far in seconds not including the next time step dt.
function Update(bandit, game_time, dt)

    local animator = bandit:GetAnimator()
    if animator:IsInTransition() then
        return
    end

    local state = animator:GetStateName()
    local action = animator.action

    local body_node = bandit:GetNode(0)
    local rigid_body = body_node:GetRigidBody()
    local skin_node = bandit:GetNode(1)
    local skin_draw = skin_node:GetDrawable()

    if KB.TestKeyDown(KB.Keys.Fire) then
        animator.attack = true
    end

    if rigid_body:HasPendingImpulse() then
        return
    end

    if KB.TestKeyDown(KB.Keys.Left) then
        ApplyWalkingImpulse(bandit, 'left', -3.0)
        SetDirection(bandit, 'left')
    end

    if KB.TestKeyDown(KB.Keys.Right) then
        ApplyWalkingImpulse(bandit, 'right', 3.0)
        SetDirection(bandit, 'right')
    end

    if KB.TestKeyDown(KB.Keys.Up) then
        ApplyJumpingImpulse(bandit, 7.0)
        KB.ClearKey(KB.Keys.Up)
    end
end

-- Called on every iteration of the game loop game after *all* entities
-- in the scene have been updated. This means that all objects are in their
-- final places and it's possible to do things such as query scene spatial
-- nodes for finding interesting objects in any particular location.
function PostUpdate(bandit, game_time)
end

-- Called on collision events with other objects based on the information
-- from the physics engine. You can only get these events  when your entity
-- node(s) have rigid bodies and are colliding with other rigid bodies. The
-- contact can exist over multiple time steps depending on the type of bodies etc.
-- Node is this entity's entity node with rigid body that collided with the
-- other entity's other_node's rigid body.
function OnBeginContact(bandit, node, other_entity, other_node)
end

-- Similar to OnBeginContact except this happens when the contact ends.
function OnEndContact(bandit, node, other_entity, other_node)
end

-- Called on key down events. This is only called when the entity has enabled
-- the keyboard input processing to take place. You can find this setting under
-- 'Script callbacks' in the entity editor. Symbol is one of the virtual key
-- symbols rom the wdk.Keys table and modifier bits is the bitwise combination
-- of control keys (Ctrl, Shift, etc) at the time of the key event.
-- The modifier_bits are expressed as an object of wdk.KeyBitSet.
--
-- Note that because some platforms post repeated events when a key is
-- continuously held you can get this event multiple times without getting
-- the corresponding key up!
function OnKeyDown(bandit, symbol, modifier_bits)
    KB.KeyDown(symbol, modifier_bits, KB.ARROW)
end

-- Called on key up events. See OnKeyDown for more details.
function OnKeyUp(bandit, symbol, modifier_bits)
    KB.KeyUp(symbol, modifier_bits, KB.ARROW)
end

-- Called on mouse button press events. This is only called when the entity
-- has enabled the mouse input processing to take place. You can find this
-- setting under 'Script callbacks' in the entity editor.
-- Mouse argument is of type game.MouseEvent and provides an aggregate of
-- information about the event. You can find more details about this type in
-- the Lua API doc.
function OnMousePress(bandit, mouse)
end

-- Called on mouse button release events. See OnMousePress for more details.
function OnMouseRelease(bandit, mouse)
end

-- Called on mouse move events. See OnMousePress for more details.
function OnMouseMove(bandit, mouse)
end

-- Called on game events. Game events are broad-casted to all entities in
-- the scene.  GameEvents are useful when there's an unknown number of
-- entities possibly interested in some game event. Use Game:PostEvent to
-- post a new game event. Each entity will then receive the same event object
-- in this callback and can proceed to process the information.
function OnGameEvent(bandit, event)
end

-- Called on animation finished events, i.e. when this entity has finished
-- playing the animation in question.
function OnAnimationFinished(bandit, animation)
end

-- Called on timer events. Timers are set on an Entity by calling SetTimer.
-- When the timer expires this callback is then invoked. Timer is then the
-- name of the timer (same as in SetTimer) that fired and jitter defines
-- the difference to ideal time when the timer should have fired. In general
-- entity timers are limited in their resolution to game update resolution.
-- In other words if the game updates at 60 Hz the timer frequency is then
-- 1/60 seconds. If jitter is positive it means the timer is firing early
-- and a negative value indicates the timer fired late.
function OnTimer(bandit, timer, jitter)
end

-- Called on posted entity events. Events can be posted on particular entities
-- by calling entity:PostEvent. Unlike game.GameEvents game.EntityEvent are
-- entity specific and only ever delivered to a single entity (the receiver).
function OnEvent(bandit, event)
end
