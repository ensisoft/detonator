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

#include <string>
#include <memory>

#include "data/fwd.h"
#include "game/enum.h"
#include "game/curve_types.h"

namespace game
{
    class CurveClass
    {
    public:
        using Type = CurveType;
        using JointStyle = CurveJointStyle;
        using EndCapStyle = CurveEndCapStyle;

        explicit CurveClass(std::string id);
        CurveClass(const CurveClass& other, std::string id);
        CurveClass();

        void SetName(std::string name) noexcept
        { mName = std::move(name); }
        void SetCurveType(const Type type) noexcept
        { mCurveType = type;}
        void SetEndCapStyle(const EndCapStyle end_cap_style) noexcept
        { mEndCapStyle = end_cap_style; }
        void SetJointStyle(const JointStyle joint_style) noexcept
        { mJointStyle = joint_style; }
        void SetCurve(Curve curve) noexcept
        { mCurve = std::move(curve); }

        auto GetClassId() const
        { return mClassId; }
        auto GetName() const
        { return mName; }
        auto GetCurveType() const noexcept
        { return mCurveType; }
        auto GetEndCapStyle() const noexcept
        { return mEndCapStyle; }
        auto GetJointStyle() const noexcept
        { return mJointStyle; }
        const auto& GetCurve() const noexcept
        { return mCurve; }

        size_t GetHash() const noexcept;

        void IntoJson(data::Writer& writer) const;
        bool FromJson(const data::Reader& reader);
    private:
        std::string mClassId;
        std::string mName;
        Curve mCurve;
        Type mCurveType = Type::CatmullRom;
        JointStyle mJointStyle = JointStyle::Bevel;
        EndCapStyle mEndCapStyle = EndCapStyle::Butt;
    };

    class CurveInstance
    {
    public:
        CurveInstance(std::shared_ptr<const CurveClass> klass)
            : mClass(std::move(klass))
        {}
        auto GetClassId() const
        { return mClass->GetClassId(); }
        auto GetName() const
        { return mClass->GetName(); }
        auto GetCurveType() const noexcept
        { return mClass->GetCurveType(); }
        auto GetEndCapStyle() const noexcept
        { return mClass->GetEndCapStyle(); }
        auto GetJointStyle() const noexcept
        { return mClass->GetJointStyle(); }
        const auto& GetCurve() const noexcept
        { return mClass->GetCurve(); }

        size_t GetHash() const noexcept
        { return mClass->GetHash(); }

    private:
        std::shared_ptr<const CurveClass> mClass;
    };
} // namespace