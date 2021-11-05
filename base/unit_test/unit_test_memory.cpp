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
#include <memory>
#include <vector>
#include <unordered_map>

#include "base/test_minimal.h"
#include "base/memory.h"

struct Foobar {
    int value;
    std::string string;
};
using FoobarAllocator = mem::HeapMemoryPool<sizeof(Foobar)>;
FoobarAllocator& GetFoobarAllocator(size_t pool)
{
    static FoobarAllocator allocator(pool);
    return allocator;
}

struct PoolDeleter {
    void operator()(Foobar* f)
    {
        auto& alloc = GetFoobarAllocator(0);

        f->~Foobar();

        alloc.Free((void*)f);
    }
};
struct BumpDeleter {
    void operator()(Foobar* f)
    {
        f->~Foobar();
    }
};

std::unique_ptr<Foobar, PoolDeleter> CreateFoobar(size_t max)
{
    auto& alloc = GetFoobarAllocator(max);

    void* mem = alloc.Allocate();
    if (!mem)
        return nullptr;

    return std::unique_ptr<Foobar, PoolDeleter>(new (mem) Foobar);
}

void unit_test_pool()
{
    std::vector<std::unique_ptr<Foobar, PoolDeleter>> foos;

    for (size_t i=0; i<10; ++i)
    {
        auto foobar = CreateFoobar(10);
        foobar->value = i * 100;
        foobar->string = "foobar: " + std::to_string(i);
        foos.push_back(std::move(foobar));
    }
    TEST_REQUIRE(GetFoobarAllocator(10).GetAllocCount() == 10);
    TEST_REQUIRE(GetFoobarAllocator(10).GetFreeCount() == 0);
    TEST_REQUIRE(GetFoobarAllocator(10).Allocate() == nullptr);

    for (size_t i=0; i<10; ++i)
    {
        const auto& foo = foos[i];
        TEST_REQUIRE(foo->value == i * 100);
        TEST_REQUIRE(foo->string == "foobar: " + std::to_string(i));
    }

    foos.pop_back();
    TEST_REQUIRE(GetFoobarAllocator(10).GetAllocCount() == 9);
    TEST_REQUIRE(GetFoobarAllocator(10).GetFreeCount() == 1);

    auto foo = CreateFoobar(10);
    foo->value = 77777;
    foo->string = "string value";

    for (size_t i=0; i<9; ++i)
    {
        const auto& foo = foos[i];
        TEST_REQUIRE(foo->value == i * 100);
        TEST_REQUIRE(foo->string == "foobar: " + std::to_string(i));
        foo->value = 1;
        foo->string = "keke";
    }
    TEST_REQUIRE(foo->value == 77777);
    TEST_REQUIRE(foo->string == "string value");

    TEST_REQUIRE(GetFoobarAllocator(10).GetAllocCount() == 10);
    TEST_REQUIRE(GetFoobarAllocator(10).GetFreeCount() == 0);

    foos.clear();
    TEST_REQUIRE(GetFoobarAllocator(10).GetAllocCount() == 1);
    TEST_REQUIRE(GetFoobarAllocator(10).GetFreeCount() == 9);
    foo.reset();
    TEST_REQUIRE(GetFoobarAllocator(10).GetAllocCount() == 0);
    TEST_REQUIRE(GetFoobarAllocator(10).GetFreeCount() == 10);

    // scramble the allocation/deallocation order
    std::unordered_map<int, std::unique_ptr<Foobar, PoolDeleter>> foo_map;

    for (size_t i=0; i<10; ++i)
    {
        auto foobar = CreateFoobar(10);
        foobar->value = i * 100;
        foobar->string = "foobar: " + std::to_string(i);
        foo_map[i] = std::move(foobar);
    }

    foo_map.erase(0);
    foo_map.erase(9);
    foo_map.erase(5);
    foo_map.erase(2);
    foo_map.erase(1);
    foo_map.erase(8);
    foo_map.erase(3);
    foo_map.erase(7);
    foo_map.erase(4);
    foo_map.erase(6);

    TEST_REQUIRE(GetFoobarAllocator(10).GetAllocCount() == 0);
    TEST_REQUIRE(GetFoobarAllocator(10).GetFreeCount() == 10);

    for (size_t i=0; i<10; ++i)
    {
        auto foobar = CreateFoobar(10);
        foobar->value = i * 100;
        foobar->string = "foobar: " + std::to_string(i);
        foo_map[i] = std::move(foobar);
    }
}

void unit_test_bump()
{
    mem::HeapBumpAllocator allocator(1024);

    std::vector<std::unique_ptr<Foobar, BumpDeleter>> vec;

    while (void* mem = allocator.Allocate(sizeof(Foobar)))
    {
        auto foo = std::unique_ptr<Foobar, BumpDeleter>(new (mem) Foobar);
        foo->string = "strigigigi";
        foo->value  = 1223433;
        vec.push_back(std::move(foo));
    }

}

int test_main(int argc, char* argv[])
{
    unit_test_pool();
    unit_test_bump();

    return 0;
}