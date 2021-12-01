

#include <vector>
#include <unordered_map>
#include <string>

#include "os.h"

#if defined(VORBIS_FPU_CONTROL)
#  if defined(__GNUC__) || defined(__clang__)
#    pragma message "libvorbis SSE2 enabled"
#  elif defined(_MSC_VER)
#    pragma message("libvorbis SSE2 enabled")
#  endif
#endif

// implementation of "alloca" without actually manipulating
// the stack. this is for emscripten.
using Buffer = std::vector<char>;
static thread_local std::unordered_map<std::string, Buffer> local_buffers;

extern "C" {
void* vorbis_alloca_func(size_t bytes, const char* func, int line)
{
    std::string key;
    key.resize(128);
    const auto ret = std::snprintf(&key[0], key.size(), "%s:%d", func, line);
    key.resize(ret);

    auto it = local_buffers.find(key);
    if (it == local_buffers.end())
        it = local_buffers.insert({key, Buffer{}}).first;

    auto& buffer = it->second;
    if (buffer.size() < bytes)
    {
        buffer.resize(bytes);
        std::printf("new alloca buffer. [bytes=%d]\n", bytes);
    }

    return &buffer[0];
}
} // extern "C"