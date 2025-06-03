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

#include "config.h"

#include "warnpush.h"
#include "warnpop.h"

#include <cstdint>
#include <algorithm>
#include <vector>
#include <tuple>
#include <set>
#include <limits>

#include "base/assert.h"
#include "base/logging.h"
#include "base/utility.h"
#include "base/hash.h"
#include "data/writer.h"
#include "data/reader.h"
#include "game/tilemap.h"
#include "game/loader.h"

namespace game
{

TilemapClass::TilemapClass()
  : mId(base::RandomString(10))
{}
TilemapClass::TilemapClass(const TilemapClass& other)
{
    mId          = other.mId;
    mName        = other.mName;
    mWidth       = other.mWidth;
    mHeight      = other.mHeight;
    mTileWorldSize = other.mTileWorldSize;
    mTileRenderScale = other.mTileRenderScale;
    mScriptFile  = other.mScriptFile;
    mPerspective = other.mPerspective;
    for (auto& layer : other.mLayers)
    {
        mLayers.push_back(std::make_shared<TilemapLayerClass>(*layer));
    }
}

void TilemapClass::AddLayer(const TilemapLayerClass& layer)
{
    mLayers.push_back(std::make_shared<TilemapLayerClass>(layer));
}
void TilemapClass::AddLayer(TilemapLayerClass&& layer)
{
    mLayers.push_back(std::make_shared<TilemapLayerClass>(std::move(layer)));
}

void TilemapClass::AddLayer(std::shared_ptr<TilemapLayerClass> klass)
{
    mLayers.push_back(klass);
}

void TilemapClass::DeleteLayer(size_t index)
{
    base::SafeErase(mLayers, index);
}

void TilemapClass::SwapLayers(size_t src, size_t dst) noexcept
{
    ASSERT(src < mLayers.size());
    ASSERT(dst < mLayers.size());
    std::swap(mLayers[src], mLayers[dst]);
}

TilemapLayerClass& TilemapClass::GetLayer(size_t index)
{
    return *base::SafeIndex(mLayers, index);
}

TilemapLayerClass* TilemapClass::FindLayerById(const std::string& id)
{
    for (auto& layer : mLayers)
    {
        if (layer->GetId() == id)
            return layer.get();
    }
    return nullptr;
}

TilemapLayerClass* TilemapClass::FindLayerByName(const std::string& name)
{
    for (auto& layer : mLayers)
    {
        if (layer->GetName() == name)
            return layer.get();
    }
    return nullptr;
}

const TilemapLayerClass& TilemapClass::GetLayer(size_t index) const
{
    return *base::SafeIndex(mLayers, index);
}

const TilemapLayerClass* TilemapClass::FindLayerById(const std::string& id) const
{
    for (auto& layer : mLayers)
    {
        if (layer->GetId() == id)
            return layer.get();
    }
    return nullptr;
}

const TilemapLayerClass* TilemapClass::FindLayerByName(const std::string& name) const
{
    for (auto& layer : mLayers)
    {
        if (layer->GetName() == name)
            return layer.get();
    }
    return nullptr;
}

size_t TilemapClass::FindLayerIndex(const TilemapLayerClass* layer) const
{
    for (size_t i=0; i<mLayers.size(); ++i)
    {
        if (mLayers[i]->GetId() == layer->GetId())
            return i;
    }
    return mLayers.size();
}

size_t TilemapClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mScriptFile);
    hash = base::hash_combine(hash, mWidth);
    hash = base::hash_combine(hash, mHeight);
    hash = base::hash_combine(hash, mTileWorldSize),
    hash = base::hash_combine(hash, mTileRenderScale);
    hash = base::hash_combine(hash, mPerspective);
    for (const auto& layer : mLayers)
    {
        hash = base::hash_combine(hash, layer->GetHash());
    }
    return hash;
}

std::shared_ptr<TilemapLayerClass> TilemapClass::GetSharedLayerClass(size_t index)
{
    return base::SafeIndex(mLayers, index);
}
std::shared_ptr<const TilemapLayerClass> TilemapClass::GetSharedLayerClass(size_t index) const
{
    return base::SafeIndex(mLayers, index);
}

std::shared_ptr<TilemapLayerClass> TilemapClass::FindSharedLayerClass(const std::string& id)
{
    for (auto& klass : mLayers)
    {
        if (klass->GetId() == id)
            return klass;
    }
    return nullptr;
}

std::shared_ptr<const TilemapLayerClass> TilemapClass::FindSharedLayerClass(const std::string& id) const
{
    for (auto& klass : mLayers)
    {
        if (klass->GetId() == id)
            return klass;
    }
    return nullptr;
}


