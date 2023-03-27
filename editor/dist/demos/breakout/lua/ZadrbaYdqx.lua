-- Scene 'MainMenu' script.
-- This script will be called for every instance of 'MainMenu'
-- during gameplay.
-- You're free to delete functions you don't need.
-- Called when the scene begins play.
function BeginPlay(mainmenu)
    local bricks = mainmenu:ListEntitiesByClassName('Yellow Brick')

    while bricks:HasNext() do
        local brick = bricks:GetNext()
        local node = brick:GetNode(0)
        local body = node:GetRigidBody()
        body:Enable(true)
    end
end

-- Called when the scene ends play.
function EndPlay(mainmenu)
end

-- Called when a new entity has been spawned in the scene.
function SpawnEntity(mainmenu, entity)

end

-- Called when an entity has been killed from the scene.
function KillEntity(mainmenu, entity)

end

-- Called on every low frequency game tick.
function Tick(mainmenu, game_time, dt)

end

-- Called on every iteration of game loop.
function Update(mainmenu, game_time, dt)

end

-- Called on collision events with other objects.
function OnBeginContact(mainmenu, entity, entity_node, other, other_node)
end

-- Called on collision events with other objects.
function OnEndContact(mainmenu, entity, entity_node, other, other_node)
end

-- Called on key down events.
function OnKeyDown(mainmenu, symbol, modifier_bits)
end

-- Called on key up events.
function OnKeyUp(mainmenu, symbol, modifier_bits)
end

-- Called on mouse button press events.
function OnMousePress(mainmenu, mouse)
end

-- Called on mouse button release events.
function OnMouseRelease(mainmenu, mouse)
end

-- Called on mouse move events.
function OnMouseMove(mainmenu, mouse)
end

-- Called on game events.
function OnGameEvent(mainmenu, event)
end

-- Called on entity timer events.
function OnEntityTimer(mainmenu, entity, timer, jitter)
end

-- Called on posted entity events.
function OnEntityEvent(mainmenu, entity, event)
end

-- Called on UI open event.
function OnUIOpen(mainmenu, ui)
end

-- Called on UI close event.
function OnUIClose(mainmenu, ui, result)
end

-- Called on UI action event.
function OnUIAction(mainmenu, ui, action)
end

