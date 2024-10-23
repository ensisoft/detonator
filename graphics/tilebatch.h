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

#include "warnpush.h"
#  include <glm/vec2.hpp>
#  include <glm/vec3.hpp>
#include "warnpop.h"

#include <vector>
#include <string>

#include "graphics/drawable.h"
#include "graphics/vertex.h"

namespace gfx
{
   class TileBatch : public Drawable
    {
    public:
        enum class TileShape {
            Automatic, Square, Rectangle
        };
        enum class Projection {
            Dimetric, AxisAligned
        };

        struct Tile {
            Vec3 pos;
            // x = material palette index (for using tile material)
            // y = arbitrary data from the tile map
            Vec2 data;
        };
        TileBatch() = default;
        explicit TileBatch(const std::vector<Tile>& tiles)
          : mTiles(tiles)
        {}
        explicit TileBatch(std::vector<Tile>&& tiles) noexcept
          : mTiles(std::move(tiles))
        {}

        virtual void ApplyDynamicState(const Environment& env, ProgramState& program, RasterState& raster) const override;
        virtual ShaderSource GetShader(const Environment& env, const Device& device) const override;
        virtual std::string GetShaderId(const Environment& env) const override;
        virtual std::string GetShaderName(const Environment& env) const override;
        virtual std::string GetGeometryId(const Environment& env) const override;
        virtual bool Construct(const Environment& env, Geometry::CreateArgs& create) const override;
        virtual Primitive GetPrimitive() const override;
        virtual Type GetType() const override
        { return Type::TileBatch; }
        virtual Usage GetUsage() const override
        { return Usage::Stream; }

        inline void AddTile(const Tile& tile)
        { mTiles.push_back(tile); }
        inline void ClearTiles() noexcept
        { mTiles.clear(); }
        inline size_t GetNumTiles() const noexcept
        { return mTiles.size(); }
        inline const Tile& GetTile(size_t index) const noexcept
        { return mTiles[index]; }
        inline Tile& GetTile(size_t index) noexcept
        { return mTiles[index]; }
        inline glm::vec3 GetTileWorldSize() const noexcept
        { return mTileWorldSize; }
        inline glm::vec2 GetTileRenderSize() const noexcept
        { return mTileRenderSize; }

        // Set the tile width in tile world units.
        inline void SetTileWorldWidth(float width) noexcept
        { mTileWorldSize.x = width; }
        inline void SetTileWorldHeight(float height) noexcept
        { mTileWorldSize.y = height; }
        inline void SetTileWorldDepth(float depth) noexcept
        { mTileWorldSize.z = depth; }
        inline void SetTileWorldSize(const glm::vec3& size) noexcept
        { mTileWorldSize = size; }
        inline void SetTileRenderWidth(float width) noexcept
        { mTileRenderSize.x = width; }
        inline void SetTileRenderHeight(float height) noexcept
        { mTileRenderSize.y = height; }
        inline void SetTileRenderSize(const glm::vec2& size) noexcept
        { mTileRenderSize = size; }
        inline void SetProjection(Projection projection) noexcept
        { mProjection = projection; }

        TileShape ResolveTileShape() const noexcept
        {
            if (mShape == TileShape::Automatic) {
                if (math::equals(mTileRenderSize.x, mTileRenderSize.y))
                    return TileShape::Square;
                else return TileShape::Rectangle;
            }
            return mShape;
        }
        inline TileShape GetTileShape() const noexcept
        { return mShape; }
        inline void SetTileShape(TileShape shape) noexcept
        { mShape = shape; }
    private:
        Projection mProjection = Projection::AxisAligned;
        TileShape mShape = TileShape::Automatic;
        std::vector<Tile> mTiles;
        glm::vec3 mTileWorldSize = {0.0f, 0.0f, 0.0f};
        glm::vec2 mTileRenderSize = {0.0f, 0.0f};
    };
} // namespace