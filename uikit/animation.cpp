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

std::optional<uik::Animation::Action> ParseAction(const std::vector<std::string>& tokens)
{
    const auto& directive = GetToken(tokens, 0);

    if (directive == "resize" || directive == "grow")
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
        if (line[0] == '$')
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
        else if (directive == "delay")
        {
            float delay = 0.0f;
            if (const auto* ptr = ToNumber<float>(argument, &delay))
                mDelay = *ptr;
            else WARN("Failed to parse UI widget animation delay. [value='%1']", argument);
        }
        else if (directive == "duration")
        {
            float duration = 0.0f;
            if (const auto* ptr = ToNumber<float>(argument, &duration))
                mDuration = *ptr;
            else WARN("Failed to parse UI widget animation duration. [value='%1']", argument);
        }
        else if (directive == "interpolation")
        {
            math::Interpolation interpolation = math::Interpolation::Linear;
            if (const auto* ptr = ToEnum(argument, &interpolation))
                mInterpolation = *ptr;
            else WARN("Failed to parse UI widget animation interpolation. [value='%1']", argument);
        }
        else if (directive == "loops")
        {
            unsigned loops = 0;
            if (argument == "infinite")
                mLoops = std::numeric_limits<unsigned>::max();
            else if (const auto* ptr = ToNumber<unsigned>(argument, &loops))
                mLoops = *ptr;
            else WARN("Failed to parse UI widget animation loops. [value='%1']", argument);
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
        VERBOSE("Widget animation is inactive. [name='%1', trigger=%1, widget='%3']",
                mName, mTrigger, mWidget->GetName());

        // todo: should we transfer the reminder of the  time to current time?
        const auto t = mTime - mDuration;

        // when resetting for loop don't clear the previous state.
        mLoop++;
        mState = Animation::State::Inactive;

        if (mLoops == std::numeric_limits<unsigned>::max() || mLoop < mLoops)
        {
            mTime  = -mDelay + t;
            mState = Animation::State::Active;
            VERBOSE("Widget animation loop restart. [name='%1', loop=%1/%2, widget=%3]",
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
    VERBOSE("Widget animation is active. [name='%1', trigger=%2, widget='%3']",
            mName, mTrigger, mWidget->GetName());
}

void Animation::EnterRunState()
{
    // if the state is already initialized then don't do it
    // again. We retain the initial state for looped animation.
    if (!mInterpolationState.empty() || !mStepState.empty())
        return;

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


bool ParseAnimations(const std::string& str, std::vector<Animation>* animations)
{
    auto lines = SplitLines(str);

    bool ok = true;

    std::string line;
    while (GetLine(lines, &line))
    {
        std::optional<uik::Animation::Trigger> trigger;
        if (line == "$OnOpen")
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
    return ok;
}

} // namespace