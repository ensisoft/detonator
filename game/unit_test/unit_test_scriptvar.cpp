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
#include "base/test_help.h"
#include "game/scriptvar.h"
#include "data/json.h"

void test_script_var()
{
    TEST_CASE(test::Type::Feature)

    {
        game::ScriptVar var("foo", std::string("blabla"), false);
        TEST_REQUIRE(var.IsReadOnly() == false);
        TEST_REQUIRE(var.IsArray() == false);
        TEST_REQUIRE(var.GetName() == "foo");
        TEST_REQUIRE(var.GetType() == game::ScriptVar::Type::String);
        TEST_REQUIRE(var.GetValue<std::string>() == "blabla");

        data::JsonObject json;
        var.IntoJson(json);

        game::ScriptVar ret;
        TEST_REQUIRE(ret.FromJson(json));
        TEST_REQUIRE(ret.IsReadOnly() == false);
        TEST_REQUIRE(ret.IsArray() == false);
        TEST_REQUIRE(ret.GetName() == "foo");
        TEST_REQUIRE(ret.GetType() == game::ScriptVar::Type::String);
        TEST_REQUIRE(ret.GetValue<std::string>() == "blabla");
    }

    {
        game::ScriptVar var("foo", std::vector<int>{1, 2, 6}, false);
        TEST_REQUIRE(var.IsReadOnly() == false);
        TEST_REQUIRE(var.IsArray() == true);
        TEST_REQUIRE(var.GetName() == "foo");
        TEST_REQUIRE(var.GetType() == game::ScriptVar::Type::Integer);
        TEST_REQUIRE(var.GetArray<int>()[0] == 1);
        TEST_REQUIRE(var.GetArray<int>()[1] == 2);
        TEST_REQUIRE(var.GetArray<int>()[2] == 6);

        data::JsonObject json;
        var.IntoJson(json);

        game::ScriptVar ret;
        TEST_REQUIRE(ret.FromJson(json));
        TEST_REQUIRE(ret.IsReadOnly() == false);
        TEST_REQUIRE(ret.IsArray() == true);
        TEST_REQUIRE(ret.GetName() == "foo");
        TEST_REQUIRE(ret.GetType() == game::ScriptVar::Type::Integer);
        TEST_REQUIRE(ret.GetArray<int>()[0] == 1);
        TEST_REQUIRE(ret.GetArray<int>()[1] == 2);
        TEST_REQUIRE(ret.GetArray<int>()[2] == 6);
    }

    {
        std::vector<glm::vec2> data;
        data.push_back(glm::vec2(0.0f, 1.0f));
        data.push_back(glm::vec2(-1.0f, 3.0f));

        game::ScriptVar var("foo", data, true);
        TEST_REQUIRE(var.IsReadOnly() == true);
        TEST_REQUIRE(var.IsArray() == true);
        TEST_REQUIRE(var.GetName() == "foo");
        TEST_REQUIRE(var.GetType() == game::ScriptVar::Type::Vec2);
        TEST_REQUIRE(var.GetArray<glm::vec2>()[0] == data[0]);
        TEST_REQUIRE(var.GetArray<glm::vec2>()[1] == data[1]);

        data::JsonObject json;
        var.IntoJson(json);
        game::ScriptVar ret;
        TEST_REQUIRE(ret.FromJson(json));
        TEST_REQUIRE(ret.IsReadOnly() == true);
        TEST_REQUIRE(ret.IsArray() == true);
        TEST_REQUIRE(ret.GetName() == "foo");
        TEST_REQUIRE(ret.GetType() == game::ScriptVar::Type::Vec2);
        TEST_REQUIRE(ret.GetArray<glm::vec2>()[0] == data[0]);
        TEST_REQUIRE(ret.GetArray<glm::vec2>()[1] == data[1]);

    }

    {
        std::vector<glm::vec3> data;
        data.push_back(glm::vec3(0.0f, 1.0f, 2.0f));
        data.push_back(glm::vec3(-1.0f, 3.0f, 2.0f));

        game::ScriptVar var("foo", data, true);
        TEST_REQUIRE(var.IsReadOnly() == true);
        TEST_REQUIRE(var.IsArray() == true);
        TEST_REQUIRE(var.GetName() == "foo");
        TEST_REQUIRE(var.GetType() == game::ScriptVar::Type::Vec3);
        TEST_REQUIRE(var.GetArray<glm::vec3>()[0] == data[0]);
        TEST_REQUIRE(var.GetArray<glm::vec3>()[1] == data[1]);

        data::JsonObject json;
        var.IntoJson(json);
        game::ScriptVar ret;
        TEST_REQUIRE(ret.FromJson(json));
        TEST_REQUIRE(ret.IsReadOnly() == true);
        TEST_REQUIRE(ret.IsArray() == true);
        TEST_REQUIRE(ret.GetName() == "foo");
        TEST_REQUIRE(ret.GetType() == game::ScriptVar::Type::Vec3);
        TEST_REQUIRE(ret.GetArray<glm::vec3>()[0] == data[0]);
        TEST_REQUIRE(ret.GetArray<glm::vec3>()[1] == data[1]);

    }

    {
        std::vector<glm::vec4> data;
        data.push_back(glm::vec4(0.0f, 1.0f, 2.0f, 5.0f));
        data.push_back(glm::vec4(-1.0f, 3.0f, 2.0f, 5.0f));

        game::ScriptVar var("foo", data, true);
        TEST_REQUIRE(var.IsReadOnly() == true);
        TEST_REQUIRE(var.IsArray() == true);
        TEST_REQUIRE(var.GetName() == "foo");
        TEST_REQUIRE(var.GetType() == game::ScriptVar::Type::Vec4);
        TEST_REQUIRE(var.GetArray<glm::vec4>()[0] == data[0]);
        TEST_REQUIRE(var.GetArray<glm::vec4>()[1] == data[1]);

        data::JsonObject json;
        var.IntoJson(json);
        game::ScriptVar ret;
        TEST_REQUIRE(ret.FromJson(json));
        TEST_REQUIRE(ret.IsReadOnly() == true);
        TEST_REQUIRE(ret.IsArray() == true);
        TEST_REQUIRE(ret.GetName() == "foo");
        TEST_REQUIRE(ret.GetType() == game::ScriptVar::Type::Vec4);
        TEST_REQUIRE(ret.GetArray<glm::vec4>()[0] == data[0]);
        TEST_REQUIRE(ret.GetArray<glm::vec4>()[1] == data[1]);

    }

    {
        std::vector<game::Color4f> data;
        data.push_back(game::Color::Red);
        data.push_back(game::Color::Green);

        game::ScriptVar var("foo", data, true);
        TEST_REQUIRE(var.IsReadOnly() == true);
        TEST_REQUIRE(var.IsArray() == true);
        TEST_REQUIRE(var.GetName() == "foo");
        TEST_REQUIRE(var.GetType() == game::ScriptVar::Type::Color);
        TEST_REQUIRE(var.GetArray<game::Color4f>()[0] == data[0]);
        TEST_REQUIRE(var.GetArray<game::Color4f>()[1] == data[1]);

        data::JsonObject json;
        var.IntoJson(json);
        game::ScriptVar ret;
        TEST_REQUIRE(ret.FromJson(json));
        TEST_REQUIRE(ret.IsReadOnly() == true);
        TEST_REQUIRE(ret.IsArray() == true);
        TEST_REQUIRE(ret.GetName() == "foo");
        TEST_REQUIRE(ret.GetType() == game::ScriptVar::Type::Color);
        TEST_REQUIRE(ret.GetArray<game::Color4f>()[0] == data[0]);
        TEST_REQUIRE(ret.GetArray<game::Color4f>()[1] == data[1]);

    }

    {
        std::vector<bool> data;
        data.push_back(true);
        data.push_back(false);
        game::ScriptVar var("foo", data, false);

        data::JsonObject json;
        var.IntoJson(json);
        game::ScriptVar ret;

        ret.FromJson(json);
        TEST_REQUIRE(ret.FromJson(json));
        TEST_REQUIRE(ret.GetArray<bool>()[0] == data[0]);
        TEST_REQUIRE(ret.GetArray<bool>()[1] == data[1]);

    }

    {
        std::vector<game::ScriptVar::EntityReference> refs;
        refs.push_back({"1234"});
        refs.push_back({"abasbs"});
        game::ScriptVar var("foo", refs, false);

        data::JsonObject json;
        var.IntoJson(json);

        game::ScriptVar ret;
        TEST_REQUIRE(ret.FromJson(json));
        TEST_REQUIRE(ret.GetArray<game::ScriptVar::EntityReference>()[0].id == refs[0].id);
        TEST_REQUIRE(ret.GetArray<game::ScriptVar::EntityReference>()[1].id == refs[1].id);
    }

    {
        std::vector<game::ScriptVar::MaterialReference> refs;
        refs.push_back({"1234"});
        refs.push_back({"abasbs"});
        game::ScriptVar var("foo", refs, false);

        data::JsonObject json;
        var.IntoJson(json);

        game::ScriptVar ret;
        TEST_REQUIRE(ret.FromJson(json));
        TEST_REQUIRE(ret.GetArray<game::ScriptVar::MaterialReference>()[0].id == refs[0].id);
        TEST_REQUIRE(ret.GetArray<game::ScriptVar::MaterialReference>()[1].id == refs[1].id);

    }

    {

        TEST_REQUIRE(game::ScriptVar::SameSame(std::vector<int>{123},
                                               std::vector<int>{123}));
        TEST_REQUIRE(!game::ScriptVar::SameSame(std::vector<int>{123},
                                               std::vector<int>{321}));
    }
    {

        TEST_REQUIRE(game::ScriptVar::SameSame(std::vector<std::string>{"foobar"},
                                               std::vector<std::string>{"foobar"}));
        TEST_REQUIRE(!game::ScriptVar::SameSame(std::vector<std::string>{"foobar"},
                                                std::vector<std::string>{"doober"}));
    }

    {
        TEST_REQUIRE(game::ScriptVar::SameSame(std::vector<float>{123.0f},
                                               std::vector<float>{123.0f}));
        TEST_REQUIRE(!game::ScriptVar::SameSame(std::vector<float>{123.0f},
                                                std::vector<float>{321.0f}));
    }

    {
        std::vector<game::ScriptVar::MaterialReference> refs;
        refs.push_back({"1234"});
        refs.push_back({"abasbs"});
        TEST_REQUIRE(game::ScriptVar::SameSame(refs, refs));

        auto other = refs;
        other[1] = game::ScriptVar::MaterialReference {"keke"};
        TEST_REQUIRE(!game::ScriptVar::SameSame(refs, other));
    }
}
EXPORT_TEST_MAIN(
int test_main(int argc, char* argv[])
{
    test_script_var();

    return 0;
}
) // TEST_MAIN