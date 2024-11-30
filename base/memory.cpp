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

#include "base/platform.h"
#include "base/assert.h"
#include "base/memory.h"

#include <cstdlib>

#if defined(LINUX_OS)
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#  include <unistd.h>

namespace mem::detail {
    struct SharedMemory {
        int shm_fd;
    };
} // mem::detail

#elif defined(WINDOWS_OS)
#  include <windows.h>

namespace mem::detail {
    struct SharedMemory  {
        HANDLE shm_fd;
    };
} // mem::detail
#endif

namespace mem::detail
{

void DestroySharedMemory(const char* name)
{
#if defined(LINUX_OS)
    shm_unlink(name);
#endif
}

void* CreateSharedMemory(const char* name, size_t bytes, SharedMemoryHandle* shm_handle)
{
#if defined(LINUX_OS)
    int shm_fd = ::shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0666);
    if (shm_fd == -1 && errno == EEXIST)
        return nullptr;
    ASSERT(shm_fd != -1);
    ASSERT(::ftruncate(shm_fd, bytes) == 0);

    void* ret = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, off_t(0));
    ASSERT(ret);

    *shm_handle = (SharedMemory*)malloc(sizeof(SharedMemory));
    (*shm_handle)->shm_fd = shm_fd;
    return ret;

#elif defined(WINDOWS_OS)
    HANDLE shm_fd = CreateFileMappingA(
            INVALID_HANDLE_VALUE, // use system paging file
            NULL, // default security
            PAGE_READWRITE,
            0, // maximum size (high order dword)
            bytes, // maximum size (low order dword)
            name);
    ASSERT(shm_fd != NULL);

    const auto err = GetLastError();
    if (err == ERROR_ALREADY_EXISTS)
    {
        ASSERT(CloseHandle(shm_fd) == TRUE);
        return nullptr;
    }

    void* ret = MapViewOfFile(shm_fd, FILE_MAP_ALL_ACCESS, 0, 0, bytes);
    ASSERT(ret);

    *shm_handle = (SharedMemory*)malloc(sizeof(SharedMemory));
    (*shm_handle)->shm_fd = shm_fd;
    return ret;
#endif
}

void* OpenSharedMemory(const char* name, size_t bytes, SharedMemoryHandle* shm_handle)
{
#if defined(LINUX_OS)
    int shm_fd = ::shm_open(name, O_RDWR, 0666);
    ASSERT(shm_fd != -1);

    void* ret = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, off_t(0));
    ASSERT(ret);

    *shm_handle = (SharedMemory*)malloc(sizeof(SharedMemory));
    (*shm_handle)->shm_fd = shm_fd;
    return ret;

#elif defined(WINDOWS_OS)
    HANDLE shm_fd = OpenFileMappingA(
            FILE_MAP_READ | FILE_MAP_WRITE,
            FALSE,
            name);
    ASSERT(shm_fd != NULL);

    void* ret = MapViewOfFile(shm_fd, FILE_MAP_ALL_ACCESS, 0, 0, bytes);
    ASSERT(ret);

    *shm_handle = (SharedMemory*)malloc(sizeof(SharedMemory));
    (*shm_handle)->shm_fd = shm_fd;
    return ret;
#endif
}

void CloseSharedMemory(void* memory, size_t bytes, SharedMemoryHandle shm_handle)
{
#if defined(LINUX_OS)
    ASSERT(::munmap(memory, bytes) != -1);
    ASSERT(::close(shm_handle->shm_fd) != -1);

    // when to do shm_unlink ?
    // we don't know how many handles we have
    ::free(shm_handle);

#elif defined(WINDOWS_OS)
    ASSERT(UnmapViewOfFile(memory) == TRUE);
    ASSERT(CloseHandle(shm_handle->shm_fd) == TRUE);
    ::free(shm_handle);
#endif
}

} // namespace
