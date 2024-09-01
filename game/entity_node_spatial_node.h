// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#include <memory>

#include "base/bitflag.h"
#include "data/fwd.h"

namespace game
{
    class SpatialNodeClass
    {
    public:
        enum class Shape {
            AABB
        };
        enum class Flags {
            Enabled,
            ReportOverlap
        };
        SpatialNodeClass();

        inline Shape GetShape() const noexcept
        { return mShape; }
        inline void SetShape(Shape shape) noexcept
        { mShape = shape; }
        inline void SetFlag(Flags flag, bool on_off) noexcept
        { mFlags.set(flag, on_off); }
        inline bool TestFlag(Flags flag) const noexcept
        { return mFlags.test(flag); }
        inline base::bitflag<Flags> GetFlags() const noexcept
        { return mFlags; }

        std::size_t GetHash() const;
        void IntoJson(data::Writer& data) const;
        bool FromJson(const data::Reader& data);
    private:
        Shape mShape = Shape::AABB;
        base::bitflag<Flags> mFlags;
    };

    class SpatialNode
    {
    public:
        using Flags = SpatialNodeClass::Flags;
        using Shape = SpatialNodeClass::Shape;
        explicit SpatialNode(std::shared_ptr<const SpatialNodeClass> klass) noexcept
          : mClass(std::move(klass))
          , mFlags(mClass->GetFlags())
        {}
        inline bool TestFlag(Flags flag) const noexcept
        { return mFlags.test(flag); }
        inline Shape GetShape() const noexcept
        { return mClass->GetShape(); }
        inline bool IsEnabled() const noexcept
        { return mFlags.test(Flags::Enabled); }
        inline void Enable(bool value) noexcept
        { mFlags.set(Flags::Enabled, value); }
        inline void SetFlag(Flags flag, bool on_off) noexcept
        { mFlags.set(flag, on_off); }

        // class access
        const SpatialNodeClass& GetClass() const noexcept
        { return *mClass; }
        const SpatialNodeClass* operator->() const noexcept
        { return mClass.get(); }
    private:
        std::shared_ptr<const SpatialNodeClass> mClass;
        base::bitflag<Flags> mFlags;
    };

} // namespace