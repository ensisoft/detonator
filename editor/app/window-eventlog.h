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

#pragma once

#include "config.h"

#include <memory>
#include <cstdint>
#include <chrono>

#include "base/assert.h"
#include "base/utility.h"
#include "data/fwd.h"
#include "wdk/system.h"
#include "wdk/events.h"
#include "wdk/listener.h"

namespace app
{
    class WindowEvent
    {
    public:
        virtual ~WindowEvent() = default;
        virtual void Apply(wdk::WindowListener& listener) const = 0;
        virtual void IntoJson(data::Writer& json) const = 0;
        virtual bool FromJson(const data::Reader& json) = 0;
        virtual std::unique_ptr<WindowEvent> Clone() const = 0;
        virtual std::string GetType() const = 0;
        virtual std::string GetDesc() const = 0;
    private:
    };

    void IntoJson(const wdk::WindowEventKeyDown& key, data::Writer& json);
    void IntoJson(const wdk::WindowEventKeyUp& key, data::Writer& json);
    void IntoJson(const wdk::WindowEventMousePress& mickey, data::Writer& json);
    void IntoJson(const wdk::WindowEventMouseRelease& mickey, data::Writer& json);
    void IntoJson(const wdk::WindowEventMouseMove& mickey, data::Writer& json);

    bool FromJson(const data::Reader& json, wdk::WindowEventKeyDown* key);
    bool FromJson(const data::Reader& json, wdk::WindowEventKeyUp* key);
    bool FromJson(const data::Reader& json, wdk::WindowEventMousePress* mickey);
    bool FromJson(const data::Reader& json, wdk::WindowEventMouseRelease* mickey);
    bool FromJson(const data::Reader& json, wdk::WindowEventMouseMove* mickey);

    std::string DescribeEvent(const wdk::WindowEventKeyDown& key);
    std::string DescribeEvent(const wdk::WindowEventKeyUp& key);
    std::string DescribeEvent(const wdk::WindowEventMousePress& mickey);
    std::string DescribeEvent(const wdk::WindowEventMouseRelease& mickey);
    std::string DescribeEvent(const wdk::WindowEventMouseMove& mickey);

    template<typename WdkEvent>
    class WdkWindowEvent : public WindowEvent
    {
    public:
        WdkWindowEvent() = default;
        WdkWindowEvent(const WdkEvent& event)
          : mEvent(event)
        {}
        virtual void Apply(wdk::WindowListener& listener) const override
        { wdk::Dispatch(mEvent, listener); }
        virtual void IntoJson(data::Writer& json) const override
        { app::IntoJson(mEvent, json); }
        virtual bool FromJson(const data::Reader& json) override
        { return app::FromJson(json, &mEvent); }
        virtual std::unique_ptr<WindowEvent> Clone() const override
        { return std::make_unique<WdkWindowEvent>(*this); }
        virtual std::string GetType() const override
        { return wdk::GetEventName(mEvent); }
        virtual std::string GetDesc() const override
        { return app::DescribeEvent(mEvent); }
        const WdkEvent& GetEventData() const
        { return mEvent; }
    private:
        WdkEvent mEvent;
    };

    using WdkMouseMoveWindowEvent    = WdkWindowEvent<wdk::WindowEventMouseMove>;
    using WdkMousePressWindowEvent   = WdkWindowEvent<wdk::WindowEventMousePress>;
    using WdkMouseReleaseWindowEvent = WdkWindowEvent<wdk::WindowEventMouseRelease>;
    using WdkKeyDownWindowEvent      = WdkWindowEvent<wdk::WindowEventKeyDown>;
    using WdkKeyUpWindowEvent        = WdkWindowEvent<wdk::WindowEventKeyUp>;

    class WindowEventLog
    {
    public:
        enum class TimeMode {
            Relative, Absolute
        };
        using event_time_t = std::uint32_t;

        WindowEventLog() = default;
        WindowEventLog(const WindowEventLog& other);
        WindowEventLog(WindowEventLog&& other);

