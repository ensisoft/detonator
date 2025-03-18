-- Scene 'level' script.
-- This script will be called for every instance of 'level' during gameplay.
-- You're free to delete functions you don't need.
-- Called when the scene begins play.
-- Map may be nil if the scene has no map set.
local level_run_time = 0.0

function BeginPlay(level, map)
    level_run_time = 0.0
end

-- Called when the scene ends play.
-- Map may be nil if the scene has no map set.
function EndPlay(level, map)
end

-- Called when a new entity has been spawned in the scene.
-- This function will be called before entity BeginPlay.
function SpawnEntity(level, map, entity)
end

-- Called when an entity has been killed from the scene.
-- This function will be called before entity EndPlay.
function KillEntity(level, map, entity)
end

-- Called on every low frequency game tick.
function Tick(level, game_time, dt)
end

-- Called on every iteration of game loop.
function Update(level, game_time, dt)

    level_run_time = level_run_time + dt

    local dino = level:FindEntity('Player')

    local hud = Game:GetTopUI()
    if hud == nil then
        return
    end

    if hud:GetName() ~= 'Game UI' then
        return
    end
    hud.time:SetText(string.format("%.2f", level_run_time))
    hud.health:SetValue(dino.health)
    hud.score:SetText(tostring(dino.score))
end

-- Physics collision callback on contact begin.
-- This is called when an entity node begins contact with another
-- entity node in the scene.
function OnBeginContact(level, entity, entity_node, other, other_node)
end

-- Physics collision callback on end contact.
-- This is called when an entity node ends contact with another
-- entity node in the scene.
function OnEndContact(level, entity, entity_node, other, other_node)
end

-- Called on keyboard key down events.
function OnKeyDown(level, symbol, modifier_bits)
end

-- Called on keyboard key up events.
function OnKeyUp(level, symbol, modifier_bits)
end

-- Called on mouse button press events.
function OnMousePress(level, mouse)
end

-- Called on mouse button release events.
function OnMouseRelease(level, mouse)
end

-- Called on mouse move events.
function OnMouseMove(level, mouse)
end

-- Called on game events.
function OnGameEvent(level, event)
end

-- Called on entity timer events.
function OnEntityTimer(level, entity, timer, jitter)
end

-- Called on posted entity events.
function OnEntityEvent(level, entity, event)
end

-- Called on UI open event.
function OnUIOpen(level, ui)
end

-- Called on UI close event.
function OnUIClose(level, ui, result)
end

-- Called on UI action event.
function OnUIAction(level, ui, action)
end
