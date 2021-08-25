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

#include "warnpush.h"
#  define SOL_ALL_SAFETIES_ON 1
#  include <sol/sol.hpp>
#include "warnpop.h"

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "base/test_help.h"
#include "base/color4f.h"
#include "game/scene.h"
#include "engine/lua.h"

// the test coverage here is quite poor but OTOH the binding code is
// for the most part very straightforward just plumbing calls from Lua
// to the C++ implementation. However there are some more complicated
// intermediate functions such as the script variable functionality that
// needs to be tested. Other key point is to test the basic functionality
// just to make sure that there are no unexpected sol2 snags/bugs.

void unit_test_util()
{
    sol::state L;
    L.open_libraries();

    game::BindUtil(L);

    L.script(R"(
function test_random_begin()
    util.RandomSeed(41231)
end
function make_random_int()
    return util.Random(0, 100)
end
)");

    L["test_random_begin"]();
    auto lua_random_int = L["make_random_int"];
    const int expected_ints[] = {
      47, 71, 5, 28, 50, 41, 57, 19, 43, 38
    };

    for (int i=0; i<10; ++i)
    {
        const int ret = lua_random_int();
        TEST_REQUIRE(ret == expected_ints[i]);
        //std::cout << ret << std::endl;
    }
}

void unit_test_glm()
{
    sol::state L;
    L.open_libraries();
    game::BindGLM(L);

    const glm::vec2 a(1.0f, 2.0f);
    const glm::vec2 b(-1.0f, -2.0f);

    L.script(
R"(
function oob(a)
    return a[3]
end

function oob_pcall()
    local v = glm.vec2:new()
    if pcall(oob, v) then
      return 'fail'
    end
    return 'ok'
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

    // oob pcall. the c++ code throws an exception
    // which should be something Lua pcall can handle.
    {
        std::string ret = L["oob_pcall"]();
        TEST_REQUIRE(ret == "ok");
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
function set_junk()
    local ret = base.Color4f:new()
    ret.SetColor(1234)
    return ret
end
function from_enum()
   local ret = base.Color4f.FromEnum(base.Colors.Green)
   return ret
end
        )");

        base::Color4f ret = L["make_red"]();
        TEST_REQUIRE(ret == base::Color4f(base::Color::Red));
        ret = L["make_green"]();
        TEST_REQUIRE(ret == base::Color4f(base::Color::Green));
        ret = L["make_blue"]();
        TEST_REQUIRE(ret == base::Color4f(base::Color::Green));

        ret = L["from_enum"]();
        TEST_REQUIRE(ret == base::Color4f(base::Color::Green));

        ret = L["set_red"]();
        TEST_REQUIRE(ret == base::Color4f(base::Color::Red));
        ret = L["set_green"]();
        TEST_REQUIRE(ret == base::Color4f(base::Color::Green));
        ret = L["set_blue"]();
        TEST_REQUIRE(ret == base::Color4f(base::Color::Blue));

        sol::function_result res = L["set_junk"]();
        TEST_REQUIRE(res.valid() == false);
    }

    // frect
    {
        sol::state L;
        L.open_libraries();
        game::BindBase(L);
        L.script(
                R"(
function test_combine()
    local a = base.FRect:new(10.0, 10.0, 20.0, 20.0)
    local b = base.FRect:new(-5.0, 3.0, 10.0, 45.0)
    return base.FRect.Combine(a, b)
end

function test_intersect()
    local a = base.FRect:new(10.0, 10.0, 20.0, 20.0)
    local b = base.FRect:new(-5.0, 3.0, 10.0, 45.0)
    return base.FRect.Intersect(a, b)
end
        )");

        base::FRect ret = L["test_combine"]();
        TEST_REQUIRE(ret == base::Union(base::FRect(10.0, 10.0, 20.0, 20.0),
                                        base::FRect(-5.0, 3.0, 10.0, 45.0)));
        ret = L["test_intersect"]();
        TEST_REQUIRE(ret == base::Intersect(base::FRect(10.0, 10.0, 20.0, 20.0),
                                            base::FRect(-5.0, 3.0, 10.0, 45.0)));
    }
}

