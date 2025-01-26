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
#  include <glm/vec3.hpp>
#include "warnpop.h"

#include <memory>
#include <string>
#include <cstddef>
#include <algorithm>

#include "base/assert.h"
#include "base/bitflag.h"
#include "base/math.h"
#include "base/utility.h"
#include "data/fwd.h"
#include "game/enum.h"
#include "game/types.h"

namespace game
{
    class BasicLightClass
    {
    public:
        using LightType = BasicLightType;

        enum class Flags {
            Enabled
        };

        BasicLightClass();

        inline int GetLayer() const noexcept
        { return mLayer; }

        inline LightType GetLightType() const noexcept
        { return mLightType; }

        inline base::bitflag<Flags> GetFlags() const noexcept
        { return mFlags; }

        inline void SetFlag(Flags flag, bool on_off) noexcept
        { mFlags.set(flag, on_off); }
        inline bool TestFlag(Flags flag) const noexcept
        { return mFlags.test(flag); }

        inline bool IsEnabled() const noexcept
        { return TestFlag(Flags::Enabled); }
        inline void Enable(bool on_off) noexcept
        { SetFlag(Flags::Enabled, on_off); }

        inline void SetLayer(int layer) noexcept
        { mLayer = layer; }

        inline void SetLightType(LightType type) noexcept
        { mLightType = type; }

        inline void SetDirection(glm::vec3 direction) noexcept
        { mDirection = direction; }
        inline void SetTranslation(glm::vec3 translation) noexcept
        { mTranslation = translation; }
        inline void SetAmbientColor(const base::Color4f& color) noexcept
        { mAmbientColor = color; }
        inline void SetDiffuseColor(const base::Color4f& color) noexcept
        { mDiffuseColor = color; }
        inline void SetSpecularColor(const base::Color4f& color) noexcept
        { mSpecularColor = color; }
        inline void SetSpotHalfAngle(base::FDegrees degrees) noexcept
        { mSpotHalfAngle = degrees; }
        inline void SetSpotHalfAngle(base::FRadians radians) noexcept
        { mSpotHalfAngle = radians; }
        inline void SetConstantAttenuation(float attenuation) noexcept
        { mConstantAttenuation = attenuation; }
        inline void SetLinearAttenuation(float attenuation) noexcept
        { mLinearAttenuation = attenuation; }
        inline void SetQuadraticAttenuation(float attenuation) noexcept
        { mQuadraticAttenuation = attenuation; }

        inline const auto& GetDirection() const noexcept
        { return mDirection; }
        inline const auto& GetTranslation() const noexcept
        { return mTranslation; }
        inline const auto& GetAmbientColor() const noexcept
        { return mAmbientColor; }
        inline const auto& GetDiffuseColor() const noexcept
        { return mDiffuseColor; }
        inline const auto& GetSpecularColor() const noexcept
        { return mSpecularColor; }
        inline const auto& GetSpotHalfAngle() const noexcept
        { return mSpotHalfAngle; }
        inline float GetConstantAttenuation() const noexcept
        { return mConstantAttenuation; }
        inline float GetLinearAttenuation() const noexcept
        { return mLinearAttenuation; }
        inline float GetQuadraticAttenuation() const noexcept
        { return mQuadraticAttenuation; }

        void IntoJson(data::Writer& data) const;
        bool FromJson(const data::Reader& data);
        size_t GetHash() const;

    private:
        LightType mLightType = LightType::Ambient;
        base::bitflag<Flags> mFlags;
        glm::vec3 mDirection = {1.0f, 0.0f, 0.0f};
        glm::vec3 mTranslation = {0.0f, 0.0f, 0.0f};
        game::Color4f mAmbientColor;
        game::Color4f mDiffuseColor;
        game::Color4f mSpecularColor;
        game::FDegrees mSpotHalfAngle;
        float mConstantAttenuation  = 1.0f;
        float mLinearAttenuation    = 0.0f;
        float mQuadraticAttenuation = 0.0f;
        int mLayer = 0;
    };

    class BasicLight
    {
    public:
        using LightType  = BasicLightClass::LightType;
        using Flags = BasicLightClass::Flags;

