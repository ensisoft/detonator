// Copyright (C) 2020-2026 Sami Väisänen
// Copyright (C) 2020-2026 Ensisoft http://www.ensisoft.com
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
#  include <glm/mat4x4.hpp>
#include "warnpop.h"

#include <vector>
#include <variant>
#include <memory>
#include <string>

#include "graphics/instance_buffer.h"
#include "graphics/instance_data.h"

namespace gfx
{
    class Device;

    class DrawCallBase {
    public :
        virtual ~DrawCallBase() = default;
        virtual bool IsInstanced() const = 0;
    };

    struct GenericInstancedDraw {
        struct Instance {
            glm::mat4 model_to_world;
        };

        explicit GenericInstancedDraw(std::string id)
            : mContentId(std::move(id))
        {}
        void SetContentName(std::string name) noexcept
        { mContentName = std::move(name); }
        void AddInstance(Instance&& instance)
        { mInstances.push_back(std::move(instance)); }
        void AddInstance(const Instance& instance)
        { mInstances.push_back(instance); }

        std::string mContentId;
        std::string mContentName;
        std::vector<Instance>  mInstances;
    };

    struct SimpleDraw {
        // currently holds no parameters the model_view matrix
        // is in the painter's DrawItem directly.
    };

    class DrawCall
    {
    public:
        DrawCall() = default;

        explicit DrawCall(SimpleDraw draw) noexcept
            : mValue(draw)
            , mInstanced(false)
        {}
        explicit DrawCall(GenericInstancedDraw draw) noexcept
            : mValue(std::move(draw))
            , mInstanced(true)
        {}

        explicit DrawCall(std::shared_ptr<const DrawCallBase> draw) noexcept
          : mValue(draw)
        {
            mInstanced = draw->IsInstanced();
        }

        template<typename T>
        const T* Get() const;

        bool IsInstanced() const noexcept
        {
            return mInstanced;
        }

        DrawCall& operator=(SimpleDraw draw)
        {
            mValue = draw;
            mInstanced = false;
            return *this;
        }
        DrawCall& operator=(const GenericInstancedDraw& draw)
        {
            mValue = draw;
            mInstanced = true;
            return *this;
        }
        DrawCall& operator=(GenericInstancedDraw&& draw)
        {
            mValue = std::move(draw);
            mInstanced = true;
            return *this;
        }
    private:
        using DrawCallPtr= std::shared_ptr<const DrawCallBase>;
        std::variant<SimpleDraw, GenericInstancedDraw, DrawCallPtr> mValue;
        bool mInstanced = false;
    };

    template<typename T>
    const T* DrawCall::Get() const
    {
        if (const auto* ptr_ptr = std::get_if<DrawCallPtr>(&mValue))
        {
            if (const auto* ptr = dynamic_cast<const T*>(ptr_ptr->get()))
                return ptr;
        }
        return nullptr;
    }

    template<> inline
    const SimpleDraw* DrawCall::Get<SimpleDraw>() const
    {
        if (const auto* ptr = std::get_if<SimpleDraw>(&mValue))
            return ptr;
        return nullptr;
    }

    template<> inline
    const GenericInstancedDraw* DrawCall::Get<GenericInstancedDraw>() const
    {
        if (const auto* ptr = std::get_if<GenericInstancedDraw>(&mValue))
            return ptr;
        return nullptr;
    }

    InstanceDataPtr PrepareDrawCall(const GenericInstancedDraw& draw, Device& device, bool editing_mode);

} // namespace