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
#include "warnpop.h"

#include <cmath>

namespace game
{
    // Box represents a rectangular object which (unlike gfx::FRect)
    // also maintains orientation.
    class FBox
    {
    public:
        // Create a new unit box by default.
        FBox(float w = 1.0f, float h = 1.0f)
        {
            mTopLeft  = glm::vec2(0.0f, 0.0f);
            mTopRight = glm::vec2(w, 0.0f);
            mBotLeft  = glm::vec2(0.0f, h);
            mBotRight = glm::vec2(w, h);
        }
        void Transform(const glm::mat4& mat)
        {
            mTopLeft  = ToVec2(mat * ToVec4(mTopLeft));
            mTopRight = ToVec2(mat * ToVec4(mTopRight));
            mBotLeft  = ToVec2(mat * ToVec4(mBotLeft));
            mBotRight = ToVec2(mat * ToVec4(mBotRight));
        }
        float GetWidth() const
        { return glm::length(mTopRight - mTopLeft); }
        float GetHeight() const
        { return glm::length(mBotLeft - mTopLeft); }
        float GetRotation() const
        {
            const auto dir = glm::normalize(mTopRight - mTopLeft);
            const auto cosine = glm::dot(glm::vec2(1.0f, 0.0f), dir);
            return std::acos(cosine);
        }
        glm::vec2 GetTopLeft() const
        { return mTopLeft; }
        glm::vec2 GetTopRight() const
        { return mTopRight; }
        glm::vec2 GetBotLeft() const
        { return mBotLeft; }
        glm::vec2 GetBotRight() const
        { return mBotRight; }
        glm::vec2 GetPosition() const
        {
            const auto diagonal = mBotRight - mTopLeft;
            return mTopLeft + diagonal * 0.5f;
        }
        glm::vec2 GetSize() const
        { return glm::vec2(GetWidth(), GetHeight()); }
    private:
        static glm::vec4 ToVec4(const glm::vec2& vec)
        { return glm::vec4(vec.x, vec.y, 1.0f, 1.0f); }
        static glm::vec2 ToVec2(const glm::vec4& vec)
        { return glm::vec2(vec.x, vec.y); }
    private:
        // store the box as 4 2d points each
        // representing one corner of the box.
        // there are alternative ways too, such as
        // position + dim vectors and rotation
        // but this representation is quite simple.
        glm::vec2 mTopLeft;
        glm::vec2 mTopRight;
        glm::vec2 mBotLeft;
        glm::vec2 mBotRight;
    };

    inline FBox TransformBox(const FBox& box, const glm::mat4& mat)
    {
        FBox ret(box);
        ret.Transform(mat);
        return ret;
    }

} // namespace


