local tick_count = 0

-- Entity 'Firework' script.
-- This script will be called for every instance of 'Firework'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.
-- Called when the game play begins for an entity in the scene.
function BeginPlay(firework, scene)
end

-- Called when the game play ends for an entity in the scene.
function EndPlay(firework, scene)
end

-- Called on every low frequency game tick.
function Tick(firework, game_time, dt)
end

-- Called on every iteration of the game loop.
function Update(firework, game_time, dt)
    if tick_count % 7 == 0 then
        local index = util.Random(0, 1)
        local launchbox = firework:FindNodeByClassName('LaunchBox' .. tostring(index))
        local scene = firework:GetScene()

        local args = game.EntityArgs:new()
        args.class = ClassLib:FindEntityClassByName('Firework Rocket')
        args.position = scene:MapPointFromEntityNode(firework, launchbox, glm.vec2:new(0, 0))
        scene:SpawnEntity(args, true)
    end
    tick_count = tick_count + 1
end

-- Called on every iteration of the game loop game
-- after *all* entities have been updated.
function PostUpdate(firework, game_time)
end

-- Called on collision events with other objects.
function OnBeginContact(firework, node, other, other_node)
end

-- Called on collision events with other objects.
function OnEndContact(firework, node, other, other_node)
end

-- Called on key down events.
function OnKeyDown(firework, symbol, modifier_bits)
end

-- Called on key up events.
function OnKeyUp(firework, symbol, modifier_bits)
end

-- Called on mouse button press events.
function OnMousePress(firework, mouse)
end

-- Called on mouse button release events.
function OnMouseRelease(firework, mouse)
end

-- Called on mouse move events.
function OnMouseMove(firework, mouse)
end

-- Called on game events.
function OnGameEvent(firework, event)
end

-- Called on animation finished events.
function OnAnimationFinished(firework, animation)
end

-- Called on timer events.
function OnTimer(firework, timer, jitter)
end

-- Called on posted entity events.
function OnEvent(firework, event)
end

