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
#include "base/math.h"

#if !defined(UNIT_TEST_BUNDLE)
#  include "base/assert.cpp"
#  include "base/utility.cpp"
#endif

struct Entity {
    std::string name;
    std::size_t value;
    base::FRect rect;
};

void unit_test_grid_mapping()
{
    TEST_CASE(test::Type::Feature)

    {
        base::DenseSpatialGrid<Entity*> grid(100.0f, 100.0f, 10, 10);
        TEST_REQUIRE(grid.MapRect(base::FRect(0.0f, 0.0f, 10.0f, 10.0f)) == base::URect(0, 0, 1, 1));
        TEST_REQUIRE(grid.MapRect(base::FRect(0.0f, 0.0f, 14.0f, 10.0f)) == base::URect(0, 0, 2, 1));
        TEST_REQUIRE(grid.MapRect(base::FRect(0.0f, 0.0f, 10.0f, 14.0f)) == base::URect(0, 0, 1, 2));
        TEST_REQUIRE(grid.MapRect(base::FRect(0.0f, 0.0f, 100.0f, 100.0f)) == base::URect(0, 0, 10, 10));
        TEST_REQUIRE(grid.MapRect(base::FRect(91.0f, 91.0f, 5.0f, 5.0f)) == base::URect(9, 9, 1, 1));

        TEST_REQUIRE(grid.MapPoint(base::FPoint(0.0f, 0.0f)) == base::UPoint(0, 0));
        TEST_REQUIRE(grid.MapPoint(base::FPoint(91.0f, 91.0f)) == base::UPoint(9, 9));
    }

    {
        base::DenseSpatialGrid<Entity*> grid(base::FRect(-50.0f, -50.0f, 100.0f, 100.0f), 10, 10);
        TEST_REQUIRE(grid.MapRect(base::FRect(-50.0f, -50.0f, 10.0f, 10.0f)) == base::URect(0, 0, 1, 1));
        TEST_REQUIRE(grid.MapRect(base::FRect(-50.0f, -50.0f, 14.0f, 10.0f)) == base::URect(0, 0, 2, 1));
        TEST_REQUIRE(grid.MapRect(base::FRect(-50.0f, -50.0f, 10.0f, 14.0f)) == base::URect(0, 0, 1, 2));
        TEST_REQUIRE(grid.MapRect(base::FRect(-50.0f, -50.0f, 100.0f, 100.0f)) == base::URect(0, 0, 10, 10));
        TEST_REQUIRE(grid.MapRect(base::FRect(41.0f, 41.0f, 5.0f, 5.0f)) == base::URect(9, 9, 1, 1));

        TEST_REQUIRE(grid.MapPoint(base::FPoint(-50.0f, -50.0f)) == base::UPoint(0, 0));
        TEST_REQUIRE(grid.MapPoint(base::FPoint(41.0f, 41.0f)) == base::UPoint(9, 9));
    }

    {
        base::DenseSpatialGrid<Entity*> grid(base::FRect(50.0f, 50.0f, 100.0f, 100.0f), 10, 10);
        TEST_REQUIRE(grid.MapRect(base::FRect(50.0f, 50.0f, 10.0f, 10.0f)) == base::URect(0, 0, 1, 1));
        TEST_REQUIRE(grid.MapRect(base::FRect(50.0f, 50.0f, 14.0f, 10.0f)) == base::URect(0, 0, 2, 1));
        TEST_REQUIRE(grid.MapRect(base::FRect(50.0f, 50.0f, 10.0f, 14.0f)) == base::URect(0, 0, 1, 2));
        TEST_REQUIRE(grid.MapRect(base::FRect(50.0f, 50.0f, 100.0f, 100.0f)) == base::URect(0, 0, 10, 10));
        TEST_REQUIRE(grid.MapRect(base::FRect(141.0f, 141.0f, 5.0f, 5.0f)) == base::URect(9, 9, 1, 1));

        TEST_REQUIRE(grid.MapPoint(base::FPoint(50.0f, 50.0f)) == base::UPoint(0, 0));
        TEST_REQUIRE(grid.MapPoint(base::FPoint(141.0f, 141.0f)) == base::UPoint(9, 9));
    }

}