TilemapClass TilemapClass::Clone() const
{
    TilemapClass ret;
    ret.mWidth       = mWidth;
    ret.mHeight      = mHeight;
    ret.mName        = mName;
    ret.mTileWorldSize   = mTileWorldSize;
    ret.mTileRenderScale = mTileRenderScale;
    ret.mScriptFile  = mScriptFile;
    ret.mPerspective = mPerspective;
    for (const auto& layer : mLayers)
    {
        auto clone = std::make_shared<TilemapLayerClass>(*layer);
        clone->SetId(base::RandomString(10));
        ret.mLayers.push_back(clone);
    }
    return ret;
}

TilemapClass& TilemapClass::operator=(const TilemapClass& other)
{
    if (this ==&other)
        return *this;

    TilemapClass tmp(other);
    std::swap(mId,          tmp.mId);
    std::swap(mWidth,       tmp.mWidth);
    std::swap(mHeight,      tmp.mHeight);
    std::swap(mName,        tmp.mName);
    std::swap(mScriptFile,  tmp.mScriptFile);
    std::swap(mLayers,      tmp.mLayers);
    std::swap(mTileWorldSize,   tmp.mTileWorldSize);
    std::swap(mTileRenderScale, tmp.mTileRenderScale);
    std::swap(mPerspective,  tmp.mPerspective);
    return *this;
}

void TilemapClass::IntoJson(data::Writer& data) const
{
    data.Write("id",           mId);
    data.Write("name",         mName);
    data.Write("script",       mScriptFile);
    data.Write("width",        mWidth);
    data.Write("height",       mHeight);
    data.Write("tile_world_size",   mTileWorldSize);
    data.Write("tile_render_scale", mTileRenderScale);
    data.Write("perspective",  mPerspective);

    for (const auto& layer : mLayers)
    {
        auto chunk = data.NewWriteChunk();
        layer->IntoJson(*chunk);
        data.AppendChunk("layers", std::move(chunk));
    }
}

bool TilemapClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",           &mId);
    ok &= data.Read("name",         &mName);
    ok &= data.Read("script",       &mScriptFile);
    ok &= data.Read("width",        &mWidth);
    ok &= data.Read("height",       &mHeight);
    ok &= data.Read("tile_world_size",   &mTileWorldSize);
    ok &= data.Read("tile_render_scale",  &mTileRenderScale);
    ok &= data.Read("perspective",  &mPerspective);

    for (unsigned i=0; i<data.GetNumChunks("layers"); ++i)
    {
        const auto& chunk = data.GetReadChunk("layers", i);
        auto layer = std::make_shared<TilemapLayerClass>();
        if (!layer->FromJson(*chunk))
            WARN("Failed to load tilemap layer. [map='%1', layer='%2']", mName, layer->GetName());

        mLayers.push_back(layer);
    }
    return ok;
}

Tilemap::Tilemap(const std::shared_ptr<const TilemapClass>& klass)
  : mClass(klass)
{
    for (size_t i=0; i<klass->GetNumLayers(); ++i)
    {
        const auto& layer_klass = klass->GetSharedLayerClass(i);
        const auto map_width    = klass->GetMapWidth();
        const auto map_height   = klass->GetMapHeight();
        mLayers.push_back(CreateTilemapLayer(layer_klass, map_width, map_height));
    }
}

Tilemap::Tilemap(const TilemapClass& klass)
  : Tilemap(std::make_shared<const TilemapClass>(klass))
{}

bool Tilemap::Load(const Loader& loader)
{
    bool success = true;
    for (auto& layer : mLayers)
    {
        const auto& klass = layer->GetClass();
        Loader::TilemapDataDesc desc;
        desc.layer     = klass.GetId();
        desc.data      = klass.GetDataId();
        desc.uri       = klass.GetDataUri();
        desc.read_only = klass.IsReadOnly();
        auto data = loader.LoadTilemapData(desc);
        if (data)
        {
            layer->Load(data);
            DEBUG("Loaded tilemap layer. [layer='%1']", klass.GetName());
        }
        else
        {
            WARN("Failed to load tilemap layer data. [layer='%1']", klass.GetName());
            success = false;
        }
    }
    DEBUG("Loaded tilemap. [map='%1', layers=%2]", mClass->GetName(), mClass->GetNumLayers());
    return success;
}

void Tilemap::AddLayer(std::unique_ptr<TilemapLayer> layer)
{
    mLayers.push_back(std::move(layer));
}