        inline void RecordEvent(const wdk::WindowEventKeyDown& key, event_time_t time)
        { RecordWdkEvent(key, time); }
        inline void RecordEvent(const wdk::WindowEventKeyUp& key, event_time_t time)
        { RecordWdkEvent(key, time); }
        inline void RecordEvent(const wdk::WindowEventMouseMove& mickey, event_time_t time)
        { RecordWdkEvent(mickey, time); }
        inline void RecordEvent(const wdk::WindowEventMousePress& mickey, event_time_t time)
        { RecordWdkEvent(mickey, time); }
        inline void RecordEvent(const wdk::WindowEventMouseRelease& mickey, event_time_t time)
        { RecordWdkEvent(mickey, time); }

        template<typename T> inline
        T* GetEventAs(size_t index)
        { return dynamic_cast<T*>(&GetEvent(index)); }
        template<typename T> inline
        const T* GetEventAs(size_t index) const
        { return dynamic_cast<const T*>(&GetEvent(index)); }
        inline size_t GetNumEvents() const
        { return mCommands.size(); }
        inline bool IsEmpty() const
        { return mCommands.empty(); }
        inline WindowEvent& GetEvent(size_t index)
        { return *base::SafeIndex(mCommands, index).cmd; }
        inline const WindowEvent& GetEvent(size_t index) const
        { return *base::SafeIndex(mCommands, index).cmd; }
        inline event_time_t GetEventTime(size_t index) const
        { return base::SafeIndex(mCommands, index).time; }
        inline std::string GetEventType(size_t index) const
        { return base::SafeIndex(mCommands, index).cmd->GetType(); }
        inline std::string GetEventDesc(size_t index) const
        { return base::SafeIndex(mCommands, index).cmd->GetDesc(); }
        inline TimeMode GetTimeMode() const
        { return mTimeMode; }
        inline void SetTimeMode(TimeMode mode)
        { mTimeMode = mode; }
        inline void Clear()
        { mCommands.clear(); }

        void IntoJson(data::Writer& json) const;
        bool FromJson(const data::Reader& json);

        WindowEventLog& operator=(const WindowEventLog& other);
    private:
        template<typename WdkEvent>
        void RecordWdkEvent(const WdkEvent& event, std::uint32_t time)
        {
            using Cmd = WdkWindowEvent<WdkEvent>;
            Command cmd;
            cmd.time = time;
            cmd.cmd  = std::make_unique<Cmd>(event);
            mCommands.push_back(std::move(cmd));
        }
    private:
        struct Command {
            event_time_t time = 0;
            std::unique_ptr<WindowEvent> cmd;
        };
        TimeMode mTimeMode = TimeMode::Relative;
        std::vector<Command> mCommands;
    };

    class EventLogPlayer
    {
    public:
        using event_time_t = WindowEventLog::event_time_t ;

        EventLogPlayer(const WindowEventLog* log)
          : mLog(log)
        {}
        void Apply(wdk::WindowListener& listener, event_time_t time);

        void Start(event_time_t time)
        { mStartTime = time; }

        bool IsDone() const
        { return mCmdIndex == mLog->GetNumEvents(); }
        std::size_t GetCurrentIndex() const
        { return mCmdIndex; }
    private:
        const WindowEventLog* mLog = nullptr;
        std::size_t mCmdIndex   = 0;
        event_time_t mStartTime  = 0;
        event_time_t mLastTime   = 0;
    };

    class EventLogRecorder
    {
    public:
        using event_time_t = WindowEventLog::event_time_t ;

        EventLogRecorder(WindowEventLog* log)
          : mLog(log)
        {}

        void Start(event_time_t time)
        { mStartTime = time; }

        template<typename T>
        void RecordEvent(const T& event, event_time_t time)
        {
            ASSERT(time >= mLastTime);
            const auto mode = mLog->GetTimeMode();
            if (mode == WindowEventLog::TimeMode::Relative)
                mLog->RecordEvent(event, time - mStartTime);
            else mLog->RecordEvent(event, time);
            mLastTime = time;
        }
    private:
        WindowEventLog* mLog = nullptr;
        event_time_t mStartTime = 0;
        event_time_t mLastTime  = 0;
    };

} // namespace