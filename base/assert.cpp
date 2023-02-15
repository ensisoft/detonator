// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <atomic>
#include <string>
#include <sstream>
#if defined(WINDOWS_OS)
#  include "warnpush.h"
#  include <windows.h>
#  include <DbgHelp.h> // for minidumps
#  include "warnpop.h"
#  pragma comment(lib, "DbgHelp.lib")
#elif defined(LINUX_OS)
#  include <execinfo.h>
#  include <sys/wait.h>
#  include <sys/ptrace.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <cstring>
#endif

#if defined(POSIX_OS)
#  include <signal.h>
#endif

#include <iostream>

#include "base/platform.h"
#include "base/assert.h"

// So where is my core file?
// check that core file is unlimited
// ulimit -c unlimited
// check the kernel core pattern
// cat /proc/sys/kernel/core_pattern
//
// Some distros such as ArchLinux use systemd to
// store the core files in the journal. these can be retrieved
// with systemd-coredumpctl

namespace {

std::atomic_flag mutex = ATOMIC_FLAG_INIT;

std::wstring ascii_to_wide_str(const std::string& narrow_str)
{
    std::wstring ret;
    for (wchar_t c : narrow_str)
        ret.push_back(c);
    return ret;
}

} // namespace

namespace debug
{

void do_break()
{
    if (has_debugger())
    {
#if defined(WINDOWS_OS)
        DebugBreak();
#elif defined(POSIX_OS)
        ::raise(SIGTRAP);
#endif
    }
    std::abort();
}

void do_assert(const char* expression, const char* file, const char* func, int line)
{
    // if one thread is already asserting then spin lock other threads here.
    if (mutex.test_and_set())
        while (1);

    // flush previous output before dumping core.
    std::cerr.flush();
    std::cout.flush();
    std::fflush(stdout);
    std::fflush(stderr);

    std::fprintf(stderr, "%s:%i: %s: Assertion `%s' failed.\n", file, line, func, expression);

    // for windows XP and Windows Server 2003 FramestoSkip and FramesToCapture
    // must be less than 63.
    const int MAX_CALLSTACK = 62;

#if defined(WINDOWS_OS)
    // todo: use StackWalk64 to print stack frame

    std::wstring filename = L"excecutable_file_name_unknown";

    HMODULE module_handle = GetModuleHandle(NULL); // get the executable handle.
    if (module_handle != NULL)
    {
        wchar_t path[MAX_PATH+1] = {};
        if (GetModuleFileNameW(module_handle, path, MAX_PATH))
        {
            std::wstring temp(path);
            const auto last_back_slash = temp.find_last_of(L"\\");
            if (last_back_slash != std::wstring::npos)
                filename = temp.substr(last_back_slash+1);
        }
    }

    // todo: get the module version.

    // write minidump
    HANDLE handle = CreateFileW((filename + L".dump").c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (handle != INVALID_HANDLE_VALUE)
    {
        // MiniDumpWithThreadInfo
        // Include thread state information. For more information, see MINIDUMP_THREAD_INFO_LIST.
        // DbgHelp 6.1 and earlier:  This value is not supported.
        //
        // MiniDumpWithIndirectlyReferencedMemory
        // Include pages with data referenced by locals or other stack memory. This option can increase the size of the minidump file significantly.
        // DbgHelp 5.1:  This value is not supported.
        const DWORD flags[] = {
            MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory,
            MiniDumpWithIndirectlyReferencedMemory,
            0
        };
        for (auto flag : flags)
        {
            const auto ret = MiniDumpWriteDump(
                GetCurrentProcess(),
                GetCurrentProcessId(),
                handle,
                MINIDUMP_TYPE(MiniDumpNormal | flag),
                nullptr, // exception info
                nullptr, // UserStreamParam
                nullptr); // callbackParam
            if (ret == TRUE)
                break;
        }
        CloseHandle(handle);
    }


    // todo: we should figure out how to display this message to the user
    // in case a non-gui thread faults. maybe need a watchdog?
    if (IsGUIThread(FALSE) == TRUE)
    {
        std::wstring msg;
        msg.append(L"Assertion failed\n");
        msg.append(ascii_to_wide_str(expression));
        msg.append(L"\n");
        msg.append(ascii_to_wide_str(file) + L":" + std::to_wstring(line));
       
        MessageBoxW(
            NULL,
            msg.c_str(),
            filename.c_str(),
            MB_ICONERROR);
    }

#elif defined(LINUX_OS)
    void* callstack[MAX_CALLSTACK] = {0};

    const int frames = backtrace(callstack, MAX_CALLSTACK);
    if (frames == 0)
        std::abort();

    // for backtrace symbols to work you need -rdynamic ld (linker) flag
    char** strings = backtrace_symbols(callstack, frames);
    if (strings == nullptr)
        std::abort();

    // todo: demangle
    for (int i=0; i<frames; ++i)
    {
        std::fprintf(stderr, "Frame (%d): @ %p, '%s'\n",
            i, callstack[i], strings[i]);
    }

    free(strings);

#endif
    std::abort();
}

bool has_debugger()
{
#if defined(WINDOWS_OS)
    return (IsDebuggerPresent() == TRUE);
#elif defined(LINUX_OS)
    // SO to the rescue.
    // https://stackoverflow.com/questions/3596781/how-to-detect-if-the-current-process-is-being-run-by-gdb

    char buf[4096];

    const int status_fd = ::open("/proc/self/status", O_RDONLY);
    if (status_fd == -1)
        return false;

    const ssize_t num_read = ::read(status_fd, buf, sizeof(buf) - 1);
    ::close(status_fd);

    if (num_read <= 0)
        return false;

    buf[num_read] = '\0';
    constexpr char tracerPidString[] = "TracerPid:";
    const auto tracer_pid_ptr = ::strstr(buf, tracerPidString);
    if (!tracer_pid_ptr)
        return false;

    for (const char* characterPtr = tracer_pid_ptr + sizeof(tracerPidString) - 1; characterPtr <= buf + num_read; ++characterPtr)
    {
        if (::isspace(*characterPtr))
            continue;
        else
            return ::isdigit(*characterPtr) != 0 && *characterPtr != '0';
    }
    return false;
#elif defined(WEB_OS)
    return false;
#else
#  error Unimplemented function
#endif
}

} // debug
