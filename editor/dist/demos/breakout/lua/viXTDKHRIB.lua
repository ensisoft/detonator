-- Entity 'Brick' script.
-- This script will be called for every instance of 'Brick'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.
--
--
function DropPowerup(brick)
    -- drop a powerup if any 
    local node = brick:GetNode(0)
    local scene = brick:GetScene()
    -- lookup for a powerup entity within the brick's vicinity
    local nodes = scene:QuerySpatialNodes(node:GetTranslation(), 'All')
    local powerup_node = nodes:Find(function(node)
        local entity = node:GetEntity()
        local klass = entity:GetClassName()
        if string.find(klass, 'Powerup') or string.find(klass, 'Points') then
            return true
        end
        return false
    end)

    if powerup_node == nil then
        return
    end

    Game:DebugPrint('Brick powerup ' .. powerup_node:GetClassName())

    local powerup = powerup_node:GetEntity()
    powerup:SetVisible(true)
    powerup:SetLayer(2)

    local powerup_body = powerup_node:GetRigidBody()
    local spatial_node = powerup_node:GetSpatialNode()
    spatial_node:Enable(false)
    powerup_body:Enable(true)
end

function PopParticles(brick)
    local brick_node = brick:GetNode(0)
    local brick_pos = brick_node:GetTranslation()
    local brick_color = brick.color

    Scene:SpawnEntity('Brick Particles', {
        pos = brick_pos,
        async = true,
        layer = 2,
        vars = {
            color = brick_color
        }
    })
end

-- Called when the game play begins for an entity in the scene.
function BeginPlay(brick, scene, map)

end

-- Called when the game play ends for an entity in the scene.
function EndPlay(brick, scene, map)

end

-- Called on every low frequency game tick.
function Tick(brick, game_time, dt)

end

-- Called on every iteration of game loop.
function Update(brick, game_time, dt)

end

-- Called on collision events with other objects.
function OnBeginContact(brick, node, other, other_node)

end

-- Called on collision events with other objects.
function OnEndContact(brick, node, other, other_node)

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

function BallHit(brick, ball)
    if brick.immortal and not ball.heavy then
        return
    end

    brick.hit_count = brick.hit_count - 1

    -- heavy ball smashes the brick at one go
    if ball.heavy then
        brick.hit_count = 0
    end

    if brick.hit_count > 0 then
        return
    end
    local class = brick:GetClass()
    Audio:PlaySoundEffect('Kill Brick', 0)
    local kill_brick = game.GameEvent:new()
    kill_brick.from = 'brick'
    kill_brick.to = 'game'
    kill_brick.message = 'kill-brick'
    kill_brick.value = class.hit_count * 100
    Game:PostEvent(kill_brick)

    brick:Die()

    DropPowerup(brick)
    PopParticles(brick)
end

