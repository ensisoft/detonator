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
#include "base/test_help.h"
#include "base/memory.h"

#if !defined(UNIT_TEST_BUNDLE)
  #include "base/assert.cpp"
  #include "base/utility.cpp"
#endif

void unit_test_detail()
{
    TEST_CASE(test::Type::Feature);

    {
        mem::detail::HeapAllocator allocator(1024);
        TEST_REQUIRE(allocator.MapMem(0));

        mem::detail::HeapAllocator other(std::move(allocator));
        TEST_REQUIRE(allocator.MapMem(0) == nullptr);

        mem::detail::HeapAllocator foo(1024);
        foo = std::move(other);
        TEST_REQUIRE(other.MapMem(0) == nullptr);
    }

    {
        struct Foobar {
            std::string foobar;
            unsigned value;
        };

        using PoolAllocator = mem::detail::MemoryPoolAllocator<mem::detail::HeapAllocator, sizeof(Foobar)>;

        PoolAllocator::AllocHeader alloc;

        PoolAllocator allocator(1024);
        TEST_REQUIRE(allocator.Allocate(&alloc));

        PoolAllocator other(std::move(allocator));
        TEST_REQUIRE(other.Allocate(&alloc));

        PoolAllocator tmp(512);
        tmp = std::move(other);
        TEST_REQUIRE(tmp.Allocate(&alloc));
    }
}

struct PtrTestType {
    static unsigned Counter;
    PtrTestType(const PtrTestType&) = delete;
    PtrTestType& operator=(const PtrTestType&) = delete;
    PtrTestType()
    {
        ++Counter;
    }
    ~PtrTestType()
    {
        TEST_REQUIRE(Counter > 0);
        --Counter;
    }
    int value = 0;
};
unsigned PtrTestType::Counter = 0;

void unit_test_ptr()
{
    TEST_CASE(test::Type::Feature);

    {
        mem::UniquePtr<PtrTestType, mem::StandardAllocatorTag> ptr;
        TEST_REQUIRE(!ptr);
        TEST_REQUIRE(ptr.get() == nullptr);
    }

    {
        auto foobar = mem::make_unique<PtrTestType, mem::StandardAllocatorTag>();
        foobar->value = 123;
        TEST_REQUIRE(foobar);
        TEST_REQUIRE(foobar.get());
        TEST_REQUIRE(PtrTestType::Counter == 1);
    }
    TEST_REQUIRE(PtrTestType::Counter == 0);

    // move ctor
    {
        auto foobar = mem::make_unique<PtrTestType, mem::StandardAllocatorTag>();

        mem::UniquePtr<PtrTestType, mem::StandardAllocatorTag> other(std::move(foobar));
        TEST_REQUIRE(other);
        TEST_REQUIRE(other.get());
        TEST_REQUIRE(foobar.get() == nullptr);
        TEST_REQUIRE(PtrTestType::Counter == 1);
        other.reset();
        TEST_REQUIRE(PtrTestType::Counter == 0);
    }
    // move assignment
    {
        auto foobar = mem::make_unique<PtrTestType, mem::StandardAllocatorTag>();
        auto other = mem::make_unique<PtrTestType, mem::StandardAllocatorTag>();
        TEST_REQUIRE(PtrTestType::Counter == 2);
        other = std::move(foobar);
        TEST_REQUIRE(PtrTestType::Counter == 1);
        other.reset();
        TEST_REQUIRE(PtrTestType::Counter == 0);
    }
}

struct Entity {
    std::string string;
    int value = 0;
};

struct EntityPoolTag {};
struct EntityStackTag {};

struct EntityPerfTestPoolTag {};
struct EntityPerfTestStackTag {};

namespace mem {
    template<>
    struct AllocatorInstance<EntityPoolTag> {
        static mem::MemoryPool<Entity>& Get() noexcept {
            using AllocatorType = mem::MemoryPool<Entity>;
            // for testing purposes, the memory pool uses 1 item per pool
            // for a total of 16 items spread over 16 pools
            static AllocatorType allocator(1);
            return allocator;
        }
    };

    template<>
    struct AllocatorInstance<EntityStackTag> {
        static mem::BumpAllocator<Entity>& Get() noexcept {
            using AllocatorType = mem::BumpAllocator<Entity>;
            // space for 1024 entities.
            static AllocatorType stack (1024);
            return stack;
        }
    };

    template<>
    struct AllocatorInstance<EntityPerfTestPoolTag> {
        static mem::MemoryPool<Entity>& Get() noexcept {
            using AllocatorType = mem::MemoryPool<Entity>;
            // for testing purposes, the memory pool uses 1 item per pool
            // for a total of 16 items spread over 16 pools
            static AllocatorType allocator(1000);
            return allocator;
        }
    };

    template<>
    struct AllocatorInstance<EntityPerfTestStackTag> {
        static mem::BumpAllocator<Entity>& Get() noexcept {
            using AllocatorType = mem::BumpAllocator<Entity>;
            // for testing purposes, the memory pool uses 1 item per pool
            // for a total of 16 items spread over 16 pools
            static AllocatorType stack(1000);
            return stack;
        }
    };

} // namespace

