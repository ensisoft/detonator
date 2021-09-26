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
#include <tuple>
#include <variant>

#include "base/types.h"
#include "base/color4f.h"
#include "base/bitflag.h"

namespace data
{
    class Reader
    {
    public:
        virtual ~Reader() = default;
        virtual std::unique_ptr<Reader> GetReadChunk(const char* name) const = 0;
        virtual std::unique_ptr<Reader> GetReadChunk(const char* name, unsigned index) const = 0;
        virtual bool Read(const char* name, float* out) const = 0;
        virtual bool Read(const char* name, int* out) const = 0;
        virtual bool Read(const char* name, unsigned* out) const = 0;
        virtual bool Read(const char* name, bool* out) const = 0;
        virtual bool Read(const char* name, std::string* out) const = 0;
        virtual bool Read(const char* name, glm::vec2* out) const = 0;
        virtual bool Read(const char* name, glm::vec3* out) const = 0;
        virtual bool Read(const char* name, glm::vec4* out) const = 0;
        virtual bool Read(const char* name, base::FRect* rect) const = 0;
        virtual bool Read(const char* name, base::FPoint* point) const = 0;
        virtual bool Read(const char* name, base::FSize* point) const = 0;
        virtual bool Read(const char* name, base::Color4f* color) const = 0;
        virtual bool HasValue(const char* name) const = 0;
        virtual bool HasChunk(const char* name) const = 0;
        virtual bool IsEmpty() const = 0;
        virtual unsigned GetNumChunks(const char* name) const = 0;

        // helpers
        template<typename... T>
        bool Read(const char* name, std::variant<T...>* out) const
        {
            return ReadVariantAlternative<0>(name, out);
        }

        template<typename T>
        bool Read(const char* name, T* out) const
        {
            if constexpr (std::is_enum<T>::value) {
                std::string str;
                if (!Read(name, &str))
                    return false;
                const auto& enum_val = magic_enum::enum_cast<T>(str);
                if (!enum_val.has_value())
                    return false;
                *out = enum_val.value();
                return true;
            } else return Deserialize(*this, name, out);
        }

        template<typename Enum, typename Bits>
        bool Read(const char* name, base::bitflag<Enum, Bits>* bitflag) const
        {
            auto chunk = GetReadChunk(name);
            if (!chunk)
                return false;

            for (const auto& flag : magic_enum::enum_values<Enum>())
            {
                // for easy versioning of bits in the flag don't require
                // that all the flags exist in the object
                const std::string flag_name(magic_enum::enum_name(flag));
                if (!chunk->HasValue(flag_name.c_str()))
                    continue;
                bool on_off = false;
                if (!chunk->Read(flag_name.c_str(), &on_off))
                    return false;
                bitflag->set(flag, on_off);
            }
            return true;
        }

        template<typename T>
        std::tuple<bool, T> Read(const char* name) const
        {
            T value;
            if (!Read(name, &value))
                return std::make_tuple(false, value);
            return std::make_tuple(true, value);
        }
        template<typename T>
        std::tuple<bool, T> Read(const char* name, const T& default_value) const
        {
            T value;
            if (!Read(name, &value))
                return std::make_tuple(false, default_value);
            return std::make_tuple(true, value);
        }
    private:
        template<size_t index, typename... T>
        bool ReadVariantAlternative(const char* key, std::variant<T...>* out) const
        {
            using Variant = std::variant<T...>;
            using Alternative = std::variant_alternative_t<index, Variant>;

            Alternative value;
            if (this->Read(key, &value)) {
                *out = value;
                return true;
            }
            constexpr auto variant_size = std::variant_size_v<Variant>;
            if constexpr (index + 1 == variant_size)
                return false;
            else return ReadVariantAlternative<index + 1>(key, out);
        }
    };
} // namespace