void unit_test_insert_query()
{
    TEST_CASE(test::Type::Feature);

    // basics
    {
        // 2x2 grid with each cell being 50x50 units.
        base::DenseSpatialGrid<Entity*> grid(100.0f, 100.0f, 2, 2);
        TEST_REQUIRE(grid.GetNumCols() == 2);
        TEST_REQUIRE(grid.GetNumRows() == 2);
        TEST_REQUIRE(grid.GetNumItems() == 0);
        TEST_REQUIRE(grid.GetNumItems(0, 0) == 0);
        TEST_REQUIRE(grid.GetNumItems(1, 0) == 0);
        TEST_REQUIRE(grid.GetNumItems(0, 1) == 0);
        TEST_REQUIRE(grid.GetNumItems(1, 1) == 0);
        TEST_REQUIRE(grid.GetRect() == base::FRect(0.0f, 0.0f, 100.0f, 100.0f));
        TEST_REQUIRE(grid.GetRect(0, 0) == base::FRect(0.0f, 0.0f, 50.0f, 50.0f));
        TEST_REQUIRE(grid.GetRect(0, 1) == base::FRect(50.0f, 0.0f, 50.0f, 50.0f));
        TEST_REQUIRE(grid.GetRect(1, 0) == base::FRect(0.0f,  50.0f, 50.0f, 50.0f));
        TEST_REQUIRE(grid.GetRect(1, 1) == base::FRect(50.0f, 50.0f, 50.0f, 50.0f));

        std::vector<Entity*> ret;
        grid.Find(base::FRect(0.0f, 0.0f, 100.0f, 100.0f), &ret);
        TEST_REQUIRE(ret.empty());

        grid.Find(base::FPoint(0.0f, 0.0f), &ret);
        TEST_REQUIRE(ret.empty());

        grid.Clear();
        TEST_REQUIRE(grid.GetNumItems()== 0);
        TEST_REQUIRE(grid.GetNumRows() == 2);
        TEST_REQUIRE(grid.GetNumCols() == 2);
    }

    // basic insert + gets
    {
        base::DenseSpatialGrid<Entity*> grid(100.0f, 100.0f, 2, 2);

        // inserting outside the grid is safe but results in no-insert
        Entity e;
        TEST_REQUIRE(grid.Insert(base::FRect(101.0f, 0.0f, 20.0f, 20.0f), &e) == false);
        TEST_REQUIRE(grid.GetNumItems() == 0);
        TEST_REQUIRE(grid.Insert(base::FRect(0.0f, 101.0f, 20.0f, 20.0f), &e) == false);
        TEST_REQUIRE(grid.GetNumItems() == 0);

        // insert into the cell at 0,0
        TEST_REQUIRE(grid.Insert(base::FRect(0.0f, 0.0f, 20.0f, 20.0f), &e));
        TEST_REQUIRE(grid.GetNumItems(0, 0) == 1);
        TEST_REQUIRE(grid.GetObject(0, 0, 0) == &e);

        // insert into the cell at 1,1
        Entity e2;
        TEST_REQUIRE(grid.Insert(base::FRect(51.0f, 51.0f, 20.0f, 20.0f), &e2));
        TEST_REQUIRE(grid.GetNumItems(0, 0) == 1);
        TEST_REQUIRE(grid.GetNumItems(1, 1) == 1);
        TEST_REQUIRE(grid.GetNumItems() == 2);
        TEST_REQUIRE(grid.GetObject(1, 1, 0) == &e2);

        // insert another item into the cell at 1,1
        Entity e3;
        TEST_REQUIRE(grid.Insert(base::FRect(51.0f, 51.0f, 20.0f, 20.0f), &e3));
        TEST_REQUIRE(grid.GetNumItems(0, 0) == 1);
        TEST_REQUIRE(grid.GetNumItems(1, 1) == 2);
        TEST_REQUIRE(grid.GetNumItems() == 3);
        TEST_REQUIRE(grid.GetObject(1, 1, 0) == &e2);
        TEST_REQUIRE(grid.GetObject(1, 1, 1) == &e3);

        // insert object that spans multiple cells column-wise
        Entity e4;
        TEST_REQUIRE(grid.Insert(base::FRect(0.0f, 0.0f, 51.0f, 20.0f), &e4));
        TEST_REQUIRE(grid.GetNumItems(0, 0) == 2);
        TEST_REQUIRE(grid.GetNumItems(0, 1) == 1);
        TEST_REQUIRE(grid.GetObject(0, 0, 0) == &e);
        TEST_REQUIRE(grid.GetObject(0, 0, 1) == &e4);
        TEST_REQUIRE(grid.GetObject(0, 1, 0) == &e4);

        // insert object that spans multiple cells row wise
        Entity e5;
        TEST_REQUIRE(grid.Insert(base::FRect(0.0f, 0.0f, 20.0f, 51.0f), &e5));
        TEST_REQUIRE(grid.GetNumItems(0, 0) == 3);
        TEST_REQUIRE(grid.GetNumItems(0, 1) == 1);
        TEST_REQUIRE(grid.GetNumItems(1, 0) == 1);

        grid.Clear();
        TEST_REQUIRE(grid.GetNumItems(0, 0) == 0);
        TEST_REQUIRE(grid.GetNumItems(1, 1) == 0);
        TEST_REQUIRE(grid.GetNumItems() == 0);
    }

    // rectangle queries.

    // query empty cell + query everything + query outside the partitioned space.
    {
        base::DenseSpatialGrid<Entity*> grid(100.0f, 100.0f, 2, 2);

        Entity e;
        grid.Insert(base::FRect(10.0f, 10.0f, 20.0f , 20.0f), &e);
        TEST_REQUIRE(grid.GetNumItems(0, 0) == 1);

        std::vector<Entity*> ret;
        grid.Find(grid.GetRect(0, 1), &ret);
        TEST_REQUIRE(ret.empty());
        grid.Find(grid.GetRect(1, 0), &ret);
        TEST_REQUIRE(ret.empty());
        grid.Find(grid.GetRect(1, 1), &ret);
        TEST_REQUIRE(ret.empty());

        grid.Find(grid.GetRect(), &ret);
        TEST_REQUIRE(ret.size() == 1);
        TEST_REQUIRE(ret[0] == &e);

        ret.clear();
        grid.Find(base::FRect(150.0f, 150.0f, 10.0f, 10.0f), &ret);
        TEST_REQUIRE(ret.empty());
    }

    // query single object within since cell
    {
        base::DenseSpatialGrid<Entity*> grid(100.0f, 100.0f, 2, 2);

        Entity e;
        grid.Insert(base::FRect(10.0f, 10.0f, 20.0f , 20.0f), &e);

        std::vector<Entity*> ret;
        grid.Find(grid.GetRect(0, 0), &ret);
        TEST_REQUIRE(ret.size() == 1);
        TEST_REQUIRE(ret[0] == &e);

        {
            std::vector<Entity*> ret;
            grid.Find(base::FPoint(0.0f, 0.0f), &ret);
            TEST_REQUIRE(ret.empty());
        }
        {
            std::vector<Entity*> ret;
            grid.Find(base::FPoint(14.0f, 15.0f), &ret);
            TEST_REQUIRE(ret.size() == 1);
            TEST_REQUIRE(ret[0] == &e);
        }
    }

    // query single object over multiple cells
    {
        base::DenseSpatialGrid<Entity*> grid(100.0f, 100.0f, 2, 2);

        Entity e;
        grid.Insert(base::FRect(10.0f, 10.0f, 50.0f , 20.0f), &e);

        std::vector<Entity*> ret;
        grid.Find(grid.GetRect(0, 0), &ret);
        TEST_REQUIRE(ret.size() == 1);
        TEST_REQUIRE(ret[0] == &e);

        ret.clear();
        grid.Find(grid.GetRect(0, 1), &ret);
        TEST_REQUIRE(ret.size() == 1);
        TEST_REQUIRE(ret[0] == &e);

    }

    // query multiple objects within a single cell
    {
        base::DenseSpatialGrid<Entity*> grid(100.0f, 100.0f, 2, 2);

        Entity e1;
        grid.Insert(base::FRect(10.0f, 10.0f, 50.0f , 20.0f), &e1);

        Entity e2;
        grid.Insert(base::FRect(5.0f, 5.0f, 20.0f , 20.0f), &e2);

        std::vector<Entity*> ret;
        grid.Find(grid.GetRect(0, 0), &ret);
        TEST_REQUIRE(ret.size() == 2);
        TEST_REQUIRE(ret[0] == &e1);
        TEST_REQUIRE(ret[1] == &e2);
    }

    {
        base::DenseSpatialGrid<Entity*> grid(100.0f, 100.0f, 1, 1);

        Entity e;
        grid.Insert(base::FRect(50.0f, 50.0f, 25.0f, 25.0f), &e);
        TEST_REQUIRE(grid.GetNumItems(0, 0) == 1);

        std::vector<Entity*> ret;
        grid.Find(base::FRect(0.0f, 0.0f, 49.0f, 49.0f), &ret);
        TEST_REQUIRE(ret.empty());

        grid.Find(base::FRect(0.0f, 0.0f, 51.0f, 51.0f), &ret);
        TEST_REQUIRE(ret.size() == 1);
        ret.clear();

        grid.Find(base::FRect(76.0f, 76.0f, 20.0f, 20.0f), &ret);
        TEST_REQUIRE(ret.empty());

    }

    {
        base::DenseSpatialGrid<Entity*> grid(base::FRect(-420.0f, -150.0f, 840.0f, 40.0f), 21, 25);

        Entity e;
        grid.Insert(base::FRect(380.0f, -130.0f, 40.0f, 20.0f), &e);

    }

    // point queries.

    {
        base::DenseSpatialGrid<Entity*> grid(100.0f, 100.0f, 2, 2);

        Entity e;
        grid.Insert(base::FRect(10.0f, 10.0f, 20.0f , 20.0f), &e);
        TEST_REQUIRE(grid.GetNumItems(0, 0) == 1);

        std::vector<Entity*> ret;
        grid.Find(base::FPoint(0.0f, 0.0f), &ret);
        TEST_REQUIRE(ret.empty());
        grid.Find(base::FPoint(-10.0f, 0.0f), &ret);
        TEST_REQUIRE(ret.empty());
        grid.Find(base::FPoint(0.0f, 101.0f), &ret);
        TEST_REQUIRE(ret.empty());

        grid.Find(base::FPoint(10.0f, 10.0f), &ret);
        TEST_REQUIRE(ret.size() == 1);
        TEST_REQUIRE(ret[0] == &e);

        ret.clear();
        grid.Find(base::FPoint(30.0f, 30.0f), &ret);
        TEST_REQUIRE(ret.size() == 1);
        TEST_REQUIRE(ret[0] == &e);

        // outside the space partition
        ret.clear();
        grid.Find(base::FPoint(150.0f, 150.0f), 10.0f, &ret);
        TEST_REQUIRE(ret.empty());

    }

    // erase nothing
    {
        base::DenseSpatialGrid<Entity*> grid(100.0f, 100.0f, 2, 2);

        Entity e;
        grid.Insert(base::FRect(10.0f, 10.0f, 20.0f , 20.0f), &e);

        grid.Erase([](Entity* entity, const base::FRect& rect) {
            return false;
        });
        TEST_REQUIRE(grid.GetNumItems(0, 0) == 1);
    }

    // erase single object within single cell
    {
        base::DenseSpatialGrid<Entity*> grid(100.0f, 100.0f, 2, 2);

        Entity e;
        grid.Insert(base::FRect(10.0f, 10.0f, 20.0f , 20.0f), &e);

        Entity e2;
        grid.Insert(base::FRect(60.0f, 60.0f, 20.0f , 20.0f), &e);
        grid.Erase(grid.GetRect(0, 0));
        TEST_REQUIRE(grid.GetNumItems(0, 0) == 0);
        TEST_REQUIRE(grid.GetNumItems(1, 1) == 1);
    }

    // erase single object over multiple cells
    {
        base::DenseSpatialGrid<Entity*> grid(100.0f, 100.0f, 2, 2);

        Entity e;
        grid.Insert(base::FRect(0.0f, 0.0f, 55.0f , 20.0f), &e);
        grid.Erase(grid.GetRect(0, 0));
        TEST_REQUIRE(grid.GetNumItems(0, 0) == 0);
        TEST_REQUIRE(grid.GetNumItems(0, 1) == 1);
    }

    // erase multiple objects within a single cell
    {
        base::DenseSpatialGrid<Entity*> grid(100.0f, 100.0f, 2, 2);

        Entity e1;
        grid.Insert(base::FRect(10.0f, 10.0f, 20.0f , 20.0f), &e1);
        Entity e2;
        grid.Insert(base::FRect(15.0f, 15.0f, 20.0f , 20.0f), &e2);

        grid.Erase(grid.GetRect(0, 0));
        TEST_REQUIRE(grid.GetNumItems(0, 0) == 0);
    }

    // erase objects by point test.
    {
        base::DenseSpatialGrid<Entity*> grid(100.0f, 100.0f, 2, 2);

        Entity e1;
        grid.Insert(base::FRect(10.0f, 10.0f, 20.0f , 20.0f), &e1);
        Entity e2;
        grid.Insert(base::FRect(15.0f, 15.0f, 20.0f , 20.0f), &e2);

        grid.Erase(base::FPoint(0.0f, 0.0f));
        TEST_REQUIRE(grid.GetNumItems() == 2);

        grid.Erase(base::FPoint(-10.0f, 0.0f));
        TEST_REQUIRE(grid.GetNumItems() == 2);

        grid.Erase(base::FPoint(110.0f, 0.0f));
        TEST_REQUIRE(grid.GetNumItems() == 2);

        grid.Erase(base::FPoint(11.0f, 11.0f));
        TEST_REQUIRE(grid.GetNumItems(0, 0) == 1);
        TEST_REQUIRE(grid.GetObject(0, 0, 0) == &e2);
    }

    {
        base::DenseSpatialGrid<Entity*> grid(100.0f, 100.0f, 10, 10);

        Entity e1;
        grid.Insert(base::FRect(90.0f, 90.0f, 10.0f, 10.0f), &e1);

        std::vector<Entity*> ret;
        grid.Find(base::FRect(50.0f, 50.0f, 50.0f, 50.0f), &ret);
        TEST_REQUIRE(ret.size() == 1);
    }


}

