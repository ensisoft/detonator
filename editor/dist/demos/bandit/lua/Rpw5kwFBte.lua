--
-- Entity 'dino' script.
--
-- This script will be called for every instance of 'dino' in the scene
-- during gameplay.
-- You're free to delete functions you don't need.
--
require('app://scripts/utility/keyboard.lua')

local direction = 'right'
local state = 'ground'
local jump_lateral_motion_left = false
local jump_lateral_motion_right = false

function HandleAirState(dino)
    local body_node = dino:GetNode(0)
    local skin_node = dino:GetNode(1)
    local dino_skin = skin_node:GetDrawable()
    local dino_body = body_node:GetRigidBody()
    local velocity = dino_body:GetLinearVelocity()

    if KB.TestKeyDown(KB.Keys.Left) then
        if not jump_lateral_motion_left then
            dino_body:ApplyImpulse(-3.0, 0.0)
            jump_lateral_motion_left = true
        end
    elseif KB.TestKeyDown(KB.Keys.Right) then
        if not jump_lateral_motion_right then
            dino_body:ApplyImpulse(3.0, 0.0)
            jump_lateral_motion_right = true
        end
    end
end

function HandleGroundState(dino)
    local body_node = dino:GetNode(0)
    local skin_node = dino:GetNode(1)
    local dino_skin = skin_node:GetDrawable()
    local dino_body = body_node:GetRigidBody()
    local velocity = dino_body:GetLinearVelocity()
    local speed = math.abs(velocity.x)

    -- when pressing a key to run to some direction apply a force
    -- in that direction continuously but to avoid picking up 
    -- too much velocity scale that force based on the current velocity.
    -- i.e. the closer to the target velocity the weaker the force.

    -- 
    -- when the key is no longer pressed in order to make the 
    -- character stop immediately apply a strong immediate impulse
    -- that brings the charactersv velocity to zero immediately.

    if KB.TestKeyDown(KB.Keys.Left) then
        local m = 1.0 - (speed / 10.0)
        if m > 0.0 then
            dino_body:ApplyForce(-30.0 * m, 0.1)
        end
        if speed > 3.0 then
            dino_skin:SetActiveTextureMap('Run')
        else
            dino_skin:SetActiveTextureMap('Walk')
        end
        if direction == 'right' then
            dino_skin:SetFlag('FlipHorizontally', true)
            skin_node:SetTranslation(-50.0, 10.0)
            direction = 'left'
        end

    elseif KB.TestKeyDown(KB.Keys.Right) then
        local m = 1.0 - (speed / 10.0)
        if m > 0.0 then
            dino_body:ApplyForce(30.0 * m, 0.1)
        end
        if speed > 3.0 then
            dino_skin:SetActiveTextureMap('Run')
        else
            dino_skin:SetActiveTextureMap('Walk')
        end
        if direction == 'left' then
            dino_skin:SetFlag('FlipHorizontally', false)
            skin_node:SetTranslation(50.0, 10.0)
            direction = 'right'
        end
    else
        local _, mass = Physics:FindMass(body_node)
        local impulse = util.FindImpulse(velocity, glm.vec2:new(0.0, 0.0), mass)
        Physics:ApplyImpulseToCenter(body_node, impulse)

        dino_skin:SetActiveTextureMap('Idle')
    end

    -- if we have jumped and are back on the ground
    -- eliminate bouncing by applying an impulse to 
    -- bring our upwards bounce velocity to zero.
    if velocity.y < 0.1 then
        local _, mass = Physics:FindMass(body_node)
        local impulse = util.FindImpulse(velocity,
                                         glm.vec2:new(velocity.x, 0.0), mass)
        Physics:ApplyImpulseToCenter(body_node, impulse)
    end

    if KB.TestKeyDown(KB.Keys.Up) then
        state = 'propel'
        dino_skin:RunSpriteCycle('Jump')
        dino_body:AddImpulse(0.0, -6.0)
        dino_body:AddImpulse(util.signum(velocity.x) * -2.0, 0.0)

        KB.ClearKey(KB.Keys.Up)

        jump_lateral_motion_left = false
        jump_lateral_motion_right = false
    end

end

-- Called once when the game play begins for the entity in the scene.
function BeginPlay(dino, scene, map)
end

-- Called once when the game play ends for the entity in the scene.
function EndPlay(dino, scene, map)
end

-- Called on every low frequency game tick. The tick frequency is
-- determined in the project settings. If you want to perform animation
-- such as move your game objects more smoothly then Update is the place
-- to do it. This function can be used to do thing such as evaluate AI or
-- path finding etc.
function Tick(dino, game_time, dt)
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

