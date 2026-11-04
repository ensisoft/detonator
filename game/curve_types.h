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
#include <array>

#include "data/fwd.h"
#include "game/types.h"

namespace game
{
    class CurvePoint
    {
    public:
        static constexpr auto DefaultWidth = 0.1f;

        CurvePoint() = default;

        CurvePoint(const Float3& position, const Float3& look_at,
            const Float2& data, float width = DefaultWidth);

        const auto& GetPosition() const noexcept
        { return mPosition; }
        const auto& GetLookAt() const noexcept
        { return mLookAt; }
        const auto& GetData() const noexcept
        { return mData; }
        auto GetWidth() const noexcept
        { return mWidth;}

        void SetPosition(const Float3& position)
        { mPosition = position; }
        void SetLookAt(const Float3& look_at)
        { mLookAt = look_at; }
        void SetData(const Float2& data)
        { mData = data; }
        void SetWidth(const float width)
        { mWidth = width; }

        void IntoJson(data::Writer& writer) const;
        bool FromJson(const data::Reader& reader);
    private:
        Float3 mPosition;
        Float3 mLookAt;
        Float2 mData;
        float mWidth = DefaultWidth;
    };

    class Curve
    {
    public:
        Curve() = default;
        Curve(const std::vector<CurvePoint>& points);
        Curve(std::vector<CurvePoint>&& points);

        void SetPoint(const CurvePoint& point, size_t index);
        void AppendPoint(const CurvePoint& point);
        void PrependPoint(const CurvePoint& point);
        void ErasePoint(size_t index);

        size_t GetHash() const noexcept;

        void IntoJson(data::Writer&) const;
        bool FromJson(const data::Reader& reader);

    private:
        std::vector<CurvePoint> mPoints;
    };
} // name