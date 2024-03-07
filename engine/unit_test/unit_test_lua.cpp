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
#include "engine/lua_game_runtime.h"
#include "engine/lua.h"
#include "engine/loader.h"
#include "engine/event.h"
#include "engine/state.h"

namespace {

bool Eq(const engine::GameEventValue& lhs, const engine::GameEventValue& rhs)
{
    return lhs == rhs;
}
template<typename T>
bool Eq(const engine::GameEventValue& lhs, const T& rhs)
{
    if (const auto* p = std::get_if<T>(&lhs))
        return *p == rhs;
    return false;
}
bool Eq(const engine::GameEventValue& lhs, const char* str)
{
    return Eq(lhs, std::string(str));
}


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
    virtual std::size_t GetByteSize() const override
    { return mData.size(); }
    virtual std::string GetSourceName() const override
    { return mName; }
private:
    const std::string mName;
    std::vector<char> mData;
};

class TestLoader : public engine::Loader {
public:
    EngineDataHandle LoadEngineDataUri(const std::string& uri) const override
    {
        if (base::Contains(uri, "this-file-doesnt exist"))
            return nullptr;

        if (base::StartsWith(uri, "fs://"))
            return std::make_shared<TestData>(uri.substr(5));
        return nullptr;
    }
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
    TEST_CASE(test::Type::Feature)

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
    TEST_CASE(test::Type::Feature)

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
    TEST_CASE(test::Type::Feature)

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
    TEST_CASE(test::Type::Feature)

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
       return 'fail read float'
    end
    _, val = json:ReadInt('int')
    if val ~= 123 then
       return 'fail read int'
    end
    _, val = json:ReadString('str')
    if val ~= 'hello world' then
       return 'fail read string'
    end
    _, val = json:ReadVec2('vec2')
    if val.x ~= 1.0 or val.y ~= 2.0 then
       return 'fail read vec2'
    end
    _, val = json:ReadVec3('vec3')
    if val.x ~= 1.0 or val.y ~= 2.0 or val.z ~= 3.0 then
       return 'fail read vec3'
    end
    _, val = json:ReadVec4('vec4')
    if val.x ~= 1.0 or val.y ~= 2.0 or val.z ~= 3.0 or val.w ~= 4.0 then
       return 'fail read vec4'
    end
    local num_chunks = json:GetNumChunks('fruits')
    if num_chunks ~= 2 then
        return 'fail get num chunks'
    end
    local chunk = json:GetReadChunk('fruits', 0)
    _, val = chunk:ReadString('name')
    if val ~= 'banana' then
        return  'fail read chunk string'
    end
    chunk = json:GetReadChunk('fruits', 1)
    _, val = chunk:ReadString('name')
    if val ~= 'apple' then
        return 'fail read chunk string'
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

void unit_test_game()
{
    TEST_CASE(test::Type::Feature)

    sol::state L;
    L.open_libraries();
    engine::BindBase(L);
    engine::BindGameLib(L);
    engine::BindGLM(L);
    engine::BindUtil(L);

    L["test_bool"] = [](bool a, bool b) {
        if (a != b)
            throw std::runtime_error("boolean fail");
    };
    L["test_int"] = [](int a, int b) {
        std::printf("test_int a=%d, b=%d\n", a, b);
        if (a != b)
            throw std::runtime_error("int fail");
    };
    L["test_float"] = [](float a, float b) {
        std::printf("test_float a=%f, b=%f\n", a, b);
        if (!real::equals(a, b))
            throw std::runtime_error("float fail");
    };

    // game event type.
    {
        L.script(R"(
function test(event)
   event.from    = 'test'
   event.to      = 'test too'
   event.message = 'message'
   event.some_int    = 123
   event.some_float  = 123.0
   event.some_bool   = true
   event.some_str    = 'foobar'
   event.some_vec2   = glm.vec2:new(1.0, 2.0)
   event.some_vec3   = glm.vec3:new(1.0, 2.0, 3.0)
   event.some_vec4   = glm.vec4:new(1.0, 2.0, 3.0, 4.0)
end
)");
        engine::GameEvent event;
        sol::function_result ret = L["test"](&event);
        TEST_REQUIRE(ret.valid());
        TEST_REQUIRE(event.from    == std::string("test"));
        TEST_REQUIRE(event.to      == std::string("test too"));
        TEST_REQUIRE(event.message == std::string("message"));
        TEST_REQUIRE(event.values["some_int"]   == 123);
        TEST_REQUIRE(event.values["some_float"] == 123.0f);
        TEST_REQUIRE(event.values["some_bool"]  == true);
        TEST_REQUIRE(event.values["some_str"]   == std::string("foobar"));
        TEST_REQUIRE(event.values["some_vec2"]  == glm::vec2(1.0, 2.0));
        TEST_REQUIRE(event.values["some_vec3"]  == glm::vec3(1.0, 2.0, 3.0));
        TEST_REQUIRE(event.values["some_vec4"]  == glm::vec4(1.0, 2.0, 3.0, 4.0));
    }

    {
        auto entity = std::make_shared<game::EntityClass>();
        entity->SetName("entity");

        auto scene = std::make_shared<game::SceneClass>();
        scene->SetName("scene");

        game::Entity e(entity);
        game::Scene s(scene);

        L.script(R"(
function test(event, object)
   event.from   = object
   event.to     = object
   event.value  = object
end
    )");
        {
            engine::GameEvent event;
            sol::function_result ret = L["test"](&event, &e);
            TEST_REQUIRE(ret.valid());
            TEST_REQUIRE(event.from == &e);
            TEST_REQUIRE(event.to == &e);
            TEST_REQUIRE(event.values["value"] == &e);
        }

        {
            engine::GameEvent event;
            sol::function_result ret = L["test"](&event, &s);
            TEST_REQUIRE(ret.valid());
            TEST_REQUIRE(event.from == &s);
            TEST_REQUIRE(event.to == &s);
            TEST_REQUIRE(event.values["value"] == &s);
        }
    }

    // key-value store
    {
        engine::KeyValueStore store;
        data::JsonObject json;
        L.script(R"(
function test(store, json)
  --doesn't exist
  test_bool(store:HasValue('int'), false)

  -- get and initialize to 123
  test_int(store:GetValue('int', 123), 123)
  test_float(store:GetValue('float', 123.0), 123.0)

  -- should get the value initialized before
  test_int(store:GetValue('int', 666), 123)
  test_float(store:GetValue('float', 666.0), 123.0)

  store:DelValue('int')
  store:DelValue('float')
  test_bool(store:HasValue('int'), false)
  test_bool(store:HasValue('float'), false)

  test_bool(store.int == nil, true)
  test_bool(store.float == nil, true)

  store:InitValue('int', 123)
  store:InitValue('float', 123.0)
  test_bool(store:HasValue('int'), true)
  test_bool(store:HasValue('float'), true)

  store['bool']   = true
  store['int']    = 123
  store['float']  = 123.0
  store['string'] = 'foobar'
  store['vec2']   = glm.vec2:new(1.0, 2.0)
  store['vec3']   = glm.vec3:new(1.0, 2.0, 3.0)
  store['vec4']   = glm.vec4:new(1.0, 2.0, 3.0, 4.0)
  store['point']  = base.FPoint:new(1.0, 2.0)
  store['rect']   = base.FRect:new(1.0, 2.0, 3.0, 4.0)
  store['size']   = base.FSize:new(1.0, 2.0)
  store['color']  = base.Color4f:new(0.1, 0.2, 0.3, 0.4)

  test_bool(store:HasValue('bool'), true)
  test_bool(store:HasValue('int'), true)
  test_bool(store:HasValue('float'), true)
  test_bool(store:HasValue('keke'), false)

  -- do reads in Lua from the store and save into _G
  the_bool   = store.bool
  the_int    = store.int
  the_float  = store.float
  the_string = store.string
  the_vec2   = store.vec2
  the_vec3   = store.vec3
  the_vec4   = store.vec4
  the_point  = store.point
  the_rect   = store.rect
  the_size   = store.size
  the_color  = store.color

  store:Persist(json)
end
)");

        sol::function_result ret = L["test"](&store, &json);
        TEST_REQUIRE(ret.valid());
        TEST_REQUIRE(store.GetValue("bool")   == true);
        TEST_REQUIRE(store.GetValue("int")    == 123);
        TEST_REQUIRE(store.GetValue("float")  == 123.0f);
        TEST_REQUIRE(store.GetValue("string") == std::string("foobar"));
        TEST_REQUIRE(store.GetValue("vec2")   == glm::vec2(1.0f, 2.0f));
        TEST_REQUIRE(store.GetValue("vec3")   == glm::vec3(1.0f, 2.0f, 3.0f));
        TEST_REQUIRE(store.GetValue("vec4")   == glm::vec4(1.0f, 2.0f, 3.0f, 4.0f));
        TEST_REQUIRE(store.GetValue("point")  == base::FPoint(1.0f, 2.0f));
        TEST_REQUIRE(store.GetValue("rect")   == base::FRect(1.0f, 2.0f, 3.0f, 4.0f));
        TEST_REQUIRE(store.GetValue("size")   == base::FSize(1.0f, 2.0f));
        TEST_REQUIRE(store.GetValue("color")  == base::Color4f(0.1f, 0.2f, 0.3f, 0.4f));

        TEST_REQUIRE(L["the_bool"]   == true);
        TEST_REQUIRE(L["the_int"]    == 123);
        TEST_REQUIRE(L["the_float"]  == 123.0f);
        TEST_REQUIRE(L["the_string"] == std::string("foobar"));
        TEST_REQUIRE(L["the_vec2"]   == glm::vec2(1.0f, 2.0f));
        TEST_REQUIRE(L["the_vec3"]   == glm::vec3(1.0f, 2.0f, 3.0f));
        TEST_REQUIRE(L["the_vec4"]   == glm::vec4(1.0f, 2.0f, 3.0f, 4.0f));
        TEST_REQUIRE(L["the_point"]  == base::FPoint(1.0f, 2.0f));
        TEST_REQUIRE(L["the_rect"]   == base::FRect(1.0f, 2.0f, 3.0f, 4.0f));
        TEST_REQUIRE(L["the_size"]   == base::FSize(1.0f, 2.0f));
        TEST_REQUIRE(L["the_color"]  == base::Color4f(0.1f, 0.2f, 0.3f, 0.4f));

        store.Clear();
        TEST_REQUIRE(store.HasValue("bool") == false);
        TEST_REQUIRE(store.HasValue("int") == false);

        L.script(R"(
function test(store, json)
   test_bool(store:Restore(json), true)
   test_bool(store:HasValue('int'), true)
   test_bool(store:HasValue('float'), true)

   test_int(store.int, 123)
   test_float(store.float, 123.0)
end
)");
        ret = L["test"](&store, &json);
        TEST_REQUIRE(ret.valid());
    }
}

void unit_test_entity_interface()
{
    TEST_CASE(test::Type::Feature)

    sol::state L;
    L.open_libraries();
    engine::BindBase(L);
    engine::BindGameLib(L);
    engine::BindGLM(L);
    engine::BindUtil(L);

    L["test_float"] = [](float a, float b) {
        std::printf("test_float a=%f, b=%f\n", a, b);
        if (!real::equals(a, b))
            throw std::runtime_error("float fail");
    };
    L["test_int"] = [](int a, int b) {
        std::printf("test_int a=%d, b=%d\n", a, b);
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
    L["test_vec3"] = [](const glm::vec3& vec, float x, float y, float z)  {
        if (vec != glm::vec3(x, y, z))
            throw std::runtime_error("vec3 fail");
    };
    L["test_vec4"] = [](const glm::vec4& vec, float x, float y, float z, float w)  {
        if (vec != glm::vec4(x, y, z, w))
            throw std::runtime_error("vec4 fail");
    };
    L["test_color"] = [](const base::Color4f& color, float r, float g, float b, float a)  {
        if (color != base::Color4f(r, g, b,a))
            throw std::runtime_error("vec4 fail");
    };

    // DrawableItem
    {
        auto klass = std::make_shared<game::DrawableItemClass>();
        klass->SetMaterialId("material");
        klass->SetDrawableId("drawable");
        klass->SetLayer(5);
        klass->SetLineWidth(2.0f);
        klass->SetTimeScale(3.0f);
        klass->SetFlag(game::DrawableItemClass::Flags::FlipHorizontally, true);
        klass->SetFlag(game::DrawableItemClass::Flags::RestartDrawable, false);

        game::DrawableItem item(klass);
        item.SetCurrentMaterialTime(2.0f);

        L.script(R"(
function test(node)
   test_str(node:GetMaterialId(), "material")
   test_str(node:GetDrawableId(), "drawable")
   test_int(node:GetLayer(), 5)
   test_float(node:GetLineWidth(), 2.0)
   test_float(node:GetTimeScale(), 3.0)
   test_float(node:GetMaterialTime(), 2.0)
   test_bool(node:TestFlag('FlipHorizontally'), true)
   test_bool(node:TestFlag('RestartDrawable'), false)
   test_bool(node:IsVisible(), true)

   node:SetVisible(false)
   test_bool(node:IsVisible(), false)
   node:SetFlag('VisibleInGame', true)
   test_bool(node:IsVisible(), true)

   node:SetTimeScale(1.0)
   test_float(node:GetTimeScale(), 1.0)

   node:SetUniform('float', 1.0)
   node:SetUniform('int',   2)
   node:SetUniform('vec2', glm.vec2:new(1.0, 2.0))
   node:SetUniform('vec3', glm.vec3:new(1.0, 2.0, 3.0))
   node:SetUniform('vec4', glm.vec4:new(1.0, 2.0, 3.0, 4.0))
   node:SetUniform('color', base.Color4f:new(0.1, 0.2, 0.3, 0.4))

   test_bool(node:HasUniform('kek'),   false)
   test_bool(node:HasUniform('float'), true)
   test_bool(node:HasUniform('int'),   true)
   test_bool(node:HasUniform('vec2'),  true)
   test_bool(node:HasUniform('vec3'),  true)
   test_bool(node:HasUniform('vec4'),  true)
   test_bool(node:HasUniform('color'), true)

   test_bool(node:FindUniform('kek') == nil, true)

   test_int(node:FindUniform('int'), 2)
   test_float(node:FindUniform('float'), 1.0)
   test_vec2(node:FindUniform('vec2'), 1.0, 2.0)
   test_vec3(node:FindUniform('vec3'), 1.0, 2.0, 3.0)
   test_vec4(node:FindUniform('vec4'), 1.0, 2.0, 3.0, 4.0)
   test_color(node:FindUniform('color'), 0.1, 0.2, 0.3, 0.4)

   node:DeleteUniform('kek') -- should do nothing
   node:DeleteUniform('int')
   test_bool(node:HasUniform('int'), false)

   node:ClearUniforms()
   test_bool(node:HasUniform('vec2'), false)
   test_bool(node:HasUniform('vec3'), false)
   test_bool(node:HasUniform('vec4'), false)
   test_bool(node:HasUniform('color'), false)

   test_bool(node:HasMaterialTimeAdjustment(), false)
   node:AdjustMaterialTime(1.0)
end
)");
        sol::function_result  ret = L["test"](&item);
        TEST_REQUIRE(ret.valid());
        TEST_REQUIRE(item.HasMaterialTimeAdjustment());
        TEST_REQUIRE(item.GetMaterialTimeAdjustment() == 1.0);

        // drawable command enqueue
        L.script(R"(
function test(item)
    item:Command('test')
    item:Command('test', 'arg', 123)
    item:Command('test', 'arg', 123.0)
    item:Command('test', 'arg', 'foobar')
    item:Command('test', { first = 123 })
    item:Command('test', { first = 123, second=123.0 })
end
)");

        ret = L["test"](&item);
        TEST_REQUIRE(ret.valid());
        TEST_REQUIRE(item.GetNumCommands() == 6);
        TEST_REQUIRE(item.GetCommand(0).name == "test");
        TEST_REQUIRE(item.GetCommand(1).name == "test");
        TEST_REQUIRE(item.GetCommand(1).args["arg"] == 123);
        TEST_REQUIRE(item.GetCommand(2).name == "test");
        TEST_REQUIRE(item.GetCommand(2).args["arg"] == 123.0f);

        TEST_REQUIRE(item.GetCommand(3).name == "test");
        TEST_REQUIRE(item.GetCommand(3).args["arg"] == std::string("foobar"));

        TEST_REQUIRE(item.GetCommand(4).name == "test");
        TEST_REQUIRE(item.GetCommand(4).args["first"] == 123);

        TEST_REQUIRE(item.GetCommand(5).args["first"] == 123);
        TEST_REQUIRE(item.GetCommand(5).args["second"] == 123.0f);
    }

    // Rigid body
    {
        auto klass = std::make_shared<game::RigidBodyItemClass>();
        klass->SetFlag(game::RigidBodyItemClass::Flags::Bullet, true);
        klass->SetFlag(game::RigidBodyItemClass::Flags::Enabled, false);
        klass->SetFlag(game::RigidBodyItemClass::Flags::Sensor, false);
        klass->SetLinearDamping(-1.0f);
        klass->SetAngularDamping(1.0f);
        klass->SetDensity(2.0f);
        klass->SetRestitution(3.0f);
        klass->SetSimulation(game::RigidBodyItemClass::Simulation::Kinematic);
        klass->SetCollisionShape(game::RigidBodyItemClass::CollisionShape::Polygon);
        klass->SetPolygonShapeId("polygon");
        klass->SetFriction(5.0f);

        game::RigidBodyItem body(klass);
        body.SetLinearVelocity(glm::vec2(1.0f, 2.0f));
        body.SetAngularVelocity(2.0f);

        L.script(R"(
function test(body)
    test_bool(body:IsEnabled(), false)
    test_bool(body:IsSensor(), false)
    test_bool(body:IsBullet(), true)
    test_bool(body:TestFlag('Bullet'), true)
    test_bool(body:TestFlag('Enabled'), false)
    test_bool(body:TestFlag('Sensor'), false)

    test_float(body:GetFriction(), 5.0)
    test_float(body:GetRestitution(), 3.0)
    test_float(body:GetAngularDamping(), 1.0)
    test_float(body:GetLinearDamping(), -1.0)
    test_float(body:GetDensity(), 2.0)
    test_str(body:GetPolygonShapeId(), 'polygon')

    test_vec2(body:GetLinearVelocity(), 1.0, 2.0)
    test_float(body:GetAngularVelocity(), 2.0)

    body:Enable(true)
    test_bool(body:IsEnabled(), true)

    test_str(body:GetSimulationType(), 'Kinematic')
    test_str(body:GetCollisionShapeType(), 'Polygon')

    body:ApplyImpulse(1.0, 2.0)
end
)");
        sol::function_result  ret = L["test"](&body);
        TEST_REQUIRE(ret.valid());
        TEST_REQUIRE(body.HasCenterImpulse());
        TEST_REQUIRE(body.GetLinearImpulseToCenter() == glm::vec2(1.0f, 2.0f));
    }

    // Text item
    {

    }

    // entity
    {
        auto entity = std::make_shared<game::EntityClass>();
        entity->SetName("entity");

        game::Entity e(entity);

        // test posted event.
L.script(R"(
function test(e)
   e:PostEvent('int', 'test', 123)
   e:PostEvent('str', 'test', 'keke')
   e:PostEvent('bool', 'test', true)
   e:PostEvent('float', 'test', 123.0)
   e:PostEvent('vec2', 'test', glm.vec2:new(1.0, 2.0))
   e:PostEvent('vec3', 'test', glm.vec3:new(1.0, 2.0, 3.0))
   e:PostEvent('vec4', 'test', glm.vec4:new(1.0, 2.0, 3.0, 4.0))
end
)");
        sol::function_result ret = L["test"](&e);
        TEST_REQUIRE(ret.valid());

        std::vector<game::Entity::Event> events;
        e.Update(0.0f, &events);
        TEST_REQUIRE(events.size() == 7);
        TEST_REQUIRE(std::get<game::Entity::PostedEvent>(events[0]).message == "int");
        TEST_REQUIRE(std::get<game::Entity::PostedEvent>(events[0]).value   == 123);
        TEST_REQUIRE(std::get<game::Entity::PostedEvent>(events[1]).message == "str");
        TEST_REQUIRE(std::get<game::Entity::PostedEvent>(events[1]).value   == std::string("keke"));
        TEST_REQUIRE(std::get<game::Entity::PostedEvent>(events[2]).message == "bool");
        TEST_REQUIRE(std::get<game::Entity::PostedEvent>(events[2]).value   == true);
        TEST_REQUIRE(std::get<game::Entity::PostedEvent>(events[3]).message == "float");
        TEST_REQUIRE(std::get<game::Entity::PostedEvent>(events[3]).value   == 123.0f);
        TEST_REQUIRE(std::get<game::Entity::PostedEvent>(events[4]).message == "vec2");
        TEST_REQUIRE(std::get<game::Entity::PostedEvent>(events[4]).value   == glm::vec2(1.0f, 2.0f));
        TEST_REQUIRE(std::get<game::Entity::PostedEvent>(events[5]).message == "vec3");
        TEST_REQUIRE(std::get<game::Entity::PostedEvent>(events[5]).value   == glm::vec3(1.0f, 2.0f, 3.0f));
        TEST_REQUIRE(std::get<game::Entity::PostedEvent>(events[6]).message == "vec4");
        TEST_REQUIRE(std::get<game::Entity::PostedEvent>(events[6]).value   == glm::vec4(1.0f, 2.0f, 3.0f, 4.0f));

    }
}

void unit_test_scene_interface()
{
    TEST_CASE(test::Type::Feature)

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
        game::EntityPlacement scene_node;
        scene_node.SetName("test_entity_1");
        scene_node.SetLayer(4);
        scene_node.SetEntity(entity);
        scene_node.SetTranslation(glm::vec2(30.0f, 40.0f));
        scene.LinkChild(nullptr, scene.PlaceEntity(scene_node));

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
    TEST_CASE(test::Type::Feature)

    base::OverwriteTextFile("entity_begin_end_play_test.lua", R"(
function BeginPlay(entity, scene, map)
   local event = game.GameEvent:new()
   event.from  = entity:GetName()
   event.message = 'begin'
   Game:PostEvent(event)
end
function EndPlay(entity, scene, map)
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
        game::EntityPlacement node;
        node.SetName("entity");
        node.SetEntity(entity);
        scene_class.LinkChild(nullptr, scene_class.PlaceEntity(node));
    }
    game::Scene scene(scene_class);

    TestLoader loader;

    engine::LuaRuntime script(".", "", "", "");
    script.SetDataLoader(&loader);
    script.Init();
    script.BeginPlay(&scene, nullptr);

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
    TEST_CASE(test::Type::Feature)

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
        game::EntityPlacement node;
        node.SetName("foo");
        node.SetEntity(foo);
        scene_class.LinkChild(nullptr, scene_class.PlaceEntity(node));
    }
    {
        game::EntityPlacement node;
        node.SetName("bar");
        node.SetEntity(bar);
        scene_class.LinkChild(nullptr, scene_class.PlaceEntity(node));
    }

    TestLoader loader;

    game::Scene scene(scene_class);
    engine::LuaRuntime script(".", "", "", "");
    script.SetDataLoader(&loader);
    script.Init();
    script.BeginPlay(&scene, nullptr);

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

void unit_test_entity_animation_state()
{
    TEST_CASE(test::Type::Feature)

    base::OverwriteTextFile("entity_animator.lua", R"(
function EnterState(animator, state, entity)
  print('Enter State: ' .. state)

  local event = game.GameEvent:new()
  event.message = 'enter state'
  event.state   = state
  Game:PostEvent(event)
end

function LeaveState(animator, state, entity)
  print('Leave State: ' .. state)

  local event = game.GameEvent:new()
  event.message = 'leave state'
  event.state   = state
  Game:PostEvent(event)
end

function UpdateState(animator, state, time, dt, entity)
  print('Update State: ' .. state)

  local event = game.GameEvent:new()
  event.message = 'update state'
  event.state   = state
  Game:PostEvent(event)
end

function EvalTransition(animator, from, to, entity)
    if from == 'idle' then
       if to == 'jump' then
           return true
       end
    end
    return false
end

function StartTransition(animator, from, to, duration, entity)
   print('Start Transition: ' .. from .. ' => ' .. to)

  local event = game.GameEvent:new()
  event.message = 'start transition'
  event.src_state = from
  event.dst_state = to
  Game:PostEvent(event)
end

function FinishTransition(animator, from, to, entity)
   print('Finish Transition: ' .. from .. ' => ' .. to)

  local event = game.GameEvent:new()
  event.message = 'finish transition'
  event.src_state = from
  event.dst_state = to
  Game:PostEvent(event)
end

function UpdateTransition(animator, from, to, duration, time, dt, entity)
   print('Update Transition: ' .. from .. ' => ' .. to)
   print("duration: " .. tostring(duration))
   print("time: " .. tostring(time) .. " dt: " .. tostring(dt))

  local event = game.GameEvent:new()
  event.message = 'update transition'
  event.src_state = from
  event.dst_state = to
  event.duration  = duration
  event.time      = time
  event.dt        = dt
  Game:PostEvent(event)
end
)");

    auto foo = std::make_shared<game::EntityClass>();
    foo->SetName("foo");

    {
        game::AnimatorClass klass;
        klass.SetName("Animator");
        klass.SetScriptId("entity_animator");

        game::AnimationStateClass idle;
        idle.SetName("idle");
        klass.AddState(idle);

        game::AnimationStateClass run;
        run.SetName("run");
        klass.AddState(run);

        game::AnimationStateClass jump;
        jump.SetName("jump");
        klass.AddState(jump);

        game::AnimationStateTransitionClass idle_to_run;
        idle_to_run.SetName("idle to run");
        idle_to_run.SetSrcStateId(idle.GetId());
        idle_to_run.SetDstStateId(run.GetId());
        klass.AddTransition(idle_to_run);

        game::AnimationStateTransitionClass run_to_idle;
        run_to_idle.SetName("run to idle");
        run_to_idle.SetSrcStateId(run.GetId());
        run_to_idle.SetDstStateId(idle.GetId());
        run_to_idle.SetDuration(1.0f);
        klass.AddTransition(run_to_idle);

        game::AnimationStateTransitionClass idle_to_jump;
        idle_to_jump.SetName("idle to jump");
        idle_to_jump.SetSrcStateId(idle.GetId());
        idle_to_jump.SetDstStateId(jump.GetId());
        klass.AddTransition(idle_to_jump);

        game::AnimationStateTransitionClass jump_to_idle;
        jump_to_idle.SetName("jump to idle");
        jump_to_idle.SetSrcStateId(jump.GetId());
        jump_to_idle.SetDstStateId(idle.GetId());
        klass.AddTransition(jump_to_idle);

        klass.SetInitialStateId(idle.GetId());

        foo->AddAnimator(std::move(klass));
    }

    game::SceneClass scene_class;
    {
        game::EntityPlacement node;
        node.SetName("foo");
        node.SetEntity(foo);
        scene_class.LinkChild(nullptr, scene_class.PlaceEntity(node));
    }

    TestLoader loader;

    float time = 0.0f;
    float step = 1.0f/60.0f;

    game::Scene scene(scene_class);
    engine::LuaRuntime script(".", "", "", "");
    script.SetDataLoader(&loader);
    script.Init();

    script.BeginPlay(&scene, nullptr);
    script.BeginLoop();
    script.Update(time, step);
    time += step;

    engine::Action action;
    TEST_REQUIRE(script.GetNextAction(&action));
    auto* event = std::get_if<engine::PostEventAction>(&action);
    TEST_REQUIRE(event->event.message == "enter state");
    TEST_REQUIRE(Eq(event->event.values["state"], "idle"));

    TEST_REQUIRE(script.GetNextAction(&action));
    event = std::get_if<engine::PostEventAction>(&action);
    TEST_REQUIRE(event->event.message == "update state");
    TEST_REQUIRE(Eq(event->event.values["state"], "idle"));

    TEST_REQUIRE(!script.GetNextAction(&action));

    script.EndLoop();
    script.BeginLoop();
    script.Update(time, step);
    time += step;

    TEST_REQUIRE(script.GetNextAction(&action));
    event = std::get_if<engine::PostEventAction>(&action);
    TEST_REQUIRE(event->event.message == "leave state");
    TEST_REQUIRE(Eq(event->event.values["state"], "idle"));

    TEST_REQUIRE(script.GetNextAction(&action));
    event = std::get_if<engine::PostEventAction>(&action);
    TEST_REQUIRE(event->event.message == "start transition");
    TEST_REQUIRE(Eq(event->event.values["src_state"], "idle"));
    TEST_REQUIRE(Eq(event->event.values["dst_state"], "jump"));

    TEST_REQUIRE(script.GetNextAction(&action));
    event = std::get_if<engine::PostEventAction>(&action);
    TEST_REQUIRE(event->event.message == "update transition");
    TEST_REQUIRE(Eq(event->event.values["src_state"], "idle"));
    TEST_REQUIRE(Eq(event->event.values["dst_state"], "jump"));

    TEST_REQUIRE(script.GetNextAction(&action));
    event = std::get_if<engine::PostEventAction>(&action);
    TEST_REQUIRE(event->event.message == "finish transition");
    TEST_REQUIRE(Eq(event->event.values["src_state"], "idle"));
    TEST_REQUIRE(Eq(event->event.values["dst_state"], "jump"));

    TEST_REQUIRE(script.GetNextAction(&action));
    event = std::get_if<engine::PostEventAction>(&action);
    TEST_REQUIRE(event->event.message == "enter state");
    TEST_REQUIRE(Eq(event->event.values["state"], "jump"));

    TEST_REQUIRE(!script.GetNextAction(&action));

    script.EndLoop();
    script.BeginLoop();
    script.Update(time, step);
    time += step;

    TEST_REQUIRE(script.GetNextAction(&action));
    event = std::get_if<engine::PostEventAction>(&action);
    TEST_REQUIRE(event->event.message == "update state");
    TEST_REQUIRE(Eq(event->event.values["state"], "jump"));
}

void unit_test_entity_private_environment()
{
    TEST_CASE(test::Type::Feature)

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
        game::EntityPlacement node;
        node.SetName("foo");
        node.SetEntity(foo);
        scene_class.LinkChild(nullptr, scene_class.PlaceEntity(node));
    }
    {
        game::EntityPlacement node;
        node.SetName("bar");
        node.SetEntity(bar);
        scene_class.LinkChild(nullptr, scene_class.PlaceEntity(node));
    }

    game::Scene instance(scene_class);

    TestLoader loader;

    engine::LuaRuntime script(".", "", "", "");
    script.SetDataLoader(&loader);
    script.Init();
    script.BeginPlay(&instance, nullptr);
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

// Test that scripting variables marked private are only available within
// the script associated with that entity type.
void unit_test_entity_private_variable()
{
    TEST_CASE(test::Type::Feature)

    // Sol2 seems to have some bug regarding accessing this_environment
    // which is crucial for checking the access to a private script var.
    // seems that using a random print provides a workaround (state gets set)
    // https://github.com/ThePhD/sol2/issues/1464

    // foo can access its own variable in the foo script
    base::OverwriteTextFile("entity_env_foo_test.lua", R"(
function Tick(foo, game_time, dt)
   print('sol2 WAR')

   print('private var', foo.my_private_var)
   print('public var', foo.my_public_var)

   local event = game.GameEvent:new()
   event.message = 'foo'
   event.value   = 'ok'
   Game:PostEvent(event)
end
)");

    // bar cannot access foo's private variable but only a public variable.
    base::OverwriteTextFile("entity_env_bar_test.lua", R"(
function try_access_private_var(foo)
   print('workaround for sol2 bug with this_environment')

  --print('foo private var', foo.my_private_var)
  local val = foo.my_private_var
end

function Tick(bar, game_time, dt)
   local scene = bar:GetScene()
   local foo = scene:FindEntityByInstanceName('foo')

   if pcall(try_access_private_var, foo) then
     error('failed testing private variable')
   end

   print('foo public var', foo.my_public_var)

   local event = game.GameEvent:new()
   event.message = 'bar'
   event.value   = 'ok'
   Game:PostEvent(event)
end
)");

    game::ScriptVar private_var("my_private_var", 1.0f);
    private_var.SetPrivate(true);
    private_var.SetArray(false);
    private_var.SetReadOnly(true);

    game::ScriptVar public_var("my_public_var", 2.0f);
    public_var.SetPrivate(false);
    public_var.SetArray(false);
    public_var.SetReadOnly(true);

    auto foo = std::make_shared<game::EntityClass>();
    foo->SetName("foo");
    foo->SetSriptFileId("entity_env_foo_test");
    foo->SetFlag(game::EntityClass::Flags::TickEntity, true);
    foo->AddScriptVar(private_var);
    foo->AddScriptVar(public_var);

    auto bar = std::make_shared<game::EntityClass>();
    bar->SetName("bar");
    bar->SetSriptFileId("entity_env_bar_test");
    bar->SetFlag(game::EntityClass::Flags::TickEntity, true);

    game::SceneClass scene_class;
    {
        game::EntityPlacement node;
        node.SetName("foo");
        node.SetEntity(foo);
        scene_class.LinkChild(nullptr, scene_class.PlaceEntity(node));
    }
    {
        game::EntityPlacement node;
        node.SetName("bar");
        node.SetEntity(bar);
        scene_class.LinkChild(nullptr, scene_class.PlaceEntity(node));
    }

    game::Scene instance(scene_class);

    TestLoader loader;

    engine::LuaRuntime script(".", "", "", "");
    script.SetDataLoader(&loader);
    script.Init();
    script.BeginPlay(&instance, nullptr);
    script.Tick(0.0, 0.0);

    engine::Action action1;
    engine::Action action2;
    TEST_REQUIRE(script.GetNextAction(&action1));
    TEST_REQUIRE(script.GetNextAction(&action2));
}

void unit_test_entity_cross_env_call()
{
    TEST_CASE(test::Type::Feature)

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
        game::EntityPlacement node;
        node.SetName("foo");
        node.SetEntity(foo);
        scene_class.LinkChild(nullptr, scene_class.PlaceEntity(node));
    }
    {
        game::EntityPlacement node;
        node.SetName("bar");
        node.SetEntity(bar);
        scene_class.LinkChild(nullptr, scene_class.PlaceEntity(node));
    }

    game::Scene instance(scene_class);

    TestLoader loader;

    engine::LuaRuntime script(".", "", "", "");
    script.SetDataLoader(&loader);
    script.Init();
    script.BeginPlay(&instance, nullptr);
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
    TEST_CASE(test::Type::Feature)

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
        game::EntityPlacement node;
        node.SetName("foo");
        node.SetEntity(foo);
        scene_class.LinkChild(nullptr, scene_class.PlaceEntity(node));
    }
    {
        game::EntityPlacement node;
        node.SetName("bar");
        node.SetEntity(bar);
        scene_class.LinkChild(nullptr, scene_class.PlaceEntity(node));
    }

