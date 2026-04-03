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
#include "warnpop.h"

#include <string>
#include <memory>

#include "base/utility.h"
#include "graphics/drawable.h"
#include "graphics/simple_shape_base.h"

namespace gfx
{
    class SimpleShapeClass : public DrawableClass
    {
    public:
        using Style = SimpleShapeStyle;
        using Shape = SimpleShapeType;
        using ShapeAttribute = SimpleShapeAttribute;

        SimpleShapeClass() = default;
        explicit SimpleShapeClass(SimpleShapeType shape, detail::SimpleShapeArgs args, std::string id, std::string name) noexcept
          : mId(std::move(id))
          , mName(std::move(name))
          , mShape(shape)
          , mArgs(args)
        {}
        SimpleShapeClass(const SimpleShapeClass& other, std::string id)
          : mId(std::move(id))
          , mName(other.mName)
          , mShape(other.mShape)
          , mArgs(other.mArgs)
        {}
        const detail::SimpleShapeArgs& GetShapeArgs() const noexcept
        { return mArgs; }
        void SetShapeArgs(detail::SimpleShapeArgs args) noexcept
        { mArgs = args; }
        Shape GetShapeType() const noexcept
        { return mShape; }

        Type GetType() const override
        { return Type::SimpleShape; }
        std::string GetId() const override
        { return mId; }
        std::string GetName() const override
        { return mName; }
        void SetName(const std::string& name) override
        { mName = name; }
        std::unique_ptr<DrawableClass> Clone() const override
        { return std::make_unique<SimpleShapeClass>(*this, base::RandomString(10)); }
        std::unique_ptr<DrawableClass> Copy() const override
        { return std::make_unique<SimpleShapeClass>(*this); }
        std::size_t GetHash() const override;
        SpatialMode GetSpatialMode() const override;;
        void IntoJson(data::Writer& data) const override;
        bool FromJson(const data::Reader& data) override;
        float GetShapeAttribute(ShapeAttribute attribute) const noexcept;
    private:
        std::string mId;
        std::string mName;
        SimpleShapeType mShape = SimpleShapeType::Arrow;
        detail::SimpleShapeArgs mArgs;
    };
} // namespace