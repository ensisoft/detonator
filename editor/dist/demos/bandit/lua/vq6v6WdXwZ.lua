-- Entity 'Player' script.
-- This script will be called for every instance.

local Scene = nil

-- world vectors
WorldUp    = glm.vec2:new(0.0, -1.0)
WorldRight = glm.vec2:new(1.0, 0.0)

function TurnPlayerLeft(player)
    local max_nodes = player:GetNumNodes()
    for i=0, max_nodes-1, 1 do
        local node = player:GetNode(i)
        local draw = node:GetDrawable()
        if draw ~= nil then
           draw:SetFlag('FlipHorizontally', true)
        end
    end
end

function TurnPlayerRight(player)
    local max_nodes = player:GetNumNodes()
    for i=0, max_nodes-1, 1 do
        local node = player:GetNode(i)
        local draw = node:GetDrawable()
        if draw ~= nil then
            draw:SetFlag('FlipHorizontally', false)
        end
    end
end


function BeginPlay(player, scene)
    Scene = scene
end

function Tick(player, game_time, dt)
end

function Update(player, game_time, dt)

end

function OnBeginContact(player, node, other, other_node)
    local what = other:GetClassName()
    if what ~= 'Coin' then
        return
    end
    local coins = player.coins
    coins = coins + 1
    player.coins = coins

    Game:DebugPrint('Coin collected!')
    Scene:KillEntity(other)
end

function OnEndContact(player, node, other, other_node)

end

function OnKeyDown(player, symbol, modifier_bits)
    local body  = player:FindNodeByClassName('Body')
    local jump  = player.jump
    local slide = player.slide

    -- control player by applying an impulse to the
    -- player chracters body
    if symbol == wdk.Keys.ArrowRight then
        TurnPlayerRight(player)
        Physics:ApplyImpulseToCenter(body, WorldRight * slide)
        player:PlayAnimationByName('Walk')
    elseif symbol == wdk.Keys.ArrowLeft then
        TurnPlayerLeft(player)
        Physics:ApplyImpulseToCenter(body, WorldRight * slide * -1.0)
        player:PlayAnimationByName('Walk')
    elseif symbol == wdk.Keys.ArrowUp then
        Physics:ApplyImpulseToCenter(body, WorldUp * jump)
        player:PlayAnimationByName('Jump')
    elseif symbol == wdk.Keys.Space then
        player:PlayAnimationByName('Attack')
    end
end