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

#include "game/types.h"

namespace game
{

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

inline float GetRotationFromMatrix(const glm::mat4& mat) noexcept
{
    glm::vec3 scale;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::quat orientation;
    glm::decompose(mat, scale, orientation, translation, skew, perspective);
    return glm::angle(orientation);
}

inline glm::vec2 GetScaleFromMatrix(const glm::mat4& mat) noexcept
{
    glm::vec3 scale;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::quat orientation;
    glm::decompose(mat, scale, orientation, translation, skew, perspective);
    return scale;
}

inline glm::vec2 GetTranslationFromMatrix(const glm::mat4& mat) noexcept
{
    glm::vec3 scale;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::quat orientation;
    glm::decompose(mat, scale, orientation, translation, skew, perspective);
    return translation;
}

// Rotate a vector on the xy plane around the Z axis.
inline glm::vec2 RotateVectorAroundZ(const glm::vec2& vec, float angle) noexcept
{
    return glm::eulerAngleZ(angle) * glm::vec4(vec.x, vec.y, 0.0f, 0.0f);
}


// transform a direction vector (such as a normal) safely even if the
// transformation matrix contains a non-uniform scale.
inline glm::vec4 TransformNormalVector(const glm::mat4& matrix, const glm::vec4& vector) noexcept
{
    return glm::transpose(glm::inverse(matrix)) * vector;
}
// transform a direction vector (such as a normal) safely even if the
// transformation matrix contains a non-uniform scale.
inline glm::vec2 TransformNormalVector(const glm::mat4& matrix, const glm::vec2& vector) noexcept
{
    return TransformNormalVector(matrix, glm::vec4(vector, 0.0f, 0.0f));
}

inline glm::vec4 TransformVector(const glm::mat4& matrix, const glm::vec4& vector) noexcept
{
    return glm::normalize(matrix * glm::vec4(vector.x, vector.y, vector.z, 0.0f)); // disregard translation
}
inline glm::vec4 TransformVector(const glm::mat4& matrix, const glm::vec2& vector) noexcept
{
    return glm::normalize(matrix * glm::vec4(vector.x, vector.y, 0.0f, 0.0f));
}

inline glm::vec4 TransformPoint(const glm::mat4& matrix, const glm::vec4& point) noexcept
{
    return matrix * point;
}

inline glm::vec4 TransformPoint(const glm::mat4& matrix, const glm::vec2& point) noexcept
{
    return matrix * glm::vec4(point.x, point.y, 0.0f, 1.0f);
}

// Find the angle that rotates the basis vector X such that
// it's collinear with the parameter vector.
// returns the angle in radians.
inline float FindVectorRotationAroundZ(const glm::vec2& vec) noexcept
{
    const auto cosine = glm::dot(glm::normalize(vec), glm::vec2(1.0f, 0.0f));
    if (vec.y > 0.0f)
        return std::acos(cosine);
    return -std::acos(cosine);
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