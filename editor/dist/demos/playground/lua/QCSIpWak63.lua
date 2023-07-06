-- Entity 'Static Block' script.
-- This script will be called for every instance of 'Static Block'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.
-- Called when the game play begins for an entity in the scene.
function BeginPlay(static_block, scene, map)

end

-- Called when the game play ends for an entity in the scene.
function EndPlay(static_block, scene, map)

end

-- Called on every low frequency game tick.
function Tick(static_block, game_time, dt)

end

-- Called on every iteration of the game loop.
function Update(static_block, game_time, dt)

end

-- Called on every iteration of the game loop game
-- after *all* entities have been updated.
function PostUpdate(static_block, game_time)
end

-- Called on collision events with other objects.
function OnBeginContact(static_block, node, other, other_node)
end

-- Called on collision events with other objects.
function OnEndContact(static_block, node, other, other_node)
end

-- Called on key down events.
function OnKeyDown(static_block, symbol, modifier_bits)
end

-- Called on key up events.
function OnKeyUp(static_block, symbol, modifier_bits)
end

-- Called on mouse button press events.
function OnMousePress(static_block, mouse)
    if not mouse.over_scene then
        return
    end

    local node = static_block:GetNode(0)
    local rect = Scene:FindEntityNodeBoundingRect(static_block, node)

    if not rect:TestPoint(util.ToPoint(mouse.scene_coord)) then
        return
    end
    static_block.dragging = true
    Game:DebugPrint('Dragging ' .. static_block:GetName())
end

-- Called on mouse button release events.
function OnMouseRelease(static_block, mouse)
    static_block.dragging = false
end

-- Called on mouse move events.
function OnMouseMove(static_block, mouse)
    if not mouse.over_scene then
        return
    end
    if not static_block.dragging then
        return
    end
    local node = static_block:GetNode(0)
    node:SetTranslation(mouse.scene_coord)
end

