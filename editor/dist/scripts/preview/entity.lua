
-- This script is essentially an alternative version to a regular
-- game main script except that the purpose of this script is to set
-- things up for spawning a preview version of entities while keeping
-- things as close as possible to the "real thing".

function SpawnEntity()
    -- spawn an instance of the entity we want to preview.
    -- there's a special name mapping that maps the generic _entity_preview_
    -- class name to the current entity class that is being previewed.
    local args    = game.EntityArgs:new()
    args.class    = ClassLib:FindEntityClassByName('_entity_preview_')
    args.logging  = true
    args.name     = 'entity'
    args.position = glm.vec2:new(0.0, 0.0)
    args.scale    = glm.vec2:new(1.0, 1.0)
    args.rotation = 0.0
    Scene:SpawnEntity(args, true)
    return true
end

function StartGame()
    Game:DebugPrint('Start preview')

    -- the way things are setup there's a special name for a scene that
    -- can be used in the preview setting to spawn entities. It's possible
    -- to create a scene by this name in the editor and then have that scene
    -- get used here. (as long as the name matches!)
    Game:Play('_entity_preview_scene_')

    -- todo: currently the game sets its own viewport any way it wants to
    -- and the editor's viewport is completely separate. we're using the
    -- surface size here for setting up the viewport
    Game:SetViewport(base.FRect:new(-SurfaceWidth*0.5, -SurfaceHeight*0.5, SurfaceWidth, SurfaceHeight))
end

function BeginPlay(scene)
    SpawnEntity()
end

function OnKeyDown(symbol, modifier_bits)
    if symbol == wdk.Keys.Escape then
        Game:Play('_entity_preview_scene_')
        SpawnEntity()
    end
end

-- Special function, only called in editing/preview mode.
function OnRenderingSurfaceResized(width, height)
    Game:SetViewport(base.FRect:new(-width*0.5, -height*0.5, width, height))
end