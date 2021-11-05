-- Top level game callbacks.
-- You're free to delete functions that you don't need.

-- Called when the game is started.
function StartGame()
    Game:SetViewport(base.FRect:new(-500.0, -400.0, 1000.0, 800.0))
    Game:Play('MainMenu')
    Game:OpenUI('MainMenu')
end

-- Called as a response to Game:Play when an instance
-- of the scene has been created and the game play begins.
function BeginPlay(scene)
    Game:DebugPrint('BeginPlay called.')
end


-- Called as a response to Game:Stop when the game play ends.
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
    if symbol == wdk.Keys.Escape then 
--        Game:Play('Level 0')
        Game:Play('TestLevel')
    end
end

function OnKeyUp(symbol, modifier_bits)
    Game:DebugPrint('KeyUp ' .. wdk.KeyStr(symbol))
end


-- Called on mouse button press events.
function OnMousePress(mouse)
    Game:DebugPrint('MousePress ' .. wdk.BtnStr(mouse.button))
end

-- Called on mouse button release events.
function OnMouseRelease(mouse)
    Game:DebugPrint('MouseRelease ' .. wdk.BtnStr(mouse.button))
end

-- Called on mouse move events.
function OnMouseMove(mouse)

end

-- Called when the UI has been opened through a call to Game:OpenUI
function OnUIOpen(ui)

end

-- Called when the UI is has been closed. Result is the int value
-- given to Game:CloseUI.vvv
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
    if action.name == 'exit' then 
        Game:Quit(0)
    elseif action.name == 'play' then 
        Game:CloseUI(0)
        Game:Play('Level 0')
    end    
end

-- Called when an audio event happens.
-- An audio event has the following fields:
-- 'type'  - the type of the event. 'MusicTrackDone' or 'SoundEffectDone'
-- 'track' - the name of the audio track that generated the event.
function OnAudioEvent(event)

end

