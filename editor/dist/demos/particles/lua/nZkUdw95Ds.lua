-- Entity 'My Entity' script.
-- This script will be called for every instance of 'My Entity'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.
-- Called when the game play begins for an entity in the scene.
function BeginPlay(my_entity, scene, map)
    local rocket = my_entity:FindNodeByClassName('Rocket')
    local body = rocket:GetRigidBody()
    local xdir = util.Random(-0.1, 0.1)
    local ydir = util.Random(-0.45, -0.4)

    body:ApplyImpulse(xdir, ydir)

    local direction = glm.vec2:new(xdir, ydir)
    local angle = glm.dot(glm.vec2:new(1.0, 0.0), direction:normalize())
    rocket:SetRotation(angle)

end

-- Called when the game play ends for an entity in the scene.
function EndPlay(my_entity, scene, map)

end

-- Called on every low frequency game tick.
function Tick(my_entity, game_time, dt)
end

-- Called on every iteration of the game loop.
function Update(my_entity, game_time, dt)
    if my_entity:HasBeenKilled() then
        return
    end

    local rocket = my_entity:FindNodeByClassName('Rocket')
    local body = rocket:GetRigidBody()
    local velocity = body:GetLinearVelocity()

    local explosions = {}
    explosions[0] = 'Fireworks Explosion Red'
    explosions[1] = 'Fireworks Explosion Green'
    explosions[2] = 'Fireworks Explosion Blue'

    local index = util.Random(0, 2)
    local explosion = explosions[index]

    -- when the rocket reaches the apex and the velocity becomes
    -- positive on the up vector kill the entity
    if velocity.y > 0.0 then
        local args = game.EntityArgs:new()
        args.class = ClassLib:FindEntityClassByName(explosion)
        args.position = Scene:MapPointFromEntityNode(my_entity, rocket,
                                                     glm.vec2:new(0.0, 0.0))
        Scene:SpawnEntity(args, true)
        my_entity:Die()
    end
end

-- Called on every iteration of the game loop game
-- after *all* entities have been updated.
function PostUpdate(my_entity, game_time)
end

-- Called on collision events with other objects.
function OnBeginContact(my_entity, node, other, other_node)
end

-- Called on collision events with other objects.
function OnEndContact(my_entity, node, other, other_node)
end

-- Called on key down events.
function OnKeyDown(my_entity, symbol, modifier_bits)
end

-- Called on key up events.
function OnKeyUp(my_entity, symbol, modifier_bits)
end

-- Called on mouse button press events.
function OnMousePress(my_entity, mouse)
end

-- Called on mouse button release events.
function OnMouseRelease(my_entity, mouse)
end

-- Called on mouse move events.
function OnMouseMove(my_entity, mouse)
end

-- Called on game events.
function OnGameEvent(my_entity, event)
end

-- Called on animation finished events.
function OnAnimationFinished(my_entity, animation)
end

-- Called on timer events.
function OnTimer(my_entity, timer, jitter)
end

-- Called on posted entity events.
function OnEvent(my_entity, event)
end