-- Called on every iteration of the game loop. game_time is the current
-- game time so far in seconds not including the next time step dt.
function Update(dino, game_time, dt)

    local body_node = dino:GetNode(0)
    local position = body_node:GetTranslation()

    -- if you allow an multiple impulses or continuous application
    -- of force to left/right direction when jumping
    -- then you allow flying. BUT if you don't allow it then the
    -- character movement is janky because jumping and then moving 
    -- is a natural game character movement in a platformer

    -- Checking for "jumping" or "falling" using velocity is wrong
    -- because the character could be on a platform that moves 
    -- and thus has velocity even though the relative velocity is 0.0

    -- Using force to move the body works differently if the character
    -- is on the ground or off the ground because of friction

    -- Applying an impulse will easily accumulate too much velocity
    -- without some throttling on the impulse 

    -- clipping or capping the velocity to hard caps and then 
    -- manually adjusting the current velocity causes janks.

    -- running left / right should continue automatically as
    -- long as the key is held pressed even if the character jumps 
    -- in between. this means the keyboard state cannot be cleared
    -- when going left or right. BUT every jump should be a single
    -- event that is triggered by the Jump (UP) key

    -- Conclusions:
    -- Only allow jumping by applying an upwards impulse once 
    -- when the character is on the ground as reported by the 
    -- "foot" sensor.

    -- When on the ground use continuous application of force
    -- to move laterally (along X axis)    

    -- If the character has jumped and is in the air let the 
    -- character to move laterally once, implement this with
    -- a flag and by applying a small impulse once.

    if state == 'airborne' then
        HandleAirState(dino)
    elseif state == 'ground' then
        HandleGroundState(dino)
    elseif state ~= 'propel' then
        base.warn('Unexpected character state' .. state)
    end

    Game:SetCameraPosition(position.x + 300.0, position.y - 100.0)

end

-- Called on every iteration of the game loop game after *all* entities
-- in the scene have been updated. This means that all objects are in their
-- final places and it's possible to do things such as query scene spatial
-- nodes for finding interesting objects in any particular location.
function PostUpdate(dino, game_time)
end

-- Called on collision events with other objects based on the information
-- from the physics engine. You can only get these events  when your entity
-- node(s) have rigid bodies and are colliding with other rigid bodies. The
-- contact can exist over multiple time steps depending on the type of bodies etc.
-- Node is this entity's entity node with rigid body that collided with the
-- other entity's other_node's rigid body.
function OnBeginContact(dino, node, other_entity, other_node)
    local other_entity_name = other_entity:GetClassName()

    local node_name = node:GetClassName()
    if node_name == 'Foot' then
        if other_entity_name == 'Bee' then
            Game:DebugPrint('You killed the bee!')

            other_entity:Die()

        elseif other_entity_name == 'Coin' then

        else
            state = 'ground'
        end
    end

    if node_name ~= 'Body' then
        return
    end

    if other_entity:IsDying() then
        return
    end

    if other_entity_name == 'Coin' then
        dino.score = dino.score + 100
        other_entity:Die()
        Scene:SpawnEntity('Coins Particles', {
            pos = other_node:GetTranslation(),
            async = true
        })

    elseif other_entity_name == 'Shroom' then
        Game:DebugPrint('You ate some shrooms!')
        other_entity:Die()

        dino.health = dino.health - 0.2

    elseif other_entity_name == 'Bee' then
        Game:DebugPrint('Oh no! Stung by a bee!')
        other_entity:Die()

        dino.health = dino.health - 0.1

    end

end

-- Similar to OnBeginContact except this happens when the contact ends.
function OnEndContact(dino, node, other_entity, other_node)

    local node_name = node:GetClassName()
    if node_name == 'Foot' then
        state = 'airborne'
    end

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
function OnKeyDown(dino, symbol, modifier_bits)
    KB.KeyDown(symbol, modifier_bits, KB.ARROW)
end

-- Called on key up events. See OnKeyDown for more details.
function OnKeyUp(dino, symbol, modifier_bits)
    KB.KeyUp(symbol, modifier_bits, KB.ARROW)
end

-- Called on mouse button press events. This is only called when the entity
-- has enabled the mouse input processing to take place. You can find this
-- setting under 'Script callbacks' in the entity editor.
-- Mouse argument is of type game.MouseEvent and provides an aggregate of
-- information about the event. You can find more details about this type in
-- the Lua API doc.
function OnMousePress(dino, mouse)
end

-- Called on mouse button release events. See OnMousePress for more details.
function OnMouseRelease(dino, mouse)
end

-- Called on mouse move events. See OnMousePress for more details.
function OnMouseMove(dino, mouse)
end

-- Called on game events. Game events are broad-casted to all entities in
-- the scene.  GameEvents are useful when there's an unknown number of
-- entities possibly interested in some game event. Use Game:PostEvent to
-- post a new game event. Each entity will then receive the same event object
-- in this callback and can proceed to process the information.
function OnGameEvent(dino, event)
end

-- Called on animation finished events, i.e. when this entity has finished
-- playing the animation in question.
function OnAnimationFinished(dino, animation)
end

-- Called on timer events. Timers are set on an Entity by calling SetTimer.
-- When the timer expires this callback is then invoked. Timer is then the
-- name of the timer (same as in SetTimer) that fired and jitter defines
-- the difference to ideal time when the timer should have fired. In general
-- entity timers are limited in their resolution to game update resolution.
-- In other words if the game updates at 60 Hz the timer frequency is then
-- 1/60 seconds. If jitter is positive it means the timer is firing early
-- and a negative value indicates the timer fired late.
function OnTimer(dino, timer, jitter)
end

-- Called on posted entity events. Events can be posted on particular entities
-- by calling entity:PostEvent. Unlike game.GameEvents game.EntityEvent are
-- entity specific and only ever delivered to a single entity (the receiver).
function OnEvent(dino, event)
end
