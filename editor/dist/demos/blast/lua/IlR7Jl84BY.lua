--
-- Entity 'demo' script.
-- This script will be called for every instance of 'demo' in the scene during gameplay.
-- You're free to delete functions you don't need.
--
local even_odd_counter = 1

-- Called once when the game play begins for the entity in the scene.
function BeginPlay(demo, scene, map)
    if PreviewMode then
        local node = demo:GetNode(0)
        node:SetTranslation(300.0, 0.0)
    end

    local demo_body_node = demo:GetNode(0)
    local demo_body_pos = demo_body_node:GetTranslation()
    local demo_body_dir = glm.normalize(glm.vec2:new(0.0, 0.0) - demo_body_pos)
    local demo_body_angle = math.atan2(demo_body_dir.y, demo_body_dir.x)
    demo.circ_angle = demo_body_angle

    demo.inf_origin = demo_body_pos
    if demo_body_pos.x >= 0.0 then
        demo.inf_angle_dir = -1.0
        demo.inf_angle = math.pi
    end

end

-- Called once when the game play ends for the entity in the scene.
function EndPlay(demo, scene, map)
end

-- Called on every low frequency game tick. The tick frequency is
-- determined in the project settings. If you want to perform animation
-- such as move your game objects more smoothly then Update is the place
-- to do it. This function can be used to do thing such as evaluate AI or
-- path finding etc.
function Tick(demo, game_time, dt)
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

function CircularMotion(demo, game_time, dt)
    local angle = demo.circ_angle
    local origin = demo.circ_origin
    local radius = demo.circ_radius
    local demo_body_node = demo:GetNode(0)
    local demo_body_pos = demo_body_node:GetTranslation()

    local angle_velocity = 2.0 -- radians per sec
    angle = angle + angle_velocity * dt

    demo_body_pos = origin + glm.vec2:new(math.cos(angle), math.sin(angle)) *
                        radius

    demo.circ_angle = math.fmod(angle, math.pi * 2)

    demo_body_node:SetTranslation(demo_body_pos)

end

function InfinityMotion(demo, game_time, dt)
    local angle = demo.inf_angle
    local angle_accum = demo.inf_angle_accum
    local angle_dir = demo.inf_angle_dir

    local demo_body_node = demo:GetNode(0)
    local angle_velocity = 2.0 -- radians per sec
    local angle_increment = (angle_velocity * dt * angle_dir)
    angle = angle + angle_increment

    local orbit_origin = demo.inf_origin
    local radius = math.abs(orbit_origin.x)

    local demo_body_pos = orbit_origin +
                              glm.vec2:new(math.cos(angle), math.sin(angle)) *
                              radius

    local angle_ahead = angle + 0.1 * angle_dir
    local lookat_pos = orbit_origin +
                           glm.vec2:new(math.cos(angle_ahead),
                                        math.sin(angle_ahead)) * radius

    angle_accum = angle_accum + math.abs(angle_increment)

    if angle_accum >= math.pi * 2.0 then
        -- Game:DebugPrint('change direction')
        local angle_reminder = math.fmod(angle_accum, math.pi * 2.0)
        if angle_dir > 0.0 then
            angle = math.pi - angle_reminder
        else
            angle = angle_reminder
        end
        angle_dir = angle_dir * -1.0
        angle_accum = angle_reminder
        orbit_origin.x = orbit_origin.x * -1.0
        demo.inf_origin = orbit_origin
    end

    demo.inf_angle = angle
    demo.inf_angle_accum = angle_accum
    demo.inf_angle_dir = angle_dir

    demo_body_node:SetTranslation(demo_body_pos)

    LookAt(demo, lookat_pos)
end

function MoveInOut(demo, game_time, dt)
    local node = demo:GetNode(1)
    local direction = glm.vec2:new(0.0, 1.0)
    local t = math.fmod(game_time, 2.0) / 2.0
    local offset = math.sin(t * math.pi * 2.0) * direction * 100.0
    node:SetTranslation(offset)
