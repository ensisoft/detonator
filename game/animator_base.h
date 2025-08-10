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

#include "base/bitflag.h"
#include "base/utility.h"
#include "base/math.h"
#include "game/animator.h"

namespace game::detail
{
    template<typename T>
    class AnimatorClassBase : public AnimatorClass
    {
    public:
        void SetNodeId(const std::string& id) override
        { mNodeId = id; }
        void SetName(const std::string& name) override
        { mName = name; }
        std::string GetName() const override
        { return mName; }
        std::string GetId() const override
        { return mId; }
        std::string GetNodeId() const override
        { return mNodeId; }
        float GetStartTime() const override
        { return mStartTime; }
        float GetDuration() const override
        { return mDuration; }
        void SetFlag(Flags flag, bool on_off) override
        { mFlags.set(flag, on_off); }
        bool TestFlag(Flags flag) const override
        { return mFlags.test(flag); }
        void SetStartTime(float start) override
        { mStartTime = math::clamp(0.0f, 1.0f, start); }
        void SetDuration(float duration) override
        { mDuration = math::clamp(0.0f, 1.0f, duration); }
        std::unique_ptr<AnimatorClass> Copy() const override
        { return std::make_unique<T>(*static_cast<const T*>(this)); }
        std::unique_ptr<AnimatorClass> Clone() const override
        {
            auto ret = std::make_unique<T>(*static_cast<const T*>(this));
            ret->mId = base::RandomString(10);
            return ret;
        }
    protected:
        AnimatorClassBase()
        {
            mId = base::RandomString(10);
            mFlags.set(Flags::StaticInstance, true);
        }
        ~AnimatorClassBase() override = default;
    protected:
        // ID of the class.
        std::string mId;
        // Human-readable name of the class
        std::string mName;
        // id of the node that the action will be applied onto
        std::string mNodeId;
        // Normalized start time.
        float mStartTime = 0.0f;
        // Normalized duration.
        float mDuration = 1.0f;
        // bitflags set on the class.
        base::bitflag<Flags> mFlags;
    private:
    };

} // game