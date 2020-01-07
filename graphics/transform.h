// Copyright (c) 2010-2018 Sami Väisänen, Ensisoft
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
#  include <glm/gtc/matrix_transform.hpp>
#  include <glm/gtx/euler_angles.hpp>
#  include <glm/mat4x4.hpp>
#include "warnpop.h"

namespace gfx
{
    // Express a series of graphics operations such as 
    // translation, scaling and rotation as a simple 
    // transform object.
    class Transform
    {
    public:
        // Set absolute position. This will override any previously
        // accumulated translation. 
        void MoveTo(float x, float y)
        {
            mTransform[3] = glm::vec4(x, y, 0.0f, 1.0f);
        }

        // Accumulate a translation to the current transform, i.e.
        // the the movement is relative to the current position.
        void Translate(float x, float y)
        {
            mTransform = glm::translate(glm::mat4(1.0f), 
                glm::vec3(x, y, 0.0f)) * mTransform;
        }

        template<typename T>
        void Translate(const T& t)
        {
            Translate(t.GetX(), t.GetY());
        }

        // Set absolute resize. This will override any previously
        // accumulated scaling. 
        void Resize(float sx, float sy)
        {
            mTransform = glm::scale(glm::mat4(1.0f), 
                glm::vec3(1.0f/mScale.x, 1.0f/mScale.y, 1.0f)) * mTransform;
            mTransform = glm::scale(glm::mat4(1.0f), 
                glm::vec3(sx, sy, 1.0f)) * mTransform;
            mScale = glm::vec2(sx, sy);
        }

        // Accumulate a scaling operation to the current transform, i.e.
        // the scaling is relative to the current transform.
        void Scale(float sx, float sy)
        {
            mTransform = glm::scale(glm::mat4(1.0f), 
                glm::vec3(sx, sy, 1.0f)) * mTransform;
            mScale *= glm::vec2(sx, sy);
        }

        template<typename T>
        void Scale(const T& t)
        {
            Scale(t.GetWidth(), t.GetHeight());
        }

        // Accumulate rotation to the current transformation
        void Rotate(float radians)
        {
            mTransform = glm::eulerAngleZ(radians) * mTransform;
        }

        // Set absolute position. This will override any previously
        // accumulated translation. Works with any type T that has
        // x() and y() methods.
        template<typename T>
        void MoveTo(const T& t)
        {
            MoveTo(t.GetX(),t.GetY());
        }

        // Set absolute resize. This will override any previously
        // accumulated scaling. Works with any type T that has
        // width() and height() methods.
        template<typename T>
        void Resize(const T& t)
        {
            Resize(t.GetWidth(), t.GetHeight());
        }

        // Reset any transformation to identity, i.e. no transformation.
        void Reset()
        {
            mTransform = glm::mat4(1.0f);
            mScale = glm::vec2(1.0f, 1.0f);
        }

        // Get the transformation expressed as a matrix.
        const glm::mat4& GetAsMatrix() const 
        { return mTransform; }

    private:
        glm::mat4 mTransform = glm::mat4(1.0f);
        glm::vec2 mScale = glm::vec2(1.0f, 1.0f);
    };

} // namespace
