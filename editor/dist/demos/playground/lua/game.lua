-- Top level game callbacks.
-- You're free to delete functions that you don't need.

local _num_scenes = 7
local _cur_scene  = 0

local TestTable = {}

-- Called when the game is first loaded.
-- This is the place where you might want to load some 
-- previous/initial game state. 
function LoadGame()
    TestTable[0] = 'Phys Test 0'
    TestTable[1] = 'Phys Test 1'
    TestTable[2] = 'Phys Test 2'
    TestTable[3] = 'Phys Test 3'
    TestTable[4] = 'Phys Test 4'
    TestTable[5] = 'Phys Test 5'
    TestTable[6] = 'Phys Test 6'

    Game:DebugPrint('LoadGame called.')
    Game:SetViewport(-500.0, -400.0, 1000.0, 800.0)
    Game:Play(TestTable[0])
    return true
end

-- Called after the game has been loaded and the game can 
-- be started. This is likely the place where the first
-- game content is shown, such as a main menu.
function StartGame()
    Game:DebugPrint('StartGame called')
end

-- Called before the application exist.
-- This is the last chance to persist any state that should
-- be restored on next game run. 
function SaveGame() 
    Game:DebugPrint('SaveGame called')
end

-- Called when the game play should stop, i.e. the application
-- is about to exit. 
function StopGame()
    Game:DebugPrint('StopGame called')
end

-- Called as a response to Game:Play when an instance
-- of the scene has been created and the game play begins.
function BeginPlay(scene)
    Game:DebugPrint('BeginPlay called.')
end


-- Called as a response to Game:EndPlay when the game play ends.
function EndPlay(scene)
    Game:DebugPrint('EndPlay called.')
end

-- Low frequency game tick. Normally called at a lower frequency
-- than the Update function. The tick frequency is configurable in
-- the workspace settings.
-- game_time is the total current accumulated game time including dt.
-- dt is the time step to take.
-- Note that this is *not* called when then the game has been suspended.
function Tick(game_time, dt)

end

-- High frequency game update function. The update frequency is
-- configurable in the workspace settings.
-- game_time is the total current accumulated game time including dt.
-- dt is the time step to take.
-- Note that this is *not* called when the game has been suspended.
function Update(game_time, dt)

end

-- Event/input callback handlers.

-- Called when a keyboard key has been pressed.
-- If you want to perform some game action while a key is
-- continuously held down don't use OnKeyDown but rather use
-- wdk.TestKeyDown.
-- symbol is the WDK key sym of the currently pressed key.
-- modifier_bits are the current modifier keys i.e. Ctrl/Alt/Shift
-- that may or may not be pressed at the time. You can use
-- wdk.TestMod to check whether any particular modifier bit is set.
-- Example:
-- if wdk.TestMod(modifier_bits, wdk.Mods.Control) then
--    ...
-- end
function OnKeyDown(symbol, modifier_bits)
    if symbol == wdk.Keys.ArrowRight then 
        _cur_scene = _cur_scene + 1
        if _cur_scene >= _num_scenes then 
            _cur_scene = 0
        end
    elseif symbol == wdk.Keys.ArrowLeft then 
        _cur_scene = _cur_scene - 1
        if _cur_scene < 0 then 
            _cur_scene = _num_scenes -1
        end
    else
        return
    end


    local name  = TestTable[_cur_scene]
    local scene = ClassLib:FindSceneClassByName(name)
    local gravity = glm.vec2:new(0.0, 0.0)
    local variable = scene:FindScriptVarByName('gravity')
    if variable ~= nil then         
        gravity = variable:GetValue()
    end
    Physics:SetGravity(gravity)

    -- start the play of scene
    Game:Play(scene)
end

function OnKeyUp(symbol, modifier_bits)

end

-- Called on mouse button press events.
-- mouse event has the following available fields
-- 'window_coord' - mouse cursor position in window coordinates.
-- 'scene_coord'  - mouse cursor position in scene coordinates
-- 'button'       - mouse button that was pressed (if any)
-- 'modifiers'    - modifier bits of keyboard mods that were pressed (if any).
-- 'over_scene'   - true to indicate that the mouse cursor is over the scene viewport in window.
function OnMousePress(mouse)

end

-- Called on mouse button release events.
function OnMouseRelease(mouse)

end

-- Called on mouse move events.
function OnMouseMove(mouse)
end

-- Called when the UI has been opened through a call to Game:OpenUI
function OnUIOpen(ui)

end

-- Called when the UI is has been closed. Result is the int value
-- given to Game:CloseUI.
function OnUIClose(ui, result)

end

-- Called when the UI system translates some user input action
-- such as mouse button press into a UI/widget action such as a
-- button press.
-- Action has the following available fields:
-- 'name'  - the name of the UI widget that generated the action
-- 'id'    - the id of the UI widget that generated the action
-- 'type'  - type string ('ButtonPress' etc) of the action
-- 'value' - value (int, float, bool, string) of the  action if any.
function OnUIAction(ui, action)

end

-- Called when an audio event happens.
-- An audio event has the following fields:
-- 'type'  - the type of the event. 'MusicTrackDone' or 'SoundEffectDone'
-- 'track' - the name of the audio track that generated the event.
function OnAudioEvent(event)

end

-- Called when a game event has been posted.
-- A game event has the following fields:
-- 'from' a text string identifying the sender.
-- 'to' a text string identifier the receiver
-- 'message' a text message identifying the event or carrying event information
-- 'value' a value of bool, float, int, string, vec2/3/4, FRect, FSize, FPoint or Color4f
function OnGameEvent(event)

end
