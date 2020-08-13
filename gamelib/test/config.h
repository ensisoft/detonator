
#pragma once

// build time configuration file for the graphics test app

#include "base/platform.h"

// enable logging
#define BASE_LOGGING_ENABLE_LOG
#ifdef POSIX_OS
#  define BASE_LOGGING_ENABLE_CURSES
#endif

#define MATH_FORCE_DETERMINISTIC_RANDOM