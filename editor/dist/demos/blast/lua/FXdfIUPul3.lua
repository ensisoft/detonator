-- Entity 'Enemy' script.
-- This script will be called for every instance of 'Enemy'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.
require('common')

local tick_counter = 0

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

    -- ship death is handled in the native c++ code in the
    -- update function. this function will also move to native 
    -- code once the c++ game runtime features become more
    -- developed.

    if ship.type == 'basic' then
        ship:Die()
    end

    if ship.type == 'intermediate' then
        local hit_count = ship.hit_count
        hit_count = hit_count + 1
        if hit_count == 20 then
            ship:Die()
        end
        ship.hit_count = hit_count
        ship:PlayAnimation('Indicate Damage')
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

function FireWeapon(ship, weapon, bullet, bullet_speed)
    local weapon = ship:FindNode(weapon)
    local matrix = Scene:FindEntityNodeTransform(ship, weapon)
    local position = util.GetTranslationFromMatrix(matrix)

    Scene:SpawnEntity(bullet, {
        async = true,
        pos = position,
        vars = {
            velocity = bullet_speed
        }
    })

end

function BasicWeaponry(ship)
    if ship.type ~= 'basic' then
        return
    end

    local bullet_velocity = ship.velocity.y + 100.0
    FireWeapon(ship, 'Weapon', 'Bullet/Enemy/Basic', bullet_velocity)
end

function IntermediateWeaponry(ship)
    if ship.type ~= 'intermediate' then
        return
    end

    FireWeapon(ship, 'Weapon0', 'Bullet/Enemy/Advanced', 500.0)
    FireWeapon(ship, 'Weapon1', 'Bullet/Enemy/Advanced', 500.0)
    Audio:PlaySoundEffect('Fire 6')

end

function AdvancedWeaponry(ship)
    if ship.type ~= 'advanced' then
        return
    end
    FireWeapon(ship, 'Weapon0', 'Bullet/Enemy/Advanced', 1000.0)
    FireWeapon(ship, 'Weapon1', 'Bullet/Enemy/Advanced', 1000.0)
    ship:PlayAnimation('Fire Weapons')
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
        IntermediateWeaponry(ship)
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
    -- native code implements
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
