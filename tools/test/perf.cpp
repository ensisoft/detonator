
// g++ -O3 -Wall perf.cpp  -I../../ -I. -o perf

#include <vector>
#include <unordered_map>
#include <string>

#include "base/utility.h"
#include "base/format.h"
#include "base/test_help.h"

#include "base/assert.cpp"
#include "base/format.cpp"
#include "base/utility.cpp"

struct Entity {
    std::string name;
    std::string id;
    float xpos;
    float ypos;
};

void test_iteration()
{
    std::vector<Entity> entities;
    entities.reserve(10000);
    for (size_t i=0; i<10000; ++i)
    {
        Entity e;
        e.name = base::FormatString("Entity %1", i);
        e.id   = base::RandomString(5);
        e.xpos = i;
        e.ypos = i;
        entities.push_back(std::move(e));
    }

    std::unordered_set<Entity*> set;
    std::unordered_map<std::string, Entity*> string_key_map;
    std::unordered_map<Entity*, Entity*> pointer_key_map;
    std::vector<Entity*> vec;

    for (auto& e : entities)
    {
        set.insert(&e);
        string_key_map.insert({e.id, &e});
        pointer_key_map.insert({&e, &e});
        vec.push_back(&e);
    }

    // iterate over set.
    {
        float result = 0.0f;
        const auto& ret = test::TimedTest(1000, [&set, &result]() {
            for (auto it = set.begin(); it != set.end(); ++it)
            {
                const Entity* e = *it;
                result += e->xpos;
            }
        });
        test::DevNull("%f", result);
        test::PrintTestTimes("Set Iteration", ret);
    }

    // iterate over key (string) -entity map
    {
        float result = 0.0f;
        const auto& ret = test::TimedTest(1000, [&string_key_map, &result]() {
            for (auto it = string_key_map.begin(); it != string_key_map.end(); ++it)
            {
                const Entity* e = it->second;
                result += e->xpos;
            }
        });
        test::DevNull("%f", result);
        test::PrintTestTimes("String-key map Iteration", ret);
    }

    // iterate over key (pointer) -entity map
    {
        float result = 0.0f;
        const auto& ret = test::TimedTest(1000, [&pointer_key_map, &result]() {
            for (auto it = pointer_key_map.begin(); it != pointer_key_map.end(); ++it)
            {
                const Entity* e = it->second;
                result += e->xpos;
            }
        });
        test::DevNull("%f", result);
        test::PrintTestTimes("Pointer-key map Iteration", ret);
    }

    // iterate over key (pointer) -entity map
    {
        float result = 0.0f;
        const auto& ret = test::TimedTest(1000, [&vec, &result]() {
            for (auto it = vec.begin(); it != vec.end(); ++it)
            {
                const Entity* e = *it;
                result += e->xpos;
            }
        });
        test::DevNull("%f", result);
        test::PrintTestTimes("Vector Iteration", ret);
    }

}

int test_main(int argc, char* argv[])
{
    test_iteration();
    return 0;
}



