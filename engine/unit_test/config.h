
#pragma once

#include "base/platform.h"

#define BASE_FORMAT_SUPPORT_GLM
#define BASE_FORMAT_SUPPORT_MAGIC_ENUM
#define AUDIO_USE_PLAYER_THREAD

#define SOL_ALL_SAFETIES_ON 1

#if defined(LINUX_OS)
#  define AUDIO_USE_PULSEAUDIO
#elif defined(WINDOWS_OS)
#  define AUDIO_USE_WAVEOUT
#endif