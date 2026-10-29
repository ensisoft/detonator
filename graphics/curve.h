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

#include <vector>
#include <cstddef>
#include <array>

#include "base/bitflag.h"
#include "base/types.h"
#include "graphics/drawable.h"
#include "graphics/vertex.h"

namespace gfx
{
    class CurvePoint
    {
    public:
        using value_type = float;

        CurvePoint() = default;
        CurvePoint(base::Float3 position, base::Float2 data, float width = 0.01f)
        {
            mData[0] = position.x;
            mData[1] = position.y;
            mData[2] = position.z;
            mData[3] = data.x;
            mData[4] = data.y;
            mData[5] = width;
        }
        float operator[](size_t index) const noexcept
        {
            return mData[index];
        }

        float& operator[](size_t index) noexcept
        {
            return mData[index];
        }
        base::Float3 GetPosition() const noexcept
        {
            return { mData[0], mData[1], mData[2] };
        }
        base::Float2 GetData() const noexcept
        {
            return { mData[3], mData[4] };
        }
        float GetWidth() const noexcept
        {
            return mData[5];
        }
        void SetPosition(base::Float3 position) noexcept
        {
            mData[0] = position.x;
            mData[1] = position.y;
            mData[2] = position.z;
        }
        void SetData(base::Float2 data) noexcept
        {
            mData[3] = data.x;
            mData[4] = data.y;
        }
        void SetWidth(float width) noexcept
        {
            mData[5] = width;
        }

        void IntoJson(data::Writer& writer) const;
        bool FromJson(const data::Reader& reader);

    private:
        std::array<float, 6> mData = {0.0f};
    };

    float GetPointDistance(const CurvePoint& p0, const CurvePoint& p1) noexcept;

    // defines a curve as a set of control points and properties
    // that control how the curve gets tessellated into a triangle
    // strip for rendering.
    class CurveClass : public DrawableClass
    {
    public:
        enum class CurveType {
            PolylineSpline, CatmullRomSpline
        };

        enum class JointStyle {
            Miter, Bevel, Round
        };

        enum class EndCapStyle {
            Square, Round, Butt
        };

        enum  class Flags {
            StaticContent
        };

        using ControlPoint = CurvePoint;

        void SetCurveType(CurveType type) noexcept
        { mCurveType = type; }
        void SetJointStyle(JointStyle style) noexcept
        { mJointStyle = style; }
        void SetEndCapStyle(EndCapStyle style) noexcept
        { mEndCapStyle = style; }

        auto GetCurveType() const noexcept
        { return mCurveType; }
        auto GetJointStyle() const noexcept
        { return  mJointStyle; }
        auto GetEndCapStyle() const noexcept
        { return mEndCapStyle; }

        void AddPoint(const ControlPoint& point)
        { mPoints.push_back(point); }
        void SetContentHash(size_t hash)
        { mContentHash = hash; }
        auto GetContentHash() const noexcept
        { return mContentHash; }
        void ClearContent()
        {
            mPoints.clear();
            mContentHash = 0;
        }

        auto IsStatic() const noexcept
        { return mFlags.test(Flags::StaticContent); }
        void SetStatic(bool on_off) noexcept
        { mFlags.set(Flags::StaticContent, on_off); }

        SpatialMode GetSpatialMode() const override;
        Type GetType() const override;

        std::unique_ptr<DrawableClass> Clone() const override;
        std::unique_ptr<DrawableClass> Copy() const override;
        std::size_t GetHash() const override;
        std::string GetName() const override;
        std::string GetId() const override;

        void SetName(const std::string& name) override;
        void IntoJson(data::Writer& writer) const override;
        bool FromJson(const data::Reader& data) override;

        bool Construct(GeometryBuffer* buffer) const;
    private:
        std::string mId;
        std::string mName;
        std::size_t mContentHash = 0;
        std::vector<ControlPoint> mPoints;
        CurveType mCurveType = CurveType::CatmullRomSpline;
        JointStyle mJointStyle = JointStyle::Miter;
        EndCapStyle mEndCapStyle = EndCapStyle::Square;
        base::bitflag<Flags> mFlags;
        float mWidth = 0.01f;
    };

    class CurveInstance : public Drawable
    {
    public:
        using ControlPoint = CurveClass::ControlPoint;

        CurveInstance(std::shared_ptr<const CurveClass> klass) noexcept;
        CurveInstance(const CurveClass& klass);
        CurveInstance(CurveClass&& klass);

        bool ApplyDynamicState(const Environment& env, const DrawCall& draw, const DrawGeometryHandle& geometry,
            Device& device, ProgramState& program, RasterState& raster) const override;

        ShaderSource GetShader(const Environment& env, const Device& device) const override;
        std::string GetShaderId(const Environment& env) const override;
        std::string GetShaderName(const Environment& env) const override;

        DrawGeometryHandle GetGeometry(const Environment& env, Device& device) const override;
        DrawGeometryBuffer Construct(const Environment& env) const override;

        Type GetType() const override;
        DrawPrimitive GetDrawPrimitive() const override;
        SpatialMode GetSpatialMode() const override;

    private:
        std::shared_ptr<const CurveClass> mClass;
    };

} // namespace
