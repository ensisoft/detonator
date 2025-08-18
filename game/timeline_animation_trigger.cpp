// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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

#include <set>

#include "base/assert.h"
#include "base/logging.h"
#include "base/hash.h"
#include "data/writer.h"
#include "data/reader.h"
#include "game/entity_node.h"
#include "game/entity_node_drawable_item.h"
#include "game/timeline_animation_trigger.h"

namespace game
{

AnimationTriggerClass::AnimationTriggerClass(Type type) noexcept
  : mType(type)
  , mId(base::RandomString(10))
{
    mFlags.set(Flags::Enabled, true);
}

std::unique_ptr<AnimationTriggerClass> AnimationTriggerClass::Copy() const
{
    return std::make_unique<AnimationTriggerClass>(*this);
}
std::unique_ptr<AnimationTriggerClass> AnimationTriggerClass::Clone() const
{
    auto ret = std::make_unique<AnimationTriggerClass>(*this);
    ret->mId = base::RandomString(10);
    return ret;
}

size_t AnimationTriggerClass::GetHash() const noexcept
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mTargetNodeId);
    hash = base::hash_combine(hash, mTimelineId);
    hash = base::hash_combine(hash, mType);
    hash = base::hash_combine(hash, mTime);
    hash = base::hash_combine(hash, mFlags);

    std::set<std::string> keys;
    for (const auto& pair : mParameters)
        keys.insert(pair.first);

    for (const auto& key : keys)
    {
        hash = base::hash_combine(hash, key);
        hash = base::hash_combine(hash, *base::SafeFind(mParameters, key));
    }
    return hash;
}
void AnimationTriggerClass::IntoJson(data::Writer& data) const
{
    data.Write("trigger-id"  , mId);
    data.Write("trigger-name", mName);
    data.Write("trigger-type", mType);
    data.Write("trigger-time", mTime);
    data.Write("trigger-target-node-id", mTargetNodeId);
    data.Write("trigger-timeline-id", mTimelineId);
    data.Write("trigger-flags", mFlags);

    std::set<std::string> keys;
    for (const auto& pair : mParameters)
        keys.insert(pair.first);

    for (const auto& key : keys)
    {
        auto chunk = data.NewWriteChunk();
        chunk->Write("trigger-param-name", key);
        chunk->Write("trigger-param-value", *base::SafeFind(mParameters, key));
        data.AppendChunk("trigger-parameters", std::move(chunk));
    }
}

bool AnimationTriggerClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("trigger-id"  , &mId);
    ok &= data.Read("trigger-name", &mName);
    ok &= data.Read("trigger-type", &mType);
    ok &= data.Read("trigger-time", &mTime);
    ok &= data.Read("trigger-target-node-id", &mTargetNodeId);
    ok &= data.Read("trigger-timeline-id", &mTimelineId);
    ok &= data.Read("trigger-flags", &mFlags);

    for (unsigned i=0; i<data.GetNumChunks("trigger-parameters"); ++i)
    {
        const auto& chunk = data.GetReadChunk("trigger-parameters", i);
        std::string name;
        AnimationTriggerParam  value;
        ok &= chunk->Read("trigger-param-name", &name);
        ok &= chunk->Read("trigger-param-value", &value);
        mParameters[name] = value;
    }
    return ok;
}

bool AnimationTrigger::Validate(const EntityNode& node) const
{
    const auto type = mClass->GetType();
    if (type == Type::EmitParticlesTrigger)
    {
        if (!node.HasDrawable()) {
            WARN("Timeline trigger can't apply on a node without a drawable attachment. [trigger='%1']",
                 mClass->GetName());
            return false;
        }
        if (!HasParam<int>("count"))
        {
            WARN("Timeline trigger has a missing parameter (int) 'count'. [trigger='%1']",
                 mClass->GetName());
            return false;
        }
    }
    else if (type == Type::RunSpriteCycle)
    {
        if (!node.HasDrawable())
        {
            WARN("Time trigger can't apply on a node without a drawable attachment. [trigger='%1']",
                mClass->GetName());
            return false;
        }
        if (!HasParam<std::string>("sprite-cycle-id"))
        {
            WARN("Timeline trigger has a missing parameter (string) 'sprite-cycle-id'. [trigger='%1']",
                mClass->GetName());
            return false;
        }
        if (!HasParam<float>("sprite-cycle-delay"))
        {
            WARN("Timeline trigger has a missing parameter (float) 'sprite-cycle-delay'. [trigger='%1']",
                mClass->GetName());
            return false;
        }
    } else BUG("Unhandled animation trigger type.");
    return true;
}

void AnimationTrigger::Trigger(game::EntityNode& node)
{
    if (!mClass->IsEnabled())
        return;

    const auto type = mClass->GetType();
    if (type == Type::EmitParticlesTrigger)
    {
        auto* drawable = node.GetDrawable();
        if (!drawable)
            return;
        DrawableItem::Command cmd;
        cmd.name = "EmitParticles";
        cmd.args["count"] = GetParam<int>("count");
        drawable->EnqueueCommand(std::move(cmd));
    }
    else if (type == Type::RunSpriteCycle)
    {
        auto* drawable = node.GetDrawable();
        if (!drawable)
            return;
        if (!HasParam<std::string>("sprite-cycle-id"))
            return;
        DrawableItem::Command cmd;
        cmd.name = "RunSpriteCycle";
        cmd.args["id"]    = GetParam<std::string>("sprite-cycle-id");
        cmd.args["delay"] = GetParam<float>("sprite-cycle-delay");
        drawable->EnqueueCommand(std::move(cmd));
    }
    else BUG("Unhandled animation trigger type.");
}

template<typename T>
T AnimationTrigger::GetParam(const std::string& name) const
{
    const auto& map = mClass->GetParameters();
    if (const auto* value = base::SafeFind(map,name))
    {
        ASSERT(std::holds_alternative<T>(*value));
        return std::get<T>(*value);
    }
    BUG("No such animation trigger parameter.");
    return T();
}

template<typename T>
bool AnimationTrigger::HasParam(const std::string& name) const
{
    const auto& map = mClass->GetParameters();
    if (const auto* value = base::SafeFind(map, name))
    {
        if (std::holds_alternative<T>(*value))
            return true;
    }
    return false;
}

} // namespace