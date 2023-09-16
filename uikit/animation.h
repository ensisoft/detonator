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

    struct Animation {
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

        struct Action {
            enum class Type {
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
            std::variant<std::monostate, StyleProperty, FSize, FPoint, bool> end_value;
            std::variant<std::monostate, StyleProperty,  FSize, FPoint, bool> start_value;
            std::string name;
        };
        Trigger trigger = Trigger::Open;
        State state = State::Inactive;
        math::Interpolation interpolation = math::Interpolation::Linear;
        float duration = 1.0f;
        float delay    = 0.0f;
        float time     = 0.0f;
        unsigned loops = 1;

        std::vector<Action> actions;
        Widget* widget = nullptr;
    };

    bool ParseAnimations(const std::string& str, std::vector<Animation>* animations);

    using AnimationStateArray = std::vector<Animation>;

} // namespace

