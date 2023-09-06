-- Entity 'Enemy' script.
-- This script will be called for every instance of 'Enemy'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.
require('common')

local tick_counter = 0

function BroadcastDeath(ship)
    local event = game.GameEvent:new()
    event.from = 'enemy'
    event.message = 'dead'
    event.to = 'game'
    event.value = ship.score
    Game:PostEvent(event)
end

function RocketBlast(ship, mine)
    local ship_node = ship:GetNode(0)
    local ship_pos = ship_node:GetTranslation()
    local mine_node = mine:GetNode(0)
    local mine_pos = mine_node:GetTranslation()
    local dist = glm.length(mine_pos - ship_pos)

    local blast_force = 1.0 - (dist / 550.0)
    blast_force = blast_force * 0.6
    blast_force = blast_force + util.rand(0.0, 0.4)

    local blast_vector = glm.normalize(ship_pos - mine_pos)
    ship.velocity = blast_vector * blast_force * 1500.0
    ship.rotation = util.rand(-15.0, 29.4)
    ship:DieLater(util.rand(0.1, 0.6))
end

function BulletHit(ship, bullet)
    if bullet.player == false then
        return
    end

    ship:Die()
    bullet:Die()
end

-- Called when the game play begins for a scene.
function BeginPlay(ship, scene, map)

end

-- Called on every low frequency game tick.
function Tick(ship, game_time, dt)
    -- launch a few bullets towards the player.

    if math.fmod(tick_counter, 3) == 0 then
        local weapon = ship:FindNodeByClassName('Weapon')
        local matrix = Scene:FindEntityNodeTransform(ship, weapon)
        local args = game.EntityArgs:new()
        args.class = ClassLib:FindEntityClassByName('Bullet/Enemy')
        args.position = util.GetTranslationFromMatrix(matrix)
        args.name = 'blue bullet'
        local bullet = Scene:SpawnEntity(args, true)
        bullet.velocity = ship.velocity.y + 100
    end
    tick_counter = tick_counter + 1
end

-- Called on every iteration of game loop.
function Update(ship, game_time, dt)
    -- move ship forward
    local ship_body = ship:GetNode(0)
    local ship_pos = ship_body:GetTranslation()
    local ship_rot = ship_body:GetRotation()

    if ship:IsDying() then
        if ship_pos.y > 500.0 then
            return
        end

        SpawnExplosion(ship_pos, 'Enemy Explosion')
        SpawnScore(ship_pos, ship.score)
        BroadcastDeath(ship)
    end

    -- update x,y position
    ship_pos.x = ship_pos.x + dt * ship.velocity.x
    ship_pos.y = ship_pos.y + dt * ship.velocity.y

    local velocity = ship.velocity
    -- change direction at the boundaries
    if ship_pos.x >= 550 then
        velocity.x = velocity.x * -1.0
        ship_pos.x = 550
    elseif ship_pos.x <= -550 then
        velocity.x = velocity.x * -1.0
        ship_pos.x = -550
    end
    ship.velocity = velocity
    ship_body:SetTranslation(ship_pos)

    ship_rot = ship_rot + ship.rotation * dt

    ship_body:SetRotation(ship_rot)

    -- if it reached the edge of space then kill it.
    if ship_pos.y > 500 then
        ship:Die()
    end
end

