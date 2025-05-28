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

#include "config.h"

#include "data/reader.h"
#include "data/writer.h"
#include "game/transform.h"
#include "game/entity_placement.h"

namespace game
{

EntityPlacement::EntityPlacement()
  : mClassId(base::RandomString(10))
{
    SetFlag(Flags::VisibleInGame, true);
    SetFlag(Flags::VisibleInEditor, true);
}

void EntityPlacement::SetFlag(Flags flag, bool on_off) noexcept
{
    mFlagValBits.set(flag, on_off);
    mFlagSetBits.set(flag, true);
}

void EntityPlacement::SetEntity(std::shared_ptr<const EntityClass> klass)
{
    mEntityId = klass->GetId();
    mEntity   = klass;
}
void EntityPlacement::ResetEntity() noexcept
{
    mEntityId.clear();
    mEntity.reset();
}
void EntityPlacement::ResetEntityParams() noexcept
{
    mIdleAnimationId.clear();
    mLifetime.reset();
    mFlagSetBits.clear();
    mFlagValBits.clear();
    mScriptVarValues.clear();
}

EntityPlacement::ScriptVarValue* EntityPlacement::FindScriptVarValueById(const std::string& id)
{
    for (auto& value : mScriptVarValues)
    {
        if (value.id == id)
            return &value;
    }
    return nullptr;
}
const EntityPlacement::ScriptVarValue* EntityPlacement::FindScriptVarValueById(const std::string& id) const
{
    for (auto& value : mScriptVarValues)
    {
        if (value.id == id)
            return &value;
    }
    return nullptr;
}

bool EntityPlacement::DeleteScriptVarValueById(const std::string& id)
{
    for (auto it=mScriptVarValues.begin(); it != mScriptVarValues.end(); ++it)
    {
        const auto& val = *it;
        if (val.id == id)
        {
            mScriptVarValues.erase(it);
            return true;
        }
    }
    return false;
}

void EntityPlacement::SetScriptVarValue(const ScriptVarValue& value)
{
    for (auto& val : mScriptVarValues)
    {
        if (val.id == value.id)
        {
            val.value = value.value;
            return;
        }
    }
    mScriptVarValues.push_back(value);
}

void EntityPlacement::ClearStaleScriptValues(const EntityClass& klass)
{
    for (auto it=mScriptVarValues.begin(); it != mScriptVarValues.end();)
    {
        const auto& val = *it;
        const auto* var = klass.FindScriptVarById(val.id);
        if (var == nullptr) {
            it = mScriptVarValues.erase(it);
        } else if (ScriptVar::GetTypeFromVariant(val.value) != var->GetType()) {
            it = mScriptVarValues.erase(it);
        } else if(var->IsReadOnly()) {
            it = mScriptVarValues.erase(it);
        } else if (ScriptVar::SameSame(val.value, var->GetVariantValue())) {
            it = mScriptVarValues.erase(it);
        } else ++it;
    }
}

std::size_t EntityPlacement::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, mEntityId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mPosition);
    hash = base::hash_combine(hash, mScale);
    hash = base::hash_combine(hash, mRotation);
    hash = base::hash_combine(hash, mFlagValBits);
    hash = base::hash_combine(hash, mFlagSetBits);
    hash = base::hash_combine(hash, mRenderLayer);
    hash = base::hash_combine(hash, mMapLayer);
    hash = base::hash_combine(hash, mParentRenderTreeNodeId);
    hash = base::hash_combine(hash, mIdleAnimationId);
    hash = base::hash_combine(hash, mLifetime);
    hash = base::hash_combine(hash, mTagString);

    for (const auto& value : mScriptVarValues)
    {
        hash = base::hash_combine(hash, value.id);
        hash = base::hash_combine(hash, ScriptVar::GetHash(value.value));
    }
    return hash;
}

glm::mat4 EntityPlacement::GetNodeTransform() const
{
    Transform transform;
    transform.Scale(mScale);
    transform.RotateAroundZ(mRotation);
    transform.Translate(mPosition);
    return transform.GetAsMatrix();
}

EntityPlacement EntityPlacement::Clone() const
{
    EntityPlacement copy(*this);
    copy.mClassId = base::RandomString(10);
    return copy;
}

void EntityPlacement::IntoJson(data::Writer& data) const
{
    data.Write("id",                      mClassId);
    data.Write("entity",                  mEntityId);
    data.Write("name",                    mName);
    data.Write("position",                mPosition);
    data.Write("scale",                   mScale);
    data.Write("rotation",                mRotation);
    data.Write("flag_val_bits",           mFlagValBits);
    data.Write("flag_set_bits",           mFlagSetBits);
    data.Write("render_layer",            mRenderLayer);
    data.Write("map_layer",               mMapLayer);
    data.Write("parent_render_tree_node", mParentRenderTreeNodeId);
    data.Write("idle_animation_id",       mIdleAnimationId);
    data.Write("lifetime",                mLifetime);
    data.Write("tag",                     mTagString);

    for (const auto& value : mScriptVarValues)
    {
        auto chunk = data.NewWriteChunk();
        chunk->Write("id", value.id);
        ScriptVar::IntoJson(value.value, *chunk);
        data.AppendChunk("values", std::move(chunk));
    }
}

bool EntityPlacement::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",                      &mClassId);
    ok &= data.Read("entity",                  &mEntityId);
    ok &= data.Read("name",                    &mName);
    ok &= data.Read("position",                &mPosition);
    ok &= data.Read("scale",                   &mScale);
    ok &= data.Read("rotation",                &mRotation);
    ok &= data.Read("flag_val_bits",           &mFlagValBits);
    ok &= data.Read("flag_set_bits",           &mFlagSetBits);
    ok &= data.Read("parent_render_tree_node", &mParentRenderTreeNodeId);
    ok &= data.Read("idle_animation_id",       &mIdleAnimationId);
    ok &= data.Read("lifetime",                &mLifetime);
    ok &= data.Read("tag",                     &mTagString);
    ok &= data.Read("map_layer",               &mMapLayer);

    if (data.HasValue("layer") && !data.HasValue("render_layer"))
        ok &= data.Read("layer", &mRenderLayer);
    else ok &= data.Read("render_layer", &mRenderLayer);

    for (unsigned i=0; i<data.GetNumChunks("values"); ++i)
    {
        const auto& chunk = data.GetReadChunk("values", i);
        ScriptVarValue value;
        ok &= chunk->Read("id",           &value.id);
        ok &= ScriptVar::FromJson(*chunk, &value.value);
        mScriptVarValues.push_back(std::move(value));
    }
    return ok;
}

} // namespace
