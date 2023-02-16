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

#include <string>
#include <cstddef>

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "base/test_help.h"
#include "base/grid.h"

#include "base/assert.cpp"
#include "base/utility.cpp"

struct Entity {
    std::string name;
    std::size_t value;
};

void unit_test_insert_query()
{
    base::DenseSpatialGrid<Entity*> grid(200.0f, 100.0f, 10.0f, 20.0f);

    // test top left corner
    {

        Entity e;
        e.name  = "e0";
        e.value = 123;

        TEST_REQUIRE(grid.Insert(base::FRect(0.0f, 0.0f, 5.0f, 5.0f), &e));
        TEST_REQUIRE(grid.GetItemObject(0, 0, 0)->name == "e0");
        std::set<Entity*> result;
        grid.FindObjects(base::FRect(0.0f, 0.0f, 10.0f, 10.0f), &result);
        TEST_REQUIRE(base::Contains(result, &e));
        grid.Clear();
    }

    // test bottom right corner
    {
        Entity e;
        e.name = "e";
        TEST_REQUIRE(grid.Insert(base::FRect(190.0f, 90.0f, 10.0f, 10.0f), &e));
        std::set<Entity*> result;
        grid.FindObjects(base::FRect(190.0f, 90.0f, 10.0f, 10.0f), &result);
        TEST_REQUIRE(base::Contains(result, &e));
        grid.Clear();
    }

    // test object that covers multiple grid cells
    {
        Entity e;
        e.name = "e";
        TEST_REQUIRE(grid.Insert(base::FRect(0.0f, 0.0f, 20.0f, 20.0f), &e));
        std::set<Entity*> result;
        grid.FindObjects(base::FRect(0.0f, 0.0f, 10.0f, 10.0f), &result);
        TEST_REQUIRE(base::Contains(result, &e));
        result.clear();
        grid.FindObjects(base::FRect(10.0f, 10.0f, 10.0f, 10.0f), &result);
        TEST_REQUIRE(base::Contains(result, &e));
    }

    //
    {

    }
}

int test_main(int argc, char* argv[])
{
    unit_test_insert_query();

    return 0;
}