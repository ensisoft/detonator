-- this script is an alternative to the regular game main script
-- except that this script loads the specified UI.
function StartGame()
    Game:DebugPrint('Start preview')
    Game:SetViewport(-SurfaceWidth*0.5, -SurfaceHeight*0.5, SurfaceWidth, SurfaceHeight)
    Game:OpenUI('_ui_preview_')
end

function OnKeyDown(symbol, modifier_bits)
    -- remember the keys are also processed by the window
    -- so we might have a conflict here, it might be better
    -- to find a way to implement this feature in the play window
    if symbol == wdk.Keys.F5 then
        Game:CloseUI(0)
        Game:OpenUI('_ui_preview_')
    end
end

-- Special function, only called in editing/preview mode.
function OnRenderingSurfaceResized(width, height)
    Game:SetViewport(base.FRect:new(-width*0.5, -height*0.5, width, height))
end

-- special function, only called in editing mode
function OnContentClassUpdate(type, name, id, action)
    -- action is 'save' on content save and 'delete' on content delete

    -- todo: probably need a different API here just to run
    -- a different code path since this is the development time
    -- and we're probably not interested in doing the closing
    -- animations etc. (except maybe that's exactly what it is?)
    Game:CloseUI(0)
    Game:OpenUI('_ui_preview_')
end