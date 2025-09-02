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
#include "game/entity_node_mesh_effect.h"
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
    if (type == Type::StartMeshEffect)
    {
        if (!node.HasDrawable())
        {
            WARN("Timeline trigger can't apply on a node without a drawable attachment. [trigger='%1']",
                 mClass->GetName());
            return false;
        }
        if (!node.HasMeshEffect())
        {
            WARN("Timeline trigger can't apply on a node without a mesh effect attachment. [trigger='%1']",
                 mClass->GetName());
            return false;
        }
    }
    else if (type == Type::EmitParticlesTrigger)
    {
        if (!node.HasDrawable()) {
            WARN("Timeline trigger can't apply on a node without a drawable attachment. [trigger='%1']",
                 mClass->GetName());
            return false;
        }
        if (!mClass->HasParameter<int>("particle-emit-count"))
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
        if (!mClass->HasParameter<std::string>("sprite-cycle-id"))
        {
            WARN("Timeline trigger has a missing parameter (string) 'sprite-cycle-id'. [trigger='%1']",
                mClass->GetName());
            return false;
        }
        if (!mClass->HasParameter<float>("sprite-cycle-delay"))
        {
            WARN("Timeline trigger has a missing parameter (float) 'sprite-cycle-delay'. [trigger='%1']",
                mClass->GetName());
            return false;
        }
    }
    else if (type == Type::PlayAudio)
    {
        if (!mClass->HasParameter<AnimationTriggerClass::AudioStreamType>("audio-stream"))
        {
            WARN("Timeline trigger has a missing parameter (enum) 'audio-stream'. [trigger='%1']",
                mClass->GetName());
        }
        if (!mClass->HasParameter<AnimationTriggerClass::AudioStreamAction>("audio-stream-action"))
        {
            WARN("Timeline trigger has a missing parameter (enum) 'audio-stream-action'. [trigger='%1']",
                mClass->GetName());
        }
        if (!mClass->HasParameter<std::string>("audio-graph-id"))
        {
            WARN("Timeline trigger has a missing parameter (string) 'audio-graph-id'. [trigger='%1']",
                mClass->GetName());
        }
    }
    else if (type == Type::SpawnEntity)
    {
        if (!mClass->HasParameter<std::string>("entity-class-id"))
        {
            WARN("Timeline trigger has a missing parameter (string) 'entity-class-id. [trigger='%1']",
                mClass->GetName());
        }
        if (!mClass->HasParameter<int>("entity-render-layer"))
        {
            WARN("Timeline trigger has a missing parameter (int) 'entity-render-layer'. [trigger='%1']",
                mClass->GetName());
        }
    }
    else BUG("Unhandled animation trigger type.");
    return true;
}

void AnimationTrigger::Trigger(game::EntityNode& node, std::vector<Event>* events)
{
    if (!mClass->IsEnabled())
        return;

    const auto type = mClass->GetType();
    if (type == Type::StartMeshEffect)
    {
        auto* drawable = node.GetDrawable();
        if (!drawable)
            return;

        // the  effect application depends on the existence of the
        // mesh effect node attachment and the drawable.
        auto* mesh_effect = node.GetMeshEffect();
        if (!mesh_effect)
            return;

        DrawableItem::Command cmd;
        cmd.name = "EnableMeshEffect";
        cmd.args["state"] = "on";
        drawable->EnqueueCommand(std::move(cmd));

        DEBUG("homo kakki!");
    }
    else if (type == Type::EmitParticlesTrigger)
    {
        auto* drawable = node.GetDrawable();
        if (!drawable)
            return;

        int particle_emission_count = 0;
        if (!mClass->GetParameter("particle-emit-count", &particle_emission_count))
            return;

        DrawableItem::Command cmd;
        cmd.name = "EmitParticles";
        cmd.args["count"] = particle_emission_count;
        drawable->EnqueueCommand(std::move(cmd));
    }
    else if (type == Type::RunSpriteCycle)
    {
        auto* drawable = node.GetDrawable();
        if (!drawable)
            return;

        std::string sprite_cycle_id;
        float sprite_cycle_delay = 0.0f;
        if (!mClass->GetParameter("sprite-cycle-id", &sprite_cycle_id) ||
            !mClass->GetParameter("sprite-cycle-delay", &sprite_cycle_delay))
            return;

        DrawableItem::Command cmd;
        cmd.name = "RunSpriteCycle";
        cmd.args["id"]    = sprite_cycle_id;
        cmd.args["delay"] = sprite_cycle_delay;
        drawable->EnqueueCommand(std::move(cmd));
    }
    else if (type == Type::PlayAudio)
    {
        std::string graph_class_id;
        AnimationTriggerClass::AudioStreamType stream;
        AnimationTriggerClass::AudioStreamAction action;
        if (!mClass->GetParameter("audio-stream", &stream) ||
            !mClass->GetParameter("audio-stream-action", &action) ||
            !mClass->GetParameter("audio-graph-id", &graph_class_id))
            return;
        if (events)
        {
            AnimationAudioTriggerEvent e;
            e.stream = stream;
            e.action = action;
            e.audio_graph_id = std::move(graph_class_id);
            e.trigger_name = mClass->GetName();
            events->emplace_back(std::move(e));
        }
    }
    else if (type == Type::SpawnEntity)
    {
        int render_layer = 0;
        std::string entity_class_id;
        if (!mClass->GetParameter("entity-class-id", &entity_class_id) ||
            !mClass->GetParameter("entity-render-layer", &render_layer))
            return;
        if (events)
        {
            AnimationSpawnEntityTriggerEvent e;
            e.entity_class_id = std::move(entity_class_id);
            e.render_layer    = render_layer;
            e.source_node_id = node.GetId();
            e.trigger_name = mClass->GetName();
            events->emplace_back(std::move(e));
        }
    }
    else BUG("Unhandled animation trigger type.");
}

} // namespace