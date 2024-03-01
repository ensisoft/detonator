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

function IndicateBulletHit(ship)
    local smoke_emitter = ship:FindNodeByClassName('Hit Smoke')
    local flash_emitter = ship:FindNodeByClassName('Hit Flash')

    local drawable = nil
    if smoke_emitter ~= nil then
        drawable = smoke_emitter:GetDrawable()
        drawable:Command('EmitParticles', {
            count = 80
        })
    end

    if flash_emitter ~= nil then
        drawable = flash_emitter:GetDrawable()
        drawable:Command('EmitParticles', {
            count = 80
        })
    end
end

function BulletHit(ship, bullet)
    if bullet.player == false then
        return
    end

    bullet:Die()

    if ship.type == 'basic' then
        ship:Die()
    end

    if ship.type == 'advanced' then
        local node = ship:FindNodeByClassName('Health')
        local size = node:GetSize()
        local width = size.x
        local damage = 5.0
        width = width - damage
        if width < 0.0 then
            ship:Die()
            Audio:PlaySoundEffect('Laser_00')
            return
        elseif width < 20.0 then
            local smoke_node = ship:FindNodeByClassName('Engine Smoke')
            local smoke_node_draw = smoke_node:GetDrawable()
            smoke_node_draw:SetFlag('VisibleInGame', true)
            smoke_node_draw:SetFlag('UpdateDrawable', true)
            smoke_node_draw:SetFlag('UpdateMaterial', true)
        end
        IndicateBulletHit(ship)

        node:SetSize(width, 5.0)
    end

end

function FireAdvancedWeapon(ship, weapon, bullet)
    local weapon = ship:FindNodeByClassName(weapon)
    local matrix = Scene:FindEntityNodeTransform(ship, weapon)
    local args = game.EntityArgs:new()
    args.class = ClassLib:FindEntityClassByName(bullet)
    args.position = util.GetTranslationFromMatrix(matrix)
    args.name = 'bullet'
    args.async = true
    Scene:SpawnEntity(args, true)
end

function BasicWeaponry(ship)
    if ship.type ~= 'basic' then
        return
    end

    local weapon = ship:FindNodeByClassName('Weapon')
    local matrix = Scene:FindEntityNodeTransform(ship, weapon)
    Scene:SpawnEntity('Bullet/Enemy/Basic', {
        pos = util.GetTranslationFromMatrix(matrix),
        name = 'blue bullet',
        async = true,
        vars = {
            velocity = ship.velocity.y + 100.0
        }
    })
end

function AdvancedWeaponry(ship)
    if ship.type ~= 'advanced' then
        return
    end
    FireAdvancedWeapon(ship, 'Weapon0', 'Bullet/Enemy/Advanced')
    FireAdvancedWeapon(ship, 'Weapon1', 'Bullet/Enemy/Advanced')
end

-- Called when the game play begins for a scene.
function BeginPlay(ship, scene, map)
    if ship.type == 'advanced' then
        Audio:PlaySoundEffect('Alarm_Loop_01')
    end
end

-- Called on every low frequency game tick.
function Tick(ship, game_time, dt)
    -- launch a few bullets towards the player.
    if ship:IsDying() then
        return
    end

    if math.fmod(tick_counter, 3) ~= 0 then
        BasicWeaponry(ship)
    end

    if math.fmod(tick_counter, 6) ~= 0 then
        AdvancedWeaponry(ship)
    end
    if math.fmod(tick_counter, 2) ~= 0 then
        AdvancedWeaponry(ship)
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

        SpawnExplosion(ship_pos, 'Enemy/Explosion/' .. ship.type)
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

function OnKeyDown(ship, symbol, modifier_bits)
    if not PreviewMode then
        return
    end

    if symbol == wdk.Keys.Space then
        BasicWeaponry(ship)
        AdvancedWeaponry(ship)
    elseif symbol == wdk.Keys.Key1 then
        IndicateBulletHit(ship)
    end
end
