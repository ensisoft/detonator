

function SpawnExplosion(pos, kind)
    local class = ClassLib:FindEntityClassByName(kind)
    local args  = game.EntityArgs:new()
    args.class  = class
    args.name   = 'explosion'
    args.position = pos
    Scene:SpawnEntity(args, true)
end