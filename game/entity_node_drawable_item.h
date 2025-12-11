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
#  include <glm/mat4x4.hpp>
#  include <glm/vec2.hpp>
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <string>
#include <variant>
#include <unordered_map>

#include "base/bitflag.h"
#include "base/utility.h"
#include "data/fwd.h"
#include "game/enum.h"
#include "game/types.h"
#include "game/color.h"

namespace game
{
    // Drawable item defines a drawable item and its material and
    // properties that affect the rendering of the entity node
    class DrawableItemClass
    {
    public:
        // Variant of material params that can be set
        // on this drawable item. This actually needs to match
        // the Uniform definition in the gfx::MaterialClass.
        // But because we don't want to add a dependency to gfx:: here
        // the definition is repeated instead.
        using MaterialParam = std::variant<float, int,
                std::string,
                Color4f,
                glm::vec2, glm::vec3, glm::vec4>;
        // Key-value map of material params.
        using MaterialParamMap = std::unordered_map<std::string, MaterialParam>;
        using RenderPass       = game::RenderPass;
        using RenderStyle      = game::RenderStyle;
        using RenderView       = game::RenderView;
        using RenderProjection = game::RenderProjection;
        using CoordinateSpace  = game::CoordinateSpace;

        enum class Flags {
            // Whether the item is currently visible or not.
            VisibleInGame,
            // Whether the item should update material or not
            UpdateMaterial,
            // Whether the item should update drawable or not
            UpdateDrawable,
            // Whether the item should restart drawables that have
            // finished, for example particle engines.
            RestartDrawable,
            // Whether to flip (mirror) the item about the central vertical axis.
            // This changes the rendering on the direction from left to right.
            FlipHorizontally,
            // Whether to flip (mirror) the item about the central horizontal axis.
            // This changes the rendering on the direction from top to bottom.
            FlipVertically,
            // Render both front and back faces.
            DoubleSided,
            // Perform depth testing when rendering.
            DepthTest,
            // Contribute to bloom post-processing effect.
            PP_EnableBloom,
            // Enable light on this drawable (if the scene is lit)
            EnableLight,
            // Enable fog on this drawable (if the scene has fog).
            EnableFog,
        };
        DrawableItemClass();

        std::size_t GetHash() const;

        // class setters.
        void SetDrawableId(const std::string& klass)
        { mDrawableId = klass; }
        void SetMaterialId(const std::string& klass)
        { mMaterialId = klass; }
        void SetLayer(int layer) noexcept
        { mLayer = layer; }
        void ResetMaterial() noexcept
        {
            mMaterialId.clear();
            mMaterialParams.clear();
        }
        void ResetDrawable() noexcept
        { mDrawableId.clear(); }
        void SetFlag(Flags flag, bool on_off) noexcept
        { mBitFlags.set(flag, on_off); }
        void SetRenderPass(RenderPass pass) noexcept
        { mRenderPass = pass; }
        void SetCoordinateSpace(CoordinateSpace space) noexcept
        { mCoordinateSpace = space; }
        void SetTimeScale(float scale) noexcept
        { mTimeScale = scale; }
        void SetDepth(float depth) noexcept
        { mDepth = depth; }
        void SetRotator(const Rotator& rotator) noexcept
        { mRotator = rotator; }
        void SetOffset(const glm::vec3& offset) noexcept
        { mOffset = offset; }
        void SetMaterialParam(const std::string& name, const MaterialParam& value)
        { mMaterialParams[name] = value; }
        void SetMaterialParams(const MaterialParamMap& params)
        { mMaterialParams = params; }
        void SetMaterialParams(MaterialParamMap&& params)
        { mMaterialParams = std::move(params); }

