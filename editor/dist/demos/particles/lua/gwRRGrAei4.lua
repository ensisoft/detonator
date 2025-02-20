--
-- Entity 'entity' script.
--
-- This script will be called for every instance of 'entity' in the scene
-- during gameplay.
-- You're free to delete functions you don't need.
--
-- Called once when the game play begins for the entity in the scene.
function BeginPlay(entity, scene, map)
end

-- Called once when the game play ends for the entity in the scene.
function EndPlay(entity, scene, map)
end

-- Called on every low frequency game tick. The tick frequency is
-- determined in the project settings. If you want to perform animation
-- such as move your game objects more smoothly then Update is the place
-- to do it. This function can be used to do thing such as evaluate AI or
-- path finding etc.
function Tick(entity, game_time, dt)
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
function Update(entity, game_time, dt)
    local node = entity:GetNode(0)
    local x = math.cos(game_time * 0.5) * 50.0
    local y = math.sin(game_time) * 100.0
    node:SetTranslation(x, y)
end

-- Called on every iteration of the game loop game after *all* entities
-- in the scene have been updated. This means that all objects are in their
-- final places and it's possible to do things such as query scene spatial
-- nodes for finding interesting objects in any particular location.
function PostUpdate(entity, game_time)
end

-- Called on collision events with other objects based on the information
-- from the physics engine. You can only get these events  when your entity
-- node(s) have rigid bodies and are colliding with other rigid bodies. The
-- contact can exist over multiple time steps depending on the type of bodies etc.
-- Node is this entity's entity node with rigid body that collided with the
-- other entity's other_node's rigid body.
function OnBeginContact(entity, node, other_entity, other_node)
end

-- Similar to OnBeginContact except this happens when the contact ends.
function OnEndContact(entity, node, other_entity, other_node)
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
function OnKeyDown(entity, symbol, modifier_bits)
end

-- Called on key up events. See OnKeyDown for more details.
function OnKeyUp(entity, symbol, modifier_bits)
end

-- Called on mouse button press events. This is only called when the entity
-- has enabled the mouse input processing to take place. You can find this
-- setting under 'Script callbacks' in the entity editor.
-- Mouse argument is of type game.MouseEvent and provides an aggregate of
-- information about the event. You can find more details about this type in
-- the Lua API doc.
function OnMousePress(entity, mouse)
end

-- Called on mouse button release events. See OnMousePress for more details.
function OnMouseRelease(entity, mouse)
end

-- Called on mouse move events. See OnMousePress for more details.
function OnMouseMove(entity, mouse)
end

-- Called on game events. Game events are broad-casted to all entities in
-- the scene.  GameEvents are useful when there's an unknown number of
-- entities possibly interested in some game event. Use Game:PostEvent to
-- post a new game event. Each entity will then receive the same event object
-- in this callback and can proceed to process the information.
function OnGameEvent(entity, event)
end

-- Called on animation finished events, i.e. when this entity has finished
-- playing the animation in question.
function OnAnimationFinished(entity, animation)
end

-- Called on timer events. Timers are set on an Entity by calling SetTimer.
-- When the timer expires this callback is then invoked. Timer is then the
-- name of the timer (same as in SetTimer) that fired and jitter defines
-- the difference to ideal time when the timer should have fired. In general
-- entity timers are limited in their resolution to game update resolution.
-- In other words if the game updates at 60 Hz the timer frequency is then
-- 1/60 seconds. If jitter is positive it means the timer is firing early
-- and a negative value indicates the timer fired late.
function OnTimer(entity, timer, jitter)
end

-- Called on posted entity events. Events can be posted on particular entities
-- by calling entity:PostEvent. Unlike game.GameEvents game.EntityEvent are
-- entity specific and only ever delivered to a single entity (the receiver).
function OnEvent(entity, event)
end