void Tilemap::DeleteLayer(std::size_t index)
{
    base::SafeErase(mLayers, index);
}

const TilemapLayer& Tilemap::GetLayer(size_t index) const
{
    return *base::SafeIndex(mLayers, index);
}
const TilemapLayer* Tilemap::FindLayerByClassName(const std::string& name) const
{
    for (auto& layer : mLayers)
    {
        if (layer->GetClassName() == name)
            return layer.get();
    }
    return nullptr;
}
const TilemapLayer* Tilemap::FindLayerByClassId(const std::string& id) const
{
    for (auto& layer : mLayers)
    {
        if (layer->GetClassId() == id)
            return layer.get();
    }
    return nullptr;
}




TilemapLayer& Tilemap::GetLayer(size_t index)
{
    return *base::SafeIndex(mLayers, index);
}
TilemapLayer* Tilemap::FindLayerByClassName(const std::string& name)
{
    for (auto& layer : mLayers)
    {
        if (layer->GetClassName() == name)
            return layer.get();
    }
    return nullptr;
}
TilemapLayer* Tilemap::FindLayerByClassId(const std::string& id)
{
    for (auto& layer : mLayers)
    {
        if (layer->GetClassId() == id)
            return layer.get();
    }
    return nullptr;
}

std::size_t Tilemap::FindLayerIndex(const TilemapLayer* layer) const noexcept
{
    for (size_t i=0; i<mLayers.size(); ++i)
    {
        if (mLayers[i]->GetClassId() == layer->GetClassId())
            return i;
    }
    return mLayers.size();
}

void Tilemap::SwapLayers(size_t src, size_t dst) noexcept
{
    ASSERT(src < mLayers.size());
    ASSERT(dst < mLayers.size());
    std::swap(mLayers[src], mLayers[dst]);
}

TileRowCol Tilemap::MapFromPlane(const glm::vec2& xy, const TilemapLayer& layer) const noexcept
{
    const auto layer_tile_scaler = layer.GetTileSizeScaler();
    const auto layer_width_tiles = layer.GetWidth();
    const auto layer_height_tiles = layer.GetHeight();

    const auto layer_tile_width = GetTileWidth() * layer_tile_scaler;
    const auto layer_tile_height = GetTileHeight() * layer_tile_scaler;

    const auto layer_width_units = layer_tile_width * layer_width_tiles;
    const auto layer_height_units = layer_tile_height * layer_height_tiles;

    const auto pos = glm::vec2{math::clamp(0.0f, layer_width_units, xy.x),
                               math::clamp(0.0f, layer_height_units, xy.y)};

    TileRowCol ret;
    ret.col = math::clamp(0u, layer_width_tiles-1, unsigned(pos.x / layer_tile_width));
    ret.row = math::clamp(0u, layer_height_tiles-1, unsigned(pos.y / layer_tile_height));
    return ret;
}

TileRowCol Tilemap::MapFromPlane(const glm::vec2& xy, size_t layer_index) const noexcept
{
    ASSERT(layer_index < mLayers.size());
    return MapFromPlane(xy, *mLayers[layer_index]);
}

bool Tilemap::TestPlaneCoordinate(const glm::vec2& xy, const TilemapLayer& layer) const noexcept
{
    if (xy.x < 0.0f || xy.y < 0.0f)
        return false;

    const auto layer_tile_scaler = layer.GetTileSizeScaler();
    const auto layer_width_tiles = layer.GetWidth();
    const auto layer_height_tiles = layer.GetHeight();

    const auto layer_tile_width = GetTileWidth() * layer_tile_scaler;
    const auto layer_tile_height = GetTileHeight() * layer_tile_scaler;

    const auto layer_width_units = layer_tile_width * layer_width_tiles;
    const auto layer_height_units = layer_tile_height * layer_height_tiles;

    if (xy.x > layer_width_units || xy.y > layer_height_units)
        return false;

    return true;
}

bool Tilemap::TestPlaneCoordinate(const glm::vec2& xy, size_t layer_index) const noexcept
{
    ASSERT(layer_index < mLayers.size());
    return TestPlaneCoordinate(xy, *mLayers[layer_index]);
}


std::unique_ptr<Tilemap> CreateTilemap(const std::shared_ptr<const TilemapClass>& klass)
{
    return std::make_unique<Tilemap>(klass);
}
std::unique_ptr<Tilemap> CreateTilemap(const TilemapClass& klass)
{
    return std::make_unique<Tilemap>(klass);
}

} // namespace
