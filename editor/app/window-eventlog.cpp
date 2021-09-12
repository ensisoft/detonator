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

#include "base/format.h"
#include "data/writer.h"
#include "data/reader.h"
#include "editor/app/window-eventlog.h"

namespace app
{

void IntoJson(const wdk::WindowEventKeyDown& key, data::Writer& json)
{
    json.Write("symbol",    key.symbol);
    json.Write("modifiers", key.modifiers.value());
}
void IntoJson(const wdk::WindowEventKeyUp& key, data::Writer& json)
{
    json.Write("symbol",    key.symbol);
    json.Write("modifiers", key.modifiers.value());
}
void IntoJson(const wdk::WindowEventMousePress& mickey, data::Writer& json)
{
    json.Write("window_x",  mickey.window_x);
    json.Write("window_y",  mickey.window_y);
    json.Write("global_x",  mickey.global_x);
    json.Write("global_y",  mickey.global_y);
    json.Write("button",    mickey.btn);
    json.Write("modifiers", mickey.modifiers.value());
}
void IntoJson(const wdk::WindowEventMouseRelease& mickey, data::Writer& json)
{
    json.Write("window_x",  mickey.window_x);
    json.Write("window_y",  mickey.window_y);
    json.Write("global_x",  mickey.global_x);
    json.Write("global_y",  mickey.global_y);
    json.Write("button",    mickey.btn);
    json.Write("modifiers", mickey.modifiers.value());
}
void IntoJson(const wdk::WindowEventMouseMove& mickey, data::Writer& json)
{
    json.Write("window_x",  mickey.window_x);
    json.Write("window_y",  mickey.window_y);
    json.Write("global_x",  mickey.global_x);
    json.Write("global_y",  mickey.global_y);
    json.Write("button",    mickey.btn);
    json.Write("modifiers", mickey.modifiers.value());
}

bool FromJson(const data::Reader& json, wdk::WindowEventKeyDown* key)
{
    json.Read("symbol",    &key->symbol);
    json.Read("modifiers", key->modifiers.value_ptr());
    return true;
}
bool FromJson(const data::Reader& json, wdk::WindowEventKeyUp* key)
{
    json.Read("symbol",    &key->symbol);
    json.Read("modifiers", key->modifiers.value_ptr());
    return true;
}
bool FromJson(const data::Reader& json, wdk::WindowEventMousePress* mickey)
{
    json.Read("window_x",  &mickey->window_x);
    json.Read("window_y",  &mickey->window_y);
    json.Read("global_x",  &mickey->global_x);
    json.Read("global_y",  &mickey->global_y);
    json.Read("button",    &mickey->btn);
    json.Read("modifiers", mickey->modifiers.value_ptr());
    return true;
}
bool FromJson(const data::Reader& json, wdk::WindowEventMouseRelease* mickey)
{
    json.Read("window_x",  &mickey->window_x);
    json.Read("window_y",  &mickey->window_y);
    json.Read("global_x",  &mickey->global_x);
    json.Read("global_y",  &mickey->global_y);
    json.Read("button",    &mickey->btn);
    json.Read("modifiers", mickey->modifiers.value_ptr());
    return true;
}
bool FromJson(const data::Reader& json, wdk::WindowEventMouseMove* mickey)
{
    json.Read("window_x",  &mickey->window_x);
    json.Read("window_y",  &mickey->window_y);
    json.Read("global_x",  &mickey->global_x);
    json.Read("global_y",  &mickey->global_y);
    json.Read("button",    &mickey->btn);
    json.Read("modifiers", mickey->modifiers.value_ptr());
    return true;
}

std::string ModStr(const wdk::bitflag<wdk::Keymod>& bits)
{
    std::string ret;
    if (bits.test(wdk::Keymod::Control))
        ret += "Ctrl+";
    if (bits.test(wdk::Keymod::Shift))
        ret += "Shift+";
    if (bits.test(wdk::Keymod::Alt))
        ret += "Alt+";

    if (!ret.empty())
        ret.pop_back();
    return ret;
}

std::string DescribeEvent(const wdk::WindowEventKeyDown& key)
{
    const auto& mods = ModStr(key.modifiers);
    if (mods.empty())
        return base::ToString(key.symbol);
    return mods + "+" + base::ToString(key.symbol);
}
std::string DescribeEvent(const wdk::WindowEventKeyUp& key)
{
    const auto& mods = ModStr(key.modifiers);
    if (mods.empty())
        return base::ToString(key.symbol);
    return mods + "+" + base::ToString(key.symbol);
}
std::string DescribeEvent(const wdk::WindowEventMousePress& mickey)
{
    const auto& mods = ModStr(mickey.modifiers);
    return mods + " btn=" + base::ToString(mickey.btn) +
           " x=" + std::to_string(mickey.window_x) +
           " y=" + std::to_string(mickey.window_y);
}
std::string DescribeEvent(const wdk::WindowEventMouseRelease& mickey)
{
    const auto& mods = ModStr(mickey.modifiers);
    return mods + " btn=" + base::ToString(mickey.btn) +
           " x=" + std::to_string(mickey.window_x) +
           " y=" + std::to_string(mickey.window_y);
}
std::string DescribeEvent(const wdk::WindowEventMouseMove& mickey)
{
    const auto& mods = ModStr(mickey.modifiers);
    return mods + " btn=" + base::ToString(mickey.btn) +
           " x=" + std::to_string(mickey.window_x) +
           " y=" + std::to_string(mickey.window_y);
}

WindowEventLog::WindowEventLog(const WindowEventLog& other)
{
    for (auto& cmd : other.mCommands)
    {
        Command c;
        c.time = cmd.time;
        c.cmd  = cmd.cmd->Clone();
        mCommands.push_back(std::move(c));
    }
}
WindowEventLog::WindowEventLog(WindowEventLog&& other)
  : mCommands(std::move(other.mCommands))
{}

void WindowEventLog::IntoJson(data::Writer& json) const
{
    json.Write("time_mode", mTimeMode);

    for (const auto& cmd : mCommands)
    {
        auto chunk = json.NewWriteChunk();
        cmd.cmd->IntoJson(*chunk);
        ASSERT(!chunk->HasValue("time"));
        ASSERT(!chunk->HasValue("type"));
        chunk->Write("time", cmd.time);
        chunk->Write("type", cmd.cmd->GetType());
        json.AppendChunk("cmds", std::move(chunk));
    }
}

bool WindowEventLog::FromJson(const data::Reader& json)
{
    TimeMode mode;
    if (!json.Read("time_mode", &mode))
        return false;

    std::vector<Command> cmds;
    for (unsigned i=0; i<json.GetNumChunks("cmds"); ++i)
    {
        const auto& chunk = json.GetReadChunk("cmds", i);
        std::unique_ptr<WindowEvent> cmd;
        std::string type;
        event_time_t time = 0;
        if (!chunk->Read("time", &time))
            return false;
        if (!chunk->Read("type", &type))
            return false;
        else if (type == "key_down")
            cmd.reset(new WdkWindowEvent<wdk::WindowEventKeyDown>());
        else if (type == "key_up")
            cmd.reset(new WdkWindowEvent<wdk::WindowEventKeyUp>());
        else if (type == "mouse_press")
            cmd.reset(new WdkWindowEvent<wdk::WindowEventMousePress>());
        else if (type == "mouse_release")
            cmd.reset(new WdkWindowEvent<wdk::WindowEventMouseRelease>());
        else if (type == "mouse_move")
            cmd.reset(new WdkWindowEvent<wdk::WindowEventMouseMove>());
        else return false;
        if (!cmd->FromJson(*chunk))
            return false;
        Command c;
        c.time = time;
        c.cmd  = std::move(cmd);
        cmds.push_back(std::move(c));
    }
    mCommands = std::move(cmds);
    mTimeMode = mode;
    return true;
}

WindowEventLog& WindowEventLog::operator=(const WindowEventLog& other)
{
    if (this == &other)
        return *this;
    WindowEventLog tmp(other);
    std::swap(mCommands, tmp.mCommands);
    return *this;
}

void EventLogPlayer::Apply(wdk::WindowListener& listener, event_time_t time)
{
    ASSERT(time >= mLastTime);

    const auto mode  = mLog->GetTimeMode();
    event_time_t event_time = 0;
    if (mode == WindowEventLog::TimeMode::Relative)
        event_time  = time - mStartTime;
    else event_time = time;

    while (mCmdIndex < mLog->GetNumEvents() &&
           event_time >= mLog->GetEventTime(mCmdIndex))
    {
        mLog->GetEvent(mCmdIndex).Apply(listener);
        mCmdIndex++;
    }
    mLastTime = time;
}

} // namespace