        // class getters.
        const std::string& GetDrawableId() const noexcept
        { return mDrawableId; }
        const std::string& GetMaterialId() const noexcept
        { return mMaterialId; }
        int GetLayer() const noexcept
        { return mLayer; }
        float GetTimeScale() const noexcept
        { return mTimeScale; }
        float GetDepth() const noexcept
        { return mDepth; }
        bool TestFlag(Flags flag) const noexcept
        { return mBitFlags.test(flag); }
        glm::vec3 GetOffset() const noexcept
        { return mOffset; }
        Rotator GetRotator() const noexcept
        { return mRotator; }
        RenderPass GetRenderPass() const noexcept
        { return mRenderPass; }
        CoordinateSpace GetCoordinateSpace() const noexcept
        { return mCoordinateSpace; }
        base::bitflag<Flags> GetFlags() const noexcept
        { return mBitFlags; }

        MaterialParamMap GetMaterialParams() noexcept
        { return mMaterialParams; }
        const MaterialParamMap* GetMaterialParams() const noexcept
        { return &mMaterialParams; }
        bool HasMaterialParam(const std::string& name) const noexcept
        { return base::SafeFind(mMaterialParams, name) != nullptr; }
        MaterialParam* FindMaterialParam(const std::string& name) noexcept
        { return base::SafeFind(mMaterialParams, name); }
        const MaterialParam* FindMaterialParam(const std::string& name) const noexcept
        { return base::SafeFind(mMaterialParams, name); }
        template<typename T>
        T* GetMaterialParamValue(const std::string& name) noexcept
        {
            if (auto* ptr = base::SafeFind(mMaterialParams, name))
                return std::get_if<T>(ptr);
            return nullptr;
        }
        template<typename T>
        const T* GetMaterialParamValue(const std::string& name) const noexcept
        {
            if (auto* ptr = base::SafeFind(mMaterialParams, name))
                return std::get_if<T>(ptr);
            return nullptr;
        }
        void DeleteMaterialParam(const std::string& name) noexcept
        { mMaterialParams.erase(name); }
        void ClearMaterialParams() noexcept
        { mMaterialParams.clear(); }
        void SetActiveTextureMap(std::string id) noexcept
        { mMaterialParams["active_texture_map"] = std::move(id); }
        void ResetActiveTextureMap() noexcept
        { mMaterialParams.erase("active_texture_map"); }
        std::string GetActiveTextureMap() const noexcept
        {
            if (const auto* str = GetMaterialParamValue<std::string>("active_texture_map"))
                return *str;
            return "";
        }
        bool HasActiveTextureMap() const noexcept
        { return HasMaterialParam("active_texture_map"); }

        void IntoJson(data::Writer& data) const;

        bool FromJson(const data::Reader& data);
    private:
        // item's bit flags.
        base::bitflag<Flags> mBitFlags;
        // class id of the material.
        std::string mMaterialId;
        // class id of the drawable shape.
        std::string mDrawableId;
        // the layer in which this node should be drawn.
        int mLayer = 0;
        // scaler value for changing the time delta values
        // applied to the drawable (material)
        float mTimeScale = 1.0f;
        // For 3D objects depth value gives the 3rd dimension
        // that isn't available in the node itself.
        float mDepth = 1.0f;
        // drawable 3D rotator that encodes the rotational
        // transformation for producing the desired orientation.
        Rotator mRotator;
        // drawable offset in local drawable space.
        glm::vec3 mOffset = {0.0f, 0.0f, 0.0f};

        RenderPass mRenderPass = RenderPass::DrawColor;
        CoordinateSpace  mCoordinateSpace = CoordinateSpace::Scene;
        MaterialParamMap mMaterialParams;
    };

    class DrawableItem
    {
    public:
        using MaterialParam    = DrawableItemClass::MaterialParam;
        using MaterialParamMap = DrawableItemClass::MaterialParamMap;
        using Flags            = DrawableItemClass::Flags;
        using RenderPass       = DrawableItemClass::RenderPass;
        using RenderStyle      = DrawableItemClass::RenderStyle;
        using RenderView       = DrawableItemClass::RenderView;
        using RenderProjection = DrawableItemClass::RenderProjection;
        using CoordinateSpace  = DrawableItemClass::CoordinateSpace;