end

function LookAt(demo, where)
    local demo_body_node = demo:GetNode(0)
    local demo_node_pos = demo_body_node:GetTranslation()
    local lookat_dir = glm.normalize(where - demo_node_pos)
    local lookat_angle = math.atan2(lookat_dir.y, lookat_dir.x)
    demo_body_node:SetRotation(lookat_angle - math.pi * 0.5)
end

function ElasticCircleMotion(demo, game_time, dt)
    local t = math.fmod(game_time, 20.0) / 20.0
    t = easing.adjust(t, easing.Curves.Acceleration)

    local x = math.sin(t * math.pi * 2.0)
    dt = dt * x

    CircularMotion(demo, game_time, dt)
    MoveInOut(demo, game_time, dt)
    LookAt(demo, demo.circ_origin)
end

-- Called on every iteration of the game loop. game_time is the current
-- game time so far in seconds not including the next time step dt.
function Update(demo, game_time, dt)

    -- 4 InfinityMotion(demo, game_time, dt)

    ElasticCircleMotion(demo, game_time, dt)
end

-- Called on every iteration of the game loop game after *all* entities
-- in the scene have been updated. This means that all objects are in their
-- final places and it's possible to do things such as query scene spatial
-- nodes for finding interesting objects in any particular location.
function PostUpdate(demo, game_time)
end

-- Called on collision events with other objects based on the information
-- from the physics engine. You can only get these events  when your entity
-- node(s) have rigid bodies and are colliding with other rigid bodies. The
-- contact can exist over multiple time steps depending on the type of bodies etc.
-- Node is this entity's entity node with rigid body that collided with the
-- other entity's other_node's rigid body.
function OnBeginContact(demo, node, other_entity, other_node)
end

-- Similar to OnBeginContact except this happens when the contact ends.
function OnEndContact(demo, node, other_entity, other_node)
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
function OnKeyDown(demo, symbol, modifier_bits)
end

-- Called on key up events. See OnKeyDown for more details.
function OnKeyUp(demo, symbol, modifier_bits)
end

-- Called on mouse button press events. This is only called when the entity
-- has enabled the mouse input processing to take place. You can find this
-- setting under 'Script callbacks' in the entity editor.
-- Mouse argument is of type game.MouseEvent and provides an aggregate of
-- information about the event. You can find more details about this type in
-- the Lua API doc.
function OnMousePress(demo, mouse)
end

-- Called on mouse button release events. See OnMousePress for more details.
function OnMouseRelease(demo, mouse)
end

-- Called on mouse move events. See OnMousePress for more details.
function OnMouseMove(demo, mouse)
end

-- Called on game events. Game events are broad-casted to all entities in
-- the scene.  GameEvents are useful when there's an unknown number of
-- entities possibly interested in some game event. Use Game:PostEvent to
-- post a new game event. Each entity will then receive the same event object
-- in this callback and can proceed to process the information.
function OnGameEvent(demo, event)
end

-- Called on animation finished events, i.e. when this entity has finished
-- playing the animation in question.
function OnAnimationFinished(demo, animation)
end

-- Called on timer events. Timers are set on an Entity by calling SetTimer.
-- When the timer expires this callback is then invoked. Timer is then the
-- name of the timer (same as in SetTimer) that fired and jitter defines
-- the difference to ideal time when the timer should have fired. In general
-- entity timers are limited in their resolution to game update resolution.
-- In other words if the game updates at 60 Hz the timer frequency is then
-- 1/60 seconds. If jitter is positive it means the timer is firing early
-- and a negative value indicates the timer fired late.
function OnTimer(demo, timer, jitter)
end

-- Called on posted entity events. Events can be posted on particular entities
-- by calling entity:PostEvent. Unlike game.GameEvents game.EntityEvent are
-- entity specific and only ever delivered to a single entity (the receiver).
function OnEvent(demo, event)
end
