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

#include <string>

#include "base/utility.h"
#include "data/fwd.h"

namespace game
{
    class EntityStateClass
    {
    public:
        explicit EntityStateClass(std::string id = base::RandomString(10));

        inline void SetName(std::string name) noexcept
        { mName = std::move(name); }
        inline std::string GetId() const noexcept
        { return mId; }
        inline std::string GetName() const noexcept
        { return mName; }

        std::size_t GetHash() const noexcept;

        void IntoJson(data::Writer& data) const;
        bool FromJson(const data::Reader& data);

        EntityStateClass Clone() const;
    private:
        std::string mName;
        std::string mId;
    };

    class EntityStateTransitionClass
    {
    public:
        explicit EntityStateTransitionClass(std::string id = base::RandomString(10));

        inline void SetDuration(float duration) noexcept
        { mDuration = duration; }
        inline void SetName(std::string name) noexcept
        { mName = std::move(name); }
        inline std::string GetId() const noexcept
        { return mId; }
        inline std::string GetName() const noexcept
        { return mName; }
        inline std::string GetDstStateId() const noexcept
        { return mDstStateId; }
        inline std::string GetSrcStateId() const noexcept
        { return mSrcStateId; }
        inline void SetSrcStateId(std::string id) noexcept
        { mSrcStateId = std::move(id); }
        inline void SetDstStateId(std::string id) noexcept
        { mDstStateId = std::move(id); }
        inline float GetDuration() const noexcept
        { return mDuration; }

        std::size_t GetHash() const noexcept;
        void IntoJson(data::Writer& data) const;
        bool FromJson(const data::Reader& data);

        EntityStateTransitionClass Clone() const;
    private:
        std::string mName;
        std::string mId;
        std::string mSrcStateId;
        std::string mDstStateId;
        float mDuration = 0.0f;
    };

    using EntityState = EntityStateClass;
    using EntityStateTransition = EntityStateTransitionClass;

} // namespace