-- There are some global objects that are set when
-- the game is loaded. They are set up for your
-- convenience.
--
-- 'Physics' is the physics engine.
-- 'ClassLib' is the class library for accessing the resources such as entities.
-- 'Game' is the native instance of *this* game that is running. It provides
--        the native services for interfacing with the game engine.
-- 'Scene' is the current scene that is loaded.

local tick_count = 0
local ship_velo_increment = 0

function SpawnEnemy(x, y, velocity)
    local ship = ClassLib:FindEntityClassByName('Ship2')
    local args = game.EntityArgs:new()
    args.class = ship
    args.name  = "ship2"
    args.position = glm.vec2:new(x, y)
    local ship = Scene:SpawnEntity(args, true)
    ship.velocity = velocity * ship_velo_increment
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
    local maybe = math.random(0, 2)
    local scale = glm.vec2:new(1.0, 1.0)
    local radius = 30
    local time_scale = 2.0
    if maybe == 1 then
        local size = math.random(0, 2)
        if size == 1 then
            scale = glm.vec2:new(2.0, 2.0)
            radius = 60
            time_scale = 1
        end
        local asteroid = ClassLib:FindEntityClassByName('Asteroid')
        local args = game.EntityArgs:new()
        args.class = asteroid
        args.name  = 'asteroid'
        args.position = glm.vec2:new(math.random(-550, 550), -500)
        args.scale    = scale
        local entity = Scene:SpawnEntity(args, true)
        local body   = entity:FindNodeByClassName('Body')
        local item   = body:GetDrawable()
        item:SetTimeScale(time_scale)
        entity.radius = radius
    end
end

-- This is called when the game is first loaded when
-- the application is started. Before the call the loader
-- setups the global objects (as documented at the start of
-- of the file) and then proceeds to call LoadGame.
-- The intention of this function is for the game to start
-- the initial state machine. I.e. for example find a menu
-- object and ask for the game to show the menu.
function LoadGame()
    local scene = ClassLib:FindSceneClassByName('My Scene')
    Game:PlayScene(scene)

    -- set the viewport size and position
    local viewport = base.FRect:new(-600.0, -400.0, 1200, 800.0)
    Game:SetViewport(viewport)
end

function LoadBackgroundDone(background)
end

function BeginPlay(scene)
    ship_velo_increment = 1.0
    tick_count          = 0
    math.randomseed(6634)
end

function Tick(wall_time, tick_time, dt)
    if math.fmod(tick_count, 3) == 0 then
        local formation = math.random(0, 3)
        Game:DebugPrint('Spawning: ' .. tostring(formation))
        if formation == 0 then
            SpawnRow()
        elseif formation == 1 then
            SpawnWaveLeft()
        elseif formation == 2 then
            SpawnWaveRight()
        elseif formation == 3 then
            SpawnChevron()
        end
        -- increase the velocity of the enemies after each wave.
        ship_velo_increment = ship_velo_increment + 0.05
    end
    -- use the tick count to throttle spawning of objects
    tick_count = tick_count + 1

    SpawnSpaceRock()
end

function Update(wall_time, game_time, dt)
end

function EndPlay()
end

-- input event handlers
function OnKeyDown(symbol, modifier_bits)
    if symbol == wdk.Keys.Escape then
        local scene = ClassLib:FindSceneClassByName('My Scene')
        Game:PlayScene(scene)
    end
end

function OnKeyUp(symbol, modifier_bits)
end