        using CommandArg = std::variant<float, int, std::string>;
        struct Command {
            std::string name;
            std::unordered_map<std::string, CommandArg> args;
        };

        struct SpriteCycle {
            std::string name;
            double time = 0.0;
        };

        explicit DrawableItem(std::shared_ptr<const DrawableItemClass> klass) noexcept
          : mClass(std::move(klass))
          , mMaterialId(mClass->GetMaterialId())
          , mInstanceFlags(mClass->GetFlags())
          , mInstanceTimeScale(mClass->GetTimeScale())
          , mInstanceDepth(mClass->GetDepth())
          , mInstanceRotator(mClass->GetRotator())
          , mInstanceOffset(mClass->GetOffset())
        {
            const auto* params = mClass->GetMaterialParams();
            if (!params->empty())
                mMaterialParams = *params;
        }
        bool HasMaterialTimeAdjustment() const noexcept
        { return mTimeAdjustment.has_value(); }
        double GetMaterialTimeAdjustment() const noexcept
        { return mTimeAdjustment.value_or(0.0); }
        void ClearMaterialTimeAdjustment() const noexcept
        { mTimeAdjustment.reset(); }
        void AdjustMaterialTime(double time) noexcept
        { mTimeAdjustment = time; }
        const std::string& GetMaterialId() const noexcept
        { return mMaterialId; }
        const std::string& GetDrawableId() const noexcept
        { return mClass->GetDrawableId(); }
        int GetLayer() const noexcept
        { return mClass->GetLayer(); }
        RenderPass GetRenderPass() const noexcept
        { return mClass->GetRenderPass(); }
        CoordinateSpace GetCoordinateSpace() const noexcept
        { return mClass->GetCoordinateSpace(); }
        Rotator GetRotator() const noexcept
        { return mInstanceRotator; }
        glm::vec3 GetOffset() const noexcept
        { return mInstanceOffset; }
        bool TestFlag(Flags flag) const noexcept
        { return mInstanceFlags.test(flag); }
        bool IsVisible() const noexcept
        { return mInstanceFlags.test(Flags::VisibleInGame); }
        void SetVisible(bool visible) noexcept
        { mInstanceFlags.set(Flags::VisibleInGame, visible); }
        float GetTimeScale() const noexcept
        { return mInstanceTimeScale; }
        float GetDepth() const noexcept
        { return mInstanceDepth; }
        void SetFlag(Flags flag, bool on_off) noexcept
        { mInstanceFlags.set(flag, on_off); }
        void SetTimeScale(float scale) noexcept
        { mInstanceTimeScale = scale; }
        void SetDepth(float depth) noexcept
        { mInstanceDepth = depth; }
        void SetRotator(const Rotator& rotator) noexcept
        { mInstanceRotator = rotator; }
        void SetOffset(glm::vec3 offset) noexcept
        { mInstanceOffset = offset; }

        // When you set the material ID to another material remember
        // to consider whether you should also
        // - clear the material uniforms (parameters)
        // - reset the material time
        // - reset the active texture map ID.
        // You likely should do all of the above and the reason why they
        // aren't done automatically is because then doing things in a
        // different order would create another bug setting material ID
        // would accidentally destroy other previously set state.
        void SetMaterialId(std::string id) noexcept
        { mMaterialId = std::move(id); }
        void SetMaterialParam(const std::string& name, const MaterialParam& value)
        {
            if (!mMaterialParams.has_value())
                mMaterialParams = MaterialParamMap {};
            mMaterialParams.value()[name] = value;
        }

        MaterialParamMap GetMaterialParams()
        { return mMaterialParams.value_or(MaterialParamMap{}); }

