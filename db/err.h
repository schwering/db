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
 * Error codes.
 */

#ifndef __ERR_H__
#define __ERR_H__

#include <stdbool.h>

#define PRINT_STR(v)	printf(#v " = \"%s\"\n", (char *)v)
#define PRINT_CHAR(v)	printf(#v " = %c\n", (char)v)
#define PRINT_INT(v)	printf(#v " = %d\n", (int)v)
#define PRINT_FLOAT(v)	printf(#v " = %f\n", (float)v)
#define PRINT_BOOL(v)	printf(#v " = %s\n", ((bool)v) ? "true" : "false")
#define PRINT_LONG(v)	printf(#v " = %ld\n", (long)v)
#define PRINT_SIZE(v)	printf("sizeof(" #v ") = %d\n",\
			(unsigned int)sizeof(v))
#define PRINT_VSIZE(v)	printf("sizeof(" #v ") = %d\n",\
			(unsigned int)(sizeof v))
#define OUT()		printf("%s:%d %s()\n", __FILE__, __LINE__, __FUNCTION__)

enum {
	E_NULL_POINTER,
	E_ADDR_OUT_OF_RANGE,
	E_OPEN_FAILED,
	E_WRITE_FAILED,
	E_READ_FAILED,
	E_TUPLE_DELETED,
	E_UPDATE_NEXT_ADDR_FAILED,
	E_UPDATE_PREV_ADDR_FAILED,
	E_HEADER_REBUILD_FAILED,
	E_TUPLE_ACTIVE,
	E_TUPLE_AVAILABLE,
	E_INDEX_INSERT_FAILED,
	E_INDEX_DELETE_FAILED,
	E_INDEX_INCONSISTENT,
	E_PRIMARY_KEY_CONFLICT,
	E_FOREIGN_KEY_CONFLICT,
	E_COULD_NOT_OPEN_VIEW,
	E_FGNKEY_DELETE_FAILED,
	E_FGNKEY_UPDATE_FAILED,
	E_TUPLE_INSERT_FAILED,
	E_TUPLE_DELETE_FAILED,
	E_TUPLE_UPDATE_FAILED,

	E_CREATE_RELATION_FAILED,
	E_CREATE_INDEX_FAILED,
	E_OPEN_RELATION_FAILED,
	E_OPEN_INDEX_FAILED,
	E_ATTRIBUTE_NOT_FOUND,
	E_UNLINK_RELATION_FAILED,
	E_UNLINK_INDEX_FAILED,
	E_ATTRIBUTE_NOT_INITIALIZED,
	E_DIFFERENT_TYPES,
	E_EXPR_INIT_FAILED,
	E_COULD_NOT_CREATE_VIEW,
	E_COULD_NOT_DROP_VIEW,
	E_IO_ERROR,

	E_SEMANTIC_ERROR,

	E_SYNTAX_ERROR,

	E_SP_ERROR,
	E_SP_PARSING_FAILED,
	E_SP_GENERATING_FAILED,
	E_SP_VAR_NOT_FOUND,
	E_SP_FUNC_NOT_FOUND,
	E_SP_WRITE_START_FAILED,
	E_SP_WRITE_CELL_FAILED,
	E_SP_READ_CELL_FAILED,
	E_SP_READ_START_FAILED,
	E_SP_INVALID_HEADER,
	E_SP_INVALID_ARGC,
	E_SP_INVALID_ARG,
	E_SP_UNEXPECTED_CELL,
	E_SP_DECL_FAILED,
	E_SP_INVALID_VAR_ID,
	E_SP_INVALID_EXPR,
	E_SP_INVALID_EXPR_TYPE,
	E_SP_INVALID_VAR_TYPE,
	E_SP_LIST_ERROR,
	E_SP_RETURN_ERROR,
	E_SP_QUERY_FAILED,
	E_SP_INVALID_RETURN_POINTER,
	E_SP_TOO_MANY_RETURN_POINTERS,
	E_SP_VAR_NOT_INITIALIZED
};


/* Pushes an error to the error stack. */
#define ERR(no)		errset(no, #no, __FILE__, __LINE__, __FUNCTION__)
void errset(unsigned int no, const char *name, const char *file, int line,
		const char *fname);

/* Clears the last error. */
void errclear(void);

/* Clears all errors. */
void errclearall(void);

/* Returns the error number of the last error. */
int errnumber(unsigned int i);

/* Prints error information. The level of detail depends on whether NDEBUG
 * is defined or not. If it is not defined, a stack trace is printed. 
 * Otherwise, just the last stored error number is given. */
void errprint(void);

#endif

