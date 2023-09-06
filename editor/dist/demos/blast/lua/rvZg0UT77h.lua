-- Entity 'player' script.
-- This script will be called for every instance of 'player'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.
--
-- LuaFormatter off
-- 

local time_to_fire_weapon = 0
local mine_ready = true
local mine_load_time = 0.0
local health = 5

local _keydown_bits = 0

local player_ready = false

local hori_velocity = 0
local vert_velocity = 0

_Keys = {
    Left  = 0x1,
    Right = 0x2,
    Up    = 0x4,
    Down  = 0x8,
    Fire  = 0x10
}

-- LuaFormatter on

function _TestKeyDown(key)
    if game.OS == 'WASM' then
        return _keydown_bits & key ~= 0
    end
    local val = 0
    if key == _Keys.Left then
        val = wdk.Keys.ArrowLeft
    elseif key == _Keys.Right then
        val = wdk.Keys.ArrowRight
    elseif key == _Keys.Up then
        val = wdk.Keys.ArrowUp
    elseif key == _Keys.Down then
        val = wdk.Keys.ArrowDown
    elseif key == _Keys.Fire then
        val = wdk.Keys.Space
    end
    return wdk.TestKeyDown(val)
end

function BulletHit(player, bullet)
    if bullet.player then
        return
    end

    if player:IsDying() then
        return
    end

    bullet:Die()

    local scene = player:GetScene()
    local health_entity = scene:FindEntityByInstanceName('health')
    CallMethod(health_entity, 'UpdateHealth', health)

    health = health - 1
    if health > 0 then
        return
    end

    local node = player:GetNode(0)
    local pos = node:GetTranslation()
    SpawnExplosion(pos, 'Player Explosion')

    player:Die()

    Game:DebugPrint('You were hit by a bullet!')

end

function AsteroidHit(player, asteroid)

    if player:IsDying() then
        return
    end

    local node = player:GetNode(0)
    local pos = node:GetTranslation()
    SpawnExplosion(pos, 'Player Explosion')

    player:Die()

    Game:DebugPrint('You were hit by an asteroid!')
end

function FireRocket(player)
    local weapon = player:FindNodeByClassName('Mine')
    local matrix = Scene:FindEntityNodeTransform(player, weapon)

    -- setup the args for spawning a new bullet.
    local args = game.EntityArgs:new()
    args.class = ClassLib:FindEntityClassByName('Rocket')
    args.position = util.GetTranslationFromMatrix(matrix)
    args.name = 'rocket'
    args.logging = false
    Scene:SpawnEntity(args, true)
end

function FireGun(player, gun_name, ammo_name)
    -- what's the current ship's weapon world position ?
    local weapon = player:FindNodeByClassName(gun_name)
    local matrix = Scene:FindEntityNodeTransform(player, weapon)

    -- setup the args for spawning a new bullet.
    local args = game.EntityArgs:new()
    args.class = ClassLib:FindEntityClassByName(ammo_name)
    args.position = util.GetTranslationFromMatrix(matrix)
    args.name = ammo_name
    args.logging = false
    Scene:SpawnEntity(args, true)

    local emitter_node = player:FindNodeByClassName(gun_name ..
                                                        '/ParticleEmitter')
    if emitter_node == nil then
        return
    end

    local emitter_draw = emitter_node:GetDrawable()
    emitter_draw:Command('EmitParticles', {
        count = 5
    })

end

-- Called when the game play begins for an entity in scene.
function BeginPlay(player, scene, map)
    player:PlayAnimationByName('Fly In')
end

-- called when the game play ends for an entity in the scene.
function EndPlay(player, scene, map)
    local game_over = game.GameEvent:new()
    game_over.from = 'player'
    game_over.to = 'game'
    game_over.message = 'dead'
    Game:PostEvent(game_over)
end

-- Called on every low frequency game tick.
function Tick(player, game_time, dt)
end

function ClipVelocity(velo, min, max)
    if velo > max then
        velo = max
    elseif velo < min then
        velo = min
    end
    return velo
end

