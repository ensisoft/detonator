-- Top level game callbacks.
-- You're free to delete functions that you don't need.
-- LuaFormatter off
local TestTable = {}

--dd = debug draw
TestTable[0] = {name='Welcome',                    dd=false, gravity=true, msg=''}
TestTable[1] = {name='Kinematic C Velocity Balls', dd=false, gravity=false, msg=''}
TestTable[2] = {name='Dynamic C Velocity Balls',   dd=false, gravity=false, msg=''}
TestTable[3] = {name='Dynamic Balls',              dd=false, gravity=false, msg=''}
TestTable[4] = {name='Friction',                   dd=false, gravity=true,  msg='Last box down is a square!'}
TestTable[5] = {name='Restitution',                dd=false, gravity=true,  msg='Boing Boing!'}
TestTable[6] = {name='Distance Joint',             dd=true,  gravity=true,  msg='Distance Joint'}
TestTable[7] = {name='Revolute Joint',             dd=true,  gravity=true,  msg='Revolute Joint'}
TestTable[8] = {name='Pulley Joint',               dd=true,  gravity=true,  msg='Pulley Joint'}
TestTable[9] = {name='Weld Joint',                 dd=true,  gravity=true,  msg='Weld Joint'}
TestTable[10] = {name='Prismatic Joint',            dd=true,  gravity=true,  msg='Prismatic Joint'}
TestTable[11] = {name='Motor Joint',               dd=true,  gravity=true,  msg='Motor Joint'}
TestTable[12] = {name='Bridge',                     dd=false, gravity=true,  msg='The bridge to nowhere?'}
TestTable[13] = {name='Wrecking Ball',              dd=false, gravity=true,  msg='Crush Them!'}
TestTable[14] = {name='Boxes',                     dd=false, gravity=true,  msg='Find the special box!'}
TestTable[15] = {name='Raycast',                   dd=true,  gravity=false, 
    msg='Use the mouse left button (click & hold)\n' ..
        'Press keys 1, 2, 3 to change the ray cast mode.\n\n' ..
        '1 = All      = Find all objects\n' ..
        '2 = Closest  = Find geometrically closest object\n' ..
        '3 = First    = Find whichever object comes first\n\n'
}
TestTable[16] = {name='Spatial Query', dd=true, gravity=false, 
    msg='Use the mouse left button (click & hold)\n' ..
        'Press keys 1, 2, 3 to change the query mode.\n\n' ..
        '1 = All     = Find all objects\n' .. 
        '2 = Closest = Find geometrically closest object\n' ..
        '3 = First   = Find whichever object comes first\n\n' ..
        'Press R, P, C, L to change spatial query method.\n\n' ..
        'R = Rectangle   = Query rectangular area\n' ..
        'P = Point       = Query at a single point\n' .. 
        'C = PointRadius = Query circular area\n' ..
        'L = PointPoint  = Query between two points (ray)\n\n'
    }

local scene_count = #TestTable + 1
local scene_index = 0

-- LuaFormatter on

function LoadScene(index)
    scene_index = index
    if TestTable[index].gravity then
        Physics:SetGravity(glm.vec2:new(0.0, 10.0))
    else
        Physics:SetGravity(glm.vec2:new(0.0, 0.0))
    end
    Game:Play(TestTable[index].name)

    local menu = Game:GetTopUI()

    local test_list_text = 'Press arrow up/down to change test\n\n'

    for i = 0, scene_count - 1 do
        if i == scene_index then
            test_list_text = test_list_text .. ' > '
        else
            test_list_text = test_list_text .. '   '
        end

        local index = tostring(i + 1)
        if i < 9 then
            index = ' ' .. index
        end

        test_list_text = test_list_text .. index .. '. ' .. TestTable[i].name
        test_list_text = test_list_text .. '\n'
    end
    menu.list:SetText(test_list_text)
    menu.msg:SetText(TestTable[scene_index].msg)

    if TestTable[scene_index].msg == '' then
        menu.msg:SetVisible(false)
    else
        menu.msg:SetVisible(true)
    end

    Game:EnableDebugDraw(TestTable[scene_index].dd)

end

-- Called when the game is first loaded.
-- This is the place where you might want to load some 
-- previous/initial game state. 
function LoadGame()
    --    TestTable[3] = 'Distance Joint'
    --    TestTable[5] = 'Wrecking Ball'
    --    TestTable[7] = 'Fixture Widget'

    Game:DebugPrint('LoadGame called.')
    Game:SetViewport(-500.0, -400.0, 1000.0, 800.0)
    Game:OpenUI('Main Menu')
    return true
end

