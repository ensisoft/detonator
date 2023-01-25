local graphics = 1

-- UI 'Jungle Dialogs' script.
-- This script will be called for every instance of 'Jungle Dialogs.'
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
    Game:DebugPrint(action.name)

    if action.name == 'less_graphics' then
        graphics = base.clamp(0, 2, graphics - 1)
    elseif action.name == 'more_graphics' then
        graphics = base.clamp(0, 2, graphics + 1)
    end

    ui.graphics:SetValue(graphics / 2.0)
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
end

-- Optionally called on keyboard events when the flag is set.
function OnKeyUp(ui, symbol, modifier_bits)
end

