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

#ifndef ENABLE_AUDIO_TEST_SOURCE
#  define ENABLE_AUDIO_TEST_SOURCE
#endif

#include "base/test_minimal.h"
#include "base/logging.h"
#include "base/math.h"
#include "audio/source.h"
#include "audio/player.h"
#include "audio/device.h"

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

    virtual unsigned GetRateHz() const noexcept override
    { return mSampleRate; }
    virtual unsigned int GetNumChannels() const noexcept override
    { return mNumChannels;}
    virtual Format GetFormat() const noexcept override
    { return Format::Float32; }
    virtual std::string GetName() const noexcept override
    { return "test"; }
    virtual unsigned  FillBuffer(void* buff, unsigned max_bytes) override
    {
        DEBUG("FillBuffer %1", mFillCount);
        if (++mFillCount == mFailBuffer)
            throw std::runtime_error("something failed");
        return max_bytes;
    }
    virtual bool HasMore(std::uint64_t num_bytes_read) const noexcept override
    {
        const auto ret = mFillCount < mBuffers;
        DEBUG("HasNextBuffer: %1", ret);
        return ret;
    }
    virtual void Shutdown() noexcept override
    {}
    virtual void RecvCommand(std::unique_ptr<audio::Command>) noexcept override
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

void unit_test_success()
{
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
    audio::Player player(audio::Device::Create("audio_unit_test"));

    {
        const auto id = player.Play(std::make_unique<audio::SineGenerator>(300));
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
    // cancel stream while in playback
    audio::Player player(audio::Device::Create("audio_unit_test"));

    for (unsigned i=0; i<100; ++i)
    {
        const auto id = player.Play(std::make_unique<audio::SineGenerator>(300, 200));
        std::this_thread::sleep_for(std::chrono::milliseconds(math::rand<423234>(0, 100)));
        player.Cancel(id);
    }
}

void unit_test_shutdown_with_active_streams()
{
    for (unsigned i=0; i<100; ++i)
    {
        audio::Player player(audio::Device::Create("audio_unit_test"));
        const auto id = player.Play(std::make_unique<audio::SineGenerator>(300, 200));
        std::this_thread::sleep_for(std::chrono::milliseconds(math::rand<22323>(0, 100)));
    }
}


void unit_test_thread_proxy()
{
    // success.
    {
        auto test = std::make_unique<TestSource>(44100, 2, 10, 11);
        const auto size = audio::Source::BuffSize(test->GetFormat(),
                                                  test->GetNumChannels(),
                                                  test->GetRateHz(), 20);
        auto proxy = std::make_unique<audio::SourceThreadProxy>(std::move(test));

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
            const auto ret = proxy->FillBuffer(&buffer[0], buffer.size());
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
        auto proxy = std::make_unique<audio::SourceThreadProxy>(std::move(test));

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
            const auto ret = proxy->FillBuffer(&buffer[0], buffer.size());
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
        auto proxy = std::make_unique<audio::SourceThreadProxy>(std::move(test));

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
            const auto ret = proxy->FillBuffer(&buffer[0], buffer.size());
            TEST_REQUIRE(ret);
            bytes += ret;
        }
        TEST_EXCEPTION(proxy->FillBuffer(&buffer[0], buffer.size()));

        proxy->Shutdown();
    }
}

int test_main(int argc, char* argv[])
{
    base::LockedLogger<base::OStreamLogger> logger((base::OStreamLogger(std::cout)));
    base::SetGlobalLog(&logger);
    base::EnableDebugLog(true);

    unit_test_success();
    unit_test_format_fail();
    unit_test_fill_buffer_exception();
    unit_test_pause_resume();
    unit_test_cancel();
    unit_test_shutdown_with_active_streams();
    unit_test_thread_proxy();
    return 0;
}
