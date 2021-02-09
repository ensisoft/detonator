-- There are some global objects that are set when
-- the game is loaded. They are set up for your
-- convenience.
--
-- 'Physics' is the physics engine.
-- 'ClassLib' is the class library for accessing the resources such as entities.
-- 'Game' is the native instance of *this* game that is running. It provides
--        the native services for interfacing with the game engine.

-- player entity object that we're controlling
Player = nil
-- the current scene that we're playing.
Scene  = nil
-- the background *entity*
Background = nil
-- viewport into the game world, we move it as the game
-- advances to follow the player
Viewport = game.FRect:new(0.0, 0.0, 1200, 800.0)

-- world vectors
WorldUp    = glm.vec2:new(0.0, -1.0)
WorldRight = glm.vec2:new(1.0, 0.0)

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
    local background = ClassLib:FindSceneClassByName('Background')
    Game:LoadBackground(background)
    Game:PlayScene(scene)
    Game:SetViewport(Viewport)
end

function TurnPlayerLeft()
    local max_nodes = Player:GetNumNodes()
    for i=0, max_nodes-1, 1 do
        local node = Player:GetNode(i)
        local draw = node:GetDrawable()
        if draw ~= nil then
           draw:SetFlag('FlipVertically', true)
        end
    end
end

function TurnPlayerRight()
    local max_nodes = Player:GetNumNodes()
    for i=0, max_nodes-1, 1 do
        local node = Player:GetNode(i)
        local draw = node:GetDrawable()
        if draw ~= nil then
            draw:SetFlag('FlipVertically', false)
        end
    end
end


function LoadBackgroundDone(background)
    Background = background:FindEntityByInstanceName('Background')
end

function BeginPlay(scene)
    Player = scene:FindEntityByInstanceName('Player')
    Scene  = scene
end

function Tick(wall_time, tick_time, dt)

end

function Update(wall_time, game_time, dt)
    if Player == nil then
        return
    end

    local node = Player:FindNodeByClassName('Body')
    local pos  = node:GetTranslation() - glm.vec2:new(200, 600)
    local body = node:GetRigidBody()
    local velo = body:GetLinearVelocity()

    -- adjust the scrolling speed of the background layers by
    -- adjusting the time scale of the background layer drawables
    SetTimeScale(Background, 'Layer0', velo.x * 1.0)
    SetTimeScale(Background, 'Layer1', velo.x * 1.0)
    SetTimeScale(Background, 'Layer2', velo.x * 1.0)

    Viewport:Move(pos.x, pos.y)
    Game:SetViewport(Viewport)
end


function EndPlay()

end

function OnBeginContact(entityA, entityB, nodeA, nodeB)
    local class_a = entityA:GetClassName()
    local class_b = entityB:GetClassName()
    --Game:DebugPrint(class_a .. '  ' .. class_b)

    local player = nil
    local coin   = nil

    if class_a == 'Player' then
        player = entityA
    elseif class_b == 'Player' then
        player = entityB
    end

    if class_a == 'Coin' then
        coin = entityA
    elseif class_b == 'Coin' then
        coin = entityB
    end

    if player == nil or coin == nil then
        return
    end

    Game:DebugPrint('Coin collected!')
    Scene:DeleteEntity(coin)
end

function OnEndContact(entityA, entityB, nodeA, nodeB)

end

-- input event handlers
function OnKeyDown(symbol, modifier_bits)
    local body  = Player:FindNodeByClassName('Body')
    local jump  = Player.jump
    local slide = Player.slide

    if symbol == wdk.Keys.ArrowRight then
        TurnPlayerRight()
        Physics:ApplyImpulseToCenter(body, WorldRight * slide)
        Player:PlayAnimationByName('Walk')
    elseif symbol == wdk.Keys.ArrowLeft then
        TurnPlayerLeft()
        Physics:ApplyImpulseToCenter(body, WorldRight * slide * -1.0)
        Player:PlayAnimationByName('Walk')
    elseif symbol == wdk.Keys.ArrowUp then
         Physics:ApplyImpulseToCenter(body, WorldUp * jump)
         Player:PlayAnimationByName('Jump')
    elseif symbol == wdk.Keys.Space then
        Player:PlayAnimationByName('Attack')
    elseif symbol == wdk.Keys.Escape then
        local scene = ClassLib:FindSceneClassByName('My Scene')
        Game:PlayScene(scene)
    end

end

function OnKeyUp(symbol, modifier_bits)

end

