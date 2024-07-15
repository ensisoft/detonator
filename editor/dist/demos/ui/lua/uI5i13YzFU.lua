-- UI Window 'wyrmheart' script.
-- This script will be called for every instance of 'wyrmheart'.
-- You're free to delete functions you don't need.
-- Called to update the UI at game update frequency.
function Update(ui, game_time, dt)
end

--
-- Called whenever the UI has been opened. This is a good place for
-- setting some initial widget state.
function OnUIOpen(ui)
    Audio:PlayMusic('Wyrmheart Title')
end

-- Called whenever the UI is about to be closed.
-- result is the value passed in the exit_code in the call to CloseUI.
function OnUIClose(ui, result)
    Audio:KillAllMusic(0)
end

-- Called whenever some UI action such as button click etc. occurs.
-- This is most likely the place where you want to handle your user input.
function OnUIAction(ui, action)
end

-- Called on notification type of UI events for example when some UI
-- element has gained keyboard focus, lost keyboard focus etc.
-- These notifications are superficial and you're fee to ignore them
-- completely. You might want to react to these might be to trigger
-- some animation or change some widget style property etc.
function OnUINotify(ui, event)
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
