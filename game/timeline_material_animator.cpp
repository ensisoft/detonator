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

#include "base/assert.h"
#include "base/logging.h"
#include "base/hash.h"
#include "data/reader.h"
#include "data/writer.h"
#include "game/timeline_material_animator.h"
#include "game/entity_node.h"
#include "game/entity_node_drawable_item.h"

namespace game
{

void MaterialAnimatorClass::IntoJson(data::Writer& data) const
{
    data.Write("id",       mId);
    data.Write("cname",    mName);
    data.Write("node",     mNodeId);
    data.Write("timeline", mTimelineId);
    data.Write("method",   mInterpolation);
    data.Write("start",    mStartTime);
    data.Write("duration", mDuration);
    data.Write("flags",    mFlags);
    for (const auto& [key, val] : mMaterialParams)
    {
        auto chunk = data.NewWriteChunk();
        chunk->Write("name", key);
        chunk->Write("value", val);
        data.AppendChunk("params", std::move(chunk));
    }
}

bool MaterialAnimatorClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("id",       &mId);
    ok &= data.Read("cname",    &mName);
    ok &= data.Read("node",     &mNodeId);
    ok &= data.Read("timeline", &mTimelineId);
    ok &= data.Read("method",   &mInterpolation);
    ok &= data.Read("start",    &mStartTime);
    ok &= data.Read("duration", &mDuration);
    ok &= data.Read("flags",    &mFlags);
    for (unsigned i=0; i<data.GetNumChunks("params"); ++i)
    {
        const auto& chunk = data.GetReadChunk("params", i);
        std::string name;
        MaterialParam  value;
        ok &= chunk->Read("name",  &name);
        ok &= chunk->Read("value", &value);
        mMaterialParams[std::move(name)] = value;
    }
    return ok;
}
std::size_t MaterialAnimatorClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mNodeId);
    hash = base::hash_combine(hash, mTimelineId);
    hash = base::hash_combine(hash, mInterpolation);
    hash = base::hash_combine(hash, mStartTime);
    hash = base::hash_combine(hash, mDuration);

    std::set<std::string> keys;
    for (const auto& [key, val] : mMaterialParams)
        keys.insert(key);

    for (const auto& key : keys)
    {
        const auto& val = *base::SafeFind(mMaterialParams, key);
        hash = base::hash_combine(hash, key);
        hash = base::hash_combine(hash, val);
    }
    hash = base::hash_combine(hash, mFlags);
    return hash;
}

void MaterialAnimator::Start(EntityNode& node)
{
    const auto* draw = node.GetDrawable();
    if (draw == nullptr)
    {
        WARN("Entity node has no drawable item. [node='%1']", node.GetName());
        return;
    }
    const auto& params = mClass->GetMaterialParams();
    for (const auto& [key, val] : params)
    {
        if (const auto* p = draw->FindMaterialParam(key))
            mStartValues[key] = *p;
        else WARN("Entity node material parameter was not found. [node='%1', param='%2']", node.GetName(), key);
    }
}

void MaterialAnimator::Apply(EntityNode& node, float t)
{
    if (auto* draw = node.GetDrawable())
    {
        for (const auto& [key, beg_value] : mStartValues)
        {
            const auto end_value = *mClass->FindMaterialParam(key);

            if (std::holds_alternative<int>(end_value))
                draw->SetMaterialParam(key, Interpolate<int>(beg_value, end_value, t));
            else if (std::holds_alternative<float>(end_value))
                draw->SetMaterialParam(key, Interpolate<float>(beg_value, end_value, t));
            else if (std::holds_alternative<glm::vec2>(end_value))
                draw->SetMaterialParam(key, Interpolate<glm::vec2>(beg_value, end_value, t));
            else if (std::holds_alternative<glm::vec3>(end_value))
                draw->SetMaterialParam(key, Interpolate<glm::vec3>(beg_value, end_value, t));
            else if (std::holds_alternative<glm::vec4>(end_value))
                draw->SetMaterialParam(key, Interpolate<glm::vec4>(beg_value, end_value, t));
            else if (std::holds_alternative<Color4f>(end_value))
                draw->SetMaterialParam(key, Interpolate<Color4f>(beg_value, end_value, t));
            else if (std::holds_alternative<std::string>(end_value)) {
                // intentionally empty, can't interpolate
            } else  BUG("Unhandled material parameter type.");
        }
    }
}
void MaterialAnimator::Finish(EntityNode& node)
{
    if (auto* draw = node.GetDrawable())
    {
        const auto& params = mClass->GetMaterialParams();
        for (const auto& [key, val] : params)
        {
            draw->SetMaterialParam(key, val);
        }
    }
}

} // namespace
