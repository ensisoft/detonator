-- Scene 'Game' sc3ript.
-- This script will be called for every instance of 'Game'
-- during gameplay.
-- You're free to delete functions you don't need.
-- Called when the scene begins play.
--
require('common')
require('app://scripts/utility/camera.lua')
require('app://scripts/utility/utility.lua')

-- LuaFormatter off

-- shortcut to the player entity in the scene. we should always have player
-- in the scene so it's safe to hold onto it.
local the_player = nil

-- current wave of enemies that is coming.
local wave_count          = 0

-- tick count is used to throttle the spawning of enemies
local game_tick_count     = 0

-- enemy ship velocity increment. as the game goes on the enemies get
-- faster and faster, thus making the game harder and harder
local ship_velo_increment = 1.0

-- previous formation. used to avoid spawning same formation again back to back
local prev_formation      = -1

local game_over = false

local game_score = 0

-- LuaFormatter on

function SpawnEnemy(x, y, velocity)
    local ship = ClassLib:FindEntityClassByName('Enemy')
    local args = game.EntityArgs:new()
    args.class = ship
    args.name = "enemy"
    args.position = glm.vec2:new(x, y)
    args.layer = 1
    local ship = Scene:SpawnEntity(args, true)
    ship.velocity = velocity * ship_velo_increment
    ship.score = math.floor(ship.score * ship_velo_increment)
end

function SpawnWaveLeft()
    local x = -500
    local y = -500
    local velocity = glm.vec2:new(-200.0, 150.0)
    for i = 1, 10, 1 do
        SpawnEnemy(x, y, velocity)
        x = x + 100
        y = y - 40
    end
end

function SpawnWaveRight()
    local x = 500
    local y = -500
    local velocity = glm.vec2:new(200.0, 150.0)
    for i = 1, 10, 1 do
        SpawnEnemy(x, y, velocity)
        x = x - 100
        y = y - 40
    end
end

function SpawnChevron()
    local velocity = glm.vec2:new(0.0, 150.0)
    SpawnEnemy(-400.0, -660.0, velocity)
    SpawnEnemy(-300.0, -620.0, velocity)
    SpawnEnemy(-200.0, -580.0, velocity)
    SpawnEnemy(-100.0, -540.0, velocity)
    SpawnEnemy(0.0, -500.0, velocity)
    SpawnEnemy(100.0, -540.0, velocity)
    SpawnEnemy(200.0, -580.0, velocity)
    SpawnEnemy(300.0, -620.0, velocity)
    SpawnEnemy(400.0, -660.0, velocity)
end

function SpawnRow()
    local x = 0
    local y = -500
    local velocity = glm.vec2:new(0.0, 150.0)
    for i = 1, 10, 1 do
        SpawnEnemy(x, y, velocity)
        y = y - 100
    end
end

function BeginPlay(game, map)

    Camera.SetViewport(base.FRect:new(-600.0, -400.0, 1200, 800))

    -- initialize the random engine with a fixed seed number so that
    -- the random sequence is always the same on every single play
    util.RandomSeed(6634)

    -- find the player entity. generally holding onto entities is not
    -- safe unless we are 100% sure of the lifetime.
    the_player = game:FindEntityByInstanceName('Player')

    if State.play_music then
        Audio:PlayMusic('Game Music')
        Audio:SetMusicEffect('Game Music', 'FadeIn', 2000)
    end
end

-- Called when the scene ends play.
function EndPlay(game, map)
end

-- Called when a new entity has been spawned in the scene.
function SpawnEntity(game, map, entity)
end

-- Called when an entity has been killed from the scene.
function KillEntity(game, map, entity)
end

-- Called on every low frequency game tick.
function Tick(game, game_time, dt)

    SpawnSpaceRock()

    -- use the tick count to throttle spawning of objects
    if math.fmod(game_tick_count, 5) == 0 then

        -- randomly select a new formation
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

        -- increase the velocity of the enemies after each wave
        -- while we're actually playing. after game over just keep spawning
        -- at the current velocity
        if not game_over then
            ship_velo_increment = ship_velo_increment + 0.05

            -- update the wave counter in the HUD
            wave_count = wave_count + 1
            local info_entity = game:FindEntityByInstanceName('Info')
            local info_node = info_entity:GetNode(0)
            local info_text = info_node:GetTextItem()
            info_text:SetText(util.FormatString('Wave %1', wave_count))
        end
    end
    game_tick_count = game_tick_count + 1
end

-- Called on every iteration of game loop.
function Update(game, game_time, dt)
    Game:SetViewport(Camera.Update(dt))

    if the_player == nil then
        return
    end

    if the_player:IsDying() then
        the_player = nil
    end
end

-- Called on collision events with other objects.
function OnBeginContact(game, entity, entity_node, other, other_node)
end

-- Called on collision events with other objects.
function OnEndContact(game, entity, entity_node, other, other_node)
end

-- Called on key down events.
function OnKeyDown(game, symbol, modifier_bits)
    if symbol == wdk.Keys.Escape then
        Game:EndPlay()
        Game:Play('Menu')
        Game:CloseUI('GameOver', 0)
        Game:OpenUI('MainMenu')

        if State.play_music then
            -- we might be escaping from either "game over" or "play" state
            -- either case, kill all music tracks
            Audio:SetMusicEffect('Ending', 'FadeOut', 2000)
            Audio:KillMusic('Ending', 2000)

            Audio:SetMusicEffect('Game Music', 'FadeOut', 2000)
            Audio:KillMusic('Game Music', 2000)
        end
    end
end

-- Called on key up events.
function OnKeyUp(game, symbol, modifier_bits)
end

-- Called on mouse button press events.
function OnMousePress(game, mouse)
end

-- Called on mouse button release events.
function OnMouseRelease(game, mouse)
end

-- Called on mouse move events.
function OnMouseMove(game, mouse)
end

-- Called on game events.
function OnGameEvent(game, event)
    -- when an enemy ship is killed update the game score.
    if event.from == 'enemy' and event.message == 'dead' then
        Game:DebugPrint('Enemy was hit!')

        game_score = game_score + event.value

        local entity = Scene:FindEntityByInstanceName('GameScore')
        local node = entity:FindNodeByClassName('text')
        local text = node:GetTextItem()
        text:SetText(tostring(game_score))

        Camera.Shake(util.RandomVec2(-10, 10.0), 0.03)
    end

    -- when the player gets killed end the game
    if event.from == 'player' and event.message == 'dead' then
        Game:DebugPrint('Game Over!')

        local high_score = State.high_score
        local game_over = Game:OpenUI('GameOver')
        CallMethod(game_over, 'UpdateScore', game_score, high_score)

        if State.play_music then
            Audio:SetMusicEffect('Game Music', 'FadeOut', 2000)
            Audio:KillMusic('Game Music', 2000)
            Audio:PlayMusic('Ending', 5000)
        end

        -- record new highscore
        if game_score > high_score then
            State.high_score = game_score
        end

        game_over = true

        Camera.Shake(util.RandomVec2(-50.0, 50.0), 0.05)

    end
end

