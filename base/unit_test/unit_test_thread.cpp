// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#include <fstream>

#include "base/math.h"
#include "base/test_minimal.h"
#include "base/test_help.h"
#include "base/threadpool.h"
#include "base/logging.h"


void unit_test_pool()
{
    TEST_CASE(test::Type::Feature)

    std::atomic_int counter {0};

    class TestTask : public base::ThreadTask
    {
    public:
        using Affinity = base::ThreadTask::Affinity;

        TestTask(Affinity affinity, std::atomic_int& c)
          : base::ThreadTask(affinity, 0)
          , counter_(c)
        {}

    protected:
        virtual void DoTask() override
        {
            static math::RandomGenerator<unsigned, 0x33abc33f> random_wait {1u, 2u};
            static std::mutex mutex;

            unsigned random_wait_time = 0;

            {
                std::lock_guard<decltype(mutex)> lock(mutex);
                random_wait_time = random_wait();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(random_wait_time));
            counter_++;
        }
    private:
        std::atomic_int& counter_;
    };

    base::ThreadPool threads;
    threads.AddRealThread();
    threads.AddRealThread();
    threads.AddRealThread();
    threads.AddMainThread();

    constexpr base::ThreadTask::Affinity affinity[] = {
        base::ThreadTask::Affinity::MainThread,
        base::ThreadTask::Affinity::AnyThread
    };

    std::printf("\n");
    for (int i=0; i<1000; ++i)
    {
        const auto aff = affinity[i % 1];

        auto handle = threads.SubmitTask(std::make_unique<TestTask>(aff, counter));
        TEST_REQUIRE(!handle.IsComplete());
        TEST_REQUIRE(handle.GetTask() == nullptr);

        handle.Wait();
        TEST_REQUIRE(handle.IsComplete());
        auto* task = handle.GetTask();
        TEST_REQUIRE(task);

        threads.ExecuteMainThread();

#if !defined(__EMSCRIPTEN__)
        const auto done = (float)i / (float)1000;
        const auto done_percent = unsigned(done * 100.0);
        std::printf("\rTesting ...%d%%", done_percent);
#endif
    }
    std::printf("\n");
    threads.WaitAll();
    threads.Shutdown();

    TEST_REQUIRE(counter == 1000);
}


EXPORT_TEST_MAIN(
int test_main(int argc, char* argv[])
{
    test::TestLogger logger("unit_test_thread_pool.log");

    unit_test_pool();
    return 0;
}
) // TEST_MAIN
