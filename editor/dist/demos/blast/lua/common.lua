function SpawnHealthPack()
    local maybe = util.Random(0, 2)
    if maybe == 1 then
        local p = glm.vec2:new()
        p.y = -500.0
        p.x = util.Random(-450.0, 450.0)
        Scene:SpawnEntity('Health Pack', {
            pos = p,
            async = true
        })
    end
end

function SpawnExplosion(pos, kind)
    local class = ClassLib:FindEntityClassByName(kind)
    local args = game.EntityArgs:new()
    args.class = class
    args.name = 'explosion'
    args.position = pos
    args.layer = 1
    args.async = true
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
    local radius = 80.0
    local speed = 300.0
    local position = glm.vec2:new(util.Random(-550, 550), -500)
    local rotation = util.Random(0.0, 1.34)

    if maybe == 1 then
        local size = util.Random(0, 2)
        if size == 1 then
            scale = glm.vec2:new(2.0, 2.0)
            radius = 160.0
            speed = 150.0
        end
        Scene:SpawnEntity('Asteroid', {
            asyn = true,
            pos = position,
            scale = scale,
            r = rotation,
            vars = {
                radius = radius,
                speed = speed
            }
        })
    end
end

function PlayMusic(music)
    if State.play_music then
        Audio:PlayMusic(music)
    end
end

