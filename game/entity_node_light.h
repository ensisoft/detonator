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

#include <memory>
#include <string>
#include <cstddef>

#include "base/assert.h"
#include "base/bitflag.h"
#include "base/utility.h"
#include "data/fwd.h"
#include "game/enum.h"
#include "game/types.h"

namespace game
{
    class LightClass
    {
    public:
        enum class LightType {
            ScreenSpace2DLight
        };
        enum class Flags {
            Enabled
        };

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

        void SetLightParameter(const std::string& key, LightParam value)
        { mLightParams[key] = value; }

        bool HasLightParameter(const std::string& key) const noexcept
        { return base::Contains(mLightParams, key); }

        template<typename T>
        const T* GetLightParameter(const std::string& key) const noexcept
        {
            if (const auto* ptr = base::SafeFind(mLightParams, key))
            {
                ASSERT(std::holds_alternative<T>(*ptr));
                return std::get_if<T>(ptr);
            }
            return nullptr;
        }

        template<typename T>
        const T GetLightParameter(const std::string& key, const T& value) const noexcept
        {
            if (const auto* ptr = base::SafeFind(mLightParams, key))
            {
                ASSERT(std::holds_alternative<T>(*ptr));
                return std::get_if<T>(ptr);
            }
            return value;
        }

        void IntoJson(data::Writer& data) const;
        bool FromJson(const data::Reader& data);
        size_t GetHash() const;

    private:
        LightType mLightType = LightType::ScreenSpace2DLight;
        LightParamMap mLightParams;
        base::bitflag<Flags> mFlags;
        int mLayer = 0;
    };

    class Light
    {
    public:
        using Flags = LightClass::Flags;
        using Type  = LightClass::LightType;

        explicit Light(std::shared_ptr<const LightClass> klass) noexcept
          : mClass(klass)
          , mInstanceFlags(mClass->GetFlags())
        {}

        inline bool TestFlag(Flags flag) const noexcept
        { return mInstanceFlags.test(flag); }
        inline void SetFlag(Flags flag, bool on_off) noexcept
        { mInstanceFlags.set(flag, on_off); }

        inline int GetLayer() const noexcept
        { return mClass->GetLayer(); }

        inline Type GetLightType() const noexcept
        { return mClass->GetLightType(); }

        inline bool IsEnabled() const noexcept
        { return TestFlag(Flags::Enabled); }

        inline const LightClass& GetClass() const noexcept
        { return *mClass; }
        const LightClass* operator->() const noexcept
        { return mClass.get(); }

    private:
        std::shared_ptr<const LightClass> mClass;
        base::bitflag<LightClass::Flags> mInstanceFlags;
    };


} // namespace