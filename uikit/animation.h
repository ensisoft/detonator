// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#include <variant>
#include <string>
#include <vector>
#include <deque>
#include <unordered_map>

#include "base/math.h"
#include "uikit/types.h"

namespace uik
{
    class Widget;

    // $OnOpen
    // resize 100.0 200.0
    // move 45.0 56.0
    // delay 0.0
    // duration 1.0
    // interpolation Cosine
    // loops 1

    // $OnOpen
    // set flag visible true
    // set prop font-name 'app://foobar.otf'
    // set prop font-size 12
    // set prop button-shape Rect

    class Animation {
    public:
        enum class Trigger {
            Open,
            Close,
            Click,
            ValueChange,
            GainFocus,
            LostFocus,
            MouseEnter,
            MouseLeave
        };
        enum class State {
            Active, Inactive
        };
        using Interp = math::Interpolation;

        // note that float values are handled in style property.
        using ActionValue = std::variant<std::monostate, StyleProperty, FSize, FPoint, bool>;

        struct Action {
            enum class Type {
                Animate,
                Resize,
                Grow,
                Move,
                Translate,
                DelProp,
                SetProp,
                DelMaterial,
                SetMaterial,
                SetFlag
            };
            Type type = Type::Resize;
            // key applies to DelProp, SetProp, DelMaterial, SetMaterial and SetFlag
            std::string key;
            ActionValue value;
            float step = 0.5f;
        };
        using KeyFramePropertyValue = std::variant<float, uik::Color4f, FSize, FPoint>;

        struct KeyFrameProperty {
            std::string property_key;
            KeyFramePropertyValue property_value;
        };

        struct KeyFrame {
            float time = 0.0f;
            std::vector<KeyFrameProperty> properties;
        };

        struct KeyFrameAnimation {
            std::string name;
            std::vector<KeyFrame> keyframes;
        };

        explicit Animation(Trigger trigger) noexcept
          : mTrigger(trigger)
        {}

        bool TriggerOnOpen();
        bool TriggerOnClose();
        bool TriggerOnAction(const WidgetAction& action);

        void Update(double time, float dt);

        bool IsActiveOnTrigger(Trigger trigger) const noexcept;
        bool IsActiveOnAction(const WidgetAction& action) const noexcept;

        inline auto GetTrigger() const noexcept
        { return mTrigger; }
        inline auto GetState() const noexcept
        { return mState; }
        inline auto GetInterpolation() const noexcept
        { return mInterpolation; }
        inline auto GetDuration() const noexcept
        { return mDuration; }
        inline auto GetDelay() const noexcept
        { return mDelay; }
        inline auto GetTime() const noexcept
        { return mTime;}
        inline auto GetLoops() const noexcept
        { return mLoops; }
        inline auto GetLoop() const noexcept
        { return mLoop; }

        inline auto GetActionCount() const noexcept
        { return mActions.size(); }
        inline auto& GetAction(size_t index) noexcept
        { return mActions[index]; }
        inline const auto& GetAction(size_t index) const noexcept
        { return mActions[index]; }

        inline auto* GetWidget() noexcept
        { return mWidget; }
        inline const auto* GetWidget() const noexcept
        { return mWidget; }
        inline void SetWidget(Widget* widget) noexcept
        { mWidget = widget; }

        bool Parse(std::deque<std::string>& lines);

        using KeyFrameAnimationMap = std::unordered_map<std::string,
            std::shared_ptr<const KeyFrameAnimation>>;

        inline void SetKeyFrameAnimations(KeyFrameAnimationMap animations) noexcept
        { mKeyFrameAnimations = std::move(animations); }


    private:
        void EnterTriggerState();
        void EnterRunState();
        KeyFramePropertyValue GetWidgetPropertyValue(const std::string& key, bool* success) const;

    private:
        Trigger mTrigger = Trigger::Open;
        Interp mInterpolation = math::Interpolation::Linear;
        double   mDuration = 1.0f;
        double   mDelay    = 0.0f;
        unsigned mLoops    = 1;
        std::vector<Action> mActions;
        std::string mName = {"unnamed"};
        KeyFrameAnimationMap mKeyFrameAnimations;

        struct InterpolationActionState {
            Action::Type type;
            ActionValue start;
            ActionValue end;
        };
        struct StepActionState {
            Action::Type type;
            std::string  key;
            ActionValue  value;
            float        step = 0.5f;
        };
        struct KeyFrameAnimationState {
            float time = 0.0f;
            std::unordered_map<std::string,
                KeyFramePropertyValue> values;
        };

        State mState    = State::Inactive;
        Widget* mWidget = nullptr;
        // a vector of key frames for each key frame animation
        std::vector<std::vector<KeyFrameAnimationState>> mKeyFrameState;
        std::vector<InterpolationActionState> mInterpolationState;
        std::vector<StepActionState> mStepState;
        unsigned mLoop = 0;
        double   mTime = 0.0f;
    };

    bool ParseAnimations(const std::string& str, std::vector<Animation>* animations);

    using AnimationStateArray = std::vector<Animation>;

} // namespace

