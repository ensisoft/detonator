-- There are some global objects that are set when
-- the game is loaded. They are set up for your
-- convenience.
--
-- 'Physics' is the physics engine.
-- 'ClassLib' is the class library for accessing the resources such as entities.
-- 'Game' is the native instance of *this* game that is running. It provides
--        the native services for interfacing with the game engine.
-- 'Scene' is the current scene that is loaded.

local game_tick_count = 0
local ship_velo_increment = 0
local prev_formation = -1

States = {
    Menu = 1,
    Play = 2
}
Menus = {
    MainMenu = 1,
    GameOver = 2
}

local GameState = {
    state = States.Menu,
    menu  = Menus.MainMenu,
    music_volume   = 1.0,
    effects_volume = 1.0,
    play_music  = true,
    play_effects = true
}

function SpawnEnemy(x, y, velocity)
    local ship = ClassLib:FindEntityClassByName('Ship2')
    local args = game.EntityArgs:new()
    args.class = ship
    args.name  = "ship2"
    args.position = glm.vec2:new(x, y)
    local ship = Scene:SpawnEntity(args, true)
    ship.velocity = velocity * ship_velo_increment
    ship.score    = math.floor(ship.score * ship_velo_increment)
end

function SpawnWaveLeft()
    local x = -500
    local y = -500
    local velocity = glm.vec2:new(-200.0, 150.0)
    for i=1, 5, 1 do
        SpawnEnemy(x, y, velocity)
        x = x + 200
        y = y - 80
    end
end

function SpawnWaveRight()
    local x =  500
    local y = -500
    local velocity = glm.vec2:new(200.0, 150.0)
    for i=1, 5, 1 do
        SpawnEnemy(x, y, velocity)
        x = x - 200
        y = y - 80
    end
end

function SpawnChevron()
    local velocity = glm.vec2:new(0.0, 150.0)
    SpawnEnemy(-300.0, -650.0, velocity)
    SpawnEnemy(-150.0, -550.0, velocity)
    SpawnEnemy( 150.0, -550.0, velocity)
    SpawnEnemy( 300.0, -650.0, velocity)
    SpawnEnemy(   0.0, -500.0, velocity)
end

function SpawnRow()
    local x =  0
    local y = -500
    local velocity = glm.vec2:new(0.0, 150.0)
    for i=1, 5, 1 do
        SpawnEnemy(x, y, velocity)
        y = y - 180
    end
end


function SpawnSpaceRock()
    local maybe = util.Random(0, 2)
    local scale = glm.vec2:new(1.0, 1.0)
    local radius = 30
    local time_scale = 2.0
    if maybe == 1 then
        local size = util.Random(0, 2)
        if size == 1 then
            scale = glm.vec2:new(2.0, 2.0)
            radius = 60
            time_scale = 1
        end
        local asteroid = ClassLib:FindEntityClassByName('Asteroid')
        local args = game.EntityArgs:new()
        args.class = asteroid
        args.name  = 'asteroid'
        args.position = glm.vec2:new(util.Random(-550, 550), -500)
        args.scale    = scale
        local entity = Scene:SpawnEntity(args, true)
        local body   = entity:FindNodeByClassName('Body')
        local item   = body:GetDrawable()
        item:SetTimeScale(time_scale)
        entity.radius = radius
    end
end

-- closes the menu and starts game.
function EnterGame()
    Game:ShowMouse(false)
    Game:Delay(0.5)
    Game:CloseUI(0)
    Game:Play('Game')
    Audio:SetMusicEffect('Menu Music', 'FadeOut', 2000)
    Audio:KillMusic('Menu Music', 2000)
    if State.play_music then
        Audio:PlayMusic('Game Music')
        Audio:SetMusicEffect('Game Music', 'FadeIn', 2000)
    end
end

function EnterMenu()
    Game:DebugPrint('Enter Menu')
    Game:Stop(0)
    Game:CloseUI(0)
    Game:Play('Menu')
    Game:OpenUI('MainMenu')
    Game:ShowMouse(true)
    Audio:KillMusic('Game Music')
    Audio:KillMusic('Ending')
    if State.play_music then
        Audio:PlayMusic('Menu Music')
    end
end

function InitGameState()
    ship_velo_increment = 1.0
    game_tick_count = 0
    prev_formation  = -1
    util.RandomSeed(6634)
end

function LoadGame()
    Game:DebugPrint('LoadGame')
    local file = util.JoinPath(game.home, 'state.json')
    if util.FileExists(file) then
        local json, error = data.ReadJsonFile(file)
        if json == nil then
            base.error('Failed to load game state.')
            base.error(error)
            return false
        end
        State:Restore(json)
    end 
    Audio:SetMusicGain(State:GetValue('music_gain', 1.0))
    Audio:SetSoundEffectGain(State:GetValue('effects_gain', 1.0))
    State:InitValue('play_music', true)
    State:InitValue('play_effects', true)
    Game:DebugPrint('State loaded')
    return true
