--
-- Entity 'big_sob' script.
-- This script will be called for every instance of 'big_sob' in the scene during gameplay.
-- You're free to delete functions you don't need.
--
-- luaformatter off
local sob_state = 'stand'
local sob_direction = 'left'
local sob_stop_time = 0.0
local sob_weapon_fire_time = 0.0
local sob_health = 1.0

local EvilSounds = {}
EvilSounds[0] = 'Merc/Enemy/SOB/Laughter'
EvilSounds[1] = 'Merc/Enemy/SOB/DieDieDie'
EvilSounds[2] = 'Merc/Enemy/SOB/Fire!'

-- luaformatter on

function BulletHit(sob, bullet)
    Game:DebugPrint('SOB HIT!')

    local sob_health_node = sob:FindNodeByClassName('Health')
    local sob_health_draw = sob_health_node:GetDrawable()

    local damage = 0.0
    local bullet_class = bullet:GetClassName()
    if bullet_class == 'Merc/Projectile/Laser' then
        damage = 0.005
    elseif bullet_class == 'Merc/Projectile/Slime' then
        damage = 0.01
    elseif bullet_class == 'Merc/Projectile/Flame' then
        damage = 0.001
    end

    sob_health = sob_health - damage
    Game:DebugPrint(tostring(sob_health))
    sob_health_draw:SetUniform('kValue', sob_health)
end

function Walk(sob, direction)
    if sob_state == 'walk' and sob_direction == direction then
        return
    end

    local sob_body_node = sob:GetNode(0)
    local sob_body_draw = sob_body_node:GetDrawable()
    local sob_body_transformer = sob_body_node:GetTransformer()

    if direction == 'left' then
        sob_body_draw:SetFlag('UpdateMaterial', true)
        sob_body_draw:AdjustMaterialTime(0.0)
        sob_body_draw:SetTimeScale(1.0)
        sob_body_transformer:SetLinearVelocity(-120.0, 0.0)
    elseif direction == 'right' then
        sob_body_draw:SetFlag('UpdateMaterial', true)
        sob_body_draw:AdjustMaterialTime(0.0)
        sob_body_draw:SetTimeScale(-1.0)
        sob_body_transformer:SetLinearVelocity(120.0, 0.0)
    end

    sob_body_transformer:Enable(true)

    sob_state = 'walk'
    sob_direction = direction
    Game:DebugPrint('SOB State: ' .. sob_state)

    Audio:PlaySoundEffect('Merc/Enemy/SOB/Step')
end

function OppositeDirection(direction)
    if direction == 'left' then
        return 'right'
    elseif direction == 'right' then
        return
    end
end

function Stop(sob, game_time)
    if sob_state == 'stand' then
        return
    end

    local sob_body_node = sob:GetNode(0)
    local sob_body_draw = sob_body_node:GetDrawable()
    local sob_body_transformer = sob_body_node:GetTransformer()
    sob_body_draw:SetFlag('UpdateMaterial', false)
    sob_body_draw:AdjustMaterialTime(0.0)
    sob_body_transformer:Enable(false)

    sob_stop_time = game_time
    sob_state = 'stand'
    Game:DebugPrint('SOB State: ' .. sob_state)

    Audio:KillSoundEffect('Merc/Enemy/SOB/Step')
end

function FireWeapon(sob, game_time)
    if sob_state ~= 'stand' then
        return
    end

    if (game_time - sob_stop_time) < 1.0 then
        return
    end

    if (game_time - sob_weapon_fire_time) < 2.0 then
        return
    end

    local sob_flame_node = sob:GetNode(2)
    local sob_flame_draw = sob_flame_node:GetDrawable()
    sob_flame_draw:AdjustMaterialTime(0.0)

    Game:DebugPrint('Sob weapon fire!')
    sob:PlayAnimation('Blow Fire')
    sob_weapon_fire_time = game_time

    Audio:PlaySoundEffect('Merc/Weapon/Flame')

    local evil_sound = util.Random(0, 6)
    if evil_sound >= 3 then
        return
    end

    Audio:PlaySoundEffect(EvilSounds[evil_sound])
end

-- Called once when the game play begins for the entity in the scene.
function BeginPlay(big_sob, scene, map)
    Audio:PlaySoundEffect('Merc/Enemy/SOB/Engine')
