-- Entity 'Ship2' script.
-- This script will be called for every instance of 'Ship2'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.
local tick_counter = 0

BulletAction = {
    NoPanic = 'NoPanic',
    DodgeLeft = 'DodgeLeft',
    DodgeRight = 'DodgeRight'
}

function CheckBulletAction(ship_pos, bullet_pos)
    local ship_vertical = ship_pos.x
    local bullet_vertical = bullet_pos.x
    -- whats the horizontal safety margin on our vertical
    -- path vs. bullet's vertical path ?
    local margin = bullet_vertical - ship_vertical
    if margin >= 15 or margin <= -15 then
        return BulletAction.NoPanic
    end
    local distance = bullet_pos.y - ship_pos.y
    -- if it's too far ahead (or behind us) it doesn't matter
    if distance >= 500 or distance <= 0 then
        return BulletAction.NoPanic
    end

    if margin > 0 then
        return BulletAction.DodgeLeft
    end
    return BulletAction.DodgeRight
end

-- Called when the game play begins for a scene.
function BeginPlay(ship2, scene, map)

end

-- Called on every low frequency game tick.
function Tick(ship2, game_time, dt)
    -- launch a few bullets towards the player.

    if math.fmod(tick_counter, 3) == 0 then
        local weapon = ship2:FindNodeByClassName('Weapon')
        local matrix = Scene:FindEntityNodeTransform(ship2, weapon)
        local args = game.EntityArgs:new()
        args.class = ClassLib:FindEntityClassByName('BlueBullet')
        args.position = util.GetTranslationFromMatrix(matrix)
        args.name = 'blue bullet'
        local bullet = Scene:SpawnEntity(args, true)
        bullet.velocity = ship2.velocity.y + 100
    end
    tick_counter = tick_counter + 1
end

-- Called on every iteration of game loop.
function Update(ship2, game_time, dt)
    -- move ship forward
    local ship_body = ship2:FindNodeByClassName('Body')
    local ship_pos = ship_body:GetTranslation()
    ship_pos.x = ship_pos.x + dt * ship2.velocity.x
    ship_pos.y = ship_pos.y + dt * ship2.velocity.y
    local velocity = ship2.velocity
    -- change direction at the boundaries
    if ship_pos.x >= 550 then
        velocity.x = velocity.x * -1.0
        ship_pos.x = 550
        -- ship2:PlayAnimationByName('Turn Exhaust Right')
    elseif ship_pos.x <= -550 then
        velocity.x = velocity.x * -1.0
        ship_pos.x = -550
        -- ship2:PlayAnimationByName('Turn Exhaust Left')
    end
    ship2.velocity = velocity
    ship_body:SetTranslation(ship_pos)
    -- if it reached the edge of space then kill it.
    if ship_pos.y > 500 then
        Scene:KillEntity(ship2)
    end
end

-- Called on collision events with other objects.
function OnBeginContact(ship2, node, other, other_node)
end

-- Called on collision events with other objects.
function OnEndContact(ship2, node, other, other_node)
end

