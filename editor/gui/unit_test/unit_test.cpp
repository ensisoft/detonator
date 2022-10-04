// Copyright (C) 2020-2022 Sami Väisänen
// Copyright (C) 2020-2022 Ensisoft http://www.ensisoft.com
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
#include "warnpop.h"

#include "base/test_minimal.h"
#include "editor/gui/settings.h"

void unit_test_settings_json()
{
    enum class Fruits {
        Bananana, Kiwi, Watermelon
    };

    {
        gui::Settings s("gui-settings.json");
        s.SetValue("foo", "string", QString("jeesus ajaa mopolla"));
        s.SetValue("foo", "int", int(123));
        s.SetValue("foo", "float", float(1.0f));
        s.SetValue("foo", "utf8-string", std::string("joo joo €€"));
        s.SetValue("foo", "uint64", size_t(0xffffffffffffffff));
        s.SetValue("foo", "fruit", Fruits::Kiwi);
        TEST_REQUIRE(s.Save());
    }

    {
        gui::Settings s("gui-settings.json");
        TEST_REQUIRE(s.Load());
        QString str;
        TEST_REQUIRE(s.GetValue("foo", "string", &str));
        TEST_REQUIRE(str == "jeesus ajaa mopolla");
        int value;
        TEST_REQUIRE(s.GetValue("foo", "int", &value));
        TEST_REQUIRE(value == 123);
        float flt;
        TEST_REQUIRE(s.GetValue("foo", "float", &flt));
        TEST_REQUIRE(flt == 1.0f);
        std::string utf8;
        TEST_REQUIRE(s.GetValue("foo", "utf8-string", &utf8));
        TEST_REQUIRE(utf8 == "joo joo €€");
        size_t val;
        TEST_REQUIRE(s.GetValue("foo", "uint64", &val));
        TEST_REQUIRE(val == size_t(0xffffffffffffffff));

        Fruits f;
        TEST_REQUIRE(s.GetValue("foo", "fruit", &f));
        TEST_REQUIRE(f == Fruits::Kiwi);
    }
}

int test_main(int argc, char* argv[])
{
    unit_test_settings_json();

    return 0;
}