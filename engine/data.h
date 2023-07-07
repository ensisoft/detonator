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

#include <string_view>

#include <cstddef>
#include <string>

namespace engine
{
    class EngineData
    {
    public:
        enum class DataFormat {
            Text, Json, Binary
        };
        enum class SemanticType {
            UIStyle
        };

        virtual ~EngineData() = default;
        virtual const void* GetData() const = 0;
        virtual std::size_t GetByteSize() const = 0;
        // Get the name of underlying the source for the data.
        // This could be for example a filename when the data comes
        // from a file.
        virtual std::string GetSourceName() const = 0;
        // Get a human-readable name associated with the data if any.
        virtual std::string GetName() const { return ""; }

        inline std::string_view GetStringView() const noexcept {
            const auto* str = static_cast<const char*>(GetData());
            const auto  len = GetByteSize();
            return { str, len };
        }
    private:
    };
} // namespace
