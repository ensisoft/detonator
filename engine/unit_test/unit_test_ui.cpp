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
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include "base/test_help.h"
#include "base/test_minimal.h"

#include "engine/ui.h"
#include "engine/loader.h"

void unit_test_property_value()
{
    TEST_CASE(test::Type::Feature)

    {
        engine::UIProperty prop;
        TEST_REQUIRE(prop.HasValue() == false);

        prop.SetValue(123);

        std::string str;
        TEST_REQUIRE(!prop.GetValue(&str));
        TEST_REQUIRE(str.empty());

    }

    {
        engine::UIProperty prop;
        prop.SetValue(gfx::Color::Silver);
        TEST_REQUIRE(prop.HasValue());

        gfx::Color4f  color;
        TEST_REQUIRE(prop.GetValue(&color));
        TEST_REQUIRE(color == gfx::Color::Silver);

        gfx::Color val;
        TEST_REQUIRE(prop.GetValue(&val));
        TEST_REQUIRE(val == gfx::Color::Silver);
    }

    {
        engine::UIProperty prop;
        prop.SetValue("foobar");

        std::string str;
        TEST_REQUIRE(prop.GetValue(&str));
        TEST_REQUIRE(str == "foobar");
    }

    {
        engine::UIProperty prop;
        prop.SetValue(123);

        int value = 0;
        TEST_REQUIRE(prop.GetValue(&value));
        TEST_REQUIRE(value == 123);
    }

    {
        engine::UIProperty prop;
        prop.SetValue(123.0f);

        float value;
        TEST_REQUIRE(prop.GetValue(&value));
        TEST_REQUIRE(value == 123.0f);
    }

    {
        engine::UIProperty prop;
        prop.SetValue(engine::UIStyle::WidgetShape::Rectangle);

        std::string str;
        TEST_REQUIRE(prop.GetValue(&str));
        TEST_REQUIRE(str == "Rectangle");

        engine::UIStyle::WidgetShape shape;
        TEST_REQUIRE(prop.GetValue(&shape));
        TEST_REQUIRE(shape == engine::UIStyle::WidgetShape::Rectangle);
    }
}

void unit_test_style()
{
    nlohmann::json json;

    {
        engine::UIStyle style;
        style.SetProperty("int", 123);
        style.SetProperty("float", 123.0f);
        style.SetProperty("string", std::string("foobar"));
        style.SetProperty("const char", "bollocks");
        style.SetProperty("shape", engine::UIStyle::WidgetShape::Circle);
        style.SetProperty("color", gfx::Color::Silver);

        TEST_REQUIRE(style.HasProperty("int"));
        TEST_REQUIRE(style.HasProperty("keke") == false);

        engine::UIProperty prop;
        prop = style.GetProperty("int");
        TEST_REQUIRE(prop.HasValue());
        TEST_REQUIRE(prop.GetValue<int>() == 123);

        prop = style.GetProperty("float");
        TEST_REQUIRE(prop.HasValue());
        TEST_REQUIRE(prop.GetValue<float>() == 123.0f);

        prop = style.GetProperty("string");
        TEST_REQUIRE(prop.HasValue());
        TEST_REQUIRE(prop.GetValue<std::string>() == "foobar");

        prop = style.GetProperty("const char");
        TEST_REQUIRE(prop.HasValue());
        TEST_REQUIRE(prop.GetValue<std::string>() == "bollocks");

        prop = style.GetProperty("shape");
        TEST_REQUIRE(prop.HasValue());
        TEST_REQUIRE(prop.GetValue<engine::UIStyle::WidgetShape>() == engine::UIStyle::WidgetShape::Circle);

        prop = style.GetProperty("color");
        TEST_REQUIRE(prop.HasValue());
        TEST_REQUIRE(prop.GetValue<gfx::Color4f>() == gfx::Color::Silver);
        TEST_REQUIRE(prop.GetValue<gfx::Color>() == gfx::Color::Silver);
        TEST_REQUIRE(prop.GetValue<std::string>() == "Silver");

        style.SaveStyle(json);
    }

    {
        engine::UIStyle style;
        TEST_REQUIRE(style.LoadStyle(json));

        engine::UIProperty prop;
        prop = style.GetProperty("int");
        TEST_REQUIRE(prop.HasValue());
        TEST_REQUIRE(prop.GetValue<int>() == 123);

        prop = style.GetProperty("float");
        TEST_REQUIRE(prop.HasValue());
        TEST_REQUIRE(prop.GetValue<float>() == 123.0f);

        prop = style.GetProperty("string");
        TEST_REQUIRE(prop.HasValue());
        TEST_REQUIRE(prop.GetValue<std::string>() == "foobar");

        prop = style.GetProperty("const char");
        TEST_REQUIRE(prop.HasValue());
        TEST_REQUIRE(prop.GetValue<std::string>() == "bollocks");

        prop = style.GetProperty("shape");
        TEST_REQUIRE(prop.HasValue());
        TEST_REQUIRE(prop.GetValue<engine::UIStyle::WidgetShape>() == engine::UIStyle::WidgetShape::Circle);

        prop = style.GetProperty("color");
        TEST_REQUIRE(prop.HasValue());
        TEST_REQUIRE(prop.GetValue<gfx::Color4f>() == gfx::Color::Silver);
        TEST_REQUIRE(prop.GetValue<gfx::Color>() == gfx::Color::Silver);
        TEST_REQUIRE(prop.GetValue<std::string>() == "Silver");
    }

}


EXPORT_TEST_MAIN(
int test_main(int argc, char* argv[])
{
    unit_test_property_value();
    unit_test_style();

    return 0;
}
) // TEST_MAIN