mem::MemoryPool<Entity>& GetEntityPool()
{
    return mem::AllocatorInstance<EntityPoolTag>::Get();
}

mem::BumpAllocator<Entity>& GetEntityStack()
{
    return mem::AllocatorInstance<EntityStackTag>::Get();
}

mem::UniquePtr<Entity, EntityPoolTag> CreateEntity()
{
    return mem::make_unique<Entity, EntityPoolTag>();
}

mem::UniquePtr<Entity, EntityStackTag> CreateStackEntity()
{
    return mem::make_unique<Entity, EntityStackTag>();
}

void unit_test_pool()
{
    TEST_CASE(test::Type::Feature);

    TEST_REQUIRE(GetEntityPool().GetAllocCount() == 0);
    TEST_REQUIRE(GetEntityPool().GetFreeCount() == 16); // 16 x 1 item pool

    std::vector<mem::UniquePtr<Entity, EntityPoolTag>> entities;
    for (size_t i=0; i<16; ++i)
    {
        auto entity = CreateEntity();
        entity->value  = i;
        entity->string = std::to_string(i);
        entities.push_back(std::move(entity));
    }
    TEST_REQUIRE(GetEntityPool().GetAllocCount() == 16);
    TEST_REQUIRE(GetEntityPool().GetFreeCount() == 0);
    TEST_REQUIRE(GetEntityPool().Allocate(sizeof(Entity)) == nullptr);

    // access all entities and their memory
    for (size_t i=0; i<entities.size(); ++i)
    {
        const auto& entity = entities[i];
        TEST_REQUIRE(entity->value == i);
        TEST_REQUIRE(entity->string == std::to_string(i));
    }

    // make space for one more by deleting the last.
    entities.pop_back();
    TEST_REQUIRE(GetEntityPool().GetAllocCount() == 15);
    TEST_REQUIRE(GetEntityPool().GetFreeCount() == 1);

    // this is the new guy
    auto entity = CreateEntity();
    entity->value = 77777;
    entity->string = "string value";

    // access all previously created entities
    for (size_t i=0; i<entities.size()-1; ++i)
    {
        auto& entity = entities[i];
        TEST_REQUIRE(entity->value == i);
        TEST_REQUIRE(entity->string == std::to_string(i));
        entity->value = 1;
        entity->string = "keke";
    }
    TEST_REQUIRE(entity->value == 77777);
    TEST_REQUIRE(entity->string == "string value");

    TEST_REQUIRE(GetEntityPool().GetAllocCount() == 16);
    TEST_REQUIRE(GetEntityPool().GetFreeCount() == 0);

    entities.clear();
    TEST_REQUIRE(GetEntityPool().GetAllocCount() == 1);
    TEST_REQUIRE(GetEntityPool().GetFreeCount() == 15);
    entity.reset();
    TEST_REQUIRE(GetEntityPool().GetAllocCount() == 0);
    TEST_REQUIRE(GetEntityPool().GetFreeCount() == 16);

    // scramble the allocation/de-allocation order
    std::unordered_map<int, mem::UniquePtr<Entity, EntityPoolTag>> entity_map;

    for (size_t i=0; i<16; ++i)
    {
        auto entity = CreateEntity();
        entity->value  = i;
        entity->string = std::to_string(i);
        entity_map[i] = std::move(entity);
    }

    entity_map.erase(10);
    entity_map.erase(0);
    entity_map.erase(9);
    entity_map.erase(5);
    entity_map.erase(2);
    entity_map.erase(13);
    entity_map.erase(1);
    entity_map.erase(8);
    entity_map.erase(12);
    entity_map.erase(3);
    entity_map.erase(7);
    entity_map.erase(4);
    entity_map.erase(6);
    entity_map.erase(11);
    entity_map.erase(14);
    entity_map.erase(15);
    TEST_REQUIRE(GetEntityPool().GetAllocCount() == 0);
    TEST_REQUIRE(GetEntityPool().GetFreeCount() == 16);

    for (size_t i=0; i<16; ++i)
    {
        auto entity = CreateEntity();
        entity->value  = i;
        entity->string = std::to_string(i);
        entity_map[i] = std::move(entity);
    }
}

void unit_test_bump()
{
    TEST_CASE(test::Type::Feature);

    TEST_REQUIRE(GetEntityStack().GetCapacity() == 1024);
    TEST_REQUIRE(GetEntityStack().GetSize() == 0);

    std::vector<mem::UniquePtr<Entity, EntityStackTag>> vec;
    for (size_t i=0; i<1024; ++i)
    {
        auto entity = CreateStackEntity();
        entity->value = i;
        entity->string = std::to_string(i);
        vec.push_back(std::move(entity));
    }
    TEST_REQUIRE(GetEntityStack().GetCapacity() == 0);
    TEST_REQUIRE(GetEntityStack().GetSize() == 1024);
    vec.clear();
    GetEntityStack().Reset();
    TEST_REQUIRE(GetEntityStack().GetCapacity() == 1024);
    TEST_REQUIRE(GetEntityStack().GetSize() == 0);
}

