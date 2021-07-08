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

#include "base/test_minimal.h"
#include "base/logging.h"
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
    virtual bool HasNextBuffer(std::uint64_t num_bytes_read) const noexcept override
    {
        const auto ret = mFillCount < mBuffers;
        DEBUG("HasNextBuffer: %1", ret);
        return ret;
    }

    virtual bool Reset() noexcept override
    { return true; }
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
        audio::Player::TrackEvent e;
        if (player.GetEvent(&e))
        {
            if (cond(e))
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
            audio::Player::TrackEvent e;
            while (player.GetEvent(&e))
            {
                DEBUG("Track %1 event %%2", e.id, e.status);
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

int test_main(int argc, char* argv[])
{
    base::LockedLogger<base::OStreamLogger> logger((base::OStreamLogger(std::cout)));
    base::SetGlobalLog(&logger);
    base::EnableDebugLog(true);

    unit_test_success();
    unit_test_format_fail();
    unit_test_fill_buffer_exception();
    return 0;
}