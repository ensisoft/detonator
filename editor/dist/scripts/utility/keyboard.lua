-- Keyboard handling utilities to map wdk virtual
-- keys to logical game action keys.
-- Copyright (c) 2023 Sami Väisänen, Ensisoft

-- LuaFormatter off

KB = {}

-- Logical game action keys.
KB.Keys = {
    Left  = 0x1,
    Right = 0x2,
    Up    = 0x4,
    Down  = 0x8,
    Fire  = 0x10
}
local keydown_bits = 0

-- Keyboard mapping for mapping WASD keys to game action keys.
KB.WASD = {
    [wdk.Keys.KeyW]  = KB.Keys.Up,
    [wdk.Keys.KeyA]  = KB.Keys.Left,
    [wdk.Keys.KeyD]  = KB.Keys.Right,
    [wdk.Keys.KeyS]  = KB.Keys.Down,
    [wdk.Keys.Space] = KB.Keys.Fire
}

-- Keyboard mapping for mapping arrow keys to game action keys.
KB.ARROW = {
    [wdk.Keys.ArrowUp]    = KB.Keys.Up,
    [wdk.Keys.ArrowLeft]  = KB.Keys.Left,
    [wdk.Keys.ArrowRight] = KB.Keys.Right,
    [wdk.Keys.ArrowDown]  = KB.Keys.Down,
    [wdk.Keys.Space]      = KB.Keys.Fire
}

-- LuaFormatter on

KB.TestKeyDown = function(key)
    return keydown_bits & key ~= 0
end

KB.KeyDown = function(symbol, modifier_bits, map)
    local action = map[symbol]
    if action == nil then
        return
    end
    keydown_bits = keydown_bits | action
end

KB.KeyUp = function(symbol, modifier_bits, map)
    local action = map[symbol]
    if action == nil then
        return
    end
    keydown_bits = keydown_bits & ~action
end

KB.ClearKey = function(key)
    keydown_bits = keydown_bits & ~key
end