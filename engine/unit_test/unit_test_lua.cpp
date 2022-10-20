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
#include "base/logging.h"
#include "data/json.h"
#include "game/scene.h"
#include "engine/lua.h"
#include "engine/loader.h"

// the test coverage here is quite poor but OTOH the binding code is
// for the most part very straightforward just plumbing calls from Lua
// to the C++ implementation. However, there are some more complicated
// intermediate functions such as the script variable functionality that
// needs to be tested. Other key point is to test the basic functionality
// just to make sure that there are no unexpected sol2 snags/bugs.

class TestData : public engine::EngineData {
public:
    TestData(const std::string& file) : mName(file)
    { mData = base::LoadBinaryFile(file); }
    virtual const void* GetData() const override
    { return &mData[0]; }
    virtual std::size_t GetSize() const override
    { return mData.size(); }
    virtual std::string GetName() const override
    { return mName; }
private:
    const std::string mName;
    std::vector<char> mData;
};

class TestLoader : public engine::Loader {
public:
    EngineDataHandle LoadEngineDataUri(const std::string& uri) const override
    { return nullptr; }
    EngineDataHandle LoadEngineDataFile(const std::string& filename) const override
    {
        if (base::StartsWith(filename, "this-file-doesnt-exist"))
            return nullptr;
        return std::make_shared<TestData>(filename);
    }
    EngineDataHandle LoadEngineDataId(const std::string& id) const override
    {
        if (base::StartsWith(id, "this-file-doesnt-exist"))
            return nullptr;
        if (base::EndsWith(id, ".lua"))
            return std::make_shared<TestData>(id);
        return std::make_shared<TestData>(id + ".lua");
    }
};

