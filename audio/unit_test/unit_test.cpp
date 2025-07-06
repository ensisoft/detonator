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

#include <iostream>
#include <fstream>
#include <thread>
#include <filesystem>

#ifndef ENABLE_AUDIO_TEST_SOURCE
#  define ENABLE_AUDIO_TEST_SOURCE
#endif

#include "base/test_minimal.h"
#include "base/test_help.h"
#include "base/logging.h"
#include "base/math.h"
#include "audio/source.h"
#include "audio/player.h"
#include "audio/device.h"
#include "audio/algo.h"
#include "audio/sndfile.h"
#include "audio/sine_source.h"
#include "audio/loader.h"
#include "audio/thread_proxy_source.h"

class TestSource : public audio::Source
{
public:
    TestSource(unsigned samplerate, unsigned channels,
               unsigned buffers, unsigned fail_buffer)
      : mSampleRate(samplerate)
      , mNumChannels(channels)
      , mBuffers(buffers)
      , mFailBuffer(fail_buffer)
    { }

    unsigned GetRateHz() const noexcept override
    { return mSampleRate; }
    unsigned int GetNumChannels() const noexcept override
    { return mNumChannels;}
    Format GetFormat() const noexcept override
    { return Format::Float32; }
    std::string GetName() const noexcept override
    { return "test"; }
    unsigned  FillBuffer(void* buff, unsigned max_bytes) override
    {
        DEBUG("FillBuffer %1", mFillCount);
        if (++mFillCount == mFailBuffer)
            throw std::runtime_error("something failed");
        return max_bytes;
    }
    bool HasMore(std::uint64_t num_bytes_read) const noexcept override
    {
        const auto ret = mFillCount < mBuffers;
        DEBUG("HasNextBuffer: %1", ret);
        return ret;
    }
    void Shutdown() noexcept override
    {}
    void RecvCommand(std::unique_ptr<audio::Command>) noexcept override
    {}
private:
    const unsigned mSampleRate = 0;
    const unsigned mNumChannels = 0;
    const unsigned mBuffers = 0;
    const unsigned mFailBuffer = 0;
    unsigned mFillCount = 0;
};

template<typename Condition>
bool LoopUntilEvent(audio::Player& player, Condition cond)
{
    for (unsigned i=0; i<1000; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        audio::Player::Event e;
        if (player.GetEvent(&e))
        {
            TEST_REQUIRE(std::holds_alternative<audio::Player::SourceCompleteEvent>(e));
            auto track_event = std::get<audio::Player::SourceCompleteEvent>(e);
            if (cond(track_event))
                return true;
        }
    }
    return false;
}

std::string GetTestFile(const std::string& name)
{
    auto p = std::filesystem::path(__FILE__).parent_path();
    p /= name;
    return p.string();
}

void unit_test_success()
{
    TEST_CASE(test::Type::Feature)

    audio::Player player(audio::Device::Create("audio_unit_test"));

    // single stream
    {
        const auto id = player.Play(std::make_unique<TestSource>(44100, 2, 10, 11));
        TEST_REQUIRE(LoopUntilEvent(player, [id](const auto& event) {
            TEST_REQUIRE(event.id == id);
            TEST_REQUIRE(event.status == audio::Player::TrackStatus::Success);
            return true;
        }));
    }

    // create multiple simultaneous streams
    {
        for (unsigned i = 0; i < 100; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
            player.Play(std::make_unique<TestSource>(44100, 2, 300, 301));
            audio::Player::Event e;
            while (player.GetEvent(&e))
            {
                //DEBUG("Track %1 event %%2", e.id, e.status);
            }
        }
    }
}

void unit_test_format_fail()
{
    TEST_CASE(test::Type::Feature)

    audio::Player player(audio::Device::Create("audio_unit_test"));

    // bogus sample rate
    {
        const auto id = player.Play(std::make_unique<TestSource>(771323, 2, 10, 11));

        TEST_REQUIRE(LoopUntilEvent(player, [id](const auto& event) {
            TEST_REQUIRE(event.id == id);
            TEST_REQUIRE(event.status == audio::Player::TrackStatus::Failure);
            return true;
        }));
    }

    // bogus channel count
    {
        const auto id = player.Play(std::make_unique<TestSource>(44100, 123, 10, 11));

        TEST_REQUIRE(LoopUntilEvent(player, [id](const auto& event) {
            TEST_REQUIRE(event.id == id);
            TEST_REQUIRE(event.status == audio::Player::TrackStatus::Failure);
            return true;
        }));
    }
}

