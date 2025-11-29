// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
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

#include <vector>
#include <string>
#include <memory>
#include <variant>
#include <tuple>
#include <unordered_map>
#include <limits>
#include <cstdint>

#include "base/assert.h"
#include "data/fwd.h"
#include "game/enum.h"
#include "game/types.h"
#include "game/tilemap_types.h"
#include "game/tilemap_layer.h"

namespace game
{
    class TilemapClass
    {
    public:
        using Perspective = RenderView;

        TilemapClass();
        TilemapClass(const TilemapClass& other);

        inline void SetName(std::string name) noexcept
        { mName = std::move(name);}
        inline void SetScriptFile(std::string file) noexcept
        { mScriptFile = std::move(file); }
        inline void SetMapWidth(unsigned width) noexcept
        { mWidth = width; }
        inline void SetMapHeight(unsigned height) noexcept
        { mHeight = height;}
        inline void SetTileWidth(float width) noexcept
        { mTileWorldSize.x = width; }
        inline void SetTileHeight(float height) noexcept
        { mTileWorldSize.y = height; }
        inline void SetTileDepth(float depth) noexcept
        { mTileWorldSize.z = depth; }
        inline void SetPerspective(Perspective perspective) noexcept
        { mPerspective = perspective; }
        inline void SetTileRenderScale(glm::vec2 scale) noexcept
        { mTileRenderScale = scale; }
        inline void SetTileSize(glm::vec3 size) noexcept
        { mTileWorldSize = size; }
        inline void SetTileRenderWidthScale(float scale) noexcept
        { mTileRenderScale.x = scale; }
        inline void SetTileRenderHeightScale(float scale) noexcept
        { mTileRenderScale.y = scale; }
        inline void SetRenderLayer(int layer) noexcept
        { mRenderLayer = layer; }

        inline std::size_t GetNumLayers() const noexcept
        { return mLayers.size(); }
        inline std::string GetId() const noexcept
        { return mId; }
        inline std::string GetName() const noexcept
        { return mName; }
        inline std::string GetScriptFile() const noexcept
        { return mScriptFile;}
        inline unsigned GetMapWidth() const noexcept
        { return mWidth; }
        inline unsigned GetMapHeight() const noexcept
        { return mHeight;}
        inline float GetTileWidth() const noexcept
        { return mTileWorldSize.x; }
        inline float GetTileHeight() const noexcept
        { return mTileWorldSize.y; }
        inline float GetTileDepth() const noexcept
        { return mTileWorldSize.z; }
        inline Perspective GetPerspective() const noexcept
        { return mPerspective; }
        inline glm::vec3 GetTileSize() const noexcept
        { return mTileWorldSize; }
        inline glm::vec2 GetTileRenderScale() const noexcept
        { return mTileRenderScale; }
        inline float GetTileRenderWidthScale() const noexcept
        { return mTileRenderScale.x; }
        inline float GetTileRenderHeightScale() const noexcept
        { return mTileRenderScale.y; }
        inline int GetRenderLayer() const noexcept
        { return mRenderLayer; }

        void AddLayer(const TilemapLayerClass& layer);
        void AddLayer(TilemapLayerClass&& layer);
        void AddLayer(std::shared_ptr<TilemapLayerClass> klass);
        void DeleteLayer(size_t index);

        void SwapLayers(size_t src, size_t dst) noexcept;


        TilemapLayerClass& GetLayer(size_t index);
        TilemapLayerClass* FindLayerById(const std::string& id);
        TilemapLayerClass* FindLayerByName(const std::string& name);

        const TilemapLayerClass& GetLayer(size_t index) const;
        const TilemapLayerClass* FindLayerById(const std::string& id) const;
        const TilemapLayerClass* FindLayerByName(const std::string& name) const;

        std::size_t FindLayerIndex(const TilemapLayerClass* layer) const;

        std::size_t GetHash() const;
        std::shared_ptr<TilemapLayerClass> GetSharedLayerClass(size_t index);
        std::shared_ptr<const TilemapLayerClass> GetSharedLayerClass(size_t index) const;
        std::shared_ptr<TilemapLayerClass> FindSharedLayerClass(const std::string& id);
        std::shared_ptr<const TilemapLayerClass> FindSharedLayerClass(const std::string& id) const;