void unit_test_util()
{
    sol::state L;
    L.open_libraries();

    engine::BindUtil(L);

    L.script(R"(
function test_random_begin()
    util.RandomSeed(41231)
end
function make_random_int()
    return util.Random(0, 100)
end
function test_format_none()
    return util.FormatString('huhu')
end
function test_format_one(var)
   return util.FormatString('huhu %1', var)
end
function test_format_many()
   return util.FormatString('%1%2 %3', 'foo', 'bar', 123)
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

    {
        auto test_format_none = L["test_format_none"];
        std::string ret = test_format_none();
        TEST_REQUIRE(ret == "huhu");
    }

    {
        auto test_format_one = L["test_format_one"];
        std::string ret = test_format_one("string");
        TEST_REQUIRE(ret == "huhu string");
        ret = test_format_one(123);
        TEST_REQUIRE(ret == "huhu 123");
        ret = test_format_one(1.0f); // output format is locale specific
        //TEST_REQUIRE(ret == "huhu 1.0");
    }

    {
        auto test_format_many = L["test_format_many"];
        std::string ret = test_format_many();
        TEST_REQUIRE(ret == "foobar 123");
    }

}

void unit_test_glm()
{
    sol::state L;
    L.open_libraries();
    engine::BindGLM(L);

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
function multiply_2(scalar, vector)
   return scalar * vector
end
function multiply_3(vector_a, vector_b)
   return vector_a * vector_b
end
function divide(vector, scalar)
   return vector / scalar
end
function divide_2(scalar, vector)
   return scalar / vector
end
function divide_3(vector_a, vector_b)
   return vector_a / vector_b
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
    // multiply
    {
        glm::vec2 ret = L["multiply_2"](2.0f, a);
        TEST_REQUIRE(real::equals(ret.x, 2.0f * a.x));
        TEST_REQUIRE(real::equals(ret.y, 2.0f * a.y));
    }
    {
        glm::vec2 ret = L["multiply_3"](a, b);
        TEST_REQUIRE(real::equals(ret.x, a.x * b.x));
        TEST_REQUIRE(real::equals(ret.y, a.y * b.y));
    }

    // divide
    {
        glm::vec2 ret = L["divide"](a, 2.0f);
        TEST_REQUIRE(real::equals(ret.x, a.x / 2.0f));
        TEST_REQUIRE(real::equals(ret.y, a.y / 2.0f));
    }
    {
        glm::vec2 ret = L["divide_2"](3.0f, a);
        TEST_REQUIRE(real::equals(ret.x, 1.0f/a.x * 3.0f));
        TEST_REQUIRE(real::equals(ret.y, 1.0f/a.y * 3.0f));
    }

    {
        glm::vec2 ret = L["divide_3"](a, b);
        TEST_REQUIRE(real::equals(ret.x, a.x / b.x));
        TEST_REQUIRE(real::equals(ret.y, a.y / b.y));
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
        engine::BindBase(L);

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
    ret:SetColor(1234)
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
        engine::BindBase(L);
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

    // test math extension
    {
        sol::state L;
        L.open_libraries();
        engine::BindBase(L);

        L.script(
R"(
function test_builtin()
    return math.floor(1.5)
end
function test_ours()
    return base.wrap(1, 3, 2)
end
)");
        float floor = L["test_builtin"]();
        float wrap = L["test_ours"]();
        TEST_REQUIRE(floor == 1.0);
        TEST_REQUIRE(wrap == 2.0);
    }
}

void unit_test_data()
{
    sol::state L;
    L.open_libraries();
    engine::BindBase(L);
    engine::BindData(L);
    engine::BindGLM(L);

    // JSON writer
    {
        L.script(
                R"(
function write_json()
   local json = data.JsonObject:new()
   json:Write('float', 1.0)
   json:Write('int', 123)
   json:Write('str', 'hello world')
   json:Write('vec2', glm.vec2:new(1.0, 2.0))
   json:Write('vec3', glm.vec3:new(1.0, 2.0, 3.0))
   json:Write('vec4', glm.vec4:new(1.0, 2.0, 3.0, 4.0))
   local banana = json:NewWriteChunk('banana')
   banana:Write('name', 'banana')
   local apple = json:NewWriteChunk('apple')
   apple:Write('name', 'apple')
   json:AppendChunk('fruits', banana)
   json:AppendChunk('fruits', apple)
   return json:ToString()
end
        )");

        const std::string& json = L["write_json"]();
        std::cout << json;

        L.script(
            R"(
function read_chunk_oob(json)
   local chunk = json:GetReadChunk('fruits', 2)
end

function read_json(json_string)
    local json = data.JsonObject:new()
    local ok, error = json:ParseString('asgasgasgas')
    if ok then
       return 'parse string fail'
    end
    ok, error = json:ParseString('{ "float": 1.0 ')
    if ok then
       return 'parse string fail'
    end

    ok, error = json:ParseString(json_string)
    if not ok then
      return 'parse string fail'
    end

    local success, val = json:ReadString('doesnt_exist')
    if  success then
        return 'fail read string'
    end
    success, val = json:ReadString('str')
    if not success then
        return 'fail read string'
    end

    _, val = json:ReadFloat('float')
    if val ~= 1.0 then
       return 'fail'
    end
    _, val = json:ReadInt('int')
    if val ~= 123 then
       return 'fail'
    end
    _, val = json:ReadString('str')
    if val ~= 'hello world' then
       return 'fail'
    end
    _, val = json:ReadVec2('vec2')
    if val.x ~= 1.0 or val.y ~= 2.0 then
       return 'fail'
    end
    _, val = json:ReadVec3('vec3')
    if val.x ~= 1.0 or val.y ~= 2.0 or val.z ~= 3.0 then
       return 'fail'
    end
    _, val = json:ReadVec4('vec4')
    if val.x ~= 1.0 or val.y ~= 2.0 or val.z ~= 3.0 or val.w ~= 4.0 then
       return 'fail'
    end
    local num_chunks = json:GetNumChunks('fruits')
    if num_chunks ~= 2 then
        return 'fail'
    end
    local chunk = json:GetReadChunk('fruits', 0)
    _, val = chunk:ReadString('name')
    if val ~= 'banana' then
        return  'fail'
    end
    chunk = json:GetReadChunk('fruits', 1)
    _, val = chunk:ReadString('name')
    if val ~= 'apple' then
        return 'fail'
    end

    -- out of bounds on chunk index test
    if pcall(read_chunk_oob, json) then
        return 'fail on chunk out of bounds'
    end
    return 'ok'
end
)");
        const std::string& ret = L["read_json"](json);
        std::cout << ret;
        TEST_REQUIRE(ret == "ok");
    }
}

void unit_test_scene_interface()
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
        draw.SetFlag(game::DrawableItemClass::Flags::FlipHorizontally, true);
        draw.SetFlag(game::DrawableItemClass::Flags::RestartDrawable, false);

        game::RigidBodyItemClass body;
        // todo: body data

        game::EntityNodeClass node;
        node.SetName("foobar");
        node.SetSize(glm::vec2(150.0f, 200.0f));
        node.SetTranslation(glm::vec2(50.0f, 60.0f));
        node.SetDrawable(draw);
        node.SetRigidBody(body);
        entity->LinkChild(nullptr, entity->AddNode(node));

        // add some entity script vars
        entity->AddScriptVar(game::ScriptVar("int_var", 123, false));
        entity->AddScriptVar(game::ScriptVar("float_var", 40.0f, false));
        entity->AddScriptVar(game::ScriptVar("str_var", std::string("foobar"), false));
        entity->AddScriptVar(game::ScriptVar("bool_var", false, false));
        entity->AddScriptVar(game::ScriptVar("vec2_var", glm::vec2(3.0f, -1.0f), false));
        entity->AddScriptVar(game::ScriptVar("read_only", 43, true));

        // array
        std::vector<std::string> strs;
        strs.push_back("foo");
        strs.push_back("bar");
        entity->AddScriptVar(game::ScriptVar("str_array", strs, false));

        // node reference
        game::ScriptVar::EntityNodeReference  ref;
        ref.id = node.GetId();
        entity->AddScriptVar(game::ScriptVar("entity_node_var", ref, false));

        // node reference array
        std::vector<game::ScriptVar::EntityNodeReference> refs;
        refs.resize(1);
        refs[0].id = node.GetId();
        entity->AddScriptVar(game::ScriptVar("entity_node_var_arr", refs, false));
    }

    game::SceneClass scene;
    {
        game::SceneNodeClass scene_node;
        scene_node.SetName("test_entity_1");
        scene_node.SetLayer(4);
        scene_node.SetEntity(entity);
        scene_node.SetTranslation(glm::vec2(30.0f, 40.0f));
        scene.LinkChild(nullptr, scene.AddNode(scene_node));

        // add a reference to the entity
        game::ScriptVar::EntityReference  ref;
        ref.id = scene_node.GetId();
        scene.AddScriptVar(game::ScriptVar("entity_var", ref, false));

        // add array reference to the entity
        std::vector<game::ScriptVar::EntityReference> refs;
        refs.resize(1);
        refs[0].id = scene_node.GetId();
        scene.AddScriptVar(game::ScriptVar("entity_var_arr", refs, false));

    }

    // add some scripting variable types
    {
        scene.AddScriptVar(game::ScriptVar("int_var", 123, false));
        scene.AddScriptVar(game::ScriptVar("float_var", 40.0f, false));
        scene.AddScriptVar(game::ScriptVar("str_var", std::string("foobar"), false));
        scene.AddScriptVar(game::ScriptVar("bool_var", false, false));
        scene.AddScriptVar(game::ScriptVar("vec2_var", glm::vec2(3.0f, -1.0f), false));
        scene.AddScriptVar(game::ScriptVar("read_only", 43, true));

        // array
        std::vector<std::string> strs;
        strs.push_back("foo");
        strs.push_back("bar");
        scene.AddScriptVar(game::ScriptVar("str_array", strs, false));
    }

    // create instance
    game::Scene instance(scene);
    TEST_REQUIRE(instance.GetNumEntities() == 1);

    sol::state L;
    engine::BindBase(L);
    engine::BindGameLib(L);
    engine::BindGLM(L);
    engine::BindUtil(L);

    L.open_libraries();
    L.script(
            R"(
function try_set_read_only(obj)
  obj.read_only = 123
end
function try_set_wrong_type(obj)
  obj.int_var = 'string here'
end

function try_array_oob(obj)
   local foo = obj.str_array[4]
end

function test(scene)
   test_int(scene.int_var,     123)
   test_float(scene.float_var, 40.0)
   test_str(scene.str_var,     'foobar')
   test_bool(scene.bool_var,    false)
   test_vec2(scene.vec2_var,    3.0, -1.0)
   test_int(scene.read_only,    43)

   test_str(scene.str_array[1], 'foo')
   test_str(scene.str_array[2], 'bar')
   -- going out of bounds on array should raise an error
   if pcall(try_array_oob, scene) then
     error('fail testing array oob access')
   end

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

   print(tostring(scene.entity_var))
   if scene.entity_var:GetName() ~= 'test_entity_1' then
       error('entity variable not set')
   end
   -- test assigning to the scene entity reference variable
   scene.entity_var = scene:GetEntity(0)
   scene.entity_var = nil

   if scene.entity_var_arr[1]:GetName() ~= 'test_entity_1' then
      error('entity variable array not set properly.')
   end

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

   -- test reading the node reference
   print(tostring(entity_node_var))
   if entity.entity_node_var:GetName() ~= 'foobar' then
      error('entity node entity variable reference is not resolved properly.')
   end
   -- test assigning to the node reference var
   entity.entity_node_var = entity:GetNode(0)
   -- test assigning nil
   entity.entity_node_var = nil

   if entity.entity_node_var_arr[1]:GetName() ~= 'foobar' then
       error('entity node entity variable reference is not resolve properly.')
   end


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

   test_str(entity.str_array[1], 'foo')
   test_str(entity.str_array[2], 'bar')

   test_str(entity:GetName(), 'test_entity_1')
   test_str(entity:GetClassName(), 'test_entity')
   test_int(entity:GetNumNodes(), 1)
   test_int(entity:GetLayer(), 4)
   test_bool(entity:IsAnimating(), false)
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
        std::printf("test_str a='%s', b='%s'\n", a.c_str(), b.c_str());
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

void unit_test_entity_begin_end_play()
{
    base::OverwriteTextFile("entity_begin_end_play_test.lua", R"(
function BeginPlay(entity, scene)
   local event = game.GameEvent:new()
   event.from  = entity:GetName()
   event.message = 'begin'
   Game:PostEvent(event)
end
function EndPlay(entity, scene)
   local event = game.GameEvent:new()
   event.from = entity:GetName()
   event.message = 'end'
   Game:PostEvent(event)
end
)");

    auto entity = std::make_shared<game::EntityClass>();
    entity->SetName("entity");
    entity->SetSriptFileId("entity_begin_end_play_test");

    game::SceneClass scene_class;
    {
        game::SceneNodeClass node;
        node.SetName("entity");
        node.SetEntity(entity);
        scene_class.LinkChild(nullptr, scene_class.AddNode(node));
    }
    game::Scene scene(scene_class);

    TestLoader loader;

    engine::LuaRuntime script(".", "", "", "");
    script.SetDataLoader(&loader);
    script.Init();
    script.BeginPlay(&scene);

    // begin play should invoke BeginPlay on the entities that are
    // statically in the scene class.
    engine::Action action;
    TEST_REQUIRE(script.GetNextAction(&action));
    const auto* event = std::get_if<engine::PostEventAction>(&action);
    TEST_REQUIRE(event);
    TEST_REQUIRE(std::get<std::string>(event->event.from) == "entity");
    TEST_REQUIRE(event->event.message == "begin");
    TEST_REQUIRE(script.HasAction() == false);

    scene.BeginLoop();
        script.BeginLoop();
        scene.KillEntity(scene.FindEntityByInstanceName("entity"));
        script.EndLoop();
    scene.EndLoop();

    scene.BeginLoop();
        script.BeginLoop();
        script.EndLoop();
    scene.EndLoop();

    TEST_REQUIRE(script.GetNextAction(&action));
    event = std::get_if<engine::PostEventAction>(&action);
    TEST_REQUIRE(event);
    TEST_REQUIRE(std::get<std::string>(event->event.from) == "entity");
    TEST_REQUIRE(event->event.message == "end");
    TEST_REQUIRE(script.HasAction() == false);

    scene.BeginLoop();
        script.BeginLoop();
        game::EntityArgs args;
        args.name = "spawned";
        args.klass = entity;
        scene.SpawnEntity(args);
        script.EndLoop();
    scene.EndLoop();

    scene.BeginLoop();
        script.BeginLoop();

        script.EndLoop();
    scene.EndLoop();

    TEST_REQUIRE(script.GetNextAction(&action));
    event = std::get_if<engine::PostEventAction>(&action);
    TEST_REQUIRE(event);
    TEST_REQUIRE(std::get<std::string>(event->event.from) == "spawned");
    TEST_REQUIRE(event->event.message == "begin");
    TEST_REQUIRE(script.HasAction() == false);
}

void unit_test_entity_tick_update()
{
    base::OverwriteTextFile("entity_tick_update_test.lua", R"(
function Tick(entity, game_time, dt)
   local event   = game.GameEvent:new()
   event.message = 'tick'
   event.from    = entity:GetName()
   Game:PostEvent(event)
end
function Update(entity, game_time, dt)
   local event   = game.GameEvent:new()
   event.message = 'update'
   event.from    = entity:GetName()
   Game:PostEvent(event)
end
)");
    auto foo = std::make_shared<game::EntityClass>();
    foo->SetName("foo");
    foo->SetSriptFileId("entity_tick_update_test");
    foo->SetFlag(game::EntityClass::Flags::TickEntity, true);
    foo->SetFlag(game::EntityClass::Flags::UpdateEntity, false);

    auto bar = std::make_shared<game::EntityClass>();
    bar->SetName("bar");
    bar->SetSriptFileId("entity_tick_update_test");
    bar->SetFlag(game::EntityClass::Flags::TickEntity, false);
    bar->SetFlag(game::EntityClass::Flags::UpdateEntity, true);

    game::SceneClass scene_class;
    {
        game::SceneNodeClass node;
        node.SetName("foo");
        node.SetEntity(foo);
        scene_class.LinkChild(nullptr, scene_class.AddNode(node));
    }
    {
        game::SceneNodeClass node;
        node.SetName("bar");
        node.SetEntity(bar);
        scene_class.LinkChild(nullptr, scene_class.AddNode(node));
    }

    TestLoader loader;

    game::Scene scene(scene_class);
    engine::LuaRuntime script(".", "", "", "");
    script.SetDataLoader(&loader);
    script.Init();
    script.BeginPlay(&scene);

    script.BeginLoop();
    script.Tick(0.0, 0.0);

    engine::Action action;
    TEST_REQUIRE(script.GetNextAction(&action));
    const auto* event = std::get_if<engine::PostEventAction>(&action);
    TEST_REQUIRE(event);
    TEST_REQUIRE(std::get<std::string>(event->event.from) == "foo");
    TEST_REQUIRE(event->event.message == "tick");
    TEST_REQUIRE(script.HasAction() == false);

    script.Update(0.0, 0.0);

    TEST_REQUIRE(script.GetNextAction(&action));
    event = std::get_if<engine::PostEventAction>(&action);
    TEST_REQUIRE(event);
    TEST_REQUIRE(std::get<std::string>(event->event.from) == "bar");
    TEST_REQUIRE(event->event.message == "update");
    TEST_REQUIRE(script.HasAction() == false);


    script.EndLoop();
}

void unit_test_entity_private_environment()
{
    // test that each entity type has their own private
    // lua environment without stomping on each others data!

    base::OverwriteTextFile("entity_env_foo_test.lua", R"(
local foobar = 123
function Tick(entity, game_time, dt)
   local event   = game.GameEvent:new()
   event.message = 'foo'
   event.value   = foobar
   Game:PostEvent(event)
end
)");
    base::OverwriteTextFile("entity_env_bar_test.lua", R"(
local foobar = 321
function Tick(entity, game_time, dt)
   local event   = game.GameEvent:new()
   event.message = 'bar'
   event.value   = foobar
   Game:PostEvent(event)
end
)");
    auto foo = std::make_shared<game::EntityClass>();
    foo->SetName("foo");
    foo->SetSriptFileId("entity_env_foo_test");
    foo->SetFlag(game::EntityClass::Flags::TickEntity, true);

    auto bar = std::make_shared<game::EntityClass>();
    bar->SetName("bar");
    bar->SetSriptFileId("entity_env_bar_test");
    bar->SetFlag(game::EntityClass::Flags::TickEntity, true);

    game::SceneClass scene_class;
    {
        game::SceneNodeClass node;
        node.SetName("foo");
        node.SetEntity(foo);
        scene_class.LinkChild(nullptr, scene_class.AddNode(node));
    }
    {
        game::SceneNodeClass node;
        node.SetName("bar");
        node.SetEntity(bar);
        scene_class.LinkChild(nullptr, scene_class.AddNode(node));
    }

    game::Scene instance(scene_class);

    TestLoader loader;

    engine::LuaRuntime script(".", "", "", "");
    script.SetDataLoader(&loader);
    script.Init();
    script.BeginPlay(&instance);
    script.Tick(0.0, 0.0);

    engine::Action action1;
    engine::Action action2;
    TEST_REQUIRE(script.GetNextAction(&action1));
    TEST_REQUIRE(script.GetNextAction(&action2));

    auto* e1 = std::get_if<engine::PostEventAction>(&action1);
    auto* e2 = std::get_if<engine::PostEventAction>(&action2);
    TEST_REQUIRE(e1 && e2);
    TEST_REQUIRE(std::get<int>(e1->event.values["value"]) == 123);
    TEST_REQUIRE(std::get<int>(e2->event.values["value"]) == 321);

}

void unit_test_entity_cross_env_call()
{
    // call an entity method inside one entity environment
    // from another entity environment.
    base::OverwriteTextFile("entity_env_foo_test.lua", R"(
function TestFunction(entity, int_val, str_val, flt_val)
   local event   = game.GameEvent:new()

   event.value   = int_val
   Game:PostEvent(event)

   event.value = str_val
   Game:PostEvent(event)

   event.value = flt_val
   Game:PostEvent(event)

   return glm.vec2:new(45.0, 80.0)
end
)");
    base::OverwriteTextFile("entity_env_bar_test.lua", R"(
function Tick(entity, game_time, dt)
    local scene = entity:GetScene()
    local other = scene:FindEntityByInstanceName('foo')
    if other == nil then
       error('nil object')
    end

    local vec = CallMethod(other, 'TestFunction', 123, 'huhu', 123.5)
    local event = game.GameEvent:new()
    event.value = vec
    Game:PostEvent(event)
end
)");

    auto foo = std::make_shared<game::EntityClass>();
    foo->SetName("foo");
    foo->SetSriptFileId("entity_env_foo_test");
    foo->SetFlag(game::EntityClass::Flags::TickEntity, true);

    auto bar = std::make_shared<game::EntityClass>();
    bar->SetName("bar");
    bar->SetSriptFileId("entity_env_bar_test");
    bar->SetFlag(game::EntityClass::Flags::TickEntity, true);

    game::SceneClass scene_class;
    {
        game::SceneNodeClass node;
        node.SetName("foo");
        node.SetEntity(foo);
        scene_class.LinkChild(nullptr, scene_class.AddNode(node));
    }
    {
        game::SceneNodeClass node;
        node.SetName("bar");
        node.SetEntity(bar);
        scene_class.LinkChild(nullptr, scene_class.AddNode(node));
    }

    game::Scene instance(scene_class);

    TestLoader loader;

    engine::LuaRuntime script(".", "", "", "");
    script.SetDataLoader(&loader);
    script.Init();
    script.BeginPlay(&instance);
    script.Tick(0.0, 0.0);

    engine::Action action1;
    engine::Action action2;
    engine::Action action3;
    engine::Action action4;
    TEST_REQUIRE(script.GetNextAction(&action1));
    TEST_REQUIRE(script.GetNextAction(&action2));
    TEST_REQUIRE(script.GetNextAction(&action3));
    TEST_REQUIRE(script.GetNextAction(&action4));

    auto* e1 = std::get_if<engine::PostEventAction>(&action1);
    auto* e2 = std::get_if<engine::PostEventAction>(&action2);
    auto* e3 = std::get_if<engine::PostEventAction>(&action3);
    auto* e4 = std::get_if<engine::PostEventAction>(&action4);
    TEST_REQUIRE(e1 && e2 && e3 && e4);
    TEST_REQUIRE(std::get<int>(e1->event.values["value"]) == 123);
    TEST_REQUIRE(std::get<std::string>(e2->event.values["value"]) == "huhu");
    TEST_REQUIRE(std::get<float>(e3->event.values["value"]) == real::float32(123.5f));
    TEST_REQUIRE(std::get<glm::vec2>(e4->event.values["value"]) == glm::vec2(45.0f, 80.0f));
}

void unit_test_entity_shared_globals()
{
    base::OverwriteTextFile("entity_shared_global_test_foo.lua", R"(
function Tick(entity, game_time, dt)
    _G['foobar'] = 123
end
)");
    base::OverwriteTextFile("entity_shared_global_test_bar.lua", R"(
function Tick(entity, game_time, dt)
   local event = game.GameEvent:new()
   event.message = 'bar'
   event.value = _G['foobar']
   Game:PostEvent(event)
end
)");
    auto foo = std::make_shared<game::EntityClass>();
    foo->SetName("foo");
    foo->SetSriptFileId("entity_shared_global_test_foo");
    foo->SetFlag(game::EntityClass::Flags::TickEntity, true);

    auto bar = std::make_shared<game::EntityClass>();
    bar->SetName("bar");
    bar->SetSriptFileId("entity_shared_global_test_bar");
    bar->SetFlag(game::EntityClass::Flags::TickEntity, true);

    game::SceneClass scene_class;
    {
        game::SceneNodeClass node;
        node.SetName("foo");
        node.SetEntity(foo);
        scene_class.LinkChild(nullptr, scene_class.AddNode(node));
    }
    {
        game::SceneNodeClass node;
        node.SetName("bar");
        node.SetEntity(bar);
        scene_class.LinkChild(nullptr, scene_class.AddNode(node));
    }

    game::Scene instance(scene_class);

    TestLoader loader;

    engine::LuaRuntime script(".", "", "", "");
    script.SetDataLoader(&loader);
    script.Init();
    script.BeginPlay(&instance);
    script.Tick(0.0, 0.0);

    engine::Action action;
    TEST_REQUIRE(script.GetNextAction(&action));
    auto* event = std::get_if<engine::PostEventAction>(&action);
    TEST_REQUIRE(event->event.message == "bar");
    TEST_REQUIRE(std::get<int>(event->event.values["value"]) == 123);
}

void unit_test_game_main_script_load_success()
{
    {
        base::OverwriteTextFile("game_main_script_test.lua", R"(
function LoadGame()
  local event = game.GameEvent:new()
  event.message = 'load'
  Game:PostEvent(event)
end
    )");

        TestLoader loader;

        engine::LuaRuntime game("", "game_main_script_test.lua", "", "");
        game.SetDataLoader(&loader);
        game.Init();
        TEST_REQUIRE(game.LoadGame());

        engine::Action action;
        TEST_REQUIRE(game.GetNextAction(&action));
        const auto* event = std::get_if<engine::PostEventAction>(&action);
        TEST_REQUIRE(event->event.message == "load");
    }

    // lua script requires another script
    {
        base::OverwriteTextFile("foobar.lua", R"(
function SendMessage()
   local event = game.GameEvent:new()
   event.message = 'load'
   Game:PostEvent(event)
end
function Foobar()
   SendMessage()
end

)");
        base::OverwriteTextFile("game_main_script_test.lua", R"(
require('foobar')
function LoadGame()
  Foobar()
end
    )");
        TestLoader loader;

        engine::LuaRuntime game("", "game_main_script_test.lua", "", "");
        game.SetDataLoader(&loader);
        game.Init();
        TEST_REQUIRE(game.LoadGame());

        engine::Action action;
        TEST_REQUIRE(game.GetNextAction(&action));
        const auto* event = std::get_if<engine::PostEventAction>(&action);
        TEST_REQUIRE(event->event.message == "load");

    }
}


void unit_test_game_main_script_load_failure()
{
    base::OverwriteTextFile("broken.lua", R"(
function Broken()
endkasd
)");

    // no such file.
    {
        TestLoader loader;

        engine::LuaRuntime game("", "this-file-doesnt-exist.lua", "", "");
        game.SetDataLoader(&loader);
        TEST_EXCEPTION(game.Init());
    }

    // broken Lua code in file.
    {
        TestLoader loader;
        engine::LuaRuntime game("", "broken.lua", "", "");
        game.SetDataLoader(&loader);
        TEST_EXCEPTION(game.Init());
    }

    // requires broken lua
    {
        base::OverwriteTextFile("game_main_script_test.lua", R"(
require('broken')
function LoadGame()
  Foobar()
end
    )");
        TestLoader loader;
        engine::LuaRuntime game("", "game_main_script_test.lua", "", "");
        game.SetDataLoader(&loader);
        TEST_EXCEPTION(game.Init());

    }

    // requires lua module that doesn't exist
    {
        base::OverwriteTextFile("game_main_script_test.lua", R"(
require('this-doesnt-exist')
function LoadGame()
  Foobar()
end
    )");
        TestLoader loader;
        engine::LuaRuntime game("", "game_main_script_test.lua", "", "");
        game.SetDataLoader(&loader);
        TEST_EXCEPTION(game.Init());
    }
}

int test_main(int argc, char* argv[])
{
    base::OStreamLogger log(std::cout);
    base::SetGlobalLog(&log);
    base::EnableDebugLog(true);

    unit_test_util();
    unit_test_glm();
    unit_test_base();
    unit_test_data();
    unit_test_scene_interface();
    unit_test_entity_begin_end_play();
    unit_test_entity_tick_update();
    unit_test_entity_private_environment();
    unit_test_entity_cross_env_call();
    unit_test_entity_shared_globals();
    unit_test_game_main_script_load_success();
    unit_test_game_main_script_load_failure();
    return 0;
}