void unit_test_fill_buffer_exception()
{
    TEST_CASE(test::Type::Feature)

    audio::Player player(audio::Device::Create("audio_unit_test"));

    const auto id = player.Play(std::make_unique<TestSource>(44100, 2, 100, 45));

    TEST_REQUIRE(LoopUntilEvent(player, [id](const auto& event) {
        TEST_REQUIRE(event.id == id);
        TEST_REQUIRE(event.status == audio::Player::TrackStatus::Failure);
        return true;
    }));
}

void unit_test_pause_resume()
{
    TEST_CASE(test::Type::Feature)

    audio::Player player(audio::Device::Create("audio_unit_test"));

    {
        const auto id = player.Play(std::make_unique<audio::SineTestSource>(300));
        std::this_thread::sleep_for(std::chrono::seconds(1));
        for (unsigned i=0; i<10; ++i)
        {
            if ((i & 1) == 0)
                player.Pause(id);
            else player.Resume(id);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void unit_test_cancel()
{
    TEST_CASE(test::Type::Feature)

    // cancel stream while in playback
    audio::Player player(audio::Device::Create("audio_unit_test"));

    for (unsigned i=0; i<100; ++i)
    {
        const auto id = player.Play(std::make_unique<audio::SineTestSource>(300, 200));
        std::this_thread::sleep_for(std::chrono::milliseconds(math::rand<423234>(0, 100)));
        player.Cancel(id);
    }
}

void unit_test_shutdown_with_active_streams()
{
    TEST_CASE(test::Type::Feature)

    for (unsigned i=0; i<100; ++i)
    {
        audio::Player player(audio::Device::Create("audio_unit_test"));
        const auto id = player.Play(std::make_unique<audio::SineTestSource>(300, 200));
        std::this_thread::sleep_for(std::chrono::milliseconds(math::rand<22323>(0, 100)));
    }
}


void unit_test_thread_proxy()
{
    TEST_CASE(test::Type::Feature)

    // success.
    {
        auto test = std::make_unique<TestSource>(44100, 2, 10, 11);
        const auto size = audio::Source::BuffSize(test->GetFormat(),
                                                  test->GetNumChannels(),
                                                  test->GetRateHz(), 20);
        auto proxy = std::make_unique<audio::ThreadProxySource>(std::move(test));

        TEST_REQUIRE(proxy->GetFormat() == audio::Source::Format::Float32);
        TEST_REQUIRE(proxy->GetRateHz() == 44100);
        TEST_REQUIRE(proxy->GetNumChannels() == 2);

        proxy->Prepare(size);
        TEST_REQUIRE(proxy->HasMore(0));

        std::uint64_t bytes = 0;
        std::vector<char> buffer;
        buffer.resize(size);

        for (unsigned i=0; i<10; ++i)
        {
            const auto ret = proxy->WaitBuffer(&buffer[0], buffer.size());
            TEST_REQUIRE(ret);
            bytes += ret;
        }
        TEST_REQUIRE(proxy->HasMore(bytes) == false);

        proxy->Shutdown();
    }

    // cancellation
    {
        auto test = std::make_unique<TestSource>(44100, 2, 10, 11);
        const auto size = audio::Source::BuffSize(test->GetFormat(),
                                                  test->GetNumChannels(),
                                                  test->GetRateHz(), 20);
        auto proxy = std::make_unique<audio::ThreadProxySource>(std::move(test));

        TEST_REQUIRE(proxy->GetFormat() == audio::Source::Format::Float32);
        TEST_REQUIRE(proxy->GetRateHz() == 44100);
        TEST_REQUIRE(proxy->GetNumChannels() == 2);

        proxy->Prepare(size);
        TEST_REQUIRE(proxy->HasMore(0));

        std::uint64_t bytes = 0;
        std::vector<char> buffer;
        buffer.resize(size);

        for (unsigned i=0; i<5; ++i)
        {
            const auto ret = proxy->WaitBuffer(&buffer[0], buffer.size());
            TEST_REQUIRE(ret);
            bytes += ret;
        }
        TEST_REQUIRE(proxy->HasMore(bytes) == true);

        proxy->Shutdown();
    }


    // exception
    {
        auto test = std::make_unique<TestSource>(44100, 2, 10, 9);
        const auto size = audio::Source::BuffSize(test->GetFormat(),
                                                  test->GetNumChannels(),
                                                  test->GetRateHz(), 20);
        auto proxy = std::make_unique<audio::ThreadProxySource>(std::move(test));

        TEST_REQUIRE(proxy->GetFormat() == audio::Source::Format::Float32);
        TEST_REQUIRE(proxy->GetRateHz() == 44100);
        TEST_REQUIRE(proxy->GetNumChannels() == 2);

        proxy->Prepare(size);
        TEST_REQUIRE(proxy->HasMore(0));

        std::uint64_t bytes = 0;
        std::vector<char> buffer;
        buffer.resize(size);

        for (unsigned i=0; i<8; ++i)
        {
            const auto ret = proxy->WaitBuffer(&buffer[0], buffer.size());
            TEST_REQUIRE(ret);
            bytes += ret;
        }
        TEST_EXCEPTION(proxy->WaitBuffer(&buffer[0], buffer.size()));

        proxy->Shutdown();
    }
}

// sndfile is not thread safe.
// https://github.com/libsndfile/libsndfile/issues/279
void unit_test_sndfile_thread_safety()
{
    TEST_CASE(test::Type::Feature)

    auto stream = audio::OpenFileStream(GetTestFile("sounds/bombexplosion.ogg"));
    TEST_REQUIRE(stream);

    for (int i=0; i<100; ++i)
    {
        std::thread thread0([&stream]() {
            audio::SndFileDecoder dec;

            TEST_REQUIRE(dec.Open(stream));

        });
        std::thread thread1([stream]() {
            audio::SndFileDecoder dec;

            TEST_REQUIRE(dec.Open(stream));
        });

        thread0.join();
        thread1.join();
    }

}

void perf_test_buffer_mixing()
{
    TEST_CASE(test::Type::Performance)

    // take 10 float buffers and mix together.

    std::vector<audio::BufferHandle> buffers;
    buffers.resize(1000);
    for (auto& buffer : buffers)
    {
        audio::Format format;
        format.channel_count = 2;
        format.sample_type   = audio::SampleType::Float32;
        format.sample_rate   = 44100;

        buffer = std::make_shared<audio::VectorBuffer>(1024*10);
        buffer->SetByteSize(1024*10);
        buffer->SetFormat(format);

        const auto count = buffer->GetByteSize() / sizeof(float);
        auto* floats = (float*)buffer->GetPtr();
        for (size_t i=0; i<count; ++i)
            floats[i] = 0.2f;
    }

    test::PerfTest("mixing float32", 1, [&buffers]() {
        audio::MixBuffers(buffers, 0.1f);
    });

    for (auto& buffer : buffers)
    {
        test::DevNull("%f", *(float*)buffer->GetPtr());
    }

}

void run_tests()
{
#if defined(__EMSCRIPTEN__)
    unit_test_thread_proxy();
    perf_test_buffer_mixing();
#else
    unit_test_success();
    unit_test_format_fail();
    unit_test_fill_buffer_exception();
    unit_test_pause_resume();
    unit_test_cancel();
    unit_test_shutdown_with_active_streams();
    unit_test_thread_proxy();
    unit_test_sndfile_thread_safety();

    perf_test_buffer_mixing();
#endif
}

SET_BUNDLE_NAME("unit_test_audio")

EXPORT_TEST_MAIN(
int test_main(int argc, char* argv[])
{
    test::TestLogger logger("unit_test_audio.log");

    // helper to avoid having to have #if ... inside this
    // macro since msvs drowns in its own shit with this construct
    run_tests();
    return 0;
}
) // TEST_MAIN
