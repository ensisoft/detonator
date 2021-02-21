// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <glm/glm.hpp>
#  include <glm/gtx/matrix_decompose.hpp>
#  include <glm/gtx/euler_angles.hpp>
#include "warnpop.h"

#include <algorithm> // for min/max

#include "engine/types.h"

namespace game
{

inline FRect ComputeBoundingRect(const glm::mat4& mat)
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
    return FRect(left, top, right - left, bottom - top);
}

inline float GetRotationFromMatrix(const glm::mat4& mat)
{
    glm::vec3 scale;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::quat orientation;
    glm::decompose(mat, scale, orientation, translation, skew, perspective);
    return glm::angle(orientation);
}

inline glm::vec2 GetScaleFromMatrix(const glm::mat4& mat)
{
    glm::vec3 scale;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::quat orientation;
    glm::decompose(mat, scale, orientation, translation, skew, perspective);
    return glm::vec2(scale.x, scale.y);
}

inline glm::vec2 GetTranslationFromMatrix(const glm::mat4& mat)
{
    glm::vec3 scale;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::quat orientation;
    glm::decompose(mat, scale, orientation, translation, skew, perspective);
    return glm::vec2(translation.x, translation.y);
}

inline glm::vec2 RotateVector(const glm::vec2& vec, float angle)
{
    auto ret = glm::eulerAngleZ(angle) * glm::vec4(vec.x, vec.y, 0.0f, 0.0f);
    return glm::vec2(ret.x, ret.y);
}

} // namespace