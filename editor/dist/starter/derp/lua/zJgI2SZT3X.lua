-- Entity 'FlappyBird' script.
-- This script will be called for every instance of 'FlappyBird'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.
-- Called when the game play begins for the entity in the scene.
function BeginPlay(flappybird, scene, map)

end

-- Called when the game play ends for the entity in the scene.
function EndPlay(flappybird, scene, map)

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
        Audio:PlaySoundEffect('Boing', 0)
    end
end

-- Called on collision events with other objects.
function OnEndContact(flappybird, node, other, other_node)
end

local space_counter = 0

-- key down event handler.
-- Only ever called if entity's 'Receive key events' flag is set.
function OnKeyDown(flappybird, symbol, modifier_bits)
    if symbol == wdk.Keys.Space then
        space_counter = space_counter + 1

        local body = flappybird:GetNode(0)
        Physics:ApplyImpulseToCenter(body, glm.vec2:new(0.1, -4.0))

        if space_counter == 4 then
            local text = flappybird:GetNode(3)
            local txt = text:GetTextItem()
            txt:SetFlag('VisibleInGame', false)
        end
    end
end

-- key up event handler.
-- Only ever called if entity's 'Receive key events' flag is set.
function OnKeyUp(flappybird, symbol, modifier_bits)
end

-- mouse button press event handler.
-- Only ever called if entity's 'receive mouse events' flag is set.
function OnMousePress(flappybird, mouse)
end

-- mouse button release event handler.
-- Only ever called if entity's 'receive mouse events' flag is set.
function OnMouseMove(flappybird, mouse)
end

-- mouse move event handler.
-- Only ever called if entity's 'receive mouse events' flag is set.
function OnMouseMove(flappybird, mouse)
end
