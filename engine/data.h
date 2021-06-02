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

#include <cstddef>
#include <string>

namespace game
{
    class GameData
    {
    public:
        enum class DataFormat {
            Text, Json, Binary
        };
        enum class SemanticType {
            UIStyle
        };

        virtual ~GameData() = default;
        virtual const void* GetData() const = 0;
        virtual std::size_t GetSize() const = 0;
        virtual std::string GetName() const = 0;
    private:
    };
} // namespace
