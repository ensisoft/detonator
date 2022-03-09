-- Entity 'Player' script.
-- This script will be called for every instance.

-- world vectors
WorldUp    = glm.vec2:new(0.0, -1.0)
WorldRight = glm.vec2:new(1.0, 0.0)

local _time_to_jump = 0
local _time_to_run  = 0

local _play_coin_sound_ticks = 2

local _Keys        = wdk.KeyBitSet:new()
local _KeyJump     = wdk.Keys.ArrowUp
local _KeyRunLeft  = wdk.Keys.ArrowLeft
local _KeyRunRight = wdk.Keys.ArrowRight
local _KeyAttack   = wdk.Keys.Space

function TurnPlayerLeft(player)
    local max_nodes = player:GetNumNodes()
    for i=0, max_nodes-1, 1 do
        local node = player:GetNode(i)
        local draw = node:GetDrawable()
        if draw ~= nil then
           draw:SetFlag('FlipHorizontally', true)
        end
    end
end

function TurnPlayerRight(player)
    local max_nodes = player:GetNumNodes()
    for i=0, max_nodes-1, 1 do
        local node = player:GetNode(i)
        local draw = node:GetDrawable()
        if draw ~= nil then
            draw:SetFlag('FlipHorizontally', false)
        end
    end
end

function BeginPlay(player, scene)
    player:PlayAnimationByName('Idle')   
end

function Tick(player, game_time, dt)
    if _play_coin_sound_ticks > 0 then 
        _play_coin_sound_ticks = _play_coin_sound_ticks -1
    end
end

function Update(player, game_time, dt)
    local root = player:FindNodeByClassName('Body')
    local body = root:GetRigidBody()
    local velocity = body:GetLinearVelocity()
    local position = root:GetTranslation()
    local _, mass = Physics:FindMass(root)

    if player.died then 
        return 
    end

    if _time_to_jump > 0 then
        _time_to_jump = _time_to_jump - 1
    end
    if _time_to_run > 0 then 
       _time_to_run = _time_to_run - 1
    end


    local run_speed_max = 4.0
    local run_speed_change = run_speed_max - math.abs(velocity.x)
    local run_impulse = mass * run_speed_change

    --local can_jump = velocity.y >= -0.4 and velocity.y <= 0.0
    local jump_strength = 6.0
    local can_jump = _time_to_jump == 0
    
    if _Keys:Test(_KeyRunRight) then
        TurnPlayerRight(player)

        if player.can_move_right then
            Physics:ApplyImpulseToCenter(root, WorldRight * run_impulse)
            player:PlayAnimationByName('Walk')
        end
    elseif _Keys:Test(_KeyRunLeft) then      
        TurnPlayerLeft(player)

        if player.can_move_left then 
            Physics:ApplyImpulseToCenter(root, WorldRight * run_impulse * -1.0)
            player:PlayAnimationByName('Walk')
        end
    end

    if _Keys:Test(_KeyJump) and can_jump then
        Physics:ApplyImpulseToCenter(root, WorldUp * jump_strength)
        player:PlayAnimationByName('Jump')
        _time_to_jump = 60
        _Keys:Set(_KeyJump, false)
    end
    
    if position.y >= 300.0 and not player.died then 
        body:Enable(false)
        player:PlayAnimationByName('Die')
        player.died = true
        local event   = game.GameEvent:new()
        event.from    = 'player'
        event.to      = 'game'
        event.message = 'died'
        Game:PostEvent(event)
    end
end

function OnBeginContact(player, node, other, other_node)
    local object = other:GetClassName()
    local body_part = node:GetName()
    
    Game:DebugPrint(util.FormatString('Begin %1 contact with %2', body_part, object))

    if body_part == 'Body' then
        if object == 'Coin' then 
            player.coins = player.coins + 1
            Game:DebugPrint('Coin collected!')

            other:PlayAnimationByName('Collected')
            local coin = other:FindNodeByClassName('Coin')  
            local scene = other:GetScene()
            local spawn = game.EntityArgs:new()
            spawn.class = ClassLib:FindEntityClassByName('Coins Particles')
            spawn.position = coin:GetTranslation()
            scene:SpawnEntity(spawn, true)
            if _play_coin_sound_ticks == 0 then 
                Audio:PlaySoundEffect('Rise04')    
                _play_coin_sound_ticks = 2
            end
        elseif object == 'Violet Diamond' then 
            Game:DebugPrint('Big ass diamond Collected!')
            local finish = game.GameEvent:new()
            finish.from = 'player'
            finish.to   = 'game'
            finish.message = 'level-complete'
            Game:PostEvent(finish)

            Audio:PlaySoundEffect('Rise07')
        end
    end


    -- if the colliders detect collision with certain shapes then the 
    -- player movement to that direction is blocked
    if body_part == 'Left Collider' then    
        if object == 'Plateu' or object == 'Small Island' or object == 'StoneBlock' then
            player.can_move_left = false
        end                    
    end
    if body_part == 'Right Collider' then
        if object == 'Plateu' or object == 'Small Island' or object == 'StoneBlock' then 
            player.can_move_right = false
        end
    end
end

function OnEndContact(player, node, other, other_node)
    local object = other:GetClassName()
    local body_part = node:GetName()

    Game:DebugPrint(util.FormatString('End %1 conctact with %2', body_part, object))

    if body_part == 'Left Collider' then 
        player.can_move_left = true
    end
    if body_part == 'Right Collider' then 
        player.can_move_right = true
    end
end

function OnKeyDown(player, symbol, modifier_bits)
    if player.died then 
        return 
    end

    if symbol == _KeyAttack then 
        player:PlayAnimationByName('Attack')
    else 
        _Keys:Set(symbol, true)
    end    
end

function OnKeyUp(player, symbol, modifier_bits)
    _Keys:Set(symbol, false)
end

function OnAnimationFinished(player, animation)
    local name = animation:GetClassName()
    if name == 'Die' then 
        return
    end

    if name ~= 'Idle' then 
        player:PlayAnimationByName('Idle')
    end
end