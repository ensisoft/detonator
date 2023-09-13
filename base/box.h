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

#include "warnpush.h"
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <cmath>
#include <algorithm> // for min, max

#include "base/types.h"

namespace base
{
    // Box represents a rectangular object which (unlike base::FRect)
    // also maintains orientation.
    class FBox
    {
    public:
        FBox() = default;

        FBox(const glm::mat4& mat)
        {
            Transform(mat);
        }

        // Create a new unit box by default.
        FBox(float w, float h)
        {
            mTopLeft  = glm::vec2(0.0f, 0.0f);
            mTopRight = glm::vec2(w,    0.0f);
            mBotLeft  = glm::vec2(0.0f, h);
            mBotRight = glm::vec2(w,    h);
        }
        FBox(float x, float y, float w, float h)
        {
            mTopLeft  = glm::vec2(x,     y);
            mTopRight = glm::vec2(x + w, y);
            mBotLeft  = glm::vec2(x,     y + h);
            mBotRight = glm::vec2(x + w, y + h);
        }
        FBox(const glm::mat4& mat, float x, float y, float w, float h)
        {
            mTopLeft  = ToVec2(mat * ToVec4(glm::vec2(x,     y    )));
            mTopRight = ToVec2(mat * ToVec4(glm::vec2(x + w, y    )));
            mBotLeft  = ToVec2(mat * ToVec4(glm::vec2(x,     y + h)));
            mBotRight = ToVec2(mat * ToVec4(glm::vec2(x + w, y + h)));
        }
        FBox(const glm::mat4& mat, float w, float h)
        {
            mTopLeft  = ToVec2(mat * ToVec4(glm::vec2(0.0f, 0.0f)));
            mTopRight = ToVec2(mat * ToVec4(glm::vec2(w,    0.0f)));
            mBotLeft  = ToVec2(mat * ToVec4(glm::vec2(0.0f, h)));
            mBotRight = ToVec2(mat * ToVec4(glm::vec2(w,    h)));
        }
        void Transform(const glm::mat4& mat)
        {
            mTopLeft  = ToVec2(mat * ToVec4(mTopLeft));
            mTopRight = ToVec2(mat * ToVec4(mTopRight));
            mBotLeft  = ToVec2(mat * ToVec4(mBotLeft));
            mBotRight = ToVec2(mat * ToVec4(mBotRight));
        }
        inline float GetWidth() const noexcept
        { return glm::length(mTopRight - mTopLeft); }
        inline float GetHeight() const noexcept
        { return glm::length(mBotLeft - mTopLeft); }
        inline glm::vec2 GetTopLeft() const noexcept
        { return mTopLeft; }
        inline glm::vec2 GetTopRight() const noexcept
        { return mTopRight; }
        inline glm::vec2 GetBotLeft() const noexcept
        { return mBotLeft; }
        inline glm::vec2 GetBotRight() const noexcept
        { return mBotRight; }
        inline glm::vec2 GetSize() const noexcept
        { return glm::vec2(GetWidth(), GetHeight()); }
        inline glm::vec2 GetCenter() const noexcept
        {
            const auto diagonal = mBotRight - mTopLeft;
            return mTopLeft + diagonal * 0.5f;
        }
        float GetRotation() const noexcept
        {
            const auto dir = glm::normalize(mTopRight - mTopLeft);
            const auto cosine = glm::dot(glm::vec2(1.0f, 0.0f), dir);
            if (dir.y < 0.0f)
                return -std::acos(cosine);
            return std::acos(cosine);
        }
        void Reset(float w = 1.0f, float h = 1.0f) noexcept
        {
            mTopLeft  = glm::vec2(0.0f, 0.0f);
            mTopRight = glm::vec2(w, 0.0f);
            mBotLeft  = glm::vec2(0.0f, h);
            mBotRight = glm::vec2(w, h);
        }
        FRect GetBoundingRect() const noexcept
        {
            // for each corner of a bounding rect compute new positions per
            // the transformation matrix and then choose the min/max on each axis.
            const auto& top_left  = GetTopLeft();
            const auto& top_right = GetTopRight();
            const auto& bot_left  = GetBotLeft();
            const auto& bot_right = GetBotRight();

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
    private:
        static glm::vec4 ToVec4(const glm::vec2& vec) noexcept
        { return glm::vec4(vec.x, vec.y, 1.0f, 1.0f); }
        static glm::vec2 ToVec2(const glm::vec4& vec) noexcept
        { return glm::vec2(vec.x, vec.y); }
    private:
        // store the box as 4 2d points each
        // representing one corner of the box.
        // there are alternative ways too, such as
        // position + dim vectors and rotation
        // but this representation is quite simple.
        glm::vec2 mTopLeft  = {0.0f, 0.0f};
        glm::vec2 mTopRight = {1.0f, 0.0f};
        glm::vec2 mBotLeft  = {0.0f, 1.0f};
        glm::vec2 mBotRight = {1.0f, 1.0};
    };

    inline FBox TransformBox(const FBox& box, const glm::mat4& mat) noexcept
    {
        FBox ret(box);
        ret.Transform(mat);
        return ret;
    }
} // namespace