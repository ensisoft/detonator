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

#include <vector>

#include "base/assert.h"

namespace gfx
{
    // Express a series of graphics operations such as 
    // translation, scaling and rotation as a simple 
    // transform object. Some good resources about using matrices
    // and transformations:
    // https://fgiesen.wordpress.com/2012/02/12/row-major-vs-column-major-row-vectors-vs-column-vectors/
    // https://stackoverflow.com/questions/21923482/rotate-and-translate-object-in-local-and-global-orientation-using-glm
    class Transform
    {
    public:
        Transform()
        {
            Reset();
        }
        // Set absolute position. This will override any previously
        // accumulated translation. 
        void MoveTo(float x, float y)
        {
            mTransform.back()[3] = glm::vec4(x, y, 0.0f, 1.0f);
        }

        // Accumulate a translation to the current transform, i.e.
        // the movement is relative to the current position.
        void Translate(float x, float y)
        {
            // note that since we're using identity matrix here as the basis transformation
            // the translation is always relative to the untransformed basis, i.e.
            // the global cooridinate system.
            mTransform.back() = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f)) * mTransform.back();
        }

        template<typename T>
        void Translate(const T& t)
        {
            Translate(t.GetX(), t.GetY());
        }
        void Translate(const glm::vec2& offset)
        {
            Translate(offset.x, offset.y);
        }

        // Set absolute resize. This will override any previously
        // accumulated scaling. 
        void Resize(float sx, float sy)
        {
            const auto& x = glm::normalize(mTransform.back()[0]);
            const auto& y = glm::normalize(mTransform.back()[1]);
            const auto& z = glm::normalize(mTransform.back()[2]);
            const auto& t = mTransform.back()[3];
            mTransform.back()[0] = x * sx;
            mTransform.back()[1] = y * sy;
            mTransform.back()[2] = z;
            mTransform.back()[3] = t;
        }

        // Accumulate a scaling operation to the current transform, i.e.
        // the scaling is relative to the current transform.
        void Scale(float sx, float sy)
        {
            mTransform.back() = glm::scale(glm::mat4(1.0f),  glm::vec3(sx, sy, 1.0f)) * mTransform.back();
        }

        template<typename T>
        void Scale(const T& t)
        {
            Scale(t.GetWidth(), t.GetHeight());
        }

        // Accumulate rotation to the current transformation
        void Rotate(float radians)
        {
            mTransform.back() = glm::eulerAngleZ(radians) * mTransform.back();
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
            mTransform.clear();
            mTransform.resize(1); 
            mTransform[0] = glm::mat4(1.0f);
        }

        // Get the transformation expressed as a matrix.
        glm::mat4 GetAsMatrix() const 
        { 
            glm::mat4 ret(1.0f); 
            for (auto it = mTransform.rbegin(); it != mTransform.rend(); ++it)
            {
                // what we want is the following.
                // ret = mTransform[0] * mTransform[1] ... * mTransform[n]
                ret = *it * ret;
            }
            return ret; // mTransform;
        }
        
        // Begin a new scope for the next transformation.
        // Pushing and popping transformations allows transformations
        // to be "stacked" i.e. become relative to each other. 
        void Push()
        {
            mTransform.push_back(glm::mat4(1.0f));
        }
        // Pop the latest transform off of the transform stack.
        void Pop()
        {
            // we always have the base level at 0 index.
            ASSERT(mTransform.size() > 1);
            mTransform.pop_back();
        }

    private:
        std::vector<glm::mat4> mTransform;
    };

} // namespace
