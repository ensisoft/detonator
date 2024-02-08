

// g++ -o sdl-test sdl.cpp -I/usr/include -lSDL2

#include <SDL2/SDL.h>

#include <thread>

#include <cstring>

Uint32 wavLength;
Uint8 *wavBuffer;

Uint32 wavPos = 0;

SDL_AudioDeviceID deviceId;

void retarded_sdl_callback(void* user, Uint8* stream, int len)
{
    printf("callback for %d bytes\n", len);

    const auto bytes_to_copy = std::min(wavLength - wavPos, (Uint32)len);

    std::memcpy(stream, wavBuffer + wavPos, bytes_to_copy);

    wavPos += bytes_to_copy;

    if (wavPos == wavLength)
        SDL_PauseAudioDevice(deviceId, 1);
}

int main()
{
    // Initialize SDL
    if(SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        printf("SDL could not be initialized!\n"
               "SDL_Error: %s\n", SDL_GetError());
        return 0;
    }


    const char* const TestFile = "data/Juhani Junkala [Retro Game Music Pack] Title Screen.wav";

    // Load .WAV sound
    SDL_AudioSpec wavSpec;


    if(!SDL_LoadWAV(TestFile, &wavSpec, &wavBuffer, &wavLength))
    {
        printf(".WAV sound '%s' could not be loaded!\n"
               "SDL_Error: %s\n", TestFile, SDL_GetError());
        return 0;
    }

    // Open audio device
    wavSpec.callback = retarded_sdl_callback;

    deviceId = SDL_OpenAudioDevice(NULL, 0, &wavSpec, NULL, 0);
    if(!deviceId)
    {
        printf("Audio device could not be opened!\n"
               "SDL_Error: %s\n", SDL_GetError());
        SDL_FreeWAV(wavBuffer);
        return 0;
    }

    //SDL_QueueAudio(deviceId, wavBuffer, wavLength);

    SDL_PauseAudioDevice(deviceId, 0);

    while (SDL_GetAudioDeviceStatus(deviceId) == SDL_AUDIO_PLAYING)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return 0;
}