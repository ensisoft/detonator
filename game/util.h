// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
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
#  include <glm/glm.hpp>
#  include <glm/gtx/matrix_decompose.hpp>
#  include <glm/gtx/euler_angles.hpp>
#include "warnpop.h"

#include <algorithm> // for min/max
#include <vector>
#include <string>
#include <cmath> // for acos

#include "base/math.h"
#include "game/types.h"

namespace game
{
    using math::RotateVectorAroundZ;
    using math::TransformNormalVector;
    using math::TransformVector;
    using math::TransformPoint;
    using math::FindVectorRotationAroundZ;
    using math::GetRotationFromMatrix;
    using math::GetScaleFromMatrix;
    using math::GetTranslationFromMatrix;

inline FRect ComputeBoundingRect(const glm::mat4& mat) noexcept
{
    // for each corner of a bounding rect compute new positions per
    // the transformation matrix and then choose the min/max on each axis.
    const auto& top_left  = mat * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    const auto& top_right = mat * glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
    const auto& bot_left  = mat * glm::vec4(0.0f, 1.0f, 1.0f, 1.0f);
    const auto& bot_right = mat * glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

    // choose min/max on each axis.
    const auto left = std::min(std::min(top_left.x, top_right.x),
                               std::min(bot_left.x, bot_right.x));
    const auto right = std::max(std::max(top_left.x, top_right.x),
                                std::max(bot_left.x, bot_right.x));
    const auto top = std::min(std::min(top_left.y, top_right.y),
                              std::min(bot_left.y, bot_right.y));
    const auto bottom = std::max(std::max(top_left.y, top_right.y),
                                 std::max(bot_left.y, bot_right.y));
    return {left, top, right - left, bottom - top};
}



inline FBox TransformRect(const FRect& rect, const glm::mat4& mat) noexcept
{
    const auto x = rect.GetX();
    const auto y = rect.GetY();
    const auto w = rect.GetWidth();
    const auto h = rect.GetHeight();
    return {mat, x, y, w, h};
}


template<typename T>
bool EraseByName(std::vector<T>& vector, const std::string& name)
{
    for (auto it=vector.begin(); it != vector.end(); ++it)
    {
        if ((*it)->GetName() == name) {
            vector.erase(it);
            return true;
        }
    }
    return false;
}

template<typename T>
bool EraseById(std::vector<T>& vector, const std::string& id)
{
    for (auto it=vector.begin(); it != vector.end(); ++it)
    {
        if ((*it)->GetId() == id) {
            vector.erase(it);
            return true;
        }
    }
    return false;
}

template<typename T>
T* FindByName(std::vector<std::shared_ptr<T>>& vector, const std::string& name)
{
    for (const auto& obj : vector) {
        if (obj->GetName() == name)
            return obj.get();
    }
    return nullptr;
}

template<typename T>
T* FindById(std::vector<std::shared_ptr<T>>& vector, const std::string& id)
{
    for (const auto& obj : vector) {
        if (obj->GetId() == id)
            return obj.get();
    }
    return nullptr;
}

template<typename T>
const T* FindByName(const std::vector<std::shared_ptr<T>>& vector, const std::string& name)
{
    for (const auto& obj : vector) {
        if (obj->GetName() == name)
            return obj.get();
    }
    return nullptr;
}

template<typename T>
const T* FindById(const std::vector<std::shared_ptr<T>>& vector, const std::string& id)
{
    for (const auto& obj : vector) {
        if (obj->GetId() == id)
            return obj.get();
    }
    return nullptr;
}


} // namespace