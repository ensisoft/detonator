
#pragma once

#include "base/platform.h"

#define AUDIO_USE_OPENAL

// When this flag is defined the OpenAL API calls are checked for errors.
// Currently turned off for performance reasons. Might cause unexpected
// issues and errors that are not discovered properly.
//
//#define AUDIO_CHECK_OPENAL

// When this flag is defined the audio player in audio/player.h uses a
// separate std::thread to run the platform specific audio device and
// to manage the audio playback.
//
// #define AUDIO_USE_PLAYER_THREAD


// When AUDIO_USE_PLAYER_THREAD is defined  use this flag to control
// the type of locking around the audio queue inside the audio/player
// When the flag is defined the audio queue is a lock free queue
// otherwise a standard queue with std mutex is used.
#define AUDIO_USE_LOCK_FREE_QUEUE

#define BASE_LOGGING_ENABLE_LOG
#define BASE_FORMAT_SUPPORT_GLM
#define BASE_FORMAT_SUPPORT_MAGIC_ENUM
#define BASE_TRACING_ENABLE_TRACING
#define BASE_TEST_HELP_SUPPORT_GLM
#define GAMESTUDIO_ENABLE_PHYSICS_DEBUG
#define GAMESTUDIO_ENABLE_AUDIO

// keep these turned off on purpose for now
//#define GRAPHICS_CHECK_OPENGL

#define SOL_ALL_SAFETIES_ON 1