-- Scene 'Phys Test 5' script.

-- This script will be called for every instance of 'Phys Test 5'
-- during gameplay.
-- You're free to delete functions you don't need.

local _mouse_down = nil
local _mouse_pos  = nil

local _raycast_mode = 'All'


-- Called when the scene begins play.
function BeginPlay(phys_test_5)

end

-- Called when the scene ends play.
function EndPlay(phys_test_5)
end

-- Called when a new entity has been spawned in the scene.
function SpawnEntity(phys_test_5, entity)

end

-- Called when an entity has been killed from the scene.
function KillEntity(phys_test_5, entity)

end

-- Called on every low frequency game tick.
function Tick(phys_test_5, game_time, dt)

end

-- Called on every iteration of game loop.
function Update(phys_test_5, game_time, dt)
    if _mouse_down == nil or _mouse_pos == nil then 
        return
    end
    local pink = base.Color4f.FromEnum('HotPink')
    Game:DebugDrawLine(_mouse_down, _mouse_pos, pink, 2.0)
end

-- Called on collision events with other objects.
function OnBeginContact(phys_test_5, entity, entity_node, other, other_node)
end

-- Called on collision events with other objects.
function OnEndContact(phys_test_5, entity, entity_node, other, other_node)
end

-- Called on key down events.
function OnKeyDown(phys_test_5, symbol, modifier_bits)
    if symbol == wdk.Keys.Key1 then 
        _raycast_mode = 'All'
    elseif symbol == wdk.Keys.Key2 then 
        _raycast_mode = 'Closest'
    elseif symbol == wdk.Keys.Key3 then 
        _raycast_mode = 'First'
    end    
    Game:DebugPrint('RayCast mode: ' .. _raycast_mode)
end

-- Called on key up events.
function OnKeyUp(phys_test_5, symbol, modifier_bits)
end

-- Called on mouse button press events.
function OnMousePress(phys_test_5, mouse)
    if mouse.over_scene then 
        _mouse_down = mouse.scene_coord
    else 
        _mouse_down = nil
    end
end

-- Called on mouse button release events.
function OnMouseRelease(phys_test_5, mouse)
    
    local found = Physics:RayCast(Physics:MapVectorFromGame(_mouse_down),
                                  Physics:MapVectorFromGame(_mouse_pos), _raycast_mode)
    Game:DebugPrint('Raycast found ' .. tostring(found:Size()) .. ' objects')
    
    for i=0,found:Size()-1, 1
    do
        local item = found:GetAt(i)
        local node = item.node
        local world_point = Physics:MapVectorToGame(item.point)
        local args = game.EntityArgs:new()
        args.class = ClassLib:FindEntityClassByName('Marker')
        args.position = world_point
        Scene:SpawnEntity(args, true)     
    end

    _mouse_pos = nil
    _mouse_down = nil
end

-- Called on mouse move events.
function OnMouseMove(phys_test_5, mouse)
    if mouse.over_scene then 
        _mouse_pos = mouse.scene_coord
    else
        _mouse_pos = nil
    end
end

-- Called on game events.
function OnGameEvent(phys_test_5, event)
end