        const MaterialParamMap* GetMaterialParams() const noexcept
        {
            if (mMaterialParams.has_value())
                return &mMaterialParams.value();
            return nullptr;
        }
        bool HasMaterialParam(const std::string& name) const noexcept
        {
            if (auto* map = GetMaterialParams())
                return base::SafeFind(*map, name) != nullptr;
            return false;
        }
        MaterialParam* FindMaterialParam(const std::string& name) noexcept
        {
            if (mMaterialParams.has_value())
                return base::SafeFind(mMaterialParams.value(), name);
            return nullptr;
        }
        const MaterialParam* FindMaterialParam(const std::string& name) const noexcept
        {
            if (mMaterialParams.has_value())
                return base::SafeFind(mMaterialParams.value(), name);
            return nullptr;
        }

        template<typename T>
        T* GetMaterialParamValue(const std::string& name) noexcept
        {
            if (!mMaterialParams.has_value())
                return nullptr;
            if (auto* ptr = base::SafeFind(mMaterialParams.value(), name))
                return std::get_if<T>(ptr);
            return nullptr;
        }
        template<typename T>
        const T* GetMaterialParamValue(const std::string& name) const noexcept
        {
            if (!mMaterialParams.has_value())
                return nullptr;
            if (auto* ptr = base::SafeFind(mMaterialParams.value(), name))
                return std::get_if<T>(ptr);
            return nullptr;
        }
        void ClearMaterialParams() noexcept
        {
            mMaterialParams.reset();
        }

        void DeleteMaterialParam(const std::string& name) noexcept
        {
            if (mMaterialParams.has_value())
                mMaterialParams.value().erase(name);
        }

        void SetActiveTextureMap(std::string id) noexcept
        {
            if (!mMaterialParams.has_value())
                mMaterialParams = MaterialParamMap {};

            mMaterialParams.value()["active_texture_map"] = std::move(id);
        }

        void ResetActiveTextureMap() noexcept
        {
            if (mMaterialParams.has_value())
                mMaterialParams.value().erase("active_texture_map");
        }

        void SetCurrentSpriteCycle(SpriteCycle cycle) const
        {
            mSpriteCycle = std::move(cycle);
        }
        bool HasSpriteCycle() const noexcept
        {
            return mSpriteCycle.has_value();
        }
        void ClearCurrentSpriteCycle() const noexcept
        {
            mSpriteCycle.reset();
        }
        const SpriteCycle* GetCurrentSpriteCycle() const noexcept
        {
            if (mSpriteCycle.has_value())
                return &mSpriteCycle.value();
            return nullptr;
        }

        void SetCurrentMaterialTime(double time) const noexcept
        { mMaterialTime = time; }
        double GetCurrentMaterialTime() const noexcept
        { return mMaterialTime; }

        void EnqueueCommand(const Command& cmd)
        { mCommands.push_back(cmd); }
        void EnqueueCommand(Command&& cmd) noexcept
        { mCommands.push_back(std::move(cmd)); }
        const auto& GetCommands() const noexcept
        { return mCommands; }
        const auto& GetCommand(size_t index) const noexcept
        { return mCommands[index]; }
        auto GetCommand(size_t index) noexcept
        { return mCommands[index]; }
        void ClearCommands() const noexcept
        { mCommands.clear(); }
        size_t GetNumCommands() const noexcept
        { return mCommands.size(); }

        const DrawableItemClass& GetClass() const noexcept
        { return *mClass; }
        const DrawableItemClass* operator->() const noexcept
        { return mClass.get(); }
    private:
        std::shared_ptr<const DrawableItemClass> mClass;
        std::string mMaterialId;
        base::bitflag<Flags> mInstanceFlags;
        float mInstanceTimeScale = 1.0f;
        float mInstanceDepth = 1.0f;
        Rotator mInstanceRotator;
        glm::vec3 mInstanceOffset = {0.0f, 0.0f, 0.0f};
        mutable double mMaterialTime = 0.0f;
        mutable std::optional<double> mTimeAdjustment;
        mutable std::vector<Command> mCommands;
        mutable std::optional<SpriteCycle> mSpriteCycle;
        std::optional<MaterialParamMap> mMaterialParams;
    };

} // namespace
