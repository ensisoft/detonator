-- UI 'UI' script.
-- This script will be called for every instance of 'UI.'
-- You're free to delete functions you don't need.
require('data')

local index = 1

function PlayEffect(ui)
    local title = util.FormatString("%1/%2 %3", math.floor(index), #Particles,
                                    Particles[index])

    local text = ui:FindWidgetByName('text', 'Label')
    text:SetText(title);

    local event = game.GameEvent:new()
    event.from = 'menu'
    event.to = 'main'
    event.message = 'effect'
    event.value = Particles[index]
    Game:PostEvent(event)
end

-- Called whenever the UI has been opened.
function OnUIOpen(ui)
    PlayEffect(ui, Particles[1])
end

-- Called whenever the UI is about to be closed.
-- result is the value passed in the exit_code in the call to CloseUI.
function OnUIClose(ui, result)
end

-- Called whenever some UI action such as button click etc. occurs
function OnUIAction(ui, action)
    if action.name == 'prev' then
        index = base.wrap(1, #Particles, index - 1)
    elseif action.name == 'next' then
        index = base.wrap(1, #Particles, index + 1)
    elseif action.name == 'background' then
        local event = game.GameEvent:new()
        event.from = 'menu'
        event.to = 'main'
        event.message = 'background'
        event.value = action.value
        Game:PostEvent(event)
    elseif action.name == 'fullscreen' then
        Game:SetFullScreen(action.value)
    end
    PlayEffect(ui)
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
    if symbol == wdk.Keys.ArrowLeft then
        index = base.wrap(1, #Particles, index + 1)
    elseif symbol == wdk.Keys.ArrowRight then
        index = base.wrap(1, #Particles, index - 1)
    else
        return
    end
    PlayEffect(ui)
end

-- Optionally called on keyboard events when the flag is set.
function OnKeyUp(ui, symbol, modifier_bits)
end

