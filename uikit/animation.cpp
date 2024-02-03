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

#define LOGTAG "uikit"

#include "config.h"

#include "warnpush.h"
#  include <boost/algorithm/string.hpp>
#  include <neargye/magic_enum.hpp>
#include "warnpop.h"

#include <algorithm>
#include <string>
#include <sstream>
#include <deque>
#include <limits>

#include "base/assert.h"
#include "base/format.h"
#include "base/logging.h"
#include "base/utility.h"
#include "base/scanf.h"
#include "uikit/animation.h"
#include "uikit/widget.h"

namespace {

std::string GetToken(const std::vector<std::string>& tokens, size_t index)
{
    if (index >= tokens.size())
        return "";
    return tokens[index];
}

template<typename Enum>
const Enum* ToEnum(const std::string& str, Enum* value)
{
    const auto enum_val = magic_enum::enum_cast<Enum>(str);
    if (!enum_val.has_value())
        return nullptr;

    *value = enum_val.value();
    return value;
}

template<typename T>
const T* ToNumber(const std::string& str, T* value)
{
    if (!base::FromChars(str, value))
        return nullptr;
    return value;
}

std::vector<std::string> SplitTokens(const std::string& s)
{
    std::vector<std::string> ret;
    std::stringstream ss(s);
    while (ss.good())
    {
        std::string tok;
        ss >> tok;
        ret.push_back(std::move(tok));
    }
    return ret;
}
std::deque<std::string> SplitLines(const std::string& str)
{
    std::deque<std::string> lines;
    std::stringstream ss(str);
    std::string line;
    while (std::getline(ss, line))
        lines.push_back(line);
    return lines;
}
bool GetLine(std::deque<std::string>& lines, std::string* line)
{
    while (!lines.empty())
    {
        auto str = boost::trim_copy(lines[0]);
        if (str.empty())
        {
            lines.pop_front();
            continue;
        }
        else if (str[0] == ';' || str[0] == '#')
        {
            lines.pop_front();
            continue;
        }
        *line = std::move(str);
        lines.pop_front();
        return true;
    }
    return false;
}

std::optional<uik::Animation::KeyFrameAnimation> ParseKeyFrameAnimation(std::deque<std::string>& lines)
{
    uik::Animation::KeyFrameAnimation ret;
    std::optional<uik::Animation::KeyFrame> keyframe;

    std::string line;
    while (GetLine(lines, &line))
    {
        if (base::StartsWith(line, "$") ||
            base::StartsWith(line, "@")) {
            lines.push_front(std::move(line));
            break;
        }
        if (base::EndsWith(line, "%")) {
            float time = 0.0f;
            if (!base::Scanf(line, &time, "%")) {
                return std::nullopt;
            }
            if (keyframe.has_value())
            {
                ret.keyframes.push_back(keyframe.value());
                keyframe.reset();
            }
            uik::Animation::KeyFrame kf;
            kf.time  = math::clamp(0.0f, 100.0f, time) / 100.0f;
            keyframe = kf;
            continue;
        }
        if (!keyframe.has_value()) {
            WARN("Unexpected key frame property set without key frame start.");
            return std::nullopt;
        }

        const auto& tokens   = SplitTokens(line);
        const auto& prop_key = GetToken(tokens, 0);
        if (prop_key.empty())
            return std::nullopt;

        uik::Animation::KeyFrameProperty key_frame_property;
        key_frame_property.property_key = prop_key;
        if (prop_key == "position") {
            float x = 0.0f;
            float y = 0.0f;
            if (!base::FromChars(GetToken(tokens, 1), &x) ||
                !base::FromChars(GetToken(tokens, 2), &y))
                return std::nullopt;
            key_frame_property.property_value = uik::FPoint(x, y);
        }
        else if (prop_key == "size") {
            float w = 0.0f;
            float h = 0.0f;
            if (!base::FromChars(GetToken(tokens, 1), &w) ||
                !base::FromChars(GetToken(tokens, 2), &h))
                return std::nullopt;
            key_frame_property.property_value = uik::FSize(w, h);
        } else if (base::EndsWith(prop_key, "-color")) {
            const auto& prop_val = GetToken(tokens, 1);
            uik::Color4f color;
            uik::Color  color_enum = uik::Color::Black;
            if (base::Scanf(prop_val, &color))
                key_frame_property.property_value = color;
            else if (ToEnum(prop_val, &color_enum))
                key_frame_property.property_value = color_enum;
            else return std::nullopt;
        }
        auto& kf = keyframe.value();
        kf.properties.push_back(std::move(key_frame_property));
    }
    if (keyframe.has_value())
        ret.keyframes.push_back(keyframe.value());
    return ret;
}

std::optional<uik::Animation::Action> ParseAction(const std::vector<std::string>& tokens)
{
    const auto& directive = GetToken(tokens, 0);

    if (directive == "animate")
    {
        std::string key_frame_animation_name = GetToken(tokens, 1);
        if (key_frame_animation_name.empty())
            return std::nullopt;

        uik::Animation::Action action;
        action.type = uik::Animation::Action::Type::Animate;
        action.key  = key_frame_animation_name;
        return action;
    }
    else if (directive == "resize" || directive == "grow")
    {
        float width  = 0.0f;
        float height = 0.0f;
        if (!base::FromChars(GetToken(tokens, 1), &width) ||
            !base::FromChars(GetToken(tokens, 2), &height))
            return std::nullopt;

            uik::Animation::Action action;
            action.value = uik::FSize(width, height);
            if (directive == "resize")
                action.type  = uik::Animation::Action::Type::Resize;
            else if (directive == "grow")
                action.type  = uik::Animation::Action::Type::Grow;
            return action;
    }
    else if (directive == "move" || directive == "translate")
    {
        float x = 0.0f;
        float y = 0.0f;
        if (!base::FromChars(GetToken(tokens, 1), &x) ||
            !base::FromChars(GetToken(tokens, 2), &y))
            return std::nullopt;

        uik::Animation::Action action;
        action.value = uik::FPoint(x, y);
        if (directive == "move")
            action.type  = uik::Animation::Action::Type::Move;
        else if (directive == "translate")
            action.type = uik::Animation::Action::Type::Translate;
        return action;
    }
    else if (directive == "del")
    {
        const auto& target = GetToken(tokens, 1);
        if (target == "prop")
        {
            const auto& prop_name = GetToken(tokens, 2);
            if (prop_name.empty())
                return std::nullopt;

            uik::Animation::Action action;
            action.type = uik::Animation::Action::Type::DelProp;
            action.key  = prop_name;
            return action;
        }
        else if (target == "material")
        {
            const auto& mat_name = GetToken(tokens, 2);
            if (mat_name.empty())
                return std::nullopt;

            uik::Animation::Action action;
            action.type = uik::Animation::Action::Type::DelMaterial;
            action.key  = mat_name;
            return action;
        }
    }
    else if (directive == "set")
    {
        const auto& target = GetToken(tokens, 1);

        if (target == "flag")
        {
            const auto& flag_name  = GetToken(tokens, 2);
            const auto& flag_value = GetToken(tokens, 3);
            const auto& step_value = GetToken(tokens, 4);
            if (flag_name.empty())
                return std::nullopt;

            bool value;
            if (flag_value == "true")
                value = true;
            else if (flag_value == "false")
                value = false;
            else return std::nullopt;

            float step = 0.5f;

            uik::Animation::Action action;
            action.type  = uik::Animation::Action::Type::SetFlag;
            action.key   = flag_name;
            action.value = value;
            if (!step_value.empty())
            {
                if (const auto* ptr = ToNumber(step_value, &step))
                    action.step = *ptr;
            }

            return action;
        }
        else if (target == "prop")
        {
            const auto& prop_name = GetToken(tokens, 2);
            const auto& prop_val  = GetToken(tokens, 3);
            const auto& step_val  = GetToken(tokens, 4);
            if (prop_name.empty() || prop_val.empty())
                return std::nullopt;

            uik::Animation::Action action;
            action.type = uik::Animation::Action::Type::SetProp;
            action.key  = prop_name;

            float step = 0.5f;
            if (!step_val.empty())
            {
                if (const auto* ptr = ToNumber(step_val, &step))
                    action.step = *ptr;
            }


            {
                std::string val;
                if (base::Scanf(prop_val, &val))
                {
                    action.value = uik::StyleProperty { std::move(val) };
                    return action;
                }
            }
            {
                uik::Color4f val;
                if (base::Scanf(prop_val, &val))
                {
                    action.value = uik::StyleProperty { val };
                    return action;
                }
                uik::Color color = uik::Color::Black;
                if (ToEnum(prop_val, &color))
                {
                    action.value = uik::StyleProperty  { color };
                    return action;
                }
            }
            {
                float val = 0.0f;
                if (base::Scanf(prop_val, &val))
                {
                    action.value = uik::StyleProperty { val };
                    return action;
                }
            }
            {
                int val = 0;
                if (base::Scanf(prop_val, &val))
                {
                    action.value = uik::StyleProperty { val };
                    return action;
                }
            }
            WARN("Failed to parse widget animation value. '%1']", prop_val);
        }
        else if (target == "material")
        {
            // todo:
            return std::nullopt;
        }
        else WARN("Unknown UI widget animation directive target. [target='%1']", target);
    } //else WARN("Unknown UI widget animation directive. [directive='%1']", directive);
    return std::nullopt;
}

} // namespace

