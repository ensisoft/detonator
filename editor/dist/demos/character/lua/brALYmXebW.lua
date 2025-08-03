require('app://scripts/utility/keyboard.lua')

--
-- Entity 'hero' script.
-- This script will be called for every instance of 'hero' in the scene during gameplay.
-- You're free to delete functions you don't need.
--

local Weapons = {}
Weapons[0] = {
    name = 'Laser',
    bullet = 'Merc/Projectile/Laser',
    bullet_velocity = 1000.0,
    sound_graph = 'Merc/Weapon/Laser',
    sound_delay = 0.0,
    firing_rate = 10
}
Weapons[1] = {
    name = 'Slime Gun',
    bullet = 'Merc/Projectile/Slime',
    bullet_velocity = 500.0,
    sound_graph = 'Merc/Weapon/Slime',
    sound_delay = 0.0,
    firing_rate = 10
}
Weapons[2] = {
    name = 'Flame Thrower',
    bullet = 'Merc/Projectile/Flame',
    bullet_velocity = 700.0,
    sound_graph = 'Merc/Weapon/Flame',
    sound_delay = 1.0,
    firing_rate = 100
}

-- LuaFormatter off

-- state values, these are going directly to the material
-- in a uniform in order to change the material sprite cycle
local States = {}
States['stand'] = 0
States['kneel'] = 1
States['run']   = 2

local mouse_scene_coord
local hero_state = 'stand'
local hero_direction = 'right'
local fire_weapon = false
local weapon_dir = nil
local weapon = Weapons[0]
local weapon_fire_time = 0.0
local weapon_sound_time = 0.0

-- LuaFormatter on

function ChangeWeapon(hero, weapon_index)
    weapon = Weapons[weapon_index]
    Audio:PlaySoundEffect('Merc/Weapon Switch')
end

function SetState(hero, state)
    local hero_node = hero:GetNode(0)
    local hero_skin = hero_node:GetDrawable()
    local hero_weapon_node = hero:GetNode(1)
    local mover = hero_node:GetLinearMover()
    hero_skin:SetUniform('kState', States[state])
    hero_state = state

    if state == 'stand' or state == 'run' then
        hero_weapon_node:SetTranslation(0.0, 0.0)

        if state == 'run' then
            if hero_direction == 'right' then
                mover:SetLinearVelocity(300.0, 0.0)
            elseif hero_direction == 'left' then
                mover:SetLinearVelocity(-300.0, 0.0)
            end
        elseif state == 'stand' then
            mover:SetLinearVelocity(0.0, 0.0)
        end

    elseif state == 'kneel' then
        hero_weapon_node:SetTranslation(0.0, 30.0)

        mover:SetLinearVelocity(0.0, 0.0)
    end
end

function SetDir(hero, dir)
    local hero_node = hero:GetNode(0)
    local hero_skin = hero_node:GetDrawable()
    if dir == 'left' then
        hero_skin:SetFlag('FlipHorizontally', true)
    elseif dir == 'right' then
        hero_skin:SetFlag('FlipHorizontally', false)
    end

    hero_direction = dir

    if mouse_scene_coord == nil then
        return
    end
    AimWeapon(hero, mouse_scene_coord)
end

function MapLeftAim(angle)
    local mappedAngle = 180 - angle
    if mappedAngle < 0 then
        mappedAngle = 360 + mappedAngle
    end
    return mappedAngle
end

function AimWeapon(hero, aim_at)
    local hero_body_node = hero:GetNode(0)
    local hero_body_skin = hero_body_node:GetDrawable()
    local hero_world_pos = hero_body_node:GetTranslation()
    local hero_weapon_node = hero:GetNode(1)

    -- aim direction relative to hero
    local direction = aim_at - hero_world_pos
    local aim_tangent = math.atan2(-direction.y, direction.x)

    hero_weapon_node:SetRotation(-aim_tangent)

    local aim_angle = math.deg(aim_tangent)
    if aim_angle < 0.0 then
        aim_angle = 180 + (180 + aim_angle)
    end
    -- Game:DebugPrint(tostring(aim_angle))

    aim_angle = base.clamp(0.0, 360.0, aim_angle)

    if hero_direction == 'left' then
        aim_angle = MapLeftAim(aim_angle)
    end

    -- normalize the aim angle
    local aim_value = aim_angle / 360.0
    hero_body_skin:SetUniform('kAimAngle', aim_value)

    weapon_dir = glm.normalize(direction)
end

function FireWeapon(hero, game_time)
    if fire_weapon == false then
        return
    end

    local firing_interval = 1.0 / weapon.firing_rate
    if game_time - weapon_fire_time < firing_interval then
        return
    end

    local weapon_muzzle_node = hero:GetNode(3)
    local weapon_muzzle_transform = Scene:FindEntityNodeTransform(hero,
                                                                  weapon_muzzle_node)
    local weapon_muzzle_world_pos = util.GetTranslationFromMatrix(
                                        weapon_muzzle_transform)

    Scene:SpawnEntity(weapon.bullet, {
        pos = weapon_muzzle_world_pos,
        layer = 10,
        vars = {
            velocity = weapon_dir * weapon.bullet_velocity
        }
    })

    weapon_fire_time = game_time

    if game_time - weapon_sound_time >= weapon.sound_delay then
        Audio:PlaySoundEffect(weapon.sound_graph)
        weapon_sound_time = game_time
    end
