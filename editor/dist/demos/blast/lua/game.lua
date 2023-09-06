-- Game's "main" script. Contains the high level 
-- scripting entry points for starting game, loading
-- previous game state and saving state.
--
--
-- LuaFormatter off

-- flag to indicate whether we're showing the developer UI or not.
local developer_ui = false

-- LuaFormatter on

function LoadGame()
    Game:DebugPrint('LoadGame')

    local file = util.JoinPath(game.home, 'state.json')

    base.debug('LoadGame ' .. file)

    -- load previously saved game state that includes state
    -- such as previous highscore, audio levels and flags.

    if util.FileExists(file) then
        local json, error = data.ReadJsonFile(file)
        if json == nil then
            base.error('Failed to load game state.')
            base.error(error)
            return false
        end
        State:Restore(json)
    end

    -- Initialize audio engine settings. Settings GetValue will initialize
    -- the setting with the default value if it doesn't yet exist. 
    Audio:SetMusicGain(State:GetValue('music_volume', 0.5))
    Audio:SetSoundEffectGain(State:GetValue('effects_volume', 0.25))
    Audio:EnableEffects(State:GetValue('play_effects', true))
    State:InitValue('play_music', true)
    State:InitValue('high_score', 0)
    Game:DebugPrint('State loaded')
    return true
end

function StartGame()
    Game:DebugPrint('StartGame')
    Game:SetViewport(base.FRect:new(-600.0, -400.0, 1200, 800.0))
    Game:Play('Menu')
    Game:OpenUI('MainMenu')
end

function SaveGame()
    Game:DebugPrint('SaveGame')
    local state_json = util.JoinPath(game.home, 'state.json')
    local json = data.JsonObject:new()
    State:Persist(json)
    data.WriteJsonFile(json, state_json)
end

function BeginPlay(scene, map)
    Game:DebugPrint('BeginPlay ' .. scene:GetClassName())
end

function EndPlay(scene, map)
    Game:DebugPrint('EndPlay' .. scene:GetClassName())
end

function Tick(game_time, dt)
end

function Update(game_time, dt)
end

-- input event handlers
function OnKeyDown(symbol, modifier_bits)
    if symbol == wdk.Keys.F12 then
        developer_ui = not developer_ui
        Game:ShowDeveloperUI(developer_ui)
    end
end

function OnKeyUp(symbol, modifier_bits)
end

function OnAudioEvent(event)
    if event.track == 'Game Music' then
        if event.type == 'TrackDone' then
            Audio:PlayMusic('Game Music')
        end
    end
end

function OnUIOpen(ui)
end

function OnUIClose(ui, result)
end

function OnUIAction(ui, action)
end

function OnGameEvent(event)
end
