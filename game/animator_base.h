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

namespace game
{
    namespace detail {
        template<typename T>
        class AnimatorClassBase : public AnimatorClass
        {
        public:
            virtual void SetNodeId(const std::string& id) override
            { mNodeId = id; }
            virtual void SetName(const std::string& name) override
            { mName = name; }
            virtual std::string GetName() const override
            { return mName; }
            virtual std::string GetId() const override
            { return mId; }
            virtual std::string GetNodeId() const override
            { return mNodeId; }
            virtual float GetStartTime() const override
            { return mStartTime; }
            virtual float GetDuration() const override
            { return mDuration; }
            virtual void SetFlag(Flags flag, bool on_off) override
            { mFlags.set(flag, on_off); }
            virtual bool TestFlag(Flags flag) const override
            { return mFlags.test(flag); }
            virtual void SetStartTime(float start) override
            { mStartTime = math::clamp(0.0f, 1.0f, start); }
            virtual void SetDuration(float duration) override
            { mDuration = math::clamp(0.0f, 1.0f, duration); }
            virtual std::unique_ptr<AnimatorClass> Copy() const override
            { return std::make_unique<T>(*static_cast<const T*>(this)); }
            virtual std::unique_ptr<AnimatorClass> Clone() const override
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
            ~AnimatorClassBase() = default;
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
    } // namespace detail

} // game