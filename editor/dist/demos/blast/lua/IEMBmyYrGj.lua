-- Scene 'menu' script.
-- This script will be called for every instance of 'menu' during gameplay.
-- You're free to delete functions you don't need.
require('common')

-- Called when the scene begins play.
-- Map may be nil if the scene has no map set.
function BeginPlay(menu, map)
end

-- Called when the scene ends play.
-- Map may be nil if the scene has no map set.
function EndPlay(menu, map)
end

-- Called when a new entity has been spawned in the scene.
-- This function will be called before entity BeginPlay.
function SpawnEntity(menu, map, entity)
end

-- Called when an entity has been killed from the scene.
-- This function will be called before entity EndPlay.
function KillEntity(menu, map, entity)
end

-- Called on every low frequency game tick.
function Tick(menu, game_time, dt)

    SpawnSpaceRock()
end

-- Called on every iteration of game loop.
function Update(menu, game_time, dt)
end

-- Physics collision callback on contact begin.
-- This is called when an entity node begins contact with another
-- entity node in the scene.
function OnBeginContact(menu, entity, entity_node, other, other_node)
end

-- Physics collision callback on end contact.
-- This is called when an entity node ends contact with another
-- entity node in the scene.
function OnEndContact(menu, entity, entity_node, other, other_node)
end

-- Called on keyboard key down events.
function OnKeyDown(menu, symbol, modifier_bits)
end

-- Called on keyboard key up events.
function OnKeyUp(menu, symbol, modifier_bits)
end

-- Called on mouse button press events.
function OnMousePress(menu, mouse)
end

-- Called on mouse button release events.
function OnMouseRelease(menu, mouse)
end

-- Called on mouse move events.
function OnMouseMove(menu, mouse)
end

-- Called on game events.
function OnGameEvent(menu, event)
end

-- Called on entity timer events.
function OnEntityTimer(menu, entity, timer, jitter)
end

-- Called on posted entity events.
function OnEntityEvent(menu, entity, event)
end

-- Called on UI open event.
function OnUIOpen(menu, ui)
end

-- Called on UI close event.
function OnUIClose(menu, ui, result)
end

-- Called on UI action event.
function OnUIAction(menu, ui, action)
end
