/*
 * Copyright (c) 2006, 2007 Christoph Schwering <schwering@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This header defines a small set of constants used by several files.
 * This does not mean that all constant should be defined here. Constants 
 * that belong to just one file should still be defined in the respective 
 * file.
 * Here are also some Windows-specific definitions: currently, these include
 * the additional O_BINARY flag in the OPEN_FLAGS definition and the 
 * definitions of SEEK_SET etc.
 */

#ifndef __CONSTANTS_H__
#define __CONSTANTS_H__

typedef int db_int_t;
#define DB_INT_FMT		"%d"
#define DB_INT_FMT_NICE		DB_INT_FMT

typedef unsigned int db_uint_t;
#define DB_UINT_FMT		"%u"
#define DB_UINT_FMT_NICE	DB_UINT_FMT

typedef long db_long_t;
#define DB_LONG_FMT		"%ld"
#define DB_LONG_FMT_NICE	DB_LONG_FMT

typedef unsigned long db_ulong_t;
#define DB_ULONG_FMT		"%lu"
#define DB_ULONG_FMT_NICE	DB_ULONG_FMT

typedef float db_float_t;
#define DB_FLOAT_FMT		"%f"
#define DB_FLOAT_FMT_NICE	"%.2f"

typedef double db_double_t;
#define DB_DOUBLE_FMT		"%lf"
#define DB_DOUBLE_FMT_NICE	"%.2f"

enum domain {
	INT,
	UINT,
	LONG,
	ULONG,
	FLOAT,
	DOUBLE,
	STRING,
	BYTES
};

enum operator {
	LT,
	LEQ,
	GT,
	GEQ,
	EQ,
	NEQ,
	AND,
	OR,
	NAND,
	NOR
};

#include "parser.h"
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>

#if defined(O_NOATIME) && defined(O_DIRECT)
	#define OPEN_RD_FLAGS	(O_RDWR | O_DIRECT | O_NOATIME)
#elif !defined(O_NOATIME) && defined(O_DIRECT)
	#define OPEN_RD_FLAGS	(O_RDWR | O_DIRECT)
#elif defined(O_NOATIME) && !defined(O_DIRECT)
	#define OPEN_RD_FLAGS	(O_RDWR | O_NOATIME)
#else 
	#define OPEN_RD_FLAGS	(O_RDWR)
#endif

#if defined(O_NOATIME) && defined(O_DIRECT)
	#define OPEN_RW_FLAGS	(O_RDWR | O_DIRECT | O_NOATIME)
#elif !defined(O_NOATIME) && defined(O_DIRECT)
	#define OPEN_RW_FLAGS	(O_RDWR | O_DIRECT)
#elif defined(O_NOATIME) && !defined(O_DIRECT)
	#define OPEN_RW_FLAGS	(O_RDWR | O_NOATIME)
#else 
	#define OPEN_RW_FLAGS	(O_RDWR)
#endif

#ifdef _WIN32
	#define CREATE_FLAGS	(OPEN_RW_FLAGS | O_CREAT | O_TRUNC | O_EXCL\
					| O_BINARY)
#else
	#define CREATE_FLAGS	(OPEN_RW_FLAGS | O_CREAT | O_TRUNC | O_EXCL)
#endif

#define FILE_MODE		(S_IRUSR | S_IWUSR)

#ifndef PATH_MAX
#define PATH_MAX	4096
#endif


#define DB_BASEDIR	"./data/"
#define DB_SUFFIX	".db"

#define VW_BASEDIR	DB_BASEDIR
#define VW_SUFFIX	".vw"

#define IX_BASEDIR	DB_BASEDIR
#define IX_DELIM	"."
#define IX_SUFFIX	".ix"

#define SP_BASEDIR	DB_BASEDIR
#define SP_SUFFIX	".sp"


/* I have no clue why, but cygwin library does not define them */
#ifdef _WIN32
	#ifndef SEEK_SET
	#define SEEK_SET	0
	#endif

	#ifndef SEEK_CUR
	#define SEEK_CUR	1
	#endif

	#ifndef SEEK_END
	#define SEEK_END	2
	#endif
#endif

#endif

