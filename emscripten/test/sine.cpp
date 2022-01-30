
// for native build on Linux
// g++ -03 -o sine-test -Wall sine.cpp -lopenal

// for emscripten
// emcc -O3 sine.cpp -lopenal -o sine.html
// python -m http.server


#if defined(__EMSCRIPTEN__)
#  include <emscripten/emscripten.h>
#  include <emscripten/html5.h>
#endif

#include <AL/al.h>
#include <AL/alc.h>
#include <stdexcept>
#include <vector>
#include <thread>
#include <chrono>
#include <cmath>
#include <cstdio>


const char* ToString(ALenum e)
{
#define CASE(x) if (e == x) return #x
    CASE(AL_PLAYING);
    CASE(AL_STOPPED);
    CASE(AL_STREAMING);
    CASE(AL_PAUSED);
#undef CASE
    return "???";
}

#define AL_CALL(x) \
do {               \
    x;             \
    const auto err = alGetError(); \
    if (err != AL_NO_ERROR) {       \
        std::printf("AL Error: 0x%x\n", err); \
        std::abort();\
    }        \
} while (0)

ALCcontext* g_context;
ALCdevice* g_device;
ALuint g_source;
ALuint g_buffers[5];
ALenum g_state;

unsigned GenerateSine(void* buff, unsigned max_bytes)
{
    const auto num_channels = 2;
    const auto frequency = 200;
    const auto frame_size = num_channels * sizeof(short);
    const auto frames = max_bytes / frame_size;
    const auto radial_velocity = M_PI * 2.0 * frequency;
    const auto sample_increment = 1.0/44100.0 * radial_velocity;

    static unsigned sample_counter = 0;

    auto* ptr = (short*)buff;

    for (unsigned i=0; i<frames; ++i)
    {
        const float sample = std::sin(sample_counter++ * sample_increment);
        // http://blog.bjornroche.com/2009/12/int-float-int-its-jungle-out-there.html
        *ptr++ = (0x7fff * sample);
        *ptr++ = (0x7fff * sample);
    }
    return frames * frame_size;
}

void PlayLoopIterate()
{
    ALint buffers_processed = 0;
    ALuint buffer_handles[5];
    AL_CALL(alGetSourcei(g_source, AL_BUFFERS_PROCESSED, &buffers_processed));
    AL_CALL(alSourceUnqueueBuffers(g_source, buffers_processed, buffer_handles));

    std::vector<char> pcm;
    pcm.resize(2048);

    for (int i=0; i<buffers_processed; ++i)
    {
        const auto buffer_handle = buffer_handles[i];
        const auto pcm_bytes = GenerateSine(&pcm[0], pcm.size());
        AL_CALL(alBufferData(buffer_handle, AL_FORMAT_STEREO16, &pcm[0], pcm_bytes, 44100));
        AL_CALL(alSourceQueueBuffers(g_source, 1, &buffer_handle));
    }
}

#if defined(__EMSCRIPTEN__)
EM_BOOL OnAnimationFrame(double time, void* user)
{
    ALenum handle_state = 0;
    AL_CALL(alGetSourcei(g_source, AL_SOURCE_STATE, &handle_state));
    if (handle_state != g_state)
    {
        std::printf("State changed: %s\n", ToString(handle_state));
        g_state = handle_state;
        AL_CALL(alSourcePlay(g_source));
    }

    PlayLoopIterate();
    return EM_TRUE;
}
void OnAsyncCall(void*)
{
    OnAnimationFrame(0.0, nullptr);
}

#endif


int main()
{
    const auto* default_device_name = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
    std::printf("Using OpenAL device: '%s'\n", default_device_name);
    g_device = alcOpenDevice(default_device_name);
    if (!g_device)
        throw std::runtime_error("failed to open OpenAL audio device.");
    g_context = alcCreateContext(g_device, NULL);
    if (!g_context)
        throw std::runtime_error("failed to create OpenAL audio context.");

    AL_CALL(alcMakeContextCurrent(g_context));

    AL_CALL(alGenSources(1, &g_source));
    AL_CALL(alGenBuffers(5, &g_buffers[0]));
    std::printf("Source handle: %d, buffers = [%d, %d, %d, %d, %d]\n",
                g_source, g_buffers[0], g_buffers[1], g_buffers[2], g_buffers[3], g_buffers[4]);

    std::vector<char> pcm;
    pcm.resize(20480);

    // generate initial payloads.
    for (int i=0; i<5; ++i)
    {
        const auto buffer_handle = g_buffers[i];
        const auto pcm_bytes = GenerateSine(&pcm[0], pcm.size());
        AL_CALL(alBufferData(buffer_handle, AL_FORMAT_STEREO16, &pcm[0], pcm_bytes, 44100));
        AL_CALL(alSourceQueueBuffers(g_source, 1, &buffer_handle));
    }
    AL_CALL(alSourcePlay(g_source));
    AL_CALL(alGetSourcei(g_source, AL_SOURCE_STATE, &g_state));
    std::printf("Source state: '%s'\n", ToString(g_state));

#if defined(__EMSCRIPTEN__)
    emscripten_request_animation_frame_loop(OnAnimationFrame, nullptr);
    //emscripten_async_call(OnAsyncCall, nullptr, 1);
#else
    while (g_state == AL_PLAYING)
    {
        PlayLoopIterate();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        AL_CALL(alGetSourcei(g_source, AL_SOURCE_STATE, &g_state));
    }
    return 0;
#endif
}
