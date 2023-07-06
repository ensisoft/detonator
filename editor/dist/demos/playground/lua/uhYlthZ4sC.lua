-- Scene 'Spatial Query Test' script.
-- This script will be called for every instance of 'Spatial Query Test'
-- during gameplay.
-- You're free to delete functions you don't need.
-- 
-- LuaFormatter off

local mouse_down = nil
local mouse_pos = nil

-- The method by which the objects are queried for.
local SelectionMethod = {
    -- Find objects whose bounding boxes intersect with the
    -- search rectangle. 
    Rectangle = 'Rectangle',
    -- Find objects whose bounding boxes intersect (contain)
    -- the search point. 
    Point = 'Point',
    -- Find objects whose bounding boxes intersect (contain)
    -- any point within the given radius from the search point.
    -- In other words, this is an intersection test against a rect
    -- and a circle, where the circle is defined by point+radius.
    PointRadius = 'PointRadius',
    -- Find objects whose bounding boxes intersect with the with 
    -- the line defined by two points.
    PointPoint = 'PointPoint'
}

-- The selection mode only applies to point and point+radius
-- object queries.
local SelectionMode = {
    -- Find all objects that match the query criteria.
    All = 'All',
    -- Find the first object that matches the query criteria.
    -- The order in which the objects are evaluated is arbitrary,
    -- thus the result is whatever object happened to be match first.
    First = 'First',
    -- Find the closest object among the objects that matched the
    -- query criteria. The distance is measured to the center of the
    -- object's bounding box. For point-point queries the distnce is
    -- is measured between the starting point (point A) and the 
    -- center of the bounding box.
    Closest = 'Closest'
}

-- LuaFormatter on

local selection_mode = SelectionMode.All
local selection_method = SelectionMethod.Rectangle

-- Called when the scene begins play.
function BeginPlay(spatial_query_test, map)
end

-- Called when the scene ends play.
function EndPlay(spatial_query_test, map)
end

-- Called when a new entity has been spawned in the scene.
function SpawnEntity(spatial_query_test, map, entity)

end

-- Called when an entity has been killed from the scene.
function KillEntity(spatial_query_test, map, entity)

end

-- Called on every low frequency game tick.
function Tick(spatial_query_test, game_time, dt)

end

-- Called on every iteration of game loop.
function Update(spatial_query_test, game_time, dt)
    local pink = base.Color4f.FromEnum('HotPink')

    if selection_method == SelectionMethod.Point then
        if mouse_pos == nil then
            return
        end
        Game:DebugDrawCircle(mouse_pos, 4.0, pink, 2.0)
    end

    if mouse_down == nil or mouse_pos == nil then
        return
    end

    if selection_method == SelectionMethod.Rectangle then
        Game:DebugDrawRect(mouse_down, mouse_pos, pink, 2.0)
    elseif selection_method == SelectionMethod.PointRadius then
        local radius = glm.length(mouse_down - mouse_pos)

        Game:DebugDrawCircle(mouse_down, radius, pink, 2.0)
        Game:DebugDrawLine(mouse_down, mouse_pos, pink, 2.0)
    elseif selection_method == SelectionMethod.PointPoint then
        Game:DebugDrawLine(mouse_down, mouse_pos, pink, 2.0)
    end
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
    if symbol == wdk.Keys.Key1 then
        selection_method = SelectionMethod.Rectangle
    elseif symbol == wdk.Keys.Key2 then
        selection_method = SelectionMethod.Point
    elseif symbol == wdk.Keys.Key3 then
        selection_method = SelectionMethod.PointRadius
    elseif symbol == wdk.Keys.Key4 then
        selection_method = SelectionMethod.PointPoint
    elseif symbol == wdk.Keys.KeyA then
        selection_mode = SelectionMode.All
    elseif symbol == wdk.Keys.KeyF then
        selection_mode = SelectionMode.First
    elseif symbol == wdk.Keys.KeyC then
        selection_mode = SelectionMode.Closest
    end
    Game:DebugPrint('Selection method ' .. selection_method)
    Game:DebugPrint('Selection mode ' .. selection_mode)

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

    local nodes = nil
    if selection_method == SelectionMethod.Rectangle then
        nodes = Scene:QuerySpatialNodes(base.FRect:new(min_x, min_y, width,
                                                       height))
    elseif selection_method == SelectionMethod.Point then
        nodes = Scene:QuerySpatialNodes(mouse_pos, selection_mode)
    elseif selection_method == SelectionMethod.PointRadius then
        local radius = glm.length(mouse_pos - mouse_down)
        nodes = Scene:QuerySpatialNodes(mouse_down, radius, selection_mode)
    elseif selection_method == SelectionMethod.PointPoint then
        nodes = Scene:QuerySpatialNodes(mouse_down, mouse_pos, selection_mode)
    end

    while (nodes:HasNext()) do
        local node = nodes:GetNext()
        local pos = node:GetTranslation()

        Scene:SpawnEntity('Marker', {
            pos = pos,
            layer = 2,
            link = true
        })
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

