-- Scene 'Spatial Query Test' script.
-- This script will be called for every instance of 'Spatial Query Test'
-- during gameplay.
-- You're free to delete functions you don't need.
local mouse_down = nil
local mouse_pos = nil

-- Called when the scene begins play.
function BeginPlay(spatial_query_test)
end

-- Called when the scene ends play.
function EndPlay(spatial_query_test)
end

-- Called when a new entity has been spawned in the scene.
function SpawnEntity(spatial_query_test, entity)

end

-- Called when an entity has been killed from the scene.
function KillEntity(spatial_query_test, entity)

end

-- Called on every low frequency game tick.
function Tick(spatial_query_test, game_time, dt)

end

-- Called on every iteration of game loop.
function Update(spatial_query_test, game_time, dt)
    if mouse_down == nil or mouse_pos == nil then
        return
    end

    local pink = base.Color4f.FromEnum('HotPink')
    Game:DebugDrawRect(mouse_down, mouse_pos, pink, 2.0)
end

-- Called on collision events with other objects.
function OnBeginContact(spatial_query_test, entity, entity_node, other,
                        other_node)
end

-- Called on collision events with other objects.
function OnEndContact(spatial_query_test, entity, entity_node, other, other_node)
end

-- Called on key down events.
function OnKeyDown(spatial_query_test, symbol, modifier_bits)
end

-- Called on key up events.
function OnKeyUp(spatial_query_test, symbol, modifier_bits)
end

-- Called on mouse button press events.
function OnMousePress(spatial_query_test, mouse)
    if mouse.over_scene then
        mouse_down = mouse.scene_coord
    else
        mouse_down = nil
    end
end

-- Called on mouse button release events.
function OnMouseRelease(spatial_query_test, mouse)

    local min_x = math.min(mouse_pos.x, mouse_down.x)
    local min_y = math.min(mouse_pos.y, mouse_down.y)
    local max_x = math.max(mouse_pos.x, mouse_down.x)
    local max_y = math.max(mouse_pos.y, mouse_down.y)
    local width = max_x - min_x
    local height = max_y - min_y

    local nodes = Scene:QuerySpatialNodes(
                      base.FRect:new(min_x, min_y, width, height))

    while (nodes:HasNext()) do
        local node = nodes:GetNext()
        local args = game.EntityArgs:new()
        args.class = ClassLib:FindEntityClassByName('Marker')
        args.position = node:GetTranslation()
        args.layer = 2
        Scene:SpawnEntity(args, true)
    end

    mouse_pos = nil
    mouse_down = nil
end

-- Called on mouse move events.
function OnMouseMove(spatial_query_test, mouse)
    if mouse.over_scene then
        mouse_pos = mouse.scene_coord
    else
        mouse_pos = nil
    end
end

-- Called on game events.
function OnGameEvent(spatial_query_test, event)
end

-- Called on entity timer events.
function OnEntityTimer(spatial_query_test, entity, timer, jitter)
end

-- Called on posted entity events.
function OnEntityEvent(spatial_query_test, entity, event)
end

-- Called on UI open event.
function OnUIOpen(spatial_query_test, ui)
end

-- Called on UI close event.
function OnUIClose(spatial_query_test, ui, result)
end

-- Called on UI action event.
function OnUIAction(spatial_query_test, ui, action)
end

