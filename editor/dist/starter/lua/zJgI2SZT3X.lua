-- Entity 'FlappyBird' script.

-- This script will be called for every instance of 'FlappyBird'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.

-- Called when the game play begins for the entity in the scene.
function BeginPlay(flappybird, scene)

end

-- Called when the game play ends for the entity in the scene.
function EndPlay(flappybird, scene)

end

-- Called on every low frequency game tick.
function Tick(flappybird, game_time, dt)

end

-- Called on every iteration of game loop.
function Update(flappybird, game_time, dt)

end

-- Called on collision events with other objects.
function OnBeginContact(flappybird, node, other, other_node)
    if other:GetClassName() == 'Spike' then
        flappybird:PlayAnimationByName('Hurt')
    end
end

-- Called on collision events with other objects.
function OnEndContact(flappybird, node, other, other_node)
end

-- key event handler
function OnKeyDown(flappybird, symbol, modifier_bits)
    if symbol == wdk.Keys.Space then
        local body = flappybird:FindNodeByClassName('Body')
        Physics:ApplyImpulseToCenter(body, glm.vec2:new(0.1, -4.0))
    end
end
