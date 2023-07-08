// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "config.h"

#include "warnpush.h"
#  include <sol/sol.hpp>
#include "warnpop.h"

#include "engine/lua.h"
#include "wdk/keys.h"
#include "wdk/system.h"

using namespace engine::lua;

namespace engine
{
void BindWDK(sol::state& L)
{
    auto table = L["wdk"].get_or_create<sol::table>();
    table["KeyStr"] = [](int value) {
        const auto key = magic_enum::enum_cast<wdk::Keysym>(value);
        if (!key.has_value())
            throw GameError("No such keysym value:" + std::to_string(value));
        return std::string(magic_enum::enum_name(key.value()));
    };
    table["BtnStr"] = [](int value) {
        const auto key = magic_enum::enum_cast<wdk::MouseButton>(value);
        if (!key.has_value())
            throw GameError("No such mouse button value: " + std::to_string(value));
        return std::string(magic_enum::enum_name(key.value()));
    };
    table["ModStr"] = [](int value) {
        const auto mod = magic_enum::enum_cast<wdk::Keymod>(value);
        if (!mod.has_value())
            throw GameError("No such keymod value: " + std::to_string(value));
        return std::string(magic_enum::enum_name(mod.value()));
    };
    table["ModBitStr"] = [](int bits) {
        std::string ret;
        wdk::bitflag<wdk::Keymod> mods;
        mods.set_from_value(bits);
        if (mods.test(wdk::Keymod::Control))
            ret += "Ctrl+";
        if (mods.test(wdk::Keymod::Shift))
            ret += "Shift+";
        if (mods.test(wdk::Keymod::Alt))
            ret += "Alt+";
        if (!ret.empty())
            ret.pop_back();
        return ret;
    };
    table["TestKeyDown"] = [](int value) {
        const auto key = magic_enum::enum_cast<wdk::Keysym>(value);
        if (!key.has_value())
            throw GameError("No such key symbol: " + std::to_string(value));
#if defined(WEBASSEMBLY)
        throw GameError("TestKeyDown is not available in WASM.");
#else
        return wdk::TestKeyDown(key.value());
#endif
    };
    table["TestMod"] = [](int bits, int value) {
        const auto mod = magic_enum::enum_cast<wdk::Keymod>(value);
        if (!mod.has_value())
            throw GameError("No such modifier: " + std::to_string(value));
        wdk::bitflag<wdk::Keymod> mods;
        mods.set_from_value(bits);
        return mods.test(mod.value());
    };

    // build table for key names.
    for (const auto& key : magic_enum::enum_values<wdk::Keysym>())
    {
        const std::string key_name(magic_enum::enum_name(key));
        table[sol::create_if_nil]["Keys"][key_name] = magic_enum::enum_integer(key);
    }

    // build table for modifiers
    for (const auto& mod : magic_enum::enum_values<wdk::Keymod>())
    {
        const std::string mod_name(magic_enum::enum_name(mod));
        table[sol::create_if_nil]["Mods"][mod_name] = magic_enum::enum_integer(mod);
    }

    // build table for mouse buttons.
    for (const auto& btn : magic_enum::enum_values<wdk::MouseButton>())
    {
        const std::string btn_name(magic_enum::enum_name(btn));
        table[sol::create_if_nil]["Buttons"][btn_name] = magic_enum::enum_integer(btn);
    }

    using KeyBitSet = std::bitset<96>;

    auto key_bit_string = table.new_usertype<KeyBitSet>("KeyBitSet",
        sol::constructors<KeyBitSet()>());

    key_bit_string["Set"] = [](KeyBitSet& bits, int key, bool on_off) {
        const auto sym = magic_enum::enum_cast<wdk::Keysym>(key);
        if (!sym.has_value())
            throw GameError("No such keysym: " + std::to_string(key));
        bits[static_cast<unsigned>(sym.value())] = on_off;
    };
    key_bit_string["Test"] = [](const KeyBitSet& bits, int key) {
        const auto sym = magic_enum::enum_cast<wdk::Keysym>(key);
        if (!sym.has_value())
            throw GameError("No such keysym: " + std::to_string(key));
        const bool ret = bits[static_cast<unsigned>(sym.value())];
        return ret;
    };
    key_bit_string["AnyBit"] = &KeyBitSet::any;
    key_bit_string["Clear"]  = [](KeyBitSet& bits) { bits.reset(); };
    // todo: fix the issues in the wdk bitset.
    // somehow the lack of explicit copy constructor seems to cause issues here?
    key_bit_string[sol::meta_function::bitwise_and] = sol::overload(
        [](const KeyBitSet& a, const KeyBitSet& b) {
            return a & b;
        },
        [](const KeyBitSet& bits, int key) {
            const auto sym = magic_enum::enum_cast<wdk::Keysym>(key);
            if (!sym.has_value())
                throw GameError("No such keysym: " + std::to_string(key));
            KeyBitSet tmp;
            tmp[static_cast<unsigned>(sym.value())] = true;
            return bits & tmp;
        });
    key_bit_string[sol::meta_function::bitwise_or] = sol::overload(
        [](const KeyBitSet& a, const KeyBitSet& b) {
            return a | b;
        },
        [](const KeyBitSet& bits, int key) {
            const auto sym = magic_enum::enum_cast<wdk::Keysym>(key);
            if (!sym.has_value())
                throw GameError("No such keysym: " + std::to_string(key));
            KeyBitSet tmp;
            tmp[static_cast<unsigned>(sym.value())] = true;
            return bits | tmp;
        });
}

} // namespace