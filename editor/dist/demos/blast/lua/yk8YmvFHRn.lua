-- UI 'Options' script.
-- This script will be called for every instance of 'Options.'
-- You're free to delete functions you don't need.
-- Called whenever the UI has been opened.
function OnUIOpen(ui)
    assert(State:HasValue('effects_volume'))
    assert(State:HasValue('music_volume'))

    base.debug('effects_volume: ' .. tostring(State.effects_volume))
    base.debug('music_volume: ' .. tostring(State.music_volume))

    -- initialize the UI state from global State object
    ui.play_music:SetChecked(State.play_music)
    ui.play_effects:SetChecked(State.play_effects)
    ui.music_volume:SetValue(State.music_volume)
    ui.effects_volume:SetValue(State.effects_volume)
end

-- Called whenever the UI is about to be closed.
-- result is the value passed in the exit_code in the call to CloseUI.
function OnUIClose(ui, result)

end

-- time stamp value to control and rate limit the audio effect playback
-- when moving the effect volume slider.
local last_audio_play_time = 0

-- Called whenever some UI action such as button click etc. occurs
function OnUIAction(ui, action)
    if action.name == 'close' then
        Game:CloseUI(0)
        Audio:PlaySoundEffect('Menu Click')

    elseif action.name == 'play_music' then
        State.play_music = action.value
        if action.value == true then
            Audio:PlayMusic('Menu Music')
        else
            Audio:KillMusic('Menu Music')
        end
        Audio:PlaySoundEffect('Menu Click')

    elseif action.name == 'play_effects' then
        State.play_effects = action.value
        Audio:EnableEffects(action.value)
        Audio:PlaySoundEffect('Menu Click')

    elseif action.name == 'music_volume' then
        State.music_volume = tonumber(action.value)
        Audio:SetMusicGain(action.value)

    elseif action.name == 'effects_volume' then
        State.effects_volume = tonumber(action.value)
        Audio:SetSoundEffectGain(action.value)
        -- throttle the sample effect playback with a timestamp
        -- without this every movement on the UI slider will play
        -- audio creating an acoustic mess
        local time_now = util.GetMilliseconds()
        if time_now - last_audio_play_time > 250 then
            Audio:PlaySoundEffect('Player Weapon', 0)
            last_audio_play_time = time_now
        end
    end
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