void unit_test_scene()
{
    auto entity = std::make_shared<game::EntityClass>();
    entity->SetName("test_entity");
    {
        game::DrawableItemClass draw;
        draw.SetMaterialId("material");
        draw.SetDrawableId("drawable");
        draw.SetLayer(5);
        draw.SetLineWidth(2.0f);
        draw.SetTimeScale(3.0f);
        draw.SetFlag(game::DrawableItemClass::Flags::FlipVertically, true);
        draw.SetFlag(game::DrawableItemClass::Flags::RestartDrawable, false);

        game::RigidBodyItemClass body;
        // todo: body data

        game::EntityNodeClass node;
        node.SetName("foobar");
        node.SetSize(glm::vec2(150.0f, 200.0f));
        node.SetTranslation(glm::vec2(50.0f, 60.0f));
        node.SetDrawable(draw);
        node.SetRigidBody(body);
        entity->LinkChild(nullptr, entity->AddNode(std::move(node)));

        // add some entity script vars
        entity->AddScriptVar(game::ScriptVar("int_var", 123, false));
        entity->AddScriptVar(game::ScriptVar("float_var", 40.0f, false));
        entity->AddScriptVar(game::ScriptVar("str_var", "foobar", false));
        entity->AddScriptVar(game::ScriptVar("bool_var", false, false));
        entity->AddScriptVar(game::ScriptVar("vec2_var", glm::vec2(3.0f, -1.0f), false));
        entity->AddScriptVar(game::ScriptVar("read_only", 43, true));
    }

    game::SceneClass scene;
    {
        game::SceneNodeClass scene_node;
        scene_node.SetName("test_entity_1");
        scene_node.SetLayer(4);
        scene_node.SetEntity(entity);
        scene_node.SetTranslation(glm::vec2(30.0f, 40.0f));
        scene.LinkChild(nullptr, scene.AddNode(scene_node));
    }

    // add some scripting variable types
    {
        scene.AddScriptVar(game::ScriptVar("int_var", 123, false));
        scene.AddScriptVar(game::ScriptVar("float_var", 40.0f, false));
        scene.AddScriptVar(game::ScriptVar("str_var", "foobar", false));
        scene.AddScriptVar(game::ScriptVar("bool_var", false, false));
        scene.AddScriptVar(game::ScriptVar("vec2_var", glm::vec2(3.0f, -1.0f), false));
        scene.AddScriptVar(game::ScriptVar("read_only", 43, true));
    }

    // create instance
    game::Scene instance(scene);
    TEST_REQUIRE(instance.GetNumEntities() == 1);

    sol::state L;
    game::BindBase(L);
    game::BindGameLib(L);
    game::BindGLM(L);
    game::BindUtil(L);

    L.open_libraries();
    L.script(
            R"(
function try_set_read_only(obj)
  obj.read_only = 123
end
function try_set_wrong_type(obj)
  obj.int_var = 'string here'
end

function test(scene)
   test_int(scene.int_var,     123)
   test_float(scene.float_var, 40.0)
   test_str(scene.str_var,     'foobar')
   test_bool(scene.bool_var,    false)
   test_vec2(scene.vec2_var,    3.0, -1.0)
   test_int(scene.read_only,    43)

   -- writing read-only should raise an error
   if pcall(try_set_read_only, scene) then
     error('fail')
   end

   -- wrong type should raise an error
   if pcall(try_set_wrong_type, scene) then
     error('fail')
   end

   scene.int_var   = 55
   scene.float_var = 60.0
   scene.str_var   = 'keke'
   scene.bool_var  = true
   scene.vec2_var  = glm.vec2:new(-1.0, -2.0)
   test_int(scene.int_var,     55)
   test_float(scene.float_var, 60.0)
   test_str(scene.str_var,     'keke')
   test_bool(scene.bool_var,    true)
   test_vec2(scene.vec2_var,    -1.0, -2.0)

   test_int(scene:GetNumEntities(), 1)
   if scene:FindEntityByInstanceId('sdsdfsg') ~= nil then
     error('fail')
   end
   if scene:FindEntityByInstanceName('sdsdsd') ~= nil then
    error('fail')
   end
   local entity = scene:GetEntity(0)
   if entity == nil then
      error('fail')
   end
   if scene:FindEntityByInstanceId(entity:GetId()) == nil then
     error('fail')
   end
   if scene:FindEntityByInstanceName(entity:GetName()) == nil then
     error('fail')
   end

   test_int(entity.int_var,     123)
   test_float(entity.float_var, 40.0)
   test_str(entity.str_var,     'foobar')
   test_bool(entity.bool_var,    false)
   test_vec2(entity.vec2_var,    3.0, -1.0)
   test_int(entity.read_only,    43)

   -- writing read-only should raise an error
   if pcall(try_set_read_only, entity) then
     error('fail')
   end

   -- wrong type should raise an error
   if pcall(try_set_wrong_type, entity) then
     error('fail')
   end

   entity.int_var   = 55
   entity.float_var = 60.0
   entity.str_var   = 'keke'
   entity.bool_var  = true
   entity.vec2_var  = glm.vec2:new(-1.0, -2.0)
   test_int(entity.int_var,     55)
   test_float(entity.float_var, 60.0)
   test_str(entity.str_var,     'keke')
   test_bool(entity.bool_var,    true)
   test_vec2(entity.vec2_var,    -1.0, -2.0)

   test_str(entity:GetName(), 'test_entity_1')
   test_str(entity:GetClassName(), 'test_entity')
   test_int(entity:GetNumNodes(), 1)
   test_int(entity:GetLayer(), 4)
   test_bool(entity:IsPlaying(), false)
   test_bool(entity:HasExpired(), false)
   if entity:FindNodeByClassId('sjsjsjs') ~= nil then
     error('fail')
   end
   if entity:FindNodeByClassName('sjsjsjsj') ~= nil then
     error('fail')
   end
   if entity:FindNodeByInstanceId('123') ~= nil then
     error('fail')
   end

   local node = entity:GetNode(0)
   if entity:FindNodeByClassName(node:GetClassName()) == nil then
     error('fail')
   end
   if entity:FindNodeByClassId(node:GetClassId()) == nil then
     error('fail')
   end


   test_bool(node:HasDrawable(), true)
   if node:GetDrawable() == nil then
     error('fail')
   end
   test_bool(node:HasRigidBody(), true)
   if node:GetRigidBody() == nil then
     error('fail')
   end

end
    )");
    L["test_float"] = [](float a, float b) {
        if (!real::equals(a, b))
            throw std::runtime_error("float fail");
    };
    L["test_int"] = [](int a, int b) {
        if (a != b)
            throw std::runtime_error("int fail");
    };
    L["test_str"] = [](std::string a, std::string b) {
        if (a != b)
            throw std::runtime_error("string fail");
    };
    L["test_bool"] = [](bool a, bool b) {
        if (a != b)
            throw std::runtime_error("boolean fail");
    };
    L["test_vec2"] = [](const glm::vec2& vec, float x, float y)  {
        if (vec != glm::vec2(x, y))
            throw std::runtime_error("vec2 fail");
    };

    sol::function_result ret = L["test"](&instance);
    TEST_REQUIRE(ret.valid());
}


int test_main(int argc, char* argv[])
{
    unit_test_util();
    unit_test_glm();
    unit_test_base();
    unit_test_scene();
    return 0;
}
