
#pragma once

#include "base/platform.h"

#define BASE_LOGGING_ENABLE_LOG
#ifdef POSIX_OS
#  define BASE_LOGGING_ENABLE_CURSES
#endif

// see interface.h
#define GAMESTUDIO_IMPORT_APP_LIBRARY