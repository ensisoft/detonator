-- Entity 'player' script.

-- This script will be called for every instance of 'player'
-- in the scene during gameplay.
-- You're free to delete functions you don't need.


local time_to_fire_weapon = 0

function FireWeapon(player, ammo_name)
    -- what's the current ship's weapon world position ?
    local weapon = player:FindNodeByClassName('Weapon')
    local matrix = Scene:FindEntityNodeTransform(player, weapon)
    -- setup the args for spawning a new bullet.
    local args = game.EntityArgs:new()
    args.class    = ClassLib:FindEntityClassByName(ammo_name)
    args.position = util.GetTranslationFromMatrix(matrix)
    args.name     = ammo_name
    args.logging  = false
    Scene:SpawnEntity(args, true)
end

function SpawnExhaust(player, exhaust_name)
    local node   = player:FindNodeByClassName(exhaust_name)
    local matrix = Scene:FindEntityNodeTransform(player, node)
    local args = game.EntityArgs:new()
    args.class = ClassLib:FindEntityClassByName('Player Exhaust')
    args.position = util.GetTranslationFromMatrix(matrix)
    args.name = 'exhaust'
    args.logging = false
    Scene:SpawnEntity(args, true)
end

-- Called when the game play begins for an entity in scene.
function BeginPlay(player, scene)
end

-- called when the game play ends for an entity in the scene.
function EndPlay(player, scene)
    Game:DebugPrint('Game Over!')
    Game:Delay(1.0)
    Game:Stop(0)
    Game:Play('Menu')
    Game:OpenUI('GameOver')
end

-- Called on every low frequency game tick.
function Tick(player, game_time, dt)
end

local hori_velocity = 0
local vert_velocity = 0
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

    if wdk.TestKeyDown(wdk.Keys.ArrowLeft) then
        if hori_velocity >= 0 then
            hori_velocity = -200
        elseif hori_velocity > -600 then
            hori_velocity = hori_velocity - 100
        end
        player:PlayAnimationByName('Bank')
    elseif wdk.TestKeyDown(wdk.Keys.ArrowRight) then
        if hori_velocity <= 0 then
            hori_velocity = 200
        elseif hori_velocity < 600 then
            hori_velocity = hori_velocity + 100
        end
        player:PlayAnimationByName('Bank')
    else
        hori_velocity = 0
    end

    if wdk.TestKeyDown(wdk.Keys.ArrowUp) then
        if vert_velocity >= 0 then
            vert_velocity = -200
        elseif vert_velocity > -600 then
            vert_velocity = vert_velocity - 100
        end
    elseif wdk.TestKeyDown(wdk.Keys.ArrowDown) then
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

    local body = player:FindNodeByClassName('Body')
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

    SpawnExhaust(player, 'Left exhaust')
    SpawnExhaust(player, 'Right exhaust')

    -- fire the weapon if some key presses were detected!
    if time_to_fire_weapon == 0 then
        if wdk.TestKeyDown(wdk.Keys.Space) then
            FireWeapon(player, 'RedBullet')
            Audio:PlaySoundEffect('Player Weapon')
        end
       -- limit the firing rate
       time_to_fire_weapon = 6
       return
    end
    time_to_fire_weapon = time_to_fire_weapon - 1
end

-- low frequency keyboard event callback hander.
-- keeping the key pressed down will call repeatedly
-- OnKeyDown followed by OnKeyUp until another key is
-- pressed at which point the keyevents for the original
-- key combo stop arriving.
function OnKeyDown(player, symbol, modifier_bits)
    if symbol == wdk.Keys.Key1 then
        FireWeapon(player, 'RedMine')
    end
end

function OnKeyUp(player, symbol, modifier_bits)
end
