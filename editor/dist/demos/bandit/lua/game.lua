-- There are some global objects that are set when
-- the game is loaded. They are set up for your
-- convenience.
--
-- 'Physics' is the physics engine.
-- 'ClassLib' is the class library for accessing the resources such as entities.
-- 'Game' is the native instance of *this* game that is running. It provides
--        the native services for interfacing with the game engine.

-- the current scene that we're playing.
Scene  = nil
-- viewport into the game world, we move it as the game
-- advances to follow the player
Viewport = base.FRect:new(0.0, 0.0, 1200, 800.0)

function SetTimeScale(entity, node_name, scale)
    local node = entity:FindNodeByClassName(node_name)
    local draw = node:GetDrawable()
    draw:SetTimeScale(scale)
end

-- main game callbacks

-- This is called when the game is first loaded when
-- the application is started. Before the call the loader
-- setups the global objects (as documented at the start of
-- of the file) and then proceeds to call LoadGame.
-- The intention of this function is for the game to start
-- the initial state machine. I.e. for example find a menu
-- object and ask for the game to show the menu.
function LoadGame()
    local scene = ClassLib:FindSceneClassByName('My Scene')
    Game:Play(scene)
    Game:SetViewport(Viewport)
end

-- Called when the scene begins to play
function BeginPlay(scene)
    Scene  = scene
end

-- Called when the scene play ends.
function EndPlay(scene)

end

function Tick(wall_time, tick_time, dt)

end

function Update(wall_time, game_time, dt)
    local player = Scene:FindEntityByInstanceName('Player')
    if player == nil then
        return
    end

    local node = player:FindNodeByClassName('Body')
    local pos  = node:GetTranslation()
    local body = node:GetRigidBody()
    local velo = body:GetLinearVelocity()

    -- adjust the scrolling speed of the background layers by
    -- adjusting the time scale of the background layer drawables
    local bg = Scene:FindEntityByInstanceName('Background')
    SetTimeScale(bg, 'Layer0', velo.x * 1.0)
    SetTimeScale(bg, 'Layer1', velo.x * 1.0)
    SetTimeScale(bg, 'Layer2', velo.x * 1.0)

    -- move the game viewport to follow the player.
    local viewport_offset = glm.vec2:new(200, 600)
    local viewport_pos    = pos - viewport_offset
    Viewport:Move(viewport_pos.x, viewport_pos.y)
    Game:SetViewport(Viewport)
end




function OnBeginContact(entityA, entityB, nodeA, nodeB)
    -- nothing here.
end

function OnEndContact(entityA, entityB, nodeA, nodeB)
    -- nothing here.
end

-- input event handlers
function OnKeyDown(symbol, modifier_bits)
    if symbol == wdk.Keys.Escape then
        local scene = ClassLib:FindSceneClassByName('My Scene')
        Game:Play(scene)
    end
end

function OnKeyUp(symbol, modifier_bits)

end

