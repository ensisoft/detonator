#include <stddef.h>
#include <stdint.h>

/* Set to 1 if the compile is GNU GCC. */
#if defined(__GNUC__) && !defined(__clang__)
#  define __GCC__
#  define COMPILER_IS_GCC 1
# else
#  define COMPILER_IS_GCC 0
#endif

/* Target processor clips on negative float to int conversion. */
#define CPU_CLIPS_NEGATIVE 0

/* Target processor clips on positive float to int conversion. */
#define CPU_CLIPS_POSITIVE 0

/* Target processor is big endian. */
#define CPU_IS_BIG_ENDIAN 0

/* Target processor is little endian. */
#define CPU_IS_LITTLE_ENDIAN 1

#if defined(_WIN32)
#  define HAVE_DECL_S_IRGRP 0
#  define HAVE_ENDIAN_H 0
#  define HAVE_BYTESWAP 0
#  define HAVE_FSTAT64 0 // actually has __fstat64
#  define HAVE_FSYNC 0
#  define HAVE_GETTIMEOFDAY 0
#  define HAVE_GMTIME 0 // it is there but is it posix ??
#  define HAVE_GMTIME_R 0 // gmtime_s exists
#  define HAVE_INTTYPES_H 0
#  define HAVE_IO_H 1 // io.h
#  define HAVE_SSIZE_T 0 // not standard
#  define HAVE_SYS_TIME_H 0
#  define HAVE_SYS_TYPES_H 1
#  define HAVE_UNISTD_H 0
#else
#  define HAVE_BYTESWAP_H 1
#  define HAVE_DECL_S_IRGRP 1
#  define HAVE_ENDIAN_H 1
#  define HAVE_FSTAT64 1
#  define HAVE_FSYNC 1
#  define HAVE_GETTIMEOFDAY 1
#  define HAVE_GMTIME 1
#  define HAVE_GMTIME_R 1
#  define HAVE_INTTYPES_H 1
#  define HAVE_IO_H 0
#  define HAVE_SSIZE_T 1
#  define HAVE_SYS_TIME_H 1
#  define HAVE_SYS_TYPES_H 1
#  define HAVE_UNISTD_H 1
#endif

// todo: double check  ??
/* Define to 1 if you have the <immintrin.h> header file. */
#define HAVE_IMMINTRIN_H 1


/* Set to 1 if compiling for Win32 */
#if defined(_WIN32)
#  define OS_IS_WIN32 1
#  define USE_WINDOWS_API 1
#  define WIN32_TARGET_DLL 0 /* Set to 1 if windows DLL is being built. */
#else
#  define OS_IS_WIN32 0
#  define USE_WINDOWS_API 0
#endif

/* Set to maximum allowed value of sf_count_t type. */
#define SF_COUNT_MAX 0x7FFFFFFFFFFFFFFFLL

/* The size of `int64_t', as computed by sizeof. */
#define SIZEOF_INT64_T 8

/* The size of `long', as computed by sizeof. */
#if defined(__EMSCRIPTEN__) || defined(_MSC_VER)
#  define SIZEOF_LONG 4
#else
#  define SIZEOF_LONG 8
#endif

/* The size of `long long', as computed by sizeof. */
#define SIZEOF_LONG_LONG 8

/* Set to sizeof (long) if unknown. */
#define SIZEOF_SF_COUNT_T 8

/* The size of `wchar_t', as computed by sizeof. */
#if defined(_MSC_VER)
#  define SIZEOF_WCHAR_T 2
#else
#  define SIZEOF_WCHAR_T 4
#endif