end

function StartGame()
    Game:DebugPrint('StartGame')
    Game:SetViewport(base.FRect:new(-600.0, -400.0, 1200, 800.0))
    EnterMenu()
end

function StopGame()
    Game:DebugPrint('StopGame')
end

function SaveGame()
    Game:DebugPrint('SaveGame')
    local state_json = util.JoinPath(game.home, 'state.json')
    local json = data.JsonObject:new()
    State:Persist(json)
    data.WriteJsonFile(json, state_json)
end

function BeginPlay(scene)
    Game:DebugPrint('BeginPlay ' .. scene:GetClassName())
    if scene:GetClassName() == 'Menu' then
        GameState.state = States.Menu
    elseif scene:GetClassName() == 'Game' then
        GameState.state = States.Play
        InitGameState()
    end
end

function EndPlay(scene)
    Game:DebugPrint('EndPlay' .. scene:GetClassName())
end

function Tick(game_time, dt)
    if Scene == nil then return end

    -- spawn a space rock even in the menu.
    SpawnSpaceRock()
    if GameState.state == States.Menu then
        return
    end

    if math.fmod(game_tick_count, 3) == 0 then
        local next_formation = util.Random(0, 3)
        while next_formation == prev_formation do
            next_formation = util.Random(0, 3)
        end
        prev_formation = next_formation

        if next_formation == 0 then
            SpawnRow()
        elseif next_formation == 1 then
            SpawnWaveLeft()
        elseif next_formation == 2 then
            SpawnWaveRight()
        elseif next_formation == 3 then
            SpawnChevron()
        end
        -- increase the velocity of the enemies after each wave.
        ship_velo_increment = ship_velo_increment + 0.05
    end
    -- use the tick count to throttle spawning of objects
    game_tick_count = game_tick_count + 1
end

function Update(game_time, dt)
end


-- input event handlers
function OnKeyDown(symbol, modifier_bits)
    if symbol == wdk.Keys.Escape then
        if GameState.state == States.Play then
            EnterMenu()
        elseif GameState.state == States.Menu then
            if GameState.menu == Menus.MainMenu then
                Game:Quit(0)
            elseif GameState.menu == Menus.Options then
                Game:CloseUI(0)
            elseif GameState.menu == Menus.GameOver then 
                EnterMenu()
            end
        end
    elseif symbol == wdk.Keys.Space then
        if GameState.state == States.Menu then
            if GameState.menu == Menus.MainMenu then
                EnterGame()
            elseif GameState.menu == Menus.GameOver then
                EnterMenu()
            end
        end
    end
end

function OnKeyUp(symbol, modifier_bits)
end

function OnAudioEvent(event)
    if event.track == 'Game Music' then 
        if event.type == 'TrackDone'  and GameState.state == States.Play then
            Audio:PlayMusic('Game Music')
        end
    end 
end

function OnUIOpen(ui)
    Game:DebugPrint(ui:GetName() .. ' is open')
    if ui:GetName() == 'MainMenu' then
        GameState.menu = Menus.MainMenu
    elseif ui:GetName() == 'GameOver' then
        GameState.menu = Menus.GameOver
    elseif ui:GetName() == 'Options' then

        GameState.menu = Menus.Options
        local play_music = ui:FindWidgetByName('play_music', 'CheckBox')
        local play_effects = ui:FindWidgetByName('play_effects', 'CheckBox')
        local music_volume = ui:FindWidgetByName('music_volume', 'Slider')
        local effects_volume = ui:FindWidgetByName('effects_volume', 'Slider')     

        play_music:SetChecked(State:GetValue('play_music', true))
        play_effects:SetChecked(State:GetValue('play_effects', true))
        music_volume:SetValue(State:GetValue('music_gain', 1.0))
        effects_volume:SetValue(State:GetValue('effects_gain', 1.0))
    end    
end

function OnUIClose(ui, result)
    Game:DebugPrint(ui:GetName() .. ' is closed')
    if ui:GetName() == 'Options' then 
        GameState.menu = Menus.MainMenu
    end
end

function OnUIAction(ui, action)
    if action.name == 'exit' then
        Game:Quit(0)
    elseif action.name == 'play' then
        Audio:PlaySoundEffect('Menu Click', 0)
        EnterGame()
    elseif action.name == 'options' then
        Game:OpenUI('Options')
    elseif action.name == 'play_music' then 
        if action.value == false then
            Audio:KillMusic('Menu Music')
        else
            Audio:PlayMusic('Menu Music')
        end
        State:SetValue('play_music', action.value)
    elseif action.name == 'play_effects' then 
        State:SetValue('play_effects', action.value)
    elseif action.name == 'music_volume' then
        State:SetValue('music_gain', action.value)
        Audio:SetMusicGain(action.value)
    elseif action.name == 'effects_volume' then 
        State:SetValue('effects_gain', action.value)
        Audio:SetSoundEffectGain(action.value)
    end
end


