// Copyright (C) 2020-2026 Sami Väisänen
// Copyright (C) 2020-2026 Ensisoft http://www.ensisoft.com
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

#include "graphics/paint_context.h"

namespace gfx
{
    void WritePaintContextLogMessage(PaintContext::LogEvent type, const char* file, int line, std::string message);

    // Interface for writing a variable argument message to the
    // calling thread's logger or the global logger.
    template<typename... Args>
    void WritePaintContextLog(PaintContext::LogEvent type, const char* file, int line, const std::string& fmt, const Args&... args)
    {
        // format the message in the log statement.
        WritePaintContextLogMessage(type, file, line, base::FormatString(fmt, args...));
    }

    // these are drop in replacement macros for the base logging.
    // the idea is that we can easily capture the errors and store
    // them in the context (if any). Anything that is in the render
    // loop should use these macros instead of the base macros.
#define GFX_PAINT_VERBOSE(fmt, ...) gfx::WritePaintContextLog(gfx::PaintContext::LogEvent::Verbose, __FILE__, __LINE__, fmt, ## __VA_ARGS__)
#define GFX_PAINT_DEBUG(fmt, ...)   gfx::WritePaintContextLog(gfx::PaintContext::LogEvent::Debug,   __FILE__, __LINE__, fmt, ## __VA_ARGS__)
#define GFX_PAINT_WARN(fmt, ...)    gfx::WritePaintContextLog(gfx::PaintContext::LogEvent::Warning, __FILE__, __LINE__, fmt, ## __VA_ARGS__)
#define GFX_PAINT_INFO(fmt, ...)    gfx::WritePaintContextLog(gfx::PaintContext::LogEvent::Info,    __FILE__, __LINE__, fmt, ## __VA_ARGS__)
#define GFX_PAINT_ERROR(fmt, ...)   gfx::WritePaintContextLog(gfx::PaintContext::LogEvent::Error,   __FILE__, __LINE__, fmt, ## __VA_ARGS__)

#define GFX_PAINT_ERROR_RETURN(ret, fmt, ...) \
    do { \
        WritePaintContextLog(PaintContext::LogEvent::Error, __FILE__, __LINE__, fmt, ## __VA_ARGS__); \
        return ret; \
    } while (0)

} // namespace