namespace uik
{

bool Animation::Parse(std::deque<std::string>& lines)
{
    bool ok = true;

    std::string line;
    while (GetLine(lines, &line))
    {
        // if the line begins a new animation block then stop reading
        // current block actions and return
        if (base::StartsWith(line, "$") ||
            base::StartsWith(line, "@"))
        {
            lines.push_front(std::move(line));
            return ok;
        }

        const auto& tokens = SplitTokens(line);
        const auto& directive = GetToken(tokens, 0);
        const auto& argument  = GetToken(tokens, 1);
        if (directive == "name")
        {
            mName = argument;
        }
        else if (directive == "idle-for")
        {
            float idle_for = 0.0f;
            if (const auto* ptr = ToNumber<float>(argument, &idle_for))
                mIdleFor = *ptr;
            else WARN("Failed to parse UI widget animation value 'idle-for'.");
        }
        else if (directive == "delay")
        {
            float delay = 0.0f;
            if (const auto* ptr = ToNumber<float>(argument, &delay))
                mDelay = *ptr;
            else WARN("Failed to parse UI widget animation value 'delay'.");
        }
        else if (directive == "duration")
        {
            float duration = 0.0f;
            if (const auto* ptr = ToNumber<float>(argument, &duration))
                mDuration = *ptr;
            else WARN("Failed to parse UI widget animation value 'duration'.");
        }
        else if (directive == "interpolation")
        {
            math::Interpolation interpolation = math::Interpolation::Linear;
            if (const auto* ptr = ToEnum(argument, &interpolation))
                mInterpolation = *ptr;
            else WARN("Failed to parse UI widget animation value 'interpolation'.");
        }
        else if (directive == "loops")
        {
            unsigned loops = 0;
            if (argument == "infinite")
                mLoops = std::numeric_limits<unsigned>::max();
            else if (const auto* ptr = ToNumber<unsigned>(argument, &loops))
                mLoops = *ptr;
            else WARN("Failed to parse UI widget animation value 'loops'.");
        }
        else if (const auto& action = ParseAction(tokens))
        {
            mActions.push_back(action.value());
        }
        else
        {
            WARN("Unknown UI widget animation directive. [directive='%1']", directive);
            ok = false;
        }
    }
    return ok;
}

bool Animation::TriggerOnIdle()
{
    if (mTrigger != Trigger::Idle)
        return false;

    EnterTriggerState();
    return true;
}

bool Animation::TriggerOnOpen()
{
    if (mTrigger != Trigger::Open)
        return false;

    EnterTriggerState();
    return true;
}

bool Animation::TriggerOnClose()
{
    if (mTrigger != Trigger::Close)
        return false;

    EnterTriggerState();
    return true;
}

bool Animation::TriggerOnAction(const WidgetAction& action)
{
    if (mWidget->GetId() != action.id)
        return false;

    if (mState == State::Active)
        return false;

    if (action.type == WidgetActionType::FocusChange &&
        (mTrigger == Animation::Trigger::LostFocus ||
         mTrigger == Animation::Trigger::GainFocus))
    {
        const auto has_focus = std::get<bool>(action.value);
        const auto trigger_gain_focus = mTrigger == Animation::Trigger::GainFocus && has_focus;
        const auto trigger_lost_focus = mTrigger == Animation::Trigger::LostFocus && !has_focus;
        if (!(trigger_gain_focus || trigger_gain_focus))
            return false;
    }
    else if (action.type == WidgetActionType::ButtonPress &&
             mTrigger == Animation::Trigger::Click)
    {
        if (mWidget->GetType() != Widget::Type::PushButton)
            return false;
    }
    else if (action.type == WidgetActionType::ValueChange &&
             mTrigger != Animation::Trigger::ValueChange)
        return false;
    else if (action.type == WidgetActionType::MouseEnter &&
             mTrigger != Animation::Trigger::MouseEnter)
        return false;
    else if(action.type == WidgetActionType::MouseLeave &&
            mTrigger != Animation::Trigger::MouseLeave)
        return false;

    EnterTriggerState();
    return true;
}

void Animation::ClearIdle()
{
    if (mTrigger != Trigger::Idle)
        return;

    // could be interrupted before the animation actually transitioned
    // to running state and the state was copied.
    if (mWidgetState)
    {
        mWidget->CopyStateFrom(mWidgetState.get());
        mWidgetState.reset();
        VERBOSE("Cleared widget idle animation state. [name='%1', widget='%2']",
                mName, mWidget->GetName());
    }

    mState = State::Inactive;
}

void Animation::Update(double game_time, float dt)
{
    if (mState == Animation::State::Inactive)
        return;

    const auto prev_time = mTime;

    mTime += dt;

    // read the starting state once when the animation time goes above zero.
    if (prev_time <= 0.0f && mTime > 0.0f)
    {
        EnterRunState();
    }
    if (mTime <= 0.0f)
        return;

    // apply interpolation state update
    const float t = math::clamp(0.0, mDuration, mTime) / mDuration;

    const float key_frame_t = math::interpolate(t, mInterpolation);

    for (auto& key_frame_state : mKeyFrameState)
    {
        if (key_frame_state.empty())
            continue;

        // looks for the interpolation value bounds
        std::optional<KeyFrameAnimationState> lo_bound;
        std::optional<KeyFrameAnimationState> hi_bound;
        for (size_t i = 0; i < key_frame_state.size() - 1; ++i)
        {
            if (key_frame_t >= key_frame_state[i + 0].time &&
                key_frame_t <= key_frame_state[i + 1].time)
            {
                lo_bound = key_frame_state[i + 0];
                hi_bound = key_frame_state[i + 1];
                break;
            }
        }

        if (!lo_bound.has_value() || !hi_bound.has_value())
            continue;

        ASSERT(key_frame_t >= lo_bound->time && key_frame_t <= hi_bound->time);
        const auto segment_duration = hi_bound->time - lo_bound->time;
        const auto segment_t = (key_frame_t - lo_bound->time) / segment_duration;

        // all the keys we need to interpolate on is the intersection
        // keys in both. and when the animation key frame doesn't
        // mention a key (+value) the value is taken from the widget.
        std::unordered_set<std::string> property_keys;
        for (const auto& props: lo_bound->values)
            property_keys.insert(props.first);
        for (const auto& props: hi_bound->values)
            property_keys.insert(props.first);

        // interpolate over all property keys (values)
        for (const auto& key: property_keys)
        {
            bool have_beg_value = true;
            bool have_end_value = true;
            KeyFramePropertyValue beg;
            KeyFramePropertyValue end;
            if (const auto* ptr = base::SafeFind(lo_bound->values, key))
                beg = *ptr;
            else beg = GetWidgetPropertyValue(key, &have_beg_value);

            if (const auto* ptr = base::SafeFind(hi_bound->values, key))
                end = *ptr;
            else end = GetWidgetPropertyValue(key, &have_end_value);
            if (!have_beg_value || !have_end_value)
                continue;

            if (std::holds_alternative<FSize>(beg) && std::holds_alternative<FSize>(end))
            {
                auto size = math::interpolate(std::get<FSize>(beg), std::get<FSize>(end), segment_t,
                                              math::Interpolation::Linear);
                if (key == "size")
                    mWidget->SetSize(size);
            }
            else if (std::holds_alternative<FPoint>(beg) && std::holds_alternative<FPoint>(end))
            {
                auto pos = math::interpolate(std::get<FPoint>(beg), std::get<FPoint>(end), segment_t,
                                             math::Interpolation::Linear);
                if (key == "position")
                    mWidget->SetPosition(pos);
            }
            else if (std::holds_alternative<uik::Color4f>(beg) && std::holds_alternative<uik::Color4f>(end))
            {
                auto color = math::interpolate(std::get<uik::Color4f>(beg), std::get<uik::Color4f>(end), segment_t,
                                               math::Interpolation::Linear);

                // a hack exists in the engine's UI styling system to support defining color
                // values through properties (instead of materials) for the simple cases.
                if (base::EndsWith(key, "-color"))
                    mWidget->SetStyleProperty(key, color);
            }
        }
    }

    for (const auto& action : mInterpolationState)
    {
        if (action.type == Animation::Action::Type::Resize || action.type == Animation::Action::Type::Grow)
        {
            ASSERT(std::holds_alternative<uik::FSize>(action.start));
            ASSERT(std::holds_alternative<uik::FSize>(action.end));

            const auto start_value = std::get<uik::FSize>(action.start);
            const auto end_value = std::get<uik::FSize>(action.end);
            const auto value = math::interpolate(start_value, end_value, t, mInterpolation);
            mWidget->SetSize(value);
        }
        else if (action.type == Animation::Action::Type::Move || action.type == Animation::Action::Type::Translate)
        {
            ASSERT(std::holds_alternative<uik::FPoint>(action.start));
            ASSERT(std::holds_alternative<uik::FPoint>(action.end));

            const auto start_value = std::get<uik::FPoint>(action.start);
            const auto end_value = std::get<uik::FPoint>(action.end);
            const auto value = math::interpolate(start_value, end_value, t, mInterpolation);
            mWidget->SetPosition(value);
        } else BUG("Unhandled widget animation action.");
    }

    // apply step state update
    for (auto& action : mStepState)
    {
        // check if it's time to apply the state change
        if (t < action.step)
            continue;

        // check if the state change has already been applied.
        if (std::holds_alternative<std::monostate>(action.value))
            continue;

        if (action.type == Animation::Action::Type::SetProp)
        {
            ASSERT(std::holds_alternative<StyleProperty>(action.value));

            const auto& style_property = std::get<StyleProperty>(action.value);
            mWidget->SetStyleProperty(action.key, style_property);

        }
        else if (action.type == Animation::Action::Type::DelProp)
        {
            mWidget->DeleteStyleProperty(action.key);
        }
        else if (action.type == Animation::Action::Type::DelMaterial)
        {
            mWidget->DeleteStyleMaterial(action.key);
        }
        else if (action.type == Animation::Action::Type::SetFlag)
        {
            ASSERT(std::holds_alternative<bool>(action.value));

            const auto on_off = std::get<bool>(action.value);
            if (action.key == "Enabled")
                mWidget->SetFlag(Widget::Flags::Enabled, on_off);
            else if (action.key == "Visible")
                mWidget->SetFlag(Widget::Flags::VisibleInGame, on_off);
            else WARN("Unknown widget flag in widget animation. [widget='%1', flag='%2']",
                      mWidget->GetName(), action.key);
        } else BUG("Unhandled widget animation action.");

        // clear the value by setting to monostate (no value)
        action.value = std::monostate {};
    }

    if (mTime >= mDuration)
    {
        const auto time  = mTime - mDuration;
        const auto state = mState;

        // when resetting for loop don't clear the previous state.
        mLoop++;
        mState = Animation::State::Inactive;
        VERBOSE("Widget animation is inactive. [name='%1', trigger=%2, widget='%3']",
                mName, mTrigger, mWidget->GetName());

        if (mLoops == std::numeric_limits<unsigned>::max() || mLoop < mLoops)
        {
            mTime  = -mDelay + time;
            mState = Animation::State::Active;
            VERBOSE("Widget animation loop restart. [name='%1', loop=%2/%3, widget=%4]",
                    mName, mLoop, mLoops == std::numeric_limits<unsigned>::max() ? "inf" : std::to_string(mLoops),
                    mWidget->GetName());
        }
    }
}

bool Animation::IsActiveOnTrigger(Trigger trigger) const noexcept
{
    if (mTrigger != trigger)
        return false;
    if (mState != State::Active)
        return false;

    return true;
}

bool Animation::IsActiveOnAction(const WidgetAction& action) const noexcept
{
    if (mState == State::Inactive)
        return false;

    if (action.type == WidgetActionType::FocusChange)
    {
        const auto has_focus = std::get<bool>(action.value);
        if (has_focus && mTrigger == Trigger::GainFocus)
            return true;
        if (!has_focus && mTrigger == Trigger::LostFocus)
            return true;
    }
    else if (action.type == WidgetActionType::ButtonPress)
    {
        if (mTrigger == Animation::Trigger::Click)
            return true;
    }
    else if (action.type == WidgetActionType::ValueChange)
    {
        if (mTrigger == Animation::Trigger::ValueChange)
            return true;
    }
    else if (action.type == WidgetActionType::MouseEnter)
    {
        if (mTrigger == Animation::Trigger::MouseEnter)
            return true;
    }
    else if (action.type == WidgetActionType::MouseLeave)
    {
        if (mTrigger == Animation::Trigger::MouseLeave)
            return true;
    }
    return false;
}

void Animation::EnterTriggerState()
{
    mState = State::Active;
    mTime  = -mDelay;
    mLoop  = 0;
    mInterpolationState.clear();
    mStepState.clear();
    mKeyFrameState.clear();

    if (mTrigger == Trigger::Idle)
    {
        mTime -= mIdleFor;
    }

    VERBOSE("Widget animation is active. [name='%1', trigger=%2, widget='%3']",
            mName, mTrigger, mWidget->GetName());
}

void Animation::EnterRunState()
{
    // if the state is already initialized then don't do it
    // again. We retain the initial state for looped animation.
    if (!mInterpolationState.empty() || !mStepState.empty() || !mKeyFrameState.empty())
        return;

    if (mTrigger == Trigger::Idle)
    {
        // create a copy of the widget in order to retain the state prior to
        // going idle, but only do this on the first idle start.
        // i.e. if the idle repeats without interruption (idle clear)
        // avoid capturing the state that is the result of the previous
        // idle animation. That is likely not what we want.
        if (!mWidgetState)
            mWidgetState = mWidget->Copy();
    }

    // Create state for each key frame animations key frames' property
    // In other words we can multiple keyword animations and each has
    // multiple key frames and each key frame has multiple properties
    for (const auto& action : mActions)
    {
        if (action.type != Action::Type::Animate)
            continue;
        if (const auto* key_frame_animation = base::SafeFind(mKeyFrameAnimations, action.key))
        {
            std::vector<KeyFrameAnimationState> key_frame_state;

            for (const auto& key_frame : (*key_frame_animation)->keyframes)
            {
                KeyFrameAnimationState state;
                state.time = key_frame.time;
                for (const auto& prop : key_frame.properties)
                {
                    state.values[prop.property_key] = prop.property_value;
                }
                key_frame_state.push_back(state);
            }
            mKeyFrameState.push_back(std::move(key_frame_state));
        } else WARN("No such key frame animation was found. [name='%1']", action.key);
    }

    for (auto& key_frame_state : mKeyFrameState)
    {
        if (key_frame_state.empty())
            continue;

        // make sure our key frames are in ascending order, i.e. from 0% to 100%
        std::sort(key_frame_state.begin(),
                  key_frame_state.end(), [](const auto& a, const auto& b) {
                    return a.time < b.time;
                });

        // fabricate a 0% and 100% animation states if nothing exists
        // so that when interpolating we always have the start bound
        // and the high bound.
        // todo: maybe it should just clamp the values ?
        if (!math::equals(key_frame_state[0].time, 0.0f))
        {
            KeyFrameAnimationState foo;
            foo.time = 0.0f;
            key_frame_state.insert(key_frame_state.begin(), foo);
        }
        const auto last = key_frame_state.size() - 1;
        if (!math::equals(key_frame_state[last].time, 1.0f))
        {
            KeyFrameAnimationState foo;
            foo.time = 1.0f;
            key_frame_state.insert(key_frame_state.end(), foo);
        }
        VERBOSE("Starting key frame animation on widget animation [name='%1', widget='%2']",
                mName, mWidget->GetName());
    }

    // for each animation action read the current starting state that
    // is required for the widget state interpolation. note that
    // currently we can't interpolate style properties such as Color
    // because the starting color is not known here unless it's
    // specified explicitly in the animation string (or in the widget's
    // style string..)
    for (auto& action: mActions)
    {
        if (action.type == Animation::Action::Type::Move)
        {
            InterpolationActionState state;
            state.type = action.type;
            state.start = mWidget->GetPosition();
            state.end   = action.value;
            mInterpolationState.push_back(std::move(state));
        }
        else if (action.type == Animation::Action::Type::Resize)
        {
            InterpolationActionState state;
            state.type = action.type;
            state.start = mWidget->GetSize();
            state.end   = action.value;
            mInterpolationState.push_back(std::move(state));
        }
        else if (action.type == Animation::Action::Type::Translate)
        {
            InterpolationActionState state;
            state.type = action.type;
            state.start = mWidget->GetPosition();
            state.end   = mWidget->GetPosition() + std::get<uik::FPoint>(action.value);
            mInterpolationState.push_back(std::move(state));
        }
        else if (action.type == Animation::Action::Type::Grow)
        {
            InterpolationActionState state;
            state.type = action.type;
            state.start = mWidget->GetSize();
            state.end   = mWidget->GetSize() + std::get<uik::FSize>(action.value);
            mInterpolationState.push_back(std::move(state));
        }
        else
        {
            StepActionState state;
            state.type  = action.type;
            state.key   = action.key;
            state.value = action.value;
            state.step  = action.step;
            mStepState.push_back(std::move(state));
        }

    }
}

Animation::KeyFramePropertyValue Animation::GetWidgetPropertyValue(const std::string& key, bool* success) const
{
    // text-color and other style properties are difficult right
    // now because the values are actually not available in the widget
    // but are defined by the style.
    if (key == "position")
        return mWidget->GetPosition();
    else if (key == "size")
        return mWidget->GetSize();

    *success = false;
    return 0.0f;
}

bool ParseAnimations(const std::string& str, std::vector<Animation>* animations)
{
    auto lines = SplitLines(str);

    bool ok = true;

    uik::Animation::KeyFrameAnimationMap  key_frame_animations;

    std::string line;
    while (GetLine(lines, &line))
    {
        if (line[0] == '@')
        {
            auto anim = ParseKeyFrameAnimation(lines);
            if (anim == std::nullopt)
            {
                WARN("Failed to parse key frame animation.");
                continue;
            }
            auto key_frame_animation = std::make_shared<uik::Animation::KeyFrameAnimation>(anim.value());
            key_frame_animation->name = line;
            key_frame_animations[key_frame_animation->name] = std::move(key_frame_animation);
            continue;
        }

        std::optional<uik::Animation::Trigger> trigger;
        if (line == "$OnIdle")
            trigger = uik::Animation::Trigger::Idle;
        else if (line == "$OnOpen")
            trigger = uik::Animation::Trigger::Open;
        else if (line == "$OnClose")
            trigger = uik::Animation::Trigger::Close;
        else if (line == "$OnClick")
            trigger = uik::Animation::Trigger::Click;
        else if (line == "$OnValue")
            trigger = uik::Animation::Trigger::ValueChange;
        else if (line == "$OnFocusIn")
            trigger = uik::Animation::Trigger::GainFocus;
        else if (line == "$OnFocusOut")
            trigger = uik::Animation::Trigger::LostFocus;
        else if (line == "$OnMouseLeave")
            trigger = uik::Animation::Trigger::MouseLeave;
        else if (line == "$OnMouseEnter")
            trigger = uik::Animation::Trigger::MouseEnter;
        else WARN("No such animation trigger. [trigger='%1']", line);

        ok &= trigger.has_value();

        if (!trigger.has_value())
            continue;

        Animation animation(trigger.value());
        ok &= animation.Parse(lines);
        animations->push_back(std::move(animation));
    }

    for (auto& anim : *animations)
        anim.SetKeyFrameAnimations(key_frame_animations);

    return ok;
}

} // namespace