        TilemapClass Clone() const;

        TilemapClass& operator=(const TilemapClass& other);

        void IntoJson(data::Writer& data) const;

        bool FromJson(const data::Reader& data);
    private:
        std::string mId;
        std::string mName;
        std::string mScriptFile;
        unsigned mWidth = 0;
        unsigned mHeight = 0;
        int mRenderLayer = 0;
        glm::vec3 mTileWorldSize = {0.0f, 0.0f, 0.0f};
        glm::vec2 mTileRenderScale = {1.0f, 1.0f};
        Perspective mPerspective = Perspective::AxisAligned;
        std::vector<std::shared_ptr<TilemapLayerClass>> mLayers;
    };


    class Tilemap
    {
    public:
        using Class = TilemapClass;
        using Perspective = TilemapClass::Perspective;

        Tilemap(const std::shared_ptr<const TilemapClass>& klass);
        Tilemap(const TilemapClass& klass);

        // Load the contents of the tilemap instance layers.
        // Returns true if all layers loaded successfully or
        // false if a layer failed to load.
        bool Load(const Loader& loader);

        void AddLayer(std::unique_ptr<TilemapLayer> layer);
        void DeleteLayer(std::size_t index);

        const TilemapLayer& GetLayer(size_t index) const;
        const TilemapLayer* FindLayerByClassName(const std::string& name) const;
        const TilemapLayer* FindLayerByClassId(const std::string& id) const;

        TilemapLayer& GetLayer(size_t index);
        TilemapLayer* FindLayerByClassName(const std::string& name);
        TilemapLayer* FindLayerByClassId(const std::string& id);

        void SwapLayers(size_t src, size_t dst) noexcept;

        std::size_t FindLayerIndex(const TilemapLayer* layer) const noexcept;

        // Map a coordinate on the tile plane to rows and columns.
        // The result will be clamped to the map bounds.
        TileRowCol MapFromPlane(const glm::vec2& xy, const TilemapLayer& layer) const noexcept;
        TileRowCol MapFromPlane(const glm::vec2& xy, size_t layer_index) const noexcept;

        // Test whether a plane xy coordinate is within the tile coordinates
        // or not. Returns true if the coordinate can be converted into
        // a tile (row, col) coordinates without clamping .
        bool TestPlaneCoordinate(const glm::vec2& xy, const TilemapLayer& layer) const noexcept;
        bool TestPlaneCoordinate(const glm::vec2& xy, size_t layer_index) const noexcept;

        inline std::size_t GetNumLayers() const noexcept
        { return mLayers.size(); }
        inline std::string GetClassName() const noexcept
        { return mClass->GetName(); }
        inline std::string GetClassId() const noexcept
        { return mClass->GetId(); }
        inline int GetRenderLayer() const noexcept
        { return mClass->GetRenderLayer(); }
        inline unsigned GetMapWidth() const noexcept
        { return mClass->GetMapWidth(); }
        inline unsigned GetMapHeight() const noexcept
        { return mClass->GetMapHeight(); }
        inline float GetTileWidth() const noexcept
        { return mClass->GetTileWidth(); }
        inline float GetTileHeight() const noexcept
        { return mClass->GetTileHeight(); }
        inline float GetTileDepth() const noexcept
        { return mClass->GetTileDepth(); }
        inline Perspective GetPerspective() const noexcept
        { return mClass->GetPerspective(); }
        inline float GetTileRenderWidthScale() const noexcept
        { return mClass->GetTileRenderWidthScale(); }
        inline float GetTileRenderHeightScale() const noexcept
        { return mClass->GetTileRenderHeightScale(); }
        inline const Class& GetClass() const noexcept
        { return *mClass; }
        inline const Class* operator->() const noexcept
        { return mClass.get(); }
    private:
        std::shared_ptr<const TilemapClass> mClass;
        std::vector<std::unique_ptr<TilemapLayer>> mLayers;

    };

    std::unique_ptr<Tilemap> CreateTilemap(const std::shared_ptr<const TilemapClass>& klass);
    std::unique_ptr<Tilemap> CreateTilemap(const TilemapClass& klass);

} // namespace