void measure_insert_query_perf()
{
    // this perf measurement is assuming that
    // a) there's going to be a large number of objects that get inserted
    //    repeatedly into the grid (i.e. every frame...)
    // b) there's a relatively small number of queries for objects.
    // This leads to a (possible) optimization strategy that optimizes for insertion
    // speed by delegating more intersection testing from the insertion to the query step.

    TEST_CASE(test::Type::Other);

    std::vector<Entity> entities;
    entities.resize(10000);

    math::RandomGenerator<float, 0x55234> r;

    const auto space_width  = 10000;
    const auto space_height = 10000;
    const base::FRect space_rect(0.0f, 0.0f, space_width, space_height);

    for (auto& e : entities)
    {
        const auto width  = r(0.0f, 20.0f);
        const auto height = r(0.0f, 30.0f);
        const auto y = r(0.0f, space_height-height);
        const auto x = r(0.0f, space_width-width);
        e.rect = base::FRect(x, y, width, height);
        TEST_REQUIRE(base::Contains(space_rect, e.rect));
    }

    std::vector<base::FRect> query_rects;
    for (size_t i=0; i<1000; ++i)
    {
        const auto width  = r(10, 20.0f);
        const auto height = r(0.0f, 30.0f);
        const auto y = r(0.0f, space_height-height);
        const auto x = r(.0f, space_width-width);
        query_rects.emplace_back(x, y, width, height);
    }

    base::DenseSpatialGrid<Entity*> grid(space_rect, 20, 20);
    std::vector<Entity*> query_ret;
    query_ret.reserve(10000);

    const auto& ret = test::TimedTest(1000, [&grid, &entities, &query_rects, &query_ret]() {
        grid.Clear();
        for (auto& e : entities) {
            grid.Insert(e.rect, &e);
        }

        for (const auto& qr : query_rects) {
            query_ret.clear();
            grid.Find(qr, &query_ret);
        }
    });
    test::PrintTestTimes("random insert+query 10k", ret);
}

EXPORT_TEST_MAIN(
int test_main(int argc, char* argv[])
{
    unit_test_grid_mapping();
    unit_test_insert_query();
    measure_insert_query_perf();
    return 0;
}
) // TEST_MAIN