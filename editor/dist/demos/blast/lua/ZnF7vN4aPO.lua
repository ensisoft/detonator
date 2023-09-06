-- UI 'Credits' script.
-- This script will be called for every instance of 'Credits.'
-- You're free to delete functions you don't need.
-- Called whenever the UI has been opened.
function OnUIOpen(ui)
end

-- Called whenever the UI is about to be closed.
-- result is the value passed in the exit_code in the call to CloseUI.
function OnUIClose(ui, result)
end

-- Called whenever some UI action such as button click etc. occurs
function OnUIAction(ui, action)
    if action.name == 'close' then
        Game:CloseUI(ui, 0)
    end
    Audio:PlaySoundEffect('Menu Click')
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
        Game:CloseUI(ui, 0)
    end
end

-- Optionally called on keyboard events when the flag is set.
function OnKeyUp(ui, symbol, modifier_bits)
end

