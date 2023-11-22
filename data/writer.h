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
#  include <glm/fwd.hpp>
#  include <neargye/magic_enum.hpp>
#include "warnpop.h"

#include <memory>
#include <variant>
#include <optional>
#include <tuple>

#include "base/types.h"
#include "base/color4f.h"
#include "base/bitflag.h"
#include "base/rotator.h"

namespace data
{
    class IODevice;

    class Writer
    {
    public:
        virtual ~Writer() = default;
        virtual std::unique_ptr<Writer> NewWriteChunk() const = 0;
        virtual void Write(const char* name, int value) = 0;
        virtual void Write(const char* name, unsigned value) = 0;
        virtual void Write(const char* name, double value) = 0;
        virtual void Write(const char* name, float value) = 0;
        virtual void Write(const char* name, bool value) = 0;
        virtual void Write(const char* name, const char* str) = 0;
        virtual void Write(const char* name, const std::string& value) = 0;
        virtual void Write(const char* name, const glm::vec2& vec) = 0;
        virtual void Write(const char* name, const glm::vec3& vec) = 0;
        virtual void Write(const char* name, const glm::vec4& vec) = 0;
        virtual void Write(const char* name, const base::FRect& rect) = 0;
        virtual void Write(const char* name, const base::FPoint& point) = 0;
        virtual void Write(const char* name, const base::FSize& point) = 0;
        virtual void Write(const char* name, const base::Color4f& color) = 0;
        virtual void Write(const char* name, const base::Rotator& rotator) = 0;
        virtual void Write(const char* name, const Writer& chunk) = 0;
        virtual void Write(const char* name, std::unique_ptr<Writer> chunk) = 0;
        // array writing for primitive types (object types should be done through chunks)
        virtual void Write(const char* name, const int* array, size_t size) = 0;
        virtual void Write(const char* name, const unsigned* array, size_t size) = 0;
        virtual void Write(const char* name, const double* array, size_t size) = 0;
        virtual void Write(const char* name, const float* array, size_t size) = 0;
        virtual void Write(const char* name, const bool* array, size_t size) = 0;
        virtual void Write(const char* name, const char* const* array, size_t size) = 0;
        virtual void Write(const char* name, const std::string* array, size_t size) = 0;
        virtual void AppendChunk(const char* name, const Writer& chunk) = 0;
        virtual void AppendChunk(const char* name, std::unique_ptr<Writer> chunk) = 0;
        virtual bool HasValue(const char* name) const = 0;

        // Dump and write the contents of this chunk to the IO device.
        virtual bool Dump(IODevice& device) const = 0;

        // helpers
        template<typename... T>
        void Write(const char* name, const std::variant<T...>& variant)
        {
            std::visit([this, name](const auto& variant_value) {
                this->Write(name, variant_value);
            }, variant);
        }

        template<typename T>
        void Write(const char* name, T value)
        {
            if constexpr (std::is_enum<T>::value)
                Write(name, std::string(magic_enum::enum_name(value)));
            else Serialize(*this, name, value);
        }
        template<typename Enum, typename Bits>
        void Write(const char* name, const base::bitflag<Enum, Bits>& bitflag)
        {
            auto chunk = NewWriteChunk();
            for (const auto& flag : magic_enum::enum_values<Enum>())
            {
                const std::string flag_name(magic_enum::enum_name(flag));
                chunk->Write(flag_name.c_str(), bitflag.test(flag));
            }
            Write(name, *chunk);
        }

        template<typename T>
        void Write(const char* name, const std::optional<T>& value)
        {
            if (!value.has_value())
                return;
            Write(name, value.value());
        }
    private:
    };

} // namespace
