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

        Transform(const glm::mat4& mat)
        {
            mTransform.resize(1);
            mTransform[0] = mat;
        }
        Transform(glm::mat4&& mat)
        {
            mTransform.resize(1);
            mTransform[0] = std::move(mat);
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
        void Scale(const glm::vec2& scale)
        {
            Scale(scale.x, scale.y);
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
        void Push(const glm::mat4& mat)
        {
            mTransform.push_back(mat);
        }
        void Push(glm::mat4&& mat)
        {
            mTransform.push_back(std::move(mat));
        }

        // Pop the latest transform off of the transform stack.
        void Pop()
        {
            // we always have the base level at 0 index.
            ASSERT(mTransform.size() > 1);
            mTransform.pop_back();
        }

        // Get the number of individual transformation scopes.
        size_t GetNumTransforms()
        {
            return mTransform.size();
        }

    private:
        std::vector<glm::mat4> mTransform;
    };

} // namespace