// do some tests comparing different allocation strategies
void measure_allocation_times()
{
    TEST_CASE(test::Type::Other)

    // standard new
    {
        std::vector<std::unique_ptr<Entity>> entities;
        entities.resize(1000);

        auto ret = test::TimedTest(10000, [&entities]() {
            for (size_t i = 0; i < 1000; ++i)
            {
                auto instance = std::make_unique<Entity>();
                entities[i] = std::move(instance);
            }
        });
        test::PrintTestTimes("std alloc", ret);
    }

    // using memory pool + heap
    {
        std::vector<mem::unique_ptr<Entity, EntityPerfTestPoolTag>> entities;
        entities.resize(1000);

        auto ret = test::TimedTest(10000, [&entities]() {
            for (size_t i = 0; i < 1000; ++i)
            {
                auto instance = mem::make_unique<Entity, EntityPerfTestPoolTag>();
                entities[i] = std::move(instance);
            }
        });
        test::PrintTestTimes("pool", ret);
    }

    // using stack allocator
    {
        std::vector<mem::unique_ptr<Entity, EntityPerfTestStackTag>> entities;
        entities.resize(1000);

        auto ret = test::TimedTest(10000, [&entities]() {
            for (size_t i = 0; i < 1000; ++i)
            {
                auto instance = mem::make_unique<Entity, EntityPerfTestStackTag>();
                entities[i] = std::move(instance);
            }
            mem::AllocatorInstance<EntityPerfTestStackTag>::Get().Reset();
        });
        test::PrintTestTimes("stack", ret);
    }
}

struct RefCountEntity : public mem::RefBase {
    std::string name;
    glm::vec2 position;
    float rotation = 0.0f;

    RefCountEntity(const Entity&) = delete;
    RefCountEntity& operator=(const Entity&) = delete;
    RefCountEntity() noexcept
    {
        ++Counter;
    }
   ~RefCountEntity() noexcept
    {
        TEST_REQUIRE(Counter > 0);
        --Counter;
    }
    static uint32_t Counter;
};

uint32_t RefCountEntity::Counter = 0;


void unit_test_refcount_lifecycle()
{

    TEST_CASE(test::Type::Feature)

    mem::SharedPtr<RefCountEntity> entity(new RefCountEntity);
    TEST_REQUIRE(entity);

    {
        TEST_REQUIRE(entity->GetRefCount() == 1);
        TEST_REQUIRE(RefCountEntity::Counter == 1);
    }

    // copy ctor
    {
        mem::SharedPtr<RefCountEntity> copy(entity);
        TEST_REQUIRE(entity->GetRefCount() == 2);
        TEST_REQUIRE(RefCountEntity::Counter == 1);
    }

    // assignment
    {
        mem::SharedPtr<RefCountEntity> copy;
        copy = entity;
        TEST_REQUIRE(entity->GetRefCount() == 2);
        TEST_REQUIRE(RefCountEntity::Counter == 1);
    }

    // assignment to self
    {
        entity = entity;
        TEST_REQUIRE(entity->GetRefCount() == 1);
        TEST_REQUIRE(RefCountEntity::Counter == 1);
    }

    entity.reset();
    TEST_REQUIRE(!entity);
    TEST_REQUIRE(RefCountEntity::Counter == 0);
}

void measure_refcount_pointer_times()
{
    TEST_CASE(test::Type::Other)

    // std::shared_ptr
    {
        auto entity = std::make_shared<RefCountEntity>();
        std::vector<std::shared_ptr<RefCountEntity>> vector;
        vector.resize(1000);

        const auto ret = test::TimedTest(10000, [&vector, &entity]() {
            // copy the same entity ref into the vector.
            for (unsigned i = 0; i < 1000; ++i)
            {
                vector[i] = entity;
            }
        });
        // side effect
        for (auto& shared_ptr: vector)
            test::DevNull("%p", shared_ptr.get());

        test::PrintTestTimes("std::shared_ptr", ret);
    }

    // mem::shared_ptr
    {
        mem::shared_ptr<RefCountEntity> entity(new RefCountEntity);
        std::vector<mem::shared_ptr<RefCountEntity>> vector;
        vector.resize(1000);

        const auto ret = test::TimedTest(10000, [&vector, &entity]() {
            // copy the same entity ref into the vector.
            for (unsigned i = 0; i < 1000; ++i)
            {
                vector[i] = entity;
            }
        });
        // side effect
        for (auto& shared_ptr: vector)
            test::DevNull("%p", shared_ptr.get());

        test::PrintTestTimes("mem::shared_ptr", ret);
    }
}


EXPORT_TEST_MAIN(
int test_main(int argc, char* argv[])
{
    unit_test_detail();
    unit_test_ptr();
    unit_test_pool();
    unit_test_bump();

    unit_test_refcount_lifecycle();

    measure_allocation_times();
    measure_refcount_pointer_times();
    return 0;
}
) // TEST_MAIN