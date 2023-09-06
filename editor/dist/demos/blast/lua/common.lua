function SpawnExplosion(pos, kind)
    local class = ClassLib:FindEntityClassByName(kind)
    local args = game.EntityArgs:new()
    args.class = class
    args.name = 'explosion'
    args.position = pos
    args.layer = 1
    Scene:SpawnEntity(args, true)
end

function SpawnScore(pos, score)
    local class = ClassLib:FindEntityClassByName('Score')
    local args = game.EntityArgs:new()
    args.class = class
    args.name = 'score'
    args.position = pos
    args.logging = false
    args.layer = 1

    local entity = Scene:SpawnEntity(args, true)
    local node = entity:FindNodeByClassName('text')
    local text = node:GetTextItem()
    text:SetText(tostring(score))
end

function SpawnSpaceRock()
    local maybe = util.Random(0, 2)
    local scale = glm.vec2:new(1.0, 1.0)
    local radius = 30
    local time_scale = 2.0
    local speed = 300.0
    if maybe == 1 then
        local size = util.Random(0, 2)
        if size == 1 then
            scale = glm.vec2:new(2.0, 2.0)
            radius = 60
            time_scale = 0.3
            speed = 150.0
        end
        local asteroid = ClassLib:FindEntityClassByName('Asteroid')
        local args = game.EntityArgs:new()
        args.class = asteroid
        args.name = 'asteroid'
        args.position = glm.vec2:new(util.Random(-550, 550), -500)
        args.scale = scale
        local entity = Scene:SpawnEntity(args, true)
        local body = entity:GetNode(0)
        local item = body:GetDrawable()
        item:SetTimeScale(time_scale)
        entity.radius = radius
        entity.speed = speed
    end
end

function PlayMusic(music)
    if State.play_music then
        Audio:PlayMusic(music)
    end
end

