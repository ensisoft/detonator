
// build time configuration options for the audio test app

#include "base/platform.h"

#define AUDIO_LOCK_FREE_QUEUE
#define AUDIO_ENABLE_TEST_SOUND
#define AUDIO_USE_PLAYER_THREAD
#if defined(LINUX_OS)
#  define AUDIO_USE_PULSEAUDIO
#else
#  define AUDIO_USE_WAVEOUT
#endif

#define BASE_LOGGING_ENABLE_LOG
#define BASE_FORMAT_SUPPORT_MAGIC_ENUM
