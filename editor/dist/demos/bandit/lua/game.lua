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
Viewport = base.FRect:new(0.0, 0.0, 1200, 800.0)

_States = {
    Menu,
    Play
}

_State = _States.Menu
_Lives = 2

function _SetTimeScale(entity, node_name, scale)
    local node = entity:FindNodeByClassName(node_name)
    local draw = node:GetDrawable()
    draw:SetTimeScale(scale)
end

function _SetBackgroundVelocity(velocity)
    local bg = Scene:FindEntityByInstanceName('Background')
    _SetTimeScale(bg, 'Layer0', velocity * 1.0)
    _SetTimeScale(bg, 'Layer1', velocity * 1.0)
    _SetTimeScale(bg, 'Layer2', velocity * 1.0)
end

function _GameOver()
    Game:CloseUI(0)
    Game:Play('Menu Scene')
    Game:OpenUI('Menu UI')
    Game:SetViewport(-600, -400, 1200, 800)
end

function _RespawnLevel()
    Game:Delay(1.0)
    Game:Play('Level 0')
end

-- main game callbacks

-- start the game and show some initial content.
function StartGame()
    Game:SetViewport(-600, -400, 1200, 800)
    Game:Play('Menu Scene')
    Game:OpenUI('Menu UI')
end

-- Called when the scene begins to play
function BeginPlay(scene, map)
    if scene:GetClassName() == 'Level 0' then
        local ui = Game:GetTopUI()
        if ui == nil then
            return
        end
        local l0 = ui:FindWidgetByName('life0')
        local l1 = ui:FindWidgetByName('life1')
        Game:DebugPrint(tostring(l0))
        Game:DebugPrint(tostring(l1))

        if true then
            return
        end

        if _Lives == 2 then
            l0:SetVisible(true)
            l1:SetVisible(true)
        elseif _Lives == 1 then
            l0:SetVisible(false)
            l1:SetVisible(true)
        elseif _Lives == 0 then
            l0:SetVisible(false)
            l1:SetVisible(false)
        end
    end
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
    if player == nil or player.died then
        return
    end

    local ui = Game:GetTopUI()

    if true then
        return
    end

    local coins = ui:FindWidgetByName('coins')
    coins:SetText('x' .. tostring(player.coins))

    local node = player:FindNodeByClassName('Body')
    local pos = node:GetTranslation()
    local body = node:GetRigidBody()
    local velo = body:GetLinearVelocity()

    -- adjust the scrolling speed of the background layers by
    -- adjusting the time scale of the background layer drawables
    _SetBackgroundVelocity(velo.x)

    -- move the game viewport to follow the player.
    local viewport_offset = glm.vec2:new(200, 600)
    local viewport_pos = pos - viewport_offset
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
        if _State == _States.Menu then
            Game:Quit(0)
        end
    end
end

function OnKeyUp(symbol, modifier_bits)

end

function OnGameEvent(event)
    if event.from == 'player' and event.message == 'died' then
        Game:DebugPrint('Player died')
        if Scene:GetClassName() == 'Menu Scene' then
            Game:Play('Menu Scene')
        else
            _SetBackgroundVelocity(0.0)
            _Lives = _Lives - 1
            if (_Lives == -1) then
                _GameOver()
                return
            end
            _RespawnLevel()
        end
    elseif event.from == 'player' and event.message == 'level-complete' then
        Game:DebugPrint('Level complete')
        _GameOver()
    end
end

function OnUIAction(ui, action)
    if action.name == 'quit' then
        Game:Quit(0)
    elseif action.name == 'play' then
        _State = _States.Play
        _Lives = 2
        Game:CloseUI(0)
        Game:Play('Level 0')
        Game:OpenUI('Game UI')

    end
end