-- Called after the game has been loaded and the game can 
-- be started. This is likely the place where the first
-- game content is shown, such as a main menu.
function StartGame()
    Game:DebugPrint('StartGame called')
end

-- Called before the application exist.
-- This is the last chance to persist any state that should
-- be restored on next game run. 
function SaveGame()
    Game:DebugPrint('SaveGame called')
end

-- Called when the game play should stop, i.e. the application
-- is about to exit. 
function StopGame()
    Game:DebugPrint('StopGame called')
end

-- Called as a response to Game:Play when an instance
-- of the scene has been created and the game play begins.
function BeginPlay(scene, map)
    Game:DebugPrint('BeginPlay called.')
end

-- Called as a response to Game:EndPlay when the game play ends.
function EndPlay(scene, map)
    Game:DebugPrint('EndPlay called.')
end

-- Low frequency game tick. Normally called at a lower frequency
-- than the Update function. The tick frequency is configurable in
-- the workspace settings.
-- game_time is the total current accumulated game time including dt.
-- dt is the time step to take.
-- Note that this is *not* called when then the game has been suspended.
function Tick(game_time, dt)

end

-- High frequency game update function. The update frequency is
-- configurable in the workspace settings.
-- game_time is the total current accumulated game time including dt.
-- dt is the time step to take.
-- Note that this is *not* called when the game has been suspended.
function Update(game_time, dt)

    if Scene == nil then
        return
    end

    local demo_entity = Scene:FindEntityByInstanceName('Demo Entity')
    if demo_entity == nil then
        return
    end

    local message = demo_entity:FindScriptVarByName('message')
    if message == nil then
        return
    end
    local menu = Game:GetTopUI()
    menu.msg:SetText(message:GetValue())
end

-- Event/input callback handlers.

-- Called when a keyboard key has been pressed.
-- If you want to perform some game action while a key is
-- continuously held down don't use OnKeyDown but rather use
-- wdk.TestKeyDown.
-- symbol is the WDK key sym of the currently pressed key.
-- modifier_bits are the current modifier keys i.e. Ctrl/Alt/Shift
-- that may or may not be pressed at the time. You can use
-- wdk.TestMod to check whether any particular modifier bit is set.
-- Example:
-- if wdk.TestMod(modifier_bits, wdk.Mods.Control) then
--    ...
-- end
function OnKeyDown(symbol, modifier_bits)
    if symbol == wdk.Keys.ArrowDown then
        scene_index = scene_index + 1
    elseif symbol == wdk.Keys.ArrowUp then
        scene_index = scene_index - 1
    else
        return
    end

    scene_index = base.wrap(0, scene_count - 1, scene_index)

    LoadScene(scene_index)
end

function OnKeyUp(symbol, modifier_bits)

end

-- Called on mouse button press events.
-- mouse event has the following available fields
-- 'window_coord' - mouse cursor position in window coordinates.
-- 'scene_coord'  - mouse cursor position in scene coordinates
-- 'button'       - mouse button that was pressed (if any)
-- 'modifiers'    - modifier bits of keyboard mods that were pressed (if any).
-- 'over_scene'   - true to indicate that the mouse cursor is over the scene viewport in window.
function OnMousePress(mouse)

end

-- Called on mouse button release events.
function OnMouseRelease(mouse)

end

-- Called on mouse move events.
function OnMouseMove(mouse)
end

-- Called when the UI has been opened through a call to Game:OpenUI
function OnUIOpen(ui)
    LoadScene(0)
end

-- Called when the UI is has been closed. Result is the int value
-- given to Game:CloseUI.
function OnUIClose(ui, result)

end

-- Called when the UI system translates some user input action
-- such as mouse button press into a UI/widget action such as a
-- button press.
-- Action has the following available fields:
-- 'name'  - the name of the UI widget that generated the action
-- 'id'    - the id of the UI widget that generated the action
-- 'type'  - type string ('ButtonPress' etc) of the action
-- 'value' - value (int, float, bool, string) of the  action if any.
function OnUIAction(ui, action)

end

-- Called when an audio event happens.
-- An audio event has the following fields:
-- 'type'  - the type of the event. 'MusicTrackDone' or 'SoundEffectDone'
-- 'track' - the name of the audio track that generated the event.
function OnAudioEvent(event)

end

-- Called when a game event has been posted.
-- A game event has the following fields:
-- 'from' a text string identifying the sender.
-- 'to' a text string identifier the receiver
-- 'message' a text message identifying the event or carrying event information
-- 'value' a value of bool, float, int, string, vec2/3/4, FRect, FSize, FPoint or Color4f
function OnGameEvent(event)

end
