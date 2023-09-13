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
            action.end_value = uik::FSize(width, height);
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
        action.end_value = uik::FPoint(x, y);
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
            action.name = prop_name;
            return action;
        }
        else if (target == "material")
        {
            const auto& mat_name = GetToken(tokens, 2);
            if (mat_name.empty())
                return std::nullopt;

            uik::Animation::Action action;
            action.type = uik::Animation::Action::Type::DelMaterial;
            action.name = mat_name;
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
            if (flag_name.empty() || flag_value.empty())
                return std::nullopt;
            if (flag_value != "true" || flag_value != "false")
                return std::nullopt;

            uik::Animation::Action action;
            action.type  = uik::Animation::Action::Type::SetFlag;
            action.name  = flag_name;
            action.end_value = flag_value == "true";
            return action;
        }
        else if (target == "prop")
        {
            const auto& prop_name = GetToken(tokens, 2);
            const auto& prop_val  = GetToken(tokens, 3);
            if (prop_name.empty() || prop_val.empty())
                return std::nullopt;

            uik::Animation::Action action;
            action.type = uik::Animation::Action::Type::SetProp;
            action.name = prop_name;

            {
                std::string val;
                if (base::Scanf(prop_val, &val))
                {
                    action.end_value = uik::StyleProperty { std::move(val) };
                    return action;
                }
            }
            {
                uik::Color4f val;
                if (base::Scanf(prop_val, &val))
                {
                    action.end_value = uik::StyleProperty { val };
                    return action;
                }
            }
            {
                float val = 0.0f;
                if (base::Scanf(prop_val, &val))
                {
                    action.end_value = uik::StyleProperty { val };
                    return action;
                }
            }
            {
                int val = 0;
                if (base::Scanf(prop_val, &val))
                {
                    action.end_value = uik::StyleProperty { val };
                    return action;
                }
            }
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

bool ParseAnimation(std::deque<std::string>& lines, uik::Animation* animation)
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
        if (directive == "delay")
        {
            float delay = 0.0f;
            if (const auto* ptr = ToNumber<float>(argument, &delay))
                animation->delay = *ptr;
            else WARN("Failed to parse UI widget animation delay. [value='%1']", argument);
        }
        else if (directive == "duration")
        {
            float duration = 0.0f;
            if (const auto* ptr = ToNumber<float>(argument, &duration))
                animation->duration = *ptr;
            else WARN("Failed to parse UI widget animation duration. [value='%1']", argument);
        }
        else if (directive == "interpolation")
        {
            math::Interpolation interpolation = math::Interpolation::Linear;
            if (const auto* ptr = ToEnum(argument, &interpolation))
                animation->interpolation = *ptr;
            else WARN("Failed to parse UI widget animation interpolation. [value='%1']", argument);
        }
        else if (directive == "loops")
        {
            unsigned loops = 0;
            if (argument == "infinite")
                animation->loops = std::numeric_limits<unsigned>::max();
            else if (const auto* ptr = ToNumber<unsigned>(argument, &loops))
                animation->loops = *ptr;
            else WARN("Failed to parse UI widget animation loops. [value='%1']", argument);
        }
        else if (const auto& action = ParseAction(tokens))
        {
            animation->actions.push_back(action.value());
        }
        else
        {
            WARN("Unknown UI widget animation directive. [directive='%1']", directive);
            ok = false;
        }
    }
    return ok;
}

} // namespace

namespace uik
{

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

        Animation animation;
        animation.trigger = trigger.value();
        ok &= ParseAnimation(lines, &animation);
        animations->push_back(std::move(animation));
    }
    return ok;
}

} // namespace