-- Entity script.
-- This script will be called for every assigned entity instance
-- You're free to delete functions you don't need.
-- Called when the game play begins for an entity in the scene.
function BeginPlay(entity, scene, map)
end

-- Called when the game play ends for an entity in the scene.
function EndPlay(entity, scene, map)
end

-- Called on every low frequency game tick.
function Tick(entity, game_time, dt)
end

-- Called on every iteration of the game loop.
function Update(entity, game_time, dt)
end

-- Called on every iteration of the game loop game
-- after *all* entities have been updated.
function PostUpdate(entity, game_time)
end

-- Called on collision events with other objects.
function OnBeginContact(entity, node, other, other_node)
end

-- Called on collision events with other objects.
function OnEndContact(entity, node, other, other_node)
end

-- Called on key down events.
function OnKeyDown(entity, symbol, modifier_bits)
end

-- Called on key up events.
function OnKeyUp(entity, symbol, modifier_bits)
end

-- Called on mouse button press events.
function OnMousePress(entity, mouse)
    if mouse.over_scene == false then
        return
    end
    local scene = entity:GetScene()
    local body = entity:FindNodeByClassName('Body')
    local rect = scene:FindEntityNodeBoundingRect(entity, body)
    if rect:TestPoint(util.ToPoint(mouse.scene_coord)) then
        entity.dragging = true
    end
end

-- Called on mouse button release events.
function OnMouseRelease(entity, mouse)
    entity.dragging = false
end

-- Called on mouse move events.
function OnMouseMove(entity, mouse)
    if entity.dragging == false then
        return
    end
    local body = entity:FindNodeByClassName('Body')
    body:SetTranslation(mouse.scene_coord)
end

-- Called on game events.
function OnGameEvent(entity, event)
end

-- Called on animation finished events.
function OnAnimationFinished(entity, animation)
end

-- Called on timer events.
function OnTimer(entity, timer, jitter)
end

-- Called on posted entity events.
function OnEvent(entity, event)
end

