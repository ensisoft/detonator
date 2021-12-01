/*
** Copyright (C) 2005-2017 Erik de Castro Lopo <erikd@mega-nerd.com>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published by
** the Free Software Foundation; either version 2.1 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

/*
**	Autoconf leaves many config parameters undefined.
**	Here we change then from being undefined to defining them to 0.
**	This allows things like:
**
**		#if HAVE_CONFIG_PARAM
**
**	and
**
**		if (HAVE_CONFIG_PARAM)
**			do_something () ;
*/

#ifndef SFCONFIG_H
#define SFCONFIG_H

/* Include the Autoconf generated file. */
#include "config.h"

#if (defined __x86_64__) || (defined _M_X64)
#  define CPU_IS_X86_64	1	/* Define both for x86_64 */
#  define CPU_IS_X86	1
#elif defined (__i486__) || defined (__i586__) || defined (__i686__) || defined (_M_IX86)
#  define CPU_IS_X86 	1
#  define CPU_IS_X86_64 0
#elif defined(__EMSCRIPTEN__)
#  define CPU_IS_X86_64	1	/* Define both for x86_64 */
#  define CPU_IS_X86	1
#else
#  define CPU_IS_X86	0
#  define CPU_IS_X86_64	0
#endif

#if (defined (__SSE2__) || defined (_M_AMD64) || (defined (_M_IX86_FP) && (_M_IX86_FP >= 2)) && HAVE_IMMINTRIN_H)
#  define USE_SSE2
#endif

#if (HAVE_SSIZE_T == 0)
#  define ssize_t intptr_t
#endif


/* Set to 1 if flac, ogg and vorbis are available. */
#define HAVE_EXTERNAL_XIPH_LIBS 1

/* Set to 1 to enable experimental code. */
#define ENABLE_EXPERIMENTAL_CODE 0

/* Name of package */
#define PACKAGE "libsndfile"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "libsndfile"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "libsndfile 1.0.31"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "libsndfile"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.0.31"

/* Version number of package */
#define VERSION "1.0.31"

#endif // SFCONFIG_H
