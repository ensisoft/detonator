
#pragma once

#include "base/platform.h"

#define AUDIO_USE_OPENAL
// disable player threading, it doesn't seem to work right.
//#define AUDIO_USE_PLAYER_THREAD
#define BASE_LOGGING_ENABLE_LOG
#define BASE_FORMAT_SUPPORT_GLM
#define BASE_FORMAT_SUPPORT_MAGIC_ENUM
#define BASE_TRACING_ENABLE_TRACING
#define BASE_TEST_HELP_SUPPORT_GLM
#define GAMESTUDIO_ENABLE_PHYSICS_DEBUG
#define GAMESTUDIO_ENABLE_AUDIO

// keep these turned off on purpose for now
//#define GRAPHICS_CHECK_OPENGL
//#define AUDIO_CHECK_OPENAL
