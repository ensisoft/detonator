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

#include "game/entity_node_drawable_item.h"

#include <set>

#include "base/hash.h"
#include "base/logging.h"
#include "data/writer.h"
#include "data/reader.h"

namespace game
{

DrawableItemClass::DrawableItemClass()
{
    mBitFlags.set(Flags::VisibleInGame,    true);
    mBitFlags.set(Flags::UpdateDrawable,   true);
    mBitFlags.set(Flags::UpdateMaterial,   true);
    mBitFlags.set(Flags::RestartDrawable,  true);
    mBitFlags.set(Flags::FlipHorizontally, false);
    mBitFlags.set(Flags::FlipVertically,   false);
    mBitFlags.set(Flags::DoubleSided,      false);
    mBitFlags.set(Flags::DepthTest,        false);
    mBitFlags.set(Flags::PP_EnableBloom,   true);
    mBitFlags.set(Flags::EnableLight,      true);
    mBitFlags.set(Flags::EnableFog,        true);
}

std::size_t DrawableItemClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mBitFlags);
    hash = base::hash_combine(hash, mMaterialId);
    hash = base::hash_combine(hash, mDrawableId);
    hash = base::hash_combine(hash, mLayer);
    hash = base::hash_combine(hash, mRenderPass);
    hash = base::hash_combine(hash, mTimeScale);
    hash = base::hash_combine(hash, mDepth);
    hash = base::hash_combine(hash, mRenderRotation);
    hash = base::hash_combine(hash, mRenderTranslation);
    hash = base::hash_combine(hash, mCoordinateSpace);

    // remember the *unordered* nature of unordered_map
    std::set<std::string> keys;
    for (const auto& param : mMaterialParams)
        keys.insert(param.first);

    for (const auto& key : keys)
    {
        const auto* param = base::SafeFind(mMaterialParams, key);
        hash = base::hash_combine(hash, key);
        hash = base::hash_combine(hash, *param);
    }
    return hash;
}

void DrawableItemClass::IntoJson(data::Writer& data) const
{
    data.Write("flags",       mBitFlags);
    data.Write("material",    mMaterialId);
    data.Write("drawable",    mDrawableId);
    data.Write("layer",       mLayer);
    data.Write("renderpass",  mRenderPass);
    data.Write("timescale",   mTimeScale);
    data.Write("depth",       mDepth);
    data.Write("rotator",     mRenderRotation);
    data.Write("offset",      mRenderTranslation);
    data.Write("coordinate_space", mCoordinateSpace);

    // use an ordered set for persisting the data to make sure
    // that the order in which the uniforms are written out is
    // defined in order to avoid unnecessary changes (as perceived
    // by a version control such as Git) when there's no actual
    // change in the data.
    std::set<std::string> keys;
    for (const auto& [key, value] : mMaterialParams)
        keys.insert(key);

    for (const auto& key : keys)
    {
        const auto* param = base::SafeFind(mMaterialParams, key);
        auto chunk = data.NewWriteChunk();
        chunk->Write("name", key);
        chunk->Write("value", *param);
        data.AppendChunk("material_params", std::move(chunk));
    }
}

template<typename T>
bool ReadMaterialParam(const data::Reader& data, DrawableItemClass::MaterialParam& param)
{
    T value;
    if (!data.Read("value", &value))
        return false;
    param = value;
    return true;
}

bool DrawableItemClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("flags",       &mBitFlags);
    ok &= data.Read("material",    &mMaterialId);
    ok &= data.Read("drawable",    &mDrawableId);
    ok &= data.Read("layer",       &mLayer);
    ok &= data.Read("renderpass",  &mRenderPass);
    ok &= data.Read("timescale",   &mTimeScale);
    ok &= data.Read("depth",       &mDepth);
    ok &= data.Read("rotator",     &mRenderRotation);
    ok &= data.Read("offset",      &mRenderTranslation);
    if (data.HasValue("coordinate_space"))
        ok &= data.Read("coordinate_space", &mCoordinateSpace);

    for (unsigned i=0; i<data.GetNumChunks("material_params"); ++i)
    {
        const auto& chunk = data.GetReadChunk("material_params", i);

        MaterialParam param;
        std::string name;
        bool chunk_ok = true;
        chunk_ok &= chunk->Read("name",  &name);
        chunk_ok &= chunk->Read("value", &param);
        if (chunk_ok)
            mMaterialParams[std::move(name)] = param;

        ok &= chunk_ok;
    }
    return ok;
}


} // namespace
