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

int test_main(int argc, char* argv[])
{
    {
        game::ScriptVar var("foo", std::string("blabla"), false);
        TEST_REQUIRE(var.IsReadOnly() == false);
        TEST_REQUIRE(var.IsArray() == false);
        TEST_REQUIRE(var.GetName() == "foo");
        TEST_REQUIRE(var.GetType() == game::ScriptVar::Type::String);
        TEST_REQUIRE(var.GetValue<std::string>() == "blabla");

        data::JsonObject json;
        var.IntoJson(json);

        auto ret = game::ScriptVar::FromJson(json);
        TEST_REQUIRE(ret.has_value());
        TEST_REQUIRE(ret->IsReadOnly() == false);
        TEST_REQUIRE(ret->IsArray() == false);
        TEST_REQUIRE(ret->GetName() == "foo");
        TEST_REQUIRE(ret->GetType() == game::ScriptVar::Type::String);
        TEST_REQUIRE(ret->GetValue<std::string>() == "blabla");
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

        auto ret = game::ScriptVar::FromJson(json);
        TEST_REQUIRE(ret.has_value());
        TEST_REQUIRE(ret->IsReadOnly() == false);
        TEST_REQUIRE(ret->IsArray() == true);
        TEST_REQUIRE(ret->GetName() == "foo");
        TEST_REQUIRE(ret->GetType() == game::ScriptVar::Type::Integer);
        TEST_REQUIRE(ret->GetArray<int>()[0] == 1);
        TEST_REQUIRE(ret->GetArray<int>()[1] == 2);
        TEST_REQUIRE(ret->GetArray<int>()[2] == 6);
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
        auto ret = game::ScriptVar::FromJson(json);
        TEST_REQUIRE(ret.has_value());
        TEST_REQUIRE(ret->IsReadOnly() == true);
        TEST_REQUIRE(ret->IsArray() == true);
        TEST_REQUIRE(ret->GetName() == "foo");
        TEST_REQUIRE(ret->GetType() == game::ScriptVar::Type::Vec2);
        TEST_REQUIRE(ret->GetArray<glm::vec2>()[0] == data[0]);
        TEST_REQUIRE(ret->GetArray<glm::vec2>()[1] == data[1]);

    }

    {
        std::vector<bool> data;
        data.push_back(true);
        data.push_back(false);
        game::ScriptVar var("foo", data, false);

        data::JsonObject json;
        var.IntoJson(json);
        auto ret = game::ScriptVar::FromJson(json);
        TEST_REQUIRE(ret->GetArray<bool>()[0] == data[0]);
        TEST_REQUIRE(ret->GetArray<bool>()[1] == data[1]);

    }

    return 0;
}