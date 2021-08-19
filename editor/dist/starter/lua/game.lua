States = {
    Menu = 1,
    Play = 2
}
State = States.Menu

-- Called when the game is first loaded.
function LoadGame()
    Game:OpenUI('MainMenu')
    Game:SetViewport(base.FRect:new(-500.0, -800.0, 1000, 800))
    Audio:PlayMusicGraph('MenuMusic')
    Audio:SetMusicEffect('MenuMusic', 'FadeIn', 2000)
end

-- Called as a response to Game:Play when an instance
-- of the scene has been created and the game play begins.
function BeginPlay(scene)
    Game:DebugPrint('Press Space to fly the bird!')
    Audio:SetMusicEffect('MenuMusic', 'FadeOut', 2000)
    Audio:KillMusic('MenuMusic', 2000)
end


-- Called as a response to Game:Stop when the game play ends.
function EndPlay(scene)
    Game:DebugPrint('EndPlay called.')
    Game:OpenUI('MainMenu')
    Audio:CancelMusicCmds('MenuMusic')
    Audio:PlayMusicGraph('MenuMusic')
    Audio:SetMusicEffect('MenuMusic', 'FadeIn', 2000)
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
-- continuously held down don't use this OnKeyDown but rather use
-- wdk.TestKeyDown
-- symbol is the WDK key sym of the currently pressed key.
-- modifier bits are the current modifier keys i.e. Ctrl/Alt/Shift
-- that may or may not be pressed at the time.
function OnKeyDown(symbol, modifier_bits)
    if symbol == wdk.Keys.Escape then
        if State == States.Play then
            Game:Stop()
        elseif State == States.Menu then
            Game:Quit(0)
        end
    end
end

-- Called when the UI has been opened through a call to Game:OpenUI
function OnUIOpen(ui)
    State = States.Menu
    Game:DebugPrint(ui:GetName() .. ' is open')
end

-- Called when the UI is has been closed. Result is the int value
-- given to Game:CloseUI.
function OnUIClose(ui, result)
    State = States.Play
    Game:DebugPrint(ui:GetName() .. ' closed.')
end

-- Called when the UI system translates some user input action
-- such as mouse button press into a UI/widget action such as a
-- button press.
-- Action has the following available fields:
-- 'name'  - the name of the UI widget that generated the action
-- 'id'    - the id of the UI widget that generated the action
-- 'type'  - type string ('ButtonPress' etc) of the action
-- 'value' - value (int, float, bool, string) of the  action if any.
function OnUIAction(action)
    if action.name == 'play' then
        Game:CloseUI(0)
        Game:Play('My Scene')
        Audio:PlaySoundEffect('Click', 0)
    elseif action.name == 'quit' then
        Game:Quit(0)
    end
end
