
// build time configuration options for the audio test app

#include "base/platform.h"

#define AUDIO_ENABLE_TEST_SOUND
#if defined(LINUX_OS)
#  define AUDIO_USE_PULSEAUDIO
#else
#  define AUDIO_USE_WAVEOUT
#endif

#define BASE_LOGGING_ENABLE_LOG