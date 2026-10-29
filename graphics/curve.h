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

#include <vector>
#include <cstddef>
#include <array>

#include "base/bitflag.h"
#include "base/types.h"
#include "base/utility.h"
#include "graphics/drawable.h"
#include "graphics/enum.h"

namespace gfx
{
    class CurvePoint
    {
    public:
        using value_type = float;

        static constexpr auto DefaultWidth = 0.01f;

        CurvePoint() = default;
        CurvePoint(const float x, const float y, const float z, const float width = DefaultWidth)
        {
            mData[0] = x;
            mData[1] = y;
            mData[2] = z;
            mData[5] = width;
        }

        CurvePoint(const base::Float3& position, const float width = DefaultWidth)
        {
            mData[0] = position.x;
            mData[1] = position.y;
            mData[2] = position.z;
            mData[5] = width;
        }
        CurvePoint(const base::Float3& position, const base::Float2& data, const float width = DefaultWidth)
        {
            mData[0] = position.x;
            mData[1] = position.y;
            mData[2] = position.z;
            mData[3] = data.x;
            mData[4] = data.y;
            mData[5] = width;
        }
        float operator[](const size_t index) const noexcept
        {
            return mData[index];
        }

        float& operator[](const size_t index) noexcept
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
        void SetPosition(const base::Float3& position) noexcept
        {
            mData[0] = position.x;
            mData[1] = position.y;
            mData[2] = position.z;
        }
        void SetData(const base::Float2& data) noexcept
        {
            mData[3] = data.x;
            mData[4] = data.y;
        }
        void SetWidth(const float width) noexcept
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
    class CurveClass final : public DrawableClass
    {
    public:
        using CurveType = CurveType;
        using JointStyle = CurveJointStyle;
        using EndCapStyle = CurveEndCapStyle;
        using ControlPoint = CurvePoint;

        enum  class Flags {
            StaticContent
        };

        explicit CurveClass(std::string id = base::RandomString(10),
                            std::string name = "") noexcept
          : mId(std::move(id))
          , mName(std::move(name))
        {}

        void SetCurveType(const CurveType type) noexcept
        { mCurveType = type; }
        void SetJointStyle(const JointStyle style) noexcept
        { mJointStyle = style; }
        void SetEndCapStyle(const EndCapStyle style) noexcept
        { mEndCapStyle = style; }

        auto GetCurveType() const noexcept
        { return mCurveType; }
        auto GetJointStyle() const noexcept
        { return  mJointStyle; }
        auto GetEndCapStyle() const noexcept
        { return mEndCapStyle; }

        void AddPoint(const ControlPoint& point)
        { mPoints.push_back(point); }
        auto GetPointCount() const noexcept
        { return mPoints.size(); }
        const auto& GetPoint(size_t index) const noexcept
        { return mPoints[index]; }
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
    };

    class CurveInstance final : public Drawable
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

    class Curve final : public Drawable
    {
    public:
        using CurveType = CurveType;
        using ControlPoint = CurvePoint;
        using JointStyle = CurveJointStyle;
        using EndCapStyle = CurveEndCapStyle;

        struct Args {
            std::vector<ControlPoint> points;
            CurveType curve_type = CurveType::CatmullRomSpline;
            JointStyle joint_style = JointStyle::Miter;
            EndCapStyle end_cap_style = EndCapStyle::Square;
            bool make_fallback_on_fail = false;
        };

        Curve(const Args& args, std::string id = "");
        Curve(Args&& args, std::string id = "");

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
        void ComputeId();
    private:
        Args mArgs;
        std::string mCurveId;
        std::string mName;
    };

} // namespace
