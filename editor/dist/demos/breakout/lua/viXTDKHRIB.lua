-- Entity 'Green Brick' script.

-- This script will be called for every instance of 'Green Brick'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.

-- Called when the game play begins for an entity in the scene.
function BeginPlay(brick, scene)

end

-- Called when the game play ends for an entity in the scene.
function EndPlay(brick, scene)

end

-- Called on every low frequency game tick.
function Tick(brick, game_time, dt)

end

-- Called on every iteration of game loop.
function Update(brick, game_time, dt)

end

-- Called on collision events with other objects.
function OnBeginContact(brick, node, other, other_node)
    if other:GetClassName() == 'Ball' then 
        if brick.immortal then 
            return     
        end
        brick.hit_count = brick.hit_count - 1
        if brick.hit_count == 0 then 
            local class = brick:GetClass()
            Scene:KillEntity(brick)
            Audio:PlaySoundEffect('Kill Brick', 0)
            local kill_brick = game.GameEvent:new()
            kill_brick.from = 'brick'
            kill_brick.to   = 'game'
            kill_brick.message = 'kill-brick'
            kill_brick.value   = class.hit_count * 100
            Game:PostEvent(kill_brick)            
        end
    end
end

-- Called on collision events with other objects.
function OnEndContact(brick, node, other, other_node)
    if other:GetClassName() == 'Ball' then 
        local rigid_body = other_node:GetRigidBody()
        local velocity   = rigid_body:GetLinearVelocity()
        local direction  = velocity:normalize()
        local speed = velocity:length()
        if direction.y < 0.0 and speed < 15.0 then 
            Physics:ApplyImpulseToCenter(other_node, direction * 0.1)
        end
    end
end

-- Called on key down events.
function OnKeyDown(brick, symbol, modifier_bits)
end

-- Called on key up events.
function OnKeyUp(brick, symbol, modifier_bits)
end

-- Called on mouse button press events.
function OnMousePress(brick, mouse)
end

-- Called on mouse button release events.
function OnMouseRelease(brick, mouse)
end

-- Called on mouse move events.
function OnMouseMove(brick, mouse)
end

