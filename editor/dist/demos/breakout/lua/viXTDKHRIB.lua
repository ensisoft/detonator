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
    powerup:SetRenderLayer(2)

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
        brick:PlayAnimation('Nope')
        return
    end

    local destroyed = false
    local hits_needed = 1

    brick.hit_count = brick.hit_count + 1

    local klass = brick:GetClassName()

    if string.find(klass, 'Blue') then
        hits_needed = 1
    elseif string.find(klass, 'Green') then
        hits_needed = 1
    elseif string.find(klass, 'Yellow') then
        hits_needed = 2
    elseif string.find(klass, 'Red') then
        hits_needed = 3
    elseif string.find(klass, 'Purple') then
        hits_needed = 4
    end

    -- heavy ball smashes the brick at one go
    if ball.heavy then
        destroyed = true
    elseif brick.hit_count == hits_needed then
        destroyed = true
    end

    if not destroyed then
        local brick_node = brick:GetNode(0)
        local brick_draw = brick_node:GetDrawable()
        brick_draw:SetUniform('kBaseColor',
                              base.Color4f:new(255, 255, 255, 255) * 0.8 ^
                                  brick.hit_count)

        return
    end

    Audio:PlaySoundEffect('Kill Brick', 0)
    local kill_brick = game.GameEvent:new()
    kill_brick.from = 'brick'
    kill_brick.to = 'game'
    kill_brick.message = 'kill-brick'
    kill_brick.value = hits_needed * 100
    Game:PostEvent(kill_brick)

    brick:Die()

    DropPowerup(brick)
    PopParticles(brick)
end

