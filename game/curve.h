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
#include <optional>

#include "data/fwd.h"
#include "game/enum.h"
#include "game/spline.h"
#include "game/curve_types.h"

namespace game
{
    class CurveClass
    {
    public:
        enum class CurveSource {
            SplineMover, Drawable, User
        };

        using Type = CurveType;
        using JointStyle = CurveJointStyle;
        using EndCapStyle = CurveEndCapStyle;

        explicit CurveClass(std::string id);
        CurveClass(const CurveClass& other);
        CurveClass(const CurveClass& other, std::string id);
        CurveClass();
       ~CurveClass();

        void SetCurveSource(CurveSource source) noexcept
        { mCurveSource = source; }
        void SetName(std::string name) noexcept
        { mName = std::move(name); }
        void SetCurveType(const Type type) noexcept
        { mCurveType = type;}
        void SetEndCapStyle(const EndCapStyle end_cap_style) noexcept
        { mEndCapStyle = end_cap_style; }
        void SetJointStyle(const JointStyle joint_style) noexcept
        { mJointStyle = joint_style; }
        void SetWidthOverride(float width) noexcept
        { mWidthOverride = width; }
        auto HasWidthOverride() const noexcept
        { return mWidthOverride.has_value(); }
        void ResetWidthOverride() noexcept
        { mWidthOverride.reset(); }

        auto GetCurveSource() const noexcept
        { return mCurveSource; }

        auto GetWidthOverride() const noexcept
        { return mWidthOverride.value(); }
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

        auto& GetCurve() noexcept
        { return mCurve; }

        const auto& GetPoint(const size_t index) const noexcept
        { return mCurve.GetPoint(index); }
        auto GetPointCount() const noexcept
        { return mCurve.GetPointCount(); }

        void SetCurve(CurveData curve) noexcept;
        void SetPoint(const CurvePoint& point, size_t index);
        void AppendPoint(const CurvePoint& point);
        void PrependPoint(const CurvePoint& point);
        void ErasePoint(size_t index);

        std::size_t GetHash() const noexcept;
        std::size_t GetCurveHash() const noexcept;
        std::string GetCurveId(bool enable_id_caching) const noexcept;

        void IntoJson(data::Writer& writer) const;
        bool FromJson(const data::Reader& reader);

        std::shared_ptr<const CatmullRomSpline> MakeCatmullRomSpline() const;
        std::shared_ptr<const PolyLineSpline> MakePolylineSpline() const;
        bool InitClassRuntime(const std::string& debug_name) const;
    private:
        std::string mClassId;
        std::string mName;
        CurveData mCurve;
        CurveSource mCurveSource = CurveSource::User;
        Type mCurveType = Type::CatmullRom;
        JointStyle mJointStyle = JointStyle::Bevel;
        EndCapStyle mEndCapStyle = EndCapStyle::Butt;
        std::optional<float> mWidthOverride;
    private:
        // read comments in the curve.cpp about why having the
        // curves cached here is a bad idea.
    };

    class Curve
    {
    public:
        using Type = CurveClass::Type;

        Curve(std::shared_ptr<const CurveClass> klass)
            : mClass(std::move(klass))
        {}
        Curve(const CurveClass& klass)
            : Curve(std::make_shared<CurveClass>(klass))
        {}
        Curve(CurveClass&& klass)
            : Curve(std::make_shared<CurveClass>(std::move(klass)))
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
        auto HasWidthOverride() const noexcept
        { return mClass->HasWidthOverride(); }
        auto GetWidthOverride() const noexcept
        { return mClass->GetWidthOverride(); }

        std::size_t GetCurveHash() const noexcept;
        std::string GetCurveId(bool enable_id_caching) const;

        const auto& GetCurve() const noexcept
        {
            if (mInstanceCurve.has_value())
                return *mInstanceCurve;

            return mClass->GetCurve();
        }

        // todo: api to support the per instance curve data
        // this will involve figuring out the right place to
        // prepare the curve to avoid messing up any previous
        // state while something is used the curve (such as a mover)

        size_t GetPointCount() const noexcept
        {
            if (mInstanceCurve.has_value())
                return mInstanceCurve->GetPointCount();

            return mClass->GetPointCount();
        }

        const auto& GetPoint(size_t index) const noexcept
        {
            if (mInstanceCurve.has_value())
                return mInstanceCurve->GetPoint(index);

            return mClass->GetPoint(index);
        }

        // take the curve displacement (travel along the curve)
        // and map that to a smoothed interpolated t value for sampling
        // the spline.
        float Reparametrize(float t) const;

        Float2 GetCurvePosition(float t) const;
        Float2 GetCurveTangent(float t) const;
        double GetCurveLength() const noexcept;

        bool PrepareCurve();
    private:
        std::shared_ptr<const CurveClass> mClass;
        std::optional<CurveData> mInstanceCurve;
        mutable std::string mCurveId;

    private:
        // these have to be shared ptr because if the curve has
        // its own private data the curve functions will be
        // private to the curve, otherwise they're shared from
        // the class between all instances.
        std::shared_ptr<const CatmullRomSpline> mCatmullRom;
        std::shared_ptr<const PolyLineSpline> mPolyline;
    };
} // namespace