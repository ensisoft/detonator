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

#include "config.h"

#include "warnpush.h"
#  include <glm/glm.hpp>
#  include <glm/gtc/quaternion.hpp>
#  include <glm/gtx/matrix_decompose.hpp>
#  include <sol/sol.hpp>
#include "warnpop.h"

#include "base/format.h"
#include "engine/lua.h"

using namespace engine::lua;

namespace {
template<typename Vector>
void BindGlmVectorOp(sol::usertype<Vector>& vec)
{
    vec.set_function(sol::meta_function::addition, [](const Vector& a, const Vector& b) {
        return a + b;
    });
    vec.set_function(sol::meta_function::subtraction, [](const Vector& a, const Vector& b) {
        return a - b;
    });
    vec.set_function(sol::meta_function::multiplication, sol::overload(
        [](float scalar, const Vector& vector) {
            return vector * scalar;
        },
        [](const Vector& vector, float scalar) {
            return vector * scalar;
        },
        [](const Vector& a, const Vector& b) {
            return a * b;
        }
    ));
    vec.set_function(sol::meta_function::division, sol::overload(
        [](float scalar, const Vector& vector) {
            return scalar / vector;
        },
        [](const Vector& vector, float scalar) {
            return vector / scalar;
        },
        [](const Vector& a, const Vector& b) {
            return a / b;
        }
    ));
    vec.set_function(sol::meta_function::to_string, [](const Vector& vector) {
        return base::ToString(vector);
    });
    vec["length"]    = [](const Vector& vec) { return glm::length(vec); };
    vec["normalize"] = [](const Vector& vec) { return glm::normalize(vec); };
}
} // namespace

namespace engine
{

void BindGLM(sol::state& L)
{
    auto glm  = L.create_named_table("glm");

    auto vec2 = glm.new_usertype<glm::vec2>("vec2",
        sol::constructors<glm::vec2(), glm::vec2(float, float)>(),
        sol::meta_function::index, [](const glm::vec2& vec, int index) {
            if (index >= 2)
                throw GameError("glm.vec2 access out of bounds");
            return vec[index];
            }
    );
    vec2["x"] = &glm::vec2::x;
    vec2["y"] = &glm::vec2::y;
    BindGlmVectorOp<glm::vec2>(vec2);

    auto vec3 = glm.new_usertype<glm::vec3>("vec3",
        sol::constructors<glm::vec3(), glm::vec3(float, float, float)>(),
        sol::meta_function::index, [](const glm::vec3& vec, int index) {
                if (index >= 3)
                    throw GameError("glm.vec3 access out of bounds");
                return vec[index];
            }
    );
    vec3["x"] = &glm::vec3::x;
    vec3["y"] = &glm::vec3::y;
    vec3["z"] = &glm::vec3::z;
    BindGlmVectorOp<glm::vec3>(vec3);

    auto vec4 = glm.new_usertype<glm::vec4>("vec4",
        sol::constructors<glm::vec4(), glm::vec4(float, float, float, float)>(),
        sol::meta_function::index, [](const glm::vec4& vec, int index) {
                if (index >= 4)
                    throw GameError("glm.vec4 access out of bounds");
                return vec[index];
            }
    );
    vec4["x"] = &glm::vec4::x;
    vec4["y"] = &glm::vec4::y;
    vec4["z"] = &glm::vec4::z;
    vec4["w"] = &glm::vec4::w;
    BindGlmVectorOp<glm::vec4>(vec4);

    glm["dot"] = sol::overload(
            [](const glm::vec2& a, const glm::vec2& b) { return glm::dot(a, b); },
            [](const glm::vec3& a, const glm::vec3& b) { return glm::dot(a, b); },
            [](const glm::vec4& a, const glm::vec4& b) { return glm::dot(a, b); });
    glm["length"] = sol::overload(
            [](const glm::vec2& vec) { return glm::length(vec); },
            [](const glm::vec3& vec) { return glm::length(vec); },
            [](const glm::vec4& vec) { return glm::length(vec); });
    glm["normalize"] = sol::overload(
            [](const glm::vec2& vec) { return glm::normalize(vec); },
            [](const glm::vec3& vec) { return glm::normalize(vec); },
            [](const glm::vec4& vec) { return glm::normalize(vec); });

    auto mat4 = glm.new_usertype<glm::mat4>("mat4");
    mat4["decompose"] = [](const glm::mat4& mat) {
        glm::vec3 scale;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::quat orientation;
        glm::decompose(mat, scale, orientation, translation, skew, perspective);
        return std::make_tuple(glm::vec2(translation.x, translation.y),
                               glm::vec2(scale.x, scale.y),
                               glm::angle(orientation));
    };
}


} // namespace