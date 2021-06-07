
function LoadGame()
    local scene = ClassLib:FindSceneClassByName('My Scene')
    Game:Play(scene)
    Game:SetViewport(game.FRect:new(-500.0, -800.0, 1000, 800))
end

function BeginPlay(scene)
    Game:DebugPrint('Press Space to fly the bird!')
end

function EndPlay(scene)

end

function Tick(game_time, dt)

end

function Update(game_time, dt)

end


function OnKeyDown(symbol, modifier_bits)
    if symbol == wdk.Keys.Escape then
        local scene = ClassLib:FindSceneClassByName('My Scene')
        Game:Play(scene)
    end
end