end

-- Called once when the game play begins for the entity in the scene.
function BeginPlay(hero, scene, map)
    SetState(hero, 'stand')
end

-- Called once when the game play ends for the entity in the scene.
function EndPlay(hero, scene, map)
end

-- Called on every low frequency game tick. The tick frequency is
-- determined in the project settings. If you want to perform animation
-- such as move your game objects more smoothly then Update is the place
-- to do it. This function can be used to do thing such as evaluate AI or
-- path finding etc.
function Tick(hero, game_time, dt)
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
function Update(hero, game_time, dt)
    if mouse_scene_coord == nil then
        return
    end

    AimWeapon(hero, mouse_scene_coord)
    FireWeapon(hero, game_time)
end

-- Called on every iteration of the game loop game after *all* entities
-- in the scene have been updated. This means that all objects are in their
-- final places and it's possible to do things such as query scene spatial
-- nodes for finding interesting objects in any particular location.
function PostUpdate(hero, game_time)
end

-- Called on collision events with other objects based on the information
-- from the physics engine. You can only get these events  when your entity
-- node(s) have rigid bodies and are colliding with other rigid bodies. The
-- contact can exist over multiple time steps depending on the type of bodies etc.
-- Node is this entity's entity node with rigid body that collided with the
-- other entity's other_node's rigid body.
function OnBeginContact(hero, node, other_entity, other_node)
end

-- Similar to OnBeginContact except this happens when the contact ends.
function OnEndContact(hero, node, other_entity, other_node)
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
function OnKeyDown(hero, symbol, modifier_bits)
    KB.KeyDown(symbol, modifier_bits, KB.WASD)

    if KB.TestKeyDown(KB.Keys.Right) then
        if hero_direction == 'right' then
            if hero_state == 'stand' or hero_state == 'kneel' then
                SetState(hero, 'run')
            end
        elseif hero_direction == 'left' then
            if hero_state == 'run' then
                SetState(hero, 'stand')
            else
                SetDir(hero, 'right')
            end
        end

    elseif KB.TestKeyDown(KB.Keys.Left) then
        if hero_direction == 'left' then
            if hero_state == 'stand' or hero_state == 'kneel' then
                SetState(hero, 'run')
            end
        elseif hero_direction == 'right' then
            if hero_state == 'run' then
                SetState(hero, 'stand')
            else
                SetDir(hero, 'left')
            end
        end
    elseif KB.TestKeyDown(KB.Keys.Down) then
        SetState(hero, 'kneel')
    end

end

-- Called on key up events. See OnKeyDown for more details.
function OnKeyUp(hero, symbol, modifier_bits)
    KB.KeyUp(symbol, modifier_bits, KB.WASD)

    if KB.TestKeyUp(KB.Keys.Right) and hero_direction == 'right' then
        SetState(hero, 'stand')
    end

    if KB.TestKeyUp(KB.Keys.Left) and hero_direction == 'left' then
        SetState(hero, 'stand')
    end

    if symbol == wdk.Keys.Key1 then
        ChangeWeapon(hero, 0)
    elseif symbol == wdk.Keys.Key2 then
        ChangeWeapon(hero, 1)
    elseif symbol == wdk.Keys.Key3 then
        ChangeWeapon(hero, 2)
    end

end

-- Called on mouse button press events. This is only called when the entity
-- has enabled the mouse input processing to take place. You can find this
-- setting under 'Script callbacks' in the entity editor.
-- Mouse argument is of type game.MouseEvent and provides an aggregate of
-- information about the event. You can find more details about this type in
-- the Lua API doc.
function OnMousePress(hero, mouse)
    if mouse.button == wdk.Buttons.Left then
        fire_weapon = true
    end
end

-- Called on mouse button release events. See OnMousePress for more details.
function OnMouseRelease(hero, mouse)
    if mouse.button == wdk.Buttons.Left then
        fire_weapon = false
    end
end

-- Called on mouse move events. See OnMousePress for more details.
function OnMouseMove(hero, mouse)
    if mouse.over_scene then
        mouse_scene_coord = mouse.scene_coord
    else
        mouse_scene_coord = nil
    end

end

-- Called on game events. Game events are broad-casted to all entities in
-- the scene.  GameEvents are useful when there's an unknown number of
-- entities possibly interested in some game event. Use Game:PostEvent to
-- post a new game event. Each entity will then receive the same event object
-- in this callback and can proceed to process the information.
function OnGameEvent(hero, event)
end

-- Called on animation finished events, i.e. when this entity has finished
-- playing the animation in question.
function OnAnimationFinished(hero, animation)
end

-- Called on timer events. Timers are set on an Entity by calling SetTimer.
-- When the timer expires this callback is then invoked. Timer is then the
-- name of the timer (same as in SetTimer) that fired and jitter defines
-- the difference to ideal time when the timer should have fired. In general
-- entity timers are limited in their resolution to game update resolution.
-- In other words if the game updates at 60 Hz the timer frequency is then
-- 1/60 seconds. If jitter is positive it means the timer is firing early
-- and a negative value indicates the timer fired late.
function OnTimer(hero, timer, jitter)
end

-- Called on posted entity events. Events can be posted on particular entities
-- by calling entity:PostEvent. Unlike game.GameEvents game.EntityEvent are
-- entity specific and only ever delivered to a single entity (the receiver).
function OnEvent(hero, event)
end
