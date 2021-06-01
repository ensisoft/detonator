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

#include "config.h"

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "base/test_help.h"
#include "base/color4f.h"
#include "engine/lua.h"

void unit_test_glm()
{
    sol::state L;
    game::BindGLM(L);

    const glm::vec2 a(1.0f, 2.0f);
    const glm::vec2 b(-1.0f, -2.0f);

    L.script(
R"(
function oob(a)
    return a[3]
end
function array(a)
    return glm.vec2:new(a[0], a[1])
end
function read(a)
    return glm.vec2:new(a.x, a.y)
end
function write(a, b)
   a.x = b.x
   a.y = b.y
   return a
end
function add_vector(a, b)
  return a + b
end
function sub_vector(a, b)
  return a - b
end
function multiply(vector, scalar)
   return vector * scalar
end
function divide(vector, scalar)
   return vector / scalar
end
    )");

    // oob
    {
        sol::safe_function func = L["oob"];
        sol::function_result result = func(a);
        TEST_REQUIRE(result.valid() == false);
    }

    // read
    {
        glm::vec2 ret = L["array"](a);
        TEST_REQUIRE(real::equals(ret.x, a.x));
        TEST_REQUIRE(real::equals(ret.y, a.y));
    }

    // read
    {
        glm::vec2 ret = L["read"](a);
        TEST_REQUIRE(real::equals(ret.x, a.x));
        TEST_REQUIRE(real::equals(ret.y, a.y));
    }

    // write
    {
        glm::vec2 ret = L["write"](a, b);
        TEST_REQUIRE(real::equals(ret.x, b.x));
        TEST_REQUIRE(real::equals(ret.y, b.y));
    }

    // multiply
    {
        glm::vec2 ret = L["multiply"](a, 2.0f);
        TEST_REQUIRE(real::equals(ret.x, 2.0f * a.x));
        TEST_REQUIRE(real::equals(ret.y, 2.0f * a.y));
    }
    // divide
    {
        glm::vec2 ret = L["divide"](a, 2.0f);
        TEST_REQUIRE(real::equals(ret.x, a.x / 2.0f));
        TEST_REQUIRE(real::equals(ret.y, a.y / 2.0f));
    }

    // add vectors
    {
        glm::vec2 ret = L["add_vector"](a, b);
        TEST_REQUIRE(real::equals(ret.x, a.x + b.x));
        TEST_REQUIRE(real::equals(ret.y, a.y + b.y));
    }

    // sub vectors
    {
        glm::vec2 ret = L["sub_vector"](a, b);
        TEST_REQUIRE(real::equals(ret.x, a.x - b.x));
        TEST_REQUIRE(real::equals(ret.y, a.y - b.y));
    }
}

void unit_test_base()
{
    // color4f
    {
        sol::state L;
        L.open_libraries();
        game::BindBase(L);

        L.script(
            R"(
function make_red()
    return base.Color4f:new(1.0, 0.0, 0.0, 1.0)
end
function make_green()
    return base.Color4f:new(0.0, 1.0, 0.0, 1.0)
end
function make_blue()
    return base.Color4f:new(0.0, 1.0, 0.0, 1.0)
end

function set_red()
    local ret = base.Color4f:new()
    ret:SetColor(base.Colors.Red)
    return ret
end
function set_green()
    local ret = base.Color4f:new()
    ret:SetColor(base.Colors.Green)
    return ret
end
function set_blue()
    local ret = base.Color4f:new()
    ret:SetColor(base.Colors.Blue)
    return ret
end
        )");

        base::Color4f ret = L["make_red"]();
        TEST_REQUIRE(ret == base::Color4f(base::Color::Red));

        ret = L["make_green"]();
        TEST_REQUIRE(ret == base::Color4f(base::Color::Green));

        ret = L["make_blue"]();
        TEST_REQUIRE(ret == base::Color4f(base::Color::Green));

        ret = L["set_red"]();
        TEST_REQUIRE(ret == base::Color4f(base::Color::Red));
        ret = L["set_green"]();
        TEST_REQUIRE(ret == base::Color4f(base::Color::Green));
        ret = L["set_blue"]();
        TEST_REQUIRE(ret == base::Color4f(base::Color::Blue));
    }
}

int test_main(int argc, char* argv[])
{
    unit_test_glm();
    unit_test_base();
    return 0;
}