end

-- Called once when the game play ends for the entity in the scene.
function EndPlay(big_sob, scene, map)
end

-- Called on every low frequency game tick. The tick frequency is
-- determined in the project settings. If you want to perform animation
-- such as move your game objects more smoothly then Update is the place
-- to do it. This function can be used to do thing such as evaluate AI or
-- path finding etc.
function Tick(big_sob, game_time, dt)
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
function Update(big_sob, game_time, dt)
    local hero = Scene:FindEntityByInstanceName('Hero')
    if hero == nil then
        return
    end

    local hero_body_node = hero:GetNode(0)
    local hero_body_pos = hero_body_node:GetTranslation()
    local sob_body_node = big_sob:GetNode(0)
    local sob_body_pos = sob_body_node:GetTranslation()

    local distance = glm.length(hero_body_pos - sob_body_pos)

    local direction = 'right'
    if hero_body_pos.x < sob_body_pos.x then
        direction = 'left'
    end

    if distance > 200.0 then
        Walk(big_sob, direction)
    elseif distance < 180.0 then
        Walk(big_sob, OppositeDirection(direction))
    else
        Stop(big_sob, game_time)
    end

    FireWeapon(big_sob, game_time)

    if sob_state == 'walk' then
        local event = game.GameEvent:new()
        event.from = 'sob'
        event.to = 'game'
        event.message = 'walking'
        Game:PostEvent(event)
    end
end

-- Called on every iteration of the game loop game after *all* entities
-- in the scene have been updated. This means that all objects are in their
-- final places and it's possible to do things such as query scene spatial
-- nodes for finding interesting objects in any particular location.
function PostUpdate(big_sob, game_time)
end

-- Called on collision events with other objects based on the information
-- from the physics engine. You can only get these events  when your entity
-- node(s) have rigid bodies and are colliding with other rigid bodies. The
-- contact can exist over multiple time steps depending on the type of bodies etc.
-- Node is this entity's entity node with rigid body that collided with the
-- other entity's other_node's rigid body.
function OnBeginContact(big_sob, node, other_entity, other_node)
end

-- Similar to OnBeginContact except this happens when the contact ends.
function OnEndContact(big_sob, node, other_entity, other_node)
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
function OnKeyDown(big_sob, symbol, modifier_bits)
end

-- Called on key up events. See OnKeyDown for more details.
function OnKeyUp(big_sob, symbol, modifier_bits)
end

-- Called on mouse button press events. This is only called when the entity
-- has enabled the mouse input processing to take place. You can find this
-- setting under 'Script callbacks' in the entity editor.
-- Mouse argument is of type game.MouseEvent and provides an aggregate of
-- information about the event. You can find more details about this type in
-- the Lua API doc.
function OnMousePress(big_sob, mouse)
end

-- Called on mouse button release events. See OnMousePress for more details.
function OnMouseRelease(big_sob, mouse)
end

-- Called on mouse move events. See OnMousePress for more details.
function OnMouseMove(big_sob, mouse)
end

-- Called on game events. Game events are broad-casted to all entities in
-- the scene.  GameEvents are useful when there's an unknown number of
-- entities possibly interested in some game event. Use Game:PostEvent to
-- post a new game event. Each entity will then receive the same event object
-- in this callback and can proceed to process the information.
function OnGameEvent(big_sob, event)
end

-- Called on animation finished events, i.e. when this entity has finished
-- playing the animation in question.
function OnAnimationFinished(big_sob, animation)
end

-- Called on timer events. Timers are set on an Entity by calling SetTimer.
-- When the timer expires this callback is then invoked. Timer is then the
-- name of the timer (same as in SetTimer) that fired and jitter defines
-- the difference to ideal time when the timer should have fired. In general
-- entity timers are limited in their resolution to game update resolution.
-- In other words if the game updates at 60 Hz the timer frequency is then
-- 1/60 seconds. If jitter is positive it means the timer is firing early
-- and a negative value indicates the timer fired late.
function OnTimer(big_sob, timer, jitter)
end

-- Called on posted entity events. Events can be posted on particular entities
-- by calling entity:PostEvent. Unlike game.GameEvents game.EntityEvent are
-- entity specific and only ever delivered to a single entity (the receiver).
function OnEvent(big_sob, event)
end
