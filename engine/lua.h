// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <neargye/magic_enum.hpp>
#include "warnpop.h"

#include <stdexcept>

namespace engine {
namespace lua {

    using GameError = std::runtime_error;

    template<typename Type>
    bool TestFlag(const Type& object, const std::string& name)
    {
        using Flags = typename Type::Flags;
        const auto enum_val = magic_enum::enum_cast<Flags>(name);
        if (!enum_val.has_value())
            throw GameError("No such flag: " + name);
        return object.TestFlag(enum_val.value());
    }
    template<typename Type>
    void SetFlag(Type& object, const std::string& name, bool on_off)
    {
        using Flags = typename Type::Flags;
        const auto enum_val = magic_enum::enum_cast<Flags>(name);
        if (!enum_val.has_value())
            throw GameError("No such flag: " + name);
        object.SetFlag(enum_val.value(), on_off);
    }

} // namespace

    void BindUtil(sol::state& L);
    void BindBase(sol::state& L);
    void BindData(sol::state& L);
    void BindGLM(sol::state& L);
    void BindGFX(sol::state& L);
    void BindWDK(sol::state& L);
    void BindUIK(sol::state& L);
    void BindGameLib(sol::state& L);

} // namespace
