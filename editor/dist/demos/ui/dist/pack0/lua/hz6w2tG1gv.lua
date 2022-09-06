-- UI script.

-- This script will be called for every assigned UI instance.
-- You're free to delete functions you don't need.

-- Called whenever the UI has been opened.
function OnUIOpen(ui)
    Game:DebugPrint('UI Open')
end

 -- Called whenever the UI is about to be closed.
-- result is the value passed in the exit_code in the call to CloseUI.
function OnUIClose(ui, result)
    Game:DebugPrint('UI Close')
end

-- Called whenever some UI action such as button click etc. occurs
function OnUIAction(ui, action)
    Game:DebugPrint(tostring(action.type))

    if action.name == 'Slider_1' then 
        local prog = ui:FindWidgetByName('ProgressBar_1', 'ProgressBar')
        prog:SetValue(action.value)
    else
        Audio:PlaySoundEffect('click')
    end
end

