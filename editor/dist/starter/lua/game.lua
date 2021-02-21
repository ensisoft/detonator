
function LoadGame()
    local scene = ClassLib:FindSceneClassByName('My Scene')
    Game:PlayScene(scene)
    Game:SetViewport(game.FRect:new(-500.0, -800.0, 1000, 800))
end

function BeginPlay(scene)
    Game:DebugPrint('Press Space to fly the bird!')
end

function EndPlay()

end

function Tick(wall_time, tick_time, dt)

end

function Update(wall_time, game_time, dt)

end


function OnKeyDown(symbol, modifier_bits)
    if symbol == wdk.Keys.Escape then 
        local scene = ClassLib:FindSceneClassByName('My Scene')
        Game:PlayScene(scene)
    end
end
