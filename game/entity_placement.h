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

#pragma once

#include "warnpush.h"
#  include <glm/vec2.hpp>
#  include <glm/mat4x4.hpp>
#include "warnpop.h"

#include <string>
#include <cstddef>
#include <optional>
#include <memory>

#include "base/hash.h"
#include "base/bitflag.h"
#include "game/entity.h"
#include "game/scriptvar.h"

namespace game
{
    // EntityPlacement holds the information for placing an entity
    // into the scene when the actual scene instance is created.
    // In other words the EntityPlacement objects in the SceneClass
    // become Entity objects in the Scene.
    class EntityPlacement
    {
    public:
        using Flags = Entity::Flags;

        struct ScriptVarValue {
            std::string id;
            ScriptVar::VariantType value;
        };

        EntityPlacement();

        void SetFlag(Flags flag, bool on_off) noexcept;
        void SetEntity(std::shared_ptr<const EntityClass> klass);
        void ResetEntity() noexcept;
        void ResetEntityParams() noexcept;

        void SetTranslation(const glm::vec2& pos) noexcept
        { mPosition = pos; }
        void SetTranslation(float x, float y) noexcept
        { mPosition = glm::vec2(x, y); }
        void SetScale(const glm::vec2& scale) noexcept
        { mScale = scale; }
        void SetScale(float sx, float sy) noexcept
        { mScale = glm::vec2(sx, sy); }
        void SetRotation(float rotation) noexcept
        { mRotation = rotation; }
        void SetEntityId(const std::string& id)
        { mEntityId = id; }
        void SetName(const std::string& name)
        { mName = name; }
        void SetRenderLayer(int32_t layer) noexcept
        { mRenderLayer = layer; }
        void SetTag(const std::string& tag)
        { mTagString = tag; }
        void SetIdleAnimationId(const std::string& id)
        { mIdleAnimationId = id; }
        void SetParentRenderTreeNodeId(const std::string& id)
        { mParentRenderTreeNodeId = id; }

        void ResetLifetime() noexcept
        { mLifetime.reset(); }
        void SetLifetime(double lifetime) noexcept
        { mLifetime = lifetime; }
        void ResetTag() noexcept
        { mTagString.reset(); }

        // class getters.
        bool IsBroken() const noexcept
        { return !mEntity; }

        const glm::vec2& GetTranslation() const noexcept
        { return mPosition; }
        const glm::vec2& GetScale() const noexcept
        { return mScale; }
        float GetRotation() const noexcept
        { return mRotation; }
        const std::string& GetName() const noexcept
        { return mName; }
        const std::string& GetId() const noexcept
        { return mClassId; }
        const std::string& GetEntityId() const noexcept
        { return mEntityId; }
        const std::string& GetIdleAnimationId() const noexcept
        { return mIdleAnimationId; }
        const std::string& GetParentRenderTreeNodeId() const noexcept
        { return mParentRenderTreeNodeId; }
        std::shared_ptr<const EntityClass> GetEntityClass() const noexcept
        { return mEntity; }
        const std::string* GetTag() const noexcept
        { return mTagString ? &mTagString.value() : nullptr; }
        bool TestFlag(Flags flag) const noexcept
        { return mFlagValBits.test(flag); }
        auto GetRenderLayer() const noexcept
        { return mRenderLayer; }

        double GetLifetime() const noexcept
        { return mLifetime.value_or(0.0); }
        bool HasSpecifiedParentNode() const noexcept
        { return !mParentRenderTreeNodeId.empty(); }
        bool HasIdleAnimationSetting() const noexcept
        { return !mIdleAnimationId.empty(); }
        bool HasLifetimeSetting() const noexcept
        { return mLifetime.has_value(); }
        bool HasFlagSetting(Flags flag) const noexcept
        { return mFlagSetBits.test(flag); }
        bool HasTag() const noexcept
        { return mTagString.has_value(); }
        void ClearFlagSetting(Flags flag) noexcept
        { mFlagSetBits.set(flag, false); }
        std::size_t GetNumScriptVarValues() const noexcept
        { return mScriptVarValues.size(); }
        ScriptVarValue& GetGetScriptVarValue(size_t index) noexcept
        { return base::SafeIndex(mScriptVarValues, index); }
        const ScriptVarValue& GetScriptVarValue(size_t index) const noexcept
        { return base::SafeIndex(mScriptVarValues, index); }
        void AddScriptVarValue(const ScriptVarValue& value)
        { mScriptVarValues.push_back(value);}
        void AddScriptVarValue(ScriptVarValue&& value)
        { mScriptVarValues.push_back(std::move(value)); }
        ScriptVarValue* FindScriptVarValueById(const std::string& id);
        const ScriptVarValue* FindScriptVarValueById(const std::string& id) const;
        bool DeleteScriptVarValueById(const std::string& id);
        void SetScriptVarValue(const ScriptVarValue& value);

        void ClearStaleScriptValues(const EntityClass& klass);

        // Get the node hash value based on the properties.
        std::size_t GetHash() const;

        // Get this node's transform relative to its parent.
        glm::mat4 GetNodeTransform() const;

        // Make a clone of this node. The cloned node will
        // have all the same property values but a unique id.
        EntityPlacement Clone() const;

        // Serialize node into JSON.
        void IntoJson(data::Writer& data) const;

        // Load node and its properties from JSON.
        bool FromJson(const data::Reader& data);
    private:
        // The node's unique class id.
        std::string mClassId;
        // The id of the entity this node contains.
        std::string mEntityId;
        // When the scene node (entity) is linked (parented)
        // to another scene node (entity) this id is the
        // node in the parent entity's render tree that is to
        // be used as the parent of this entity's nodes.
        std::string mParentRenderTreeNodeId;
        // The human-readable name for the node.
        std::string mName;
        // The position of the node relative to its parent.
        glm::vec2 mPosition = {0.0f, 0.0f};
        // The scale of the node relative to its parent.
        glm::vec2 mScale = {1.0f, 1.0f};
        // The rotation of the node relative to its parent.
        float mRotation = 0.0f;
        // Node bitflags. the bits are doubled because a bit
        // is needed to indicate whether a bit is set or not
        base::bitflag<Flags> mFlagValBits;
        base::bitflag<Flags> mFlagSetBits;
        // scene render layer index.
        int32_t mRenderLayer = 0;
        // the track id of the idle animation if any.
        // this setting will override the entity class idle
        // track designation if set.
        std::string mIdleAnimationId;
        std::optional<double> mLifetime;
        std::optional<std::string> mTagString;
        std::vector<ScriptVarValue> mScriptVarValues;
    private:
        // This is the runtime class reference to the
        // entity class that this node uses. Before creating
        // a scene instance it's important that this entity
        // reference is resolved to a class object instance.
        std::shared_ptr<const EntityClass> mEntity;
    };
} // namespace
