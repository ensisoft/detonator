
#pragma once

// config.h for the helper libs combine several translation
// units together into a static helper libs in order to reduce
// the number of translation unit compilations.
// however using these libs means that the build flags cannot
// be changed. If the build flags need to differ from the build
// flags set here then the target must build the translation
// units.

#include "base/platform.h"

// some of the code uses LINUX_OS
// we expect that POSIX_OS is Linux
#ifdef POSIX_OS
#  define LINUX_OS
#endif

#define BASE_LOGGING_ENABLE_LOG

#define GAMESTUDIO_ENABLE_PHYSICS_DEBUG