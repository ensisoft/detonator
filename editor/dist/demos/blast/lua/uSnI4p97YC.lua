-- UI 'GameOver' script.
-- This script will be called for every instance of 'GameOver.'
-- You're free to delete functions you don't need.
function UpdateScore(ui, score, high_score)
    ui.score:SetText(tostring(score) .. ' points')

    local new_highscore = score > high_score

    ui.highscore:SetVisible(new_highscore)

    if new_highscore then
        Audio:PlaySoundEffect('Victory', 1000)
    end
end

-- Called whenever the UI has been opened.
function OnUIOpen(ui)
end

-- Called whenever the UI is about to be closed.
-- result is the value passed in the exit_code in the call to CloseUI.
function OnUIClose(ui, result)
end

-- Called whenever some UI action such as button click etc. occurs
function OnUIAction(ui, action)
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

