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

#include "warnpush.h"
#include "warnpop.h"

#include <iostream>
#include <cstring>

#include "base/test_minimal.h"
#include "data/json.h"
#include "editor/app/window-eventlog.h"

template<typename WdkEventType>
void check_log_event(const app::WindowEventLog& log, const WdkEventType& expected, size_t log_index)
{
    using EventType = app::WdkWindowEvent<WdkEventType>;
    const auto* ptr = log.GetEventAs<EventType>(log_index);
    const auto& data = ptr->GetEventData();
    TEST_REQUIRE(!std::memcmp(&expected, &data, sizeof(data)));
}

void unit_test_serialize()
{
    app::WindowEventLog log;
    wdk::WindowEventKeyDown down;
    down.symbol = wdk::Keysym::ArrowLeft;
    down.modifiers.set(wdk::Keymod::Shift);
    log.RecordEvent(down, 100);

    wdk::WindowEventKeyUp up;
    up.symbol = down.symbol;
    up.modifiers = down.modifiers;
    log.RecordEvent(up, 150);

    wdk::WindowEventMouseMove move;
    move.window_x = 100;
    move.window_y = 150;
    move.btn      = wdk::MouseButton::Left;
    move.modifiers.set(wdk::Keymod::Shift);
    move.global_x = 200;
    move.global_y = 240;
    log.RecordEvent(move, 155);

    wdk::WindowEventMousePress press;
    press.window_x = 200;
    press.window_y = 250;
    press.btn      = wdk::MouseButton::Right;
    press.modifiers.set(wdk::Keymod::Control);
    press.global_x = 300;
    press.global_y = 340;
    log.RecordEvent(press, 200);

    wdk::WindowEventMousePress release;
    release.window_x = 300;
    release.window_y = 350;
    release.btn      = wdk::MouseButton::Wheel;
    release.modifiers.set(wdk::Keymod::Shift);
    release.modifiers.set(wdk::Keymod::Control);
    release.global_x = 400;
    release.global_y = 540;
    log.RecordEvent(release, 250);

    TEST_REQUIRE(log.GetNumEvents() == 5);

    data::JsonObject json;
    log.IntoJson(json);
    log.Clear();
    TEST_REQUIRE(log.FromJson(json));
    TEST_REQUIRE(log.GetNumEvents() == 5);
    TEST_REQUIRE(log.GetEventTime(0) == 100);
    TEST_REQUIRE(log.GetEventTime(1) == 150);
    TEST_REQUIRE(log.GetEventTime(2) == 155);
    TEST_REQUIRE(log.GetEventTime(3) == 200);
    TEST_REQUIRE(log.GetEventTime(4) == 250);
    check_log_event(log, down, 0);
    check_log_event(log, up, 1);
    check_log_event(log, move, 2);
    check_log_event(log, press, 3);
    check_log_event(log, release, 4);
}

void unit_test_record_play()
{
    class Dummy : public wdk::WindowListener
    {
    public:

    private:
    };

    {
        app::WindowEventLog log;
        log.SetTimeMode(app::WindowEventLog::TimeMode::Relative);
        app::EventLogRecorder recorder(&log);
        recorder.Start(2500);

        wdk::WindowEventMouseMove mouse;
        recorder.RecordEvent(mouse, 2500);
        recorder.RecordEvent(mouse, 2550);
        recorder.RecordEvent(mouse, 3000);

        TEST_REQUIRE(log.GetNumEvents() == 3);
        TEST_REQUIRE(log.GetEventTime(0) == 0);
        TEST_REQUIRE(log.GetEventTime(1) == 50);
        TEST_REQUIRE(log.GetEventTime(2) == 500);

        // play, time starts at 0
        {
            Dummy dummy;
            app::EventLogPlayer player(&log);
            player.Start(0);
            player.Apply(dummy, 0);
            TEST_REQUIRE(player.GetCurrentIndex() == 1);
            player.Apply(dummy, 20);
            TEST_REQUIRE(player.GetCurrentIndex() == 1);
            player.Apply(dummy, 40);
            player.Apply(dummy, 60);
            TEST_REQUIRE(player.GetCurrentIndex() == 2);
            player.Apply(dummy, 500);
            TEST_REQUIRE(player.GetCurrentIndex() == 3);
            TEST_REQUIRE(player.IsDone());
        }

        {
            Dummy dummy;
            app::EventLogPlayer player(&log);
            player.Start(2000);
            player.Apply(dummy, 2000);
            TEST_REQUIRE(player.GetCurrentIndex() == 1);
            player.Apply(dummy, 2000);
            TEST_REQUIRE(player.GetCurrentIndex() == 1);
            player.Apply(dummy, 2040);
            player.Apply(dummy, 2060);
            TEST_REQUIRE(player.GetCurrentIndex() == 2);
            player.Apply(dummy, 2500);
            TEST_REQUIRE(player.GetCurrentIndex() == 3);
            TEST_REQUIRE(player.IsDone());
        }

        {
            Dummy dummy;
            app::EventLogPlayer player(&log);
            player.Start(2000);
            player.Apply(dummy, 3000);
            TEST_REQUIRE(player.GetCurrentIndex() == 3);
            TEST_REQUIRE(player.IsDone());
        }

    }

    {
        app::WindowEventLog log;
        log.SetTimeMode(app::WindowEventLog::TimeMode::Absolute);

        app::EventLogRecorder recorder(&log);
        recorder.Start(2500);

        wdk::WindowEventMouseMove mouse;
        recorder.RecordEvent(mouse, 2500);
        recorder.RecordEvent(mouse, 2550);
        recorder.RecordEvent(mouse, 3000);

        TEST_REQUIRE(log.GetNumEvents() == 3);
        TEST_REQUIRE(log.GetEventTime(0) == 2500);
        TEST_REQUIRE(log.GetEventTime(1) == 2550);
        TEST_REQUIRE(log.GetEventTime(2) == 3000);

        // play
        {
            Dummy dummy;
            app::EventLogPlayer player(&log);
            player.Start(100);
            player.Apply(dummy, 2400);
            TEST_REQUIRE(player.GetCurrentIndex() == 0);
            player.Apply(dummy, 2500);
            TEST_REQUIRE(player.GetCurrentIndex() == 1);
            player.Apply(dummy, 3001);
            TEST_REQUIRE(player.GetCurrentIndex() == 3);
            TEST_REQUIRE(player.IsDone());
        }

        {
            Dummy dummy;
            app::EventLogPlayer player(&log);
            player.Start(3000);
            player.Apply(dummy, 3000);
            TEST_REQUIRE(player.GetCurrentIndex() == 3);
            TEST_REQUIRE(player.IsDone());
        }
    }
}

int test_main(int argc, char* argv[])
{
    unit_test_serialize();
    unit_test_record_play();
    return 0;
}