        explicit BasicLight(std::shared_ptr<const BasicLightClass> klass) noexcept
          : mClass(klass)
          , mInstanceFlags(mClass->GetFlags())
          , mDirection(mClass->GetDirection())
          , mTranslation(mClass->GetTranslation())
          , mAmbientColor(mClass->GetAmbientColor())
          , mDiffuseColor(mClass->GetDiffuseColor())
          , mSpecularColor(mClass->GetSpecularColor())
          , mSpotHalfAngle(mClass->GetSpotHalfAngle())
          , mConstantAttenuation(mClass->GetConstantAttenuation())
          , mLinearAttenuation(mClass->GetLinearAttenuation())
          , mQuadraticAttenuation(mClass->GetQuadraticAttenuation())
        {}

        inline bool TestFlag(Flags flag) const noexcept
        { return mInstanceFlags.test(flag); }
        inline void SetFlag(Flags flag, bool on_off) noexcept
        { mInstanceFlags.set(flag, on_off); }

        inline int GetLayer() const noexcept
        { return mClass->GetLayer(); }

        inline LightType GetLightType() const noexcept
        { return mClass->GetLightType(); }

        inline bool IsEnabled() const noexcept
        { return TestFlag(Flags::Enabled); }

        inline void Enable(bool enable) noexcept
        { SetFlag(Flags::Enabled, enable); }

        inline const auto& GetDirection() const noexcept
        { return mDirection; }
        inline const auto& GetTranslation() const noexcept
        { return mTranslation; }
        inline const auto& GetAmbientColor() const noexcept
        { return mAmbientColor; }
        inline const auto& GetDiffuseColor() const noexcept
        { return mDiffuseColor; }
        inline const auto& GetSpecularColor() const noexcept
        { return mSpecularColor; }
        inline const auto& GetSpotHalfAngle() const noexcept
        { return mSpotHalfAngle; }
        inline float GetConstantAttenuation() const noexcept
        { return mConstantAttenuation; }
        inline float GetLinearAttenuation() const noexcept
        { return mLinearAttenuation; }
        inline float GetQuadraticAttenuation() const noexcept
        { return mQuadraticAttenuation; }

        void SetDirection(const glm::vec3& direction) noexcept
        { mDirection = direction; }
        void SetTranslation(const glm::vec3& translation) noexcept
        { mTranslation = translation; }
        void SetAmbientColor(const Color4f& color) noexcept
        { mAmbientColor = color; }
        void SetDiffuseColor(const Color4f& color) noexcept
        { mDiffuseColor = color; }
        void SetSpecularColor(const Color4f& color) noexcept
        { mSpecularColor = color; }
        void SetSpotHalfAngle(float degrees) noexcept
        { mSpotHalfAngle = FDegrees(math::clamp(0.0f, 180.0f, degrees)); }
        void SetLinearAttenuation(float attenuation) noexcept
        { mLinearAttenuation = std::max(attenuation, 0.0f); }
        void SetConstantAttenuation(float attenuation) noexcept
        { mConstantAttenuation = std::max(attenuation, 1.0f); }
        void SetQuadraticAttenuation(float attenuation) noexcept
        { mQuadraticAttenuation = std::max(attenuation, 0.0f);  }

        inline const BasicLightClass& GetClass() const noexcept
        { return *mClass; }
        const BasicLightClass* operator->() const noexcept
        { return mClass.get(); }

    private:
        std::shared_ptr<const BasicLightClass> mClass;
        base::bitflag<Flags> mInstanceFlags;
        glm::vec3 mDirection = {1.0f, 0.0f, 0.0f};
        glm::vec3 mTranslation = {0.0f, 0.0f, 0.0f};
        game::Color4f mAmbientColor;
        game::Color4f mDiffuseColor;
        game::Color4f mSpecularColor;
        game::FDegrees mSpotHalfAngle;
        float mConstantAttenuation  = 1.0f;
        float mLinearAttenuation    = 0.0f;
        float mQuadraticAttenuation = 0.0f;
    };

} // namespace