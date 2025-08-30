// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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

#include <string>
#include <vector>
#include <cstdint>
#include <initializer_list>

#include "base/assert.h"
#include "data/fwd.h"

namespace dev
{
    struct VertexLayout {
        struct Attribute {
            // name of the attribute in the shader code.
            std::string name;
            // the index of the attribute.
            // use glsl syntax
            // layout (binding=x) in vec3 myAttrib;
            unsigned index = 0;
            // number of vector components
            // must be one of [1, 2, 3, 4]
            unsigned num_vector_components = 0;
            // the attribute divisor. if this is 0 the
            // attribute updates for every vertex and instancing is off.
            // ignored for geometry attributes.
            unsigned divisor = 0;
            // relative offset in the vertex data
            // typically offsetof(MyVertex, member)
            unsigned offset = 0;
        };
        unsigned vertex_struct_size = 0;
        std::vector<Attribute> attributes;

        VertexLayout() = default;
        VertexLayout(std::size_t struct_size, std::initializer_list<Attribute> attrs) noexcept
            : vertex_struct_size(struct_size)
            , attributes(attrs)
        {}

        const Attribute* FindAttribute(const char* name) const noexcept
        {
            for (const auto& a : attributes)
            {
                if (a.name == name)
                    return &a;
            }
            return nullptr;
        }

        template<typename T>
        static const T* GetVertexAttributePtr(const Attribute& attribute, const void* ptr) noexcept
        {
            ASSERT(sizeof(T) == attribute.num_vector_components * sizeof(float));
            return reinterpret_cast<const T*>(reinterpret_cast<const intptr_t>(ptr)+attribute.offset);
        }

        template<typename T>
        static T* GetVertexAttributePtr(const Attribute& attribute, void* ptr) noexcept
        {
            ASSERT(sizeof(T) == attribute.num_vector_components * sizeof(float));
            return reinterpret_cast<T*>(reinterpret_cast<intptr_t>(ptr)+attribute.offset);
        }

        void AppendAttribute(Attribute attribute)
        {
            attribute.offset = vertex_struct_size;
            vertex_struct_size += attribute.num_vector_components * sizeof(float);
            attributes.push_back(attribute);
        }

        bool FromJson(const data::Reader& reader) noexcept;
        void IntoJson(data::Writer& writer) const;
        size_t GetHash() const noexcept;
    };

    bool operator==(const VertexLayout& lhs, const VertexLayout& rhs) noexcept;
    bool operator!=(const VertexLayout& lhs, const VertexLayout& rhs) noexcept;

} // namespace