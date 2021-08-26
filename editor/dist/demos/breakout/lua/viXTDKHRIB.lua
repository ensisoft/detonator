-- Entity 'Green Brick' script.

-- This script will be called for every instance of 'Green Brick'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.

-- Called when the game play begins for an entity in the scene.
function BeginPlay(green_brick, scene)

end

-- Called when the game play ends for an entity in the scene.
function EndPlay(green_brick, scene)

end

-- Called on every low frequency game tick.
function Tick(green_brick, game_time, dt)

end

-- Called on every iteration of game loop.
function Update(green_brick, game_time, dt)

end

-- Called on collision events with other objects.
function OnBeginContact(green_brick, node, other, other_node)
    if other:GetClassName() == 'Ball' then 
        if green_brick.immortal then 
            return     
        end
        green_brick.hit_count = green_brick.hit_count - 1
        if green_brick.hit_count == 0 then 
            Scene:KillEntity(green_brick)
            Audio:PlaySoundEffect('Kill Brick', 0)
        end
    end
end

-- Called on collision events with other objects.
function OnEndContact(green_brick, node, other, other_node)
    if other:GetClassName() == 'Ball' then 
        local rigid_body = other_node:GetRigidBody()
        local velocity   = rigid_body:GetLinearVelocity()
        local direction  = velocity:normalize()
        if direction.y < 0.0 then 
            Physics:ApplyImpulseToCenter(other_node, direction * 0.15)
        end
    end
end

-- Called on key down events.
function OnKeyDown(green_brick, symbol, modifier_bits)
end

-- Called on key up events.
function OnKeyUp(green_brick, symbol, modifier_bits)
end

-- Called on mouse button press events.
function OnMousePress(green_brick, mouse)
end

-- Called on mouse button release events.
function OnMouseRelease(green_brick, mouse)
end

-- Called on mouse move events.
function OnMouseMove(green_brick, mouse)
end

