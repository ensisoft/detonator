-- There are some global objects that are set when
-- the game is loaded. They are set up for your
-- convenience.
--
-- 'Physics' is the physics engine.
-- 'ClassLib' is the class library for accessing the resources such as entities.
-- 'Game' is the native instance of *this* game that is running. It provides
--        the native services for interfacing with the game engine.
-- viewport into the game world, we move it as the game
-- advances to follow the player
GameStates = {
    Menu,
    Play
}
local GameViewport = nil
local GameState = 'Menu'
local Lives = 2

function GameOver()
    Game:CloseUI(0)
    Game:Play('Menu Scene')
    Game:OpenUI('Menu UI')
    Game:SetViewport(-600, -400, 1200, 800)
end

function RespawnLevel()
    Game:Delay(1.0)
    Game:Play('Level 0')
end

-- main game callbacks

-- start the game and show some initial content.
function StartGame()
    local w = 1200.0
    local h = 800
    GameViewport = base.FRect:new(-w * 0.5, -h * 0.5, w, h)

    Game:SetViewport(GameViewport)
    Game:Play('Menu Scene')
    Game:OpenUI('Menu UI')
    Game:SetCameraPosition(0.0, 0.0)
end

-- Called when the scene begins to play
function BeginPlay(scene, map)

end

-- Called when the scene play ends.
function EndPlay(scene, map)

end

function Tick(game_time, dt)

end

function Update(game_time, dt)

    if Scene == nil then
        return
    end
    local player = Scene:FindEntityByInstanceName('Player')
    if player == nil then
        return
    end

    local player_body_node = player:FindNodeByClassName('Body')
    local player_rigid_body = player_body_node:GetRigidBody()
    local player_velocity = player_rigid_body:GetLinearVelocity()

    local background = Scene:FindEntityByInstanceName('Background')
    if background == nil then
        return
    end

    local background_node = background:GetNode(0)
    local background_draw = background_node:GetDrawable()
    background_draw:SetTimeScale(player_velocity.x * 0.01)

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
        if GameState == 'Menu' then
            Game:Quit(0)

        elseif GameState == 'Play' then
            Game:CloseUI(0)
            StartGame()
        end
    end
end

function OnKeyUp(symbol, modifier_bits)

end

function OnGameEvent(event)

end

function OnUIAction(ui, action)
    if action.name == 'quit' then
        Game:Quit(0)
    elseif action.name == 'play' then
        GameState = 'Play'
        Lives = 2
        Game:CloseUI(0)
        Game:Play('Level 0')
        Game:OpenUI('Game UI')
    end
end