    game::Scene instance(scene_class);

    TestLoader loader;

    engine::LuaRuntime script(".", "", "", "");
    script.SetDataLoader(&loader);
    script.Init();
    script.BeginPlay(&instance, nullptr);
    script.Tick(0.0, 0.0);

    engine::Action action;
    TEST_REQUIRE(script.GetNextAction(&action));
    auto* event = std::get_if<engine::PostEventAction>(&action);
    TEST_REQUIRE(event->event.message == "bar");
    TEST_REQUIRE(std::get<int>(event->event.values["value"]) == 123);
}

void unit_test_game_main_script_load_success()
{
    TEST_CASE(test::Type::Feature)

    {
        base::OverwriteTextFile("game_main_script_test.lua", R"(
function LoadGame()
  local event = game.GameEvent:new()
  event.message = 'load'
  Game:PostEvent(event)
end
    )");

        TestLoader loader;

        engine::LuaRuntime game("", "fs://game_main_script_test.lua", "", "");
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

        engine::LuaRuntime game("", "fs://game_main_script_test.lua", "", "");
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
    TEST_CASE(test::Type::Feature)

    base::OverwriteTextFile("broken.lua", R"(
function Broken()
endkasd
)");

    // no such file.
    {
        TestLoader loader;

        engine::LuaRuntime game("", "fs://this-file-doesnt-exist.lua", "", "");
        game.SetDataLoader(&loader);
        TEST_EXCEPTION(game.Init());
    }

    // broken Lua code in file.
    {
        TestLoader loader;
        engine::LuaRuntime game("", "fs://broken.lua", "", "");
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
        engine::LuaRuntime game("", "fs://game_main_script_test.lua", "", "");
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
        engine::LuaRuntime game("", "fs://game_main_script_test.lua", "", "");
        game.SetDataLoader(&loader);
        TEST_EXCEPTION(game.Init());
    }
}

enum class EntityUpdateMeasurement {
    RuntimeLua,
    RuntimeNative,
    BenchmarkNative,
    BenchmarkLua,
    EntityBatch1,
    RuntimeNodesLua
};

template<EntityUpdateMeasurement target>
void measure_entity_update_time()
{
    TEST_CASE(test::Type::Other)

    base::OverwriteTextFile("bullet.lua", R"(
function Update(bullet, game_time, dt)
    local bullet_node = bullet:GetNode(0)
    local bullet_pos = bullet_node:GetTranslation()
    local velocity = bullet.velocity
    bullet_pos.y = bullet_pos.y + dt * velocity
    bullet_node:SetTranslation(bullet_pos)
end
)");

    game::ScriptVar velocity("velocity", 1.0f);
    velocity.SetArray(false);

    game::EntityNodeClass body;
    body.SetName("body");
    body.SetSize(10.0f, 10.0f);
    body.SetTranslation(0.0f, 0.0f);

    auto bullet_class = std::make_shared<game::EntityClass>();
    bullet_class->SetName("Bullet");
    bullet_class->SetSriptFileId("bullet");
    bullet_class->SetFlag(game::EntityClass::Flags::UpdateEntity, true);
    bullet_class->AddScriptVar(velocity);
    bullet_class->AddNode(std::move(body));
    bullet_class->LinkChild(nullptr, bullet_class->FindNodeByName("body"));

    auto scene_class = std::make_shared<game::SceneClass>();
    scene_class->SetName("scene");

    game::Scene scene(scene_class);

    scene.BeginLoop();
    for (size_t i=0; i<1000; ++i)
    {
        game::EntityArgs args;
        args.klass = bullet_class;
        scene.SpawnEntity(args);
    }
    scene.EndLoop();
    scene.BeginLoop();
    scene.EndLoop();
    TEST_REQUIRE(scene.GetNumEntities() == 1000);

    TestLoader loader;

    engine::LuaRuntime script(".", "", "", "");
    script.SetDataLoader(&loader);
    script.Init();
    script.BeginPlay(&scene, nullptr);

    if (target == EntityUpdateMeasurement::RuntimeLua)
    {

        auto ret = test::TimedTest(1000, [&script]() {
            script.Update(0.0, 0.0f);
        });

        test::PrintTestTimes("Bullet Update (Lua)", ret);
    }

    if (target == EntityUpdateMeasurement::RuntimeNative)
    {
        const auto ret = test::TimedTest(1000, [&scene] {
            for (size_t i=0; i<scene.GetNumEntities(); ++i) {
                auto& entity = scene.GetEntity(i);
                auto& node   = entity.GetNode(0);
                auto pos = node.GetTranslation();
                const auto variable = entity.FindScriptVarByName("velocity");
                const auto velocity = variable->GetValue<float>();
                pos.y += velocity * 1.0f;
                node.SetTranslation(pos);
            }
        });
        test::PrintTestTimes("Bullet Update (Native)", ret);
    }


    if (target == EntityUpdateMeasurement::BenchmarkNative)
    {
        struct Bullet {
            glm::vec2 position {0.0f, 0.0f};
            glm::vec2 velocity {1.0f, 1.0f};
        };

        {
            std::vector<Bullet> bullets;
            bullets.resize(1000);

            const auto ret = test::TimedTest(1000, [&bullets] {
                for (auto& bullet: bullets)
                {
                    bullet.position += bullet.velocity * 1.0f;
                }
            });
            test::PrintTestTimes("Bullet Update Benchmark (Native, vector)", ret);
        }
        {
            std::deque<Bullet> bullets;
            bullets.resize(1000);

            const auto ret = test::TimedTest(1000, [&bullets] {
                for (auto& bullet: bullets)
                {
                    bullet.position += bullet.velocity * 1.0f;
                }
            });
            test::PrintTestTimes("Bullet Update Benchmark (Native, deque)", ret);
        }
    }


    if (target == EntityUpdateMeasurement::BenchmarkLua)
    {
        sol::state lua;
        lua.open_libraries();
        engine::BindGLM(lua);

        struct Bullet {
            glm::vec2 position {0.0f, 0.0f};
            glm::vec2 velocity {1.0f, 1.0f};

            float px;
            float py;
        };
        auto bullet_type = lua.new_usertype<Bullet>("Bullet");

        bullet_type["position"] = &Bullet::position;
        bullet_type["velocity"] = &Bullet::velocity;
        bullet_type["px"] = &Bullet::px;
        bullet_type["py"] = &Bullet::py;

        bullet_type["GetPosition"] = [](const Bullet& bullet) {
            return bullet.position;
        };
        bullet_type["SetPosition"] = [](Bullet& bullet, glm::vec2 pos) {
            bullet.position = pos;
        };
        bullet_type["GetVelocity"] = [](const Bullet& bullet) {
            return bullet.velocity;
        };

        std::vector<Bullet> bullets;
        bullets.resize(1000);

        std::vector<Bullet*> bullets_ptr;
        for (auto& b : bullets)
            bullets_ptr.push_back(&b);

        lua.script(R"(
function Update(bullet, game_time, dt)
    local velocity = bullet.velocity;
    bullet.position = bullet.position + velocity * dt;
end

function Update2(bullet, game_time, dt)
    local velocity = bullet:GetVelocity()
    local position = bullet:GetPosition()
    position.y = position.y + velocity.y * dt;
    bullet:SetPosition(position);
end

function Update3(bullets, game_time, dt)
   for i=1, #bullets do
       local bullet = bullets[i]
       local velocity = bullet.velocity
       local position = bullet.position
       position.y = position.y + velocity.y * dt
       bullet.position = position
   end
end

function Update4(bullets, game_time, dt)
    for i=1, #bullets do
       local bullet = bullets[i]
       bullet.px = bullet.px + 1.0 * dt
       bullet.py = bullet.py + 2.0 * dt
    end
end

        )");

        const auto ret = test::TimedTest(1000, [&bullets, &bullets_ptr, &lua] {

            /*
            auto func = lua["Update"];
            for (auto& bullet : bullets)
            {
                //lua["Update"](bullet, 0.0, 1.0f);
                func(bullet, 0.0, 1.0);
            }
             */

            // using vector of objects is a bit faster than using
            // a vector of pointers to objects

            // using floats is slightly faster than using glm::vec2
            //lua["Update4"](bullets, 0.0, 1.0);

            lua["Update3"](bullets, 0.0, 1.0);
        });
        test::PrintTestTimes("Bullet Update Benchmark (Lua)", ret);
    }

    if (target == EntityUpdateMeasurement::EntityBatch1)
    {
        std::vector<std::unique_ptr<game::Entity>> bullets;
        for (size_t i=0; i<1000; ++i)
        {
            auto bullet = game::CreateEntityInstance(bullet_class);
            bullets.push_back(std::move(bullet));
        }

        sol::state lua;
        lua.open_libraries();

        engine::BindGameLib(lua);
        engine::BindGLM(lua);

        lua.script(R"(
function Update(bullets, game_time, dt)
   for i=1, #bullets do
       local bullet = bullets[i]
       local bullet_node = bullet:GetNode(0)
       local bullet_pos = bullet_node:GetTranslation()
       local velocity = bullet.velocity
       bullet_pos.y = bullet_pos.y + dt * velocity
       bullet_node:SetTranslation(bullet_pos)
   end
end
        )");

        auto ret = test::TimedTest(1000, [&bullets, &lua]() {
            lua["Update"](bullets, 0.0, 0.0f);
        });

        test::PrintTestTimes("Batch Update Entity Test (Lua)", ret);
    }

    if (target == EntityUpdateMeasurement::RuntimeNodesLua)
    {
        sol::state lua;
        lua.open_libraries();

        engine::BindGameLib(lua);
        engine::BindGLM(lua);

        lua.script(R"(
function UpdateNodes1(nodes, game_time, dt, class)
    local hi = nodes:GetHighIndex()
    for i = 0, hi do
        local transform = nodes:GetTransform(i)
        if transform ~= nil then
            transform.translation.y = transform.translation.y + dt * 5.0
        end
    end
end

-- vector hack
function UpdateNodes2(nodes, game_time, dt)
   for i=1, #nodes do
      local transform = nodes[i]
      transform.translation.y = transform.translation.y + dt * 5.0
   end
end


function UpdateNodes3(nodes, game_time, dt)
    local transforms = nodes:GetTransforms()
    for i=1, #transforms do
        local transform = transforms[i]
        transform.translation.y = transform.translation.y + dt * 5.0
    end
end
        )");

        {
            auto* allocator = &bullet_class->GetAllocator();
            auto ret = test::TimedTest(1000, [allocator, &lua, bullet_class]() {
                lua["UpdateNodes1"](allocator, 0.0, 0.0f);
            });
            test::PrintTestTimes("Batch Update Entity Nodes (Lua)", ret);
        }

        {

            std::vector<game::EntityNodeTransform*> test;

            auto* allocator = &bullet_class->GetAllocator();
            auto ret = test::TimedTest(1000, [&allocator, &lua, &test]() {
                test.clear();
                test.reserve(allocator->GetHighIndex());
                game::EntityNodeTransformSequence  sequence(allocator);

                for (auto beg = sequence.begin(); beg != sequence.end(); ++beg) {
                    test.push_back(&(*beg));
                }
                lua["UpdateNodes2"](test, 0.0, 0.0f);
            });

            test::PrintTestTimes("Batch Update Entity Nodes (Lua, vector hack)", ret);
        }

        {
            auto* allocator = &bullet_class->GetAllocator();
            auto ret = test::TimedTest(1000, [allocator, &lua, bullet_class]() {
                lua["UpdateNodes3"](allocator, 0.0, 0.0f);
            });
            test::PrintTestTimes("Batch Update Entity Nodes (Lua iterator container)", ret);
        }

        {
            auto* allocator = &bullet_class->GetAllocator();
            auto ret = test::TimedTest(1000, [allocator, &lua, bullet_class]() {
                /*
                game::EntityNodeTransformSequence sequence(allocator);
                auto beg = sequence.begin();
                auto end = sequence.end();
                for (; beg != end; ++beg) {
                    auto& transform = *beg;
                    transform.translation.y = transform.translation.y + 1.0f * 2.0f;
                }
                 */
                // this is much faster than using the iterators huhu
                const auto high = allocator->GetHighIndex();
                for (size_t i=0; i<high; ++i)
                {
                    auto* transform = allocator->template GetObject<game::EntityNodeTransform>(i);
                    if (transform) {
                        transform->translation.y = transform->translation.y + 1.0 * 2.0;
                    }
                }
            });
            test::PrintTestTimes("Batch Update Entity Nodes (C++ iterator container)", ret);
        }
    }
}

} // NAMESPACE

EXPORT_TEST_MAIN(
int test_main(int argc, char* argv[])
{
    test::TestLogger logger("unit_test_lua.log");

    unit_test_util();
    unit_test_glm();
    unit_test_base();
    unit_test_data();
    unit_test_game();
    unit_test_entity_interface();
    unit_test_scene_interface();
    unit_test_entity_begin_end_play();
    unit_test_entity_tick_update();
    unit_test_entity_animation_state();
    unit_test_entity_private_environment();
    unit_test_entity_private_variable();
    unit_test_entity_cross_env_call();
    unit_test_entity_shared_globals();
    unit_test_game_main_script_load_success();
    unit_test_game_main_script_load_failure();

    measure_entity_update_time<EntityUpdateMeasurement::RuntimeLua>();
    measure_entity_update_time<EntityUpdateMeasurement::RuntimeNative>();
    measure_entity_update_time<EntityUpdateMeasurement::BenchmarkLua>();
    measure_entity_update_time<EntityUpdateMeasurement::BenchmarkNative>();
    measure_entity_update_time<EntityUpdateMeasurement::EntityBatch1>();
    measure_entity_update_time<EntityUpdateMeasurement::RuntimeNodesLua>();
    return 0;
}
) // TEST_MAIN