-- Called on every iteration of game loop.
function Update(player, game_time, dt)

    if game_time > 1.0 then
        player_ready = true
    end

    if player_ready == false then
        return
    end

    if mine_ready == false then
        local status = Scene:FindEntityByInstanceName('Mine Status')
        local node = status:FindNodeByClassName('Text')
        local text = node:GetTextItem()
        text:SetText(string.format('%.2f', mine_load_time))
        text:SetFlag('BlinkText', false)

        mine_load_time = mine_load_time - dt
        if mine_load_time <= 0.0 then
            mine_ready = true
            text:SetText('Ready!')
            text:SetFlag('BlinkText', true)
        end
    end

    if _TestKeyDown(_Keys.Left) then
        if hori_velocity >= 0 then
            hori_velocity = -200
        elseif hori_velocity > -600 then
            hori_velocity = hori_velocity - 100
        end
    elseif _TestKeyDown(_Keys.Right) then
        if hori_velocity <= 0 then
            hori_velocity = 200
        elseif hori_velocity < 600 then
            hori_velocity = hori_velocity + 100
        end
    else
        hori_velocity = 0
    end

    if _TestKeyDown(_Keys.Up) then
        if vert_velocity >= 0 then
            vert_velocity = -200
        elseif vert_velocity > -600 then
            vert_velocity = vert_velocity - 100
        end
    elseif _TestKeyDown(_Keys.Down) then
        if vert_velocity <= 0 then
            vert_velocity = 200
        elseif vert_velocity < 600 then
            vert_velocity = vert_velocity + 100
        end
    else
        vert_velocity = 0
    end
    hori_velocity = ClipVelocity(hori_velocity, -600, 600)
    vert_velocity = ClipVelocity(vert_velocity, -600, 600)

    local body = player:GetNode(0)
    local pos = body:GetTranslation()
    local x = pos.x + dt * hori_velocity
    local y = pos.y + dt * vert_velocity
    if x >= -550 and x <= 550 then
        pos.x = x
    end
    if y >= 150 and y <= 320 then
        pos.y = y
    end
    body:SetTranslation(pos)

    -- shift the background slightly to the inverse direction
    -- to create a little 'motion' effect 
    local background = Scene:FindEntityByInstanceName('Background')
    if background ~= nil then
        local background_node = background:GetNode(0)
        local horizontal_shift = x / 550.0 -- so between -1.0 to 1.0
        background_node:SetTranslation(0.0 - horizontal_shift * 100.0, 0.0)
    end

    -- fire the weapon if some key presses were detected!
    if time_to_fire_weapon == 0 then
        if _TestKeyDown(_Keys.Fire) then
            FireGun(player, 'Gun/Left', 'Bullet/Player')
            FireGun(player, 'Gun/Right', 'Bullet/Player')
            if not player:IsAnimating() then
                player:PlayAnimationByName('Fire Gun')
            end
            Audio:PlaySoundEffect('Player Weapon')
        end
        -- limit the firing rate
        time_to_fire_weapon = 6
        return
    end
    time_to_fire_weapon = time_to_fire_weapon - 1
end

-- low frequency keyboard event callback handler.
-- keeping the key pressed down will call repeatedly
-- OnKeyDown followed by OnKeyUp until another key is
-- pressed at which point the key events for the original
-- key combo stop arriving.
function OnKeyDown(player, symbol, modifier_bits)
    if player_ready == false then
        return
    end

    if symbol == wdk.Keys.Key1 then
        if mine_ready then
            FireRocket(player, 'Rocket')
            mine_ready = false
            mine_load_time = 5.0
        else
            Audio:PlaySoundEffect('Rocket Fail')
        end
    elseif symbol == wdk.Keys.ArrowLeft then
        _keydown_bits = _keydown_bits | _Keys.Left
    elseif symbol == wdk.Keys.ArrowRight then
        _keydown_bits = _keydown_bits | _Keys.Right
    elseif symbol == wdk.Keys.ArrowUp then
        _keydown_bits = _keydown_bits | _Keys.Up
    elseif symbol == wdk.Keys.ArrowDown then
        _keydown_bits = _keydown_bits | _Keys.Down
    elseif symbol == wdk.Keys.Space then
        _keydown_bits = _keydown_bits | _Keys.Fire
    end
end

function OnKeyUp(player, symbol, modifier_bits)
    if player_ready == false then
        return
    end

    if symbol == wdk.Keys.ArrowLeft then
        _keydown_bits = _keydown_bits & ~_Keys.Left
    elseif symbol == wdk.Keys.ArrowRight then
        _keydown_bits = _keydown_bits & ~_Keys.Right
    elseif symbol == wdk.Keys.ArrowUp then
        _keydown_bits = _keydown_bits & ~_Keys.Up
    elseif symbol == wdk.Keys.ArrowDown then
        _keydown_bits = _keydown_bits & ~_Keys.Down
    elseif symbol == wdk.Keys.Space then
        _keydown_bits = _keydown_bits & ~_Keys.Fire
    end
end

function OnAnimationFinished(player, animation)
    local name = animation:GetClassName()

    -- enable player ready after the initial animation to fly into the scene finishes
    if name == 'Fly In' then
        player_ready = true

        local node = player:FindNodeByClassName('Message')
        local text = node:GetTextItem()
        text:SetFlag('VisibleInGame', false)
    end
end

