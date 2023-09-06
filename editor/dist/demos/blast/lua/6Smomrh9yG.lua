-- UI 'MainMenu' script.
-- This script will be called for every instance of 'MainMenu.'
-- You're free to delete functions you don't need.
-- Called whenever the UI has been opened.
local ui_open_time = 0

function StartGame()
    Game:CloseUI(0)
    Game:ShowMouse(false)
    Game:Play('Game')

    if State.play_music then
        Audio:SetMusicEffect('Menu Music', 'FadeOut', 2000)
        Audio:KillMusic('Menu Music', 2000)
    end
end

function OnUIOpen(ui)
    Game:ShowMouse(true)

    if State.play_music then
        Audio:PlayMusic('Menu Music')
        Audio:SetMusicEffect('Menu Music', 'FadeIn', 2000)
    end

    -- use a time value to reject UI input that arrives too soon.
    -- the idea is that this helps to keep the main UI in a consistent state
    -- while the $OnOpen animations are running!
    ui_open_time = util.GetMilliseconds()

    -- If we have a highscore value then show that the in the MainMenu
    if State:HasValue('high_score') then
        ui.hiscore:SetText(string.format('High score %d', State.high_score))
    end
end

-- Called whenever the UI is about to be closed.
-- result is the value passed in the exit_code in the call to CloseUI.
function OnUIClose(ui, result)
end

-- Called whenever some UI action such as button click etc. occurs
function OnUIAction(ui, action)
    local time_now = util.GetMilliseconds()
    if time_now - ui_open_time < 500 then
        return
    end

    if action.name == 'exit' then
        Game:CloseUI(0)
        Game:Quit(0)
    elseif action.name == 'credits' then
        Game:OpenUI('Credits')
        Audio:PlaySoundEffect('Menu Click')
    elseif action.name == 'options' then
        Game:OpenUI('Options')
        Audio:PlaySoundEffect('Menu Click')
    elseif action.name == 'play' then

        StartGame()

        Audio:PlaySoundEffect('Menu Click')
    end
end

function OnUINotify(ui, event)
    Game:DebugPrint(event.type)
end

-- Optionally called on mouse events when the flag is set.
function OnMouseMove(ui, mouse)
end

-- Optionally called on mouse events when the flag is set.
function OnMousePress(ui, mouse)
end

-- Optionally called on mouse events when the flag is set.
function OnMouseRelease(ui, mouse)
end

-- Optionally called on keyboard events when the flag is set.
function OnKeyDown(ui, symbol, modifier_bits)
    if symbol == wdk.Keys.Escape then
        Game:CloseUI(0)
        Game:Quit(0)
    elseif symbol == wdk.Keys.Space then

        StartGame()

        Audio:PlaySoundEffect('Menu Click')
    end
end

-- Optionally called on keyboard events when the flag is set.
function OnKeyUp(ui, symbol, modifier_bits)
end

