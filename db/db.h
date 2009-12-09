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
 * Interface for the commonly used dingsbums functions.
 * Regarding the iterating functions db_next_buf(), db_next() and db_iterate():
 * Each invokation of any of these functions changes internal data to which
 * the returned structures point. This means that you may not access a 
 * returned tuple after invoking any iterate-function another time.
 */

#ifndef __DB_H__
#define __DB_H__

#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
namespace dingsbums {
extern "C" {
#endif

#define DB_NAME		"dingsbums"
#define DB_VERSION	"0.0"

enum db_domain {
	DB_INT,
	DB_UINT,
	DB_LONG,
	DB_ULONG,
	DB_FLOAT,
	DB_DOUBLE,
	DB_STRING,
	DB_BYTES
};

typedef struct {
	void *ptr;			/* struct stmt_result */
} DB_RESULT;

typedef struct {
	DB_RESULT result;
	void *rl;			/* struct xrel */
	void *iter;			/* struct xrel_iter */
	struct db_val *val_buf;
} DB_ITERATOR;

struct db_val {
	const char	*relation;	/* onwing relation's name */
	const char	*name;		/* attribute's name */
	enum db_domain	domain;		/* STRING, BYTES, INT ... */
	size_t		size;		/* (maximum) size in bytes */
	union {
		const char	*pstring;
		const char	*pbytes;
		int		vint;
		unsigned int	vuint;
		long		vlong;
		unsigned long	vulong;
		float		vfloat;
		double		vdouble;
	} val;
};


/* Executes a statement which might be either a data definition statement,
 * a data manipulation statement, an aggregate function or a query. */
DB_RESULT db_parse(const char *stmt);

/* Executes a statement which might be either a data definition statement,
 * a data manipulation statement, an aggregate function or a query.
 * In contrast to db_parse(), db_parsef() supports formats similar to the
 * standard printf() function. The known directives are:
 * 	`%s'	a normal string
 * 	`%e'	a string which is escaped before copying it into the statement
 * 	`%E'	a string which is escaped and then surrounded by single quotes
 * 		before copying it into the statement
 *	`%S'	same like `%E', i.e. the string is escaped and quoted
 * 	`%d'	an integer
 * 	`%f'	a float
 * Note that `%e' is superior to `%s' in the usual case that it points to a
 * STRING attribute value, because it prevents injections.
 * An additional difference to db_parse() is that the created query is 
 * terminated with a trailing semicolon (;) if this is not present after the
 * interpretation of `fmt'. */
DB_RESULT db_parsef(const char *fmt, ...);

/* Frees the memory allocated by a statement's result. 
 * Do not invoke this function as long as any result of a db_parse() or 
 * db_parsef() function call is in use.
 * Before calling db_free_result() to free a result `r', call 
 * db_free_iterator() to free all iterators affiliated with `r'! */
void db_free_result(DB_RESULT result);

/* Indicates whether the statement was executed successfully. */
bool db_success(DB_RESULT result);

/* Determines the type of the statement: DDL_STMT, DML_MODI, DML_AGGFN or
 * DML_QUERY. */
int db_type(DB_RESULT result);

/* Indicates whether the statement is a data definition statement (DDL_STMT). */
bool db_is_definition(DB_RESULT result);

/* Indicates whether the statement is a modification statement (DML_MODI). */
bool db_is_modification(DB_RESULT result);

/* Indicates whether the statement is a stored procedure call (DML_SP). */
bool db_is_sp(DB_RESULT result);

/* Indicates whether the statement is a query statement (DML_QUERY). */
bool db_is_query(DB_RESULT result);

/* Prints a database result in human readable format to `stream'. 
 * If an error occured, this is said. If the statement was executed 
 * successfully, the following is done: If it was a data defintion or a 
 * modification statement, nothing is printed. If it was an aggregate function,
 * the calculated value is printed. If the statement was a query, the 
 * resulting relation is printed nicely as a table.
 * Returns the count of tuples in the set (for queries), the count of 
 * affected tuples (for modifications), 1 (for aggregate values), 0
 * (for data definition statements or errors). */
unsigned long db_fprint(FILE *stream, DB_RESULT result);

/* Prints a database result in human readable format to stdout. 
 * If an error occured, this is said. If the statement was executed 
 * successfully, the following is done: If it was a data defintion or a 
 * modification statement, nothing is printed. If it was an aggregate function,
 * the calculated value is printed. If the statement was a query, the 
 * resulting relation is printed nicely as a table.
 * Returns the count of tuples in the set (for queries), the count of 
 * affected tuples (for modifications), 1 (for aggregate values), 0
 * (for data definition statements or errors). */
unsigned long db_print(DB_RESULT result);

/* Returns the count of affected tuples if and only if `result' is the result
 * of a successful modification statement, i.e. a insertion, deletion or update.
 * Otherwise, zero is returned. */
unsigned long db_tpcount(DB_RESULT result);

/* Returns the calculated value if the statement was a stored function. */
struct db_val db_spvalue(DB_RESULT result);

/* Returns the count of attributes if the result specifies a relation (i.e. 
 * is a result of a query and the statement was executed successfully). 
 * Otherwise -1 is returned. */
int db_attrcount(DB_RESULT result);

/* Returns an iterator. */
DB_ITERATOR db_iterator(DB_RESULT result);

/* Frees the memory allocated by an iterator. 
 * Do not invoke this function as long as any result of a db_iterator()
 * function call is in use.
 * Call db_free_iterator() to free an iterator that belongs to a result `r'
 * before calling db_free_result(r)! */
void db_free_iterator(DB_ITERATOR iter);

/* Returns a pointer to a buffer that contains the next read tuple
 * If no more tuple is in the relation, NULL is returned. Note that the
 * buffer must not be freed! But its content changes after each next 
 * iteration step. */
const char *db_next_buf(DB_ITERATOR iter);

/* Returns the next tuple as an array of `db_vals'. The array has db_attrcount()
 * elements.
 * The returned `struct db_val' array must not be free()ed explicitely. It is
 * a buffer stored in the DB_ITERATOR. The buffer is free()ed by
 * db_free_iterator().
 * If no more tuple is in the relation, NULL is returned. */
struct db_val *db_next(DB_ITERATOR iter);

/* Calls `func' to give it information about the relation header.
 *
 * Some words about the callback function `func':
 * The argument `ctx' is the same pointer like the `ctx' given to db_header().
 * `ctx' is inteded to point to some context structure one might want to have.
 * `ctx' can be NULL.
 * `cnt' is the count of attributes in the relation.
 * `vals' is an array with `cnt' elements and each element is a db_val
 * structure that contains the attribute's owning relation, its name, domain,
 * and size. (But, of course, no value is specified.) */
void db_header(DB_RESULT result, void *ctx,
		void (*func)(void *ctx, unsigned short cnt,
			const struct db_val *vals));

/* Iterates over the relation and invokes `func' for each tuple. The 
 * tuple is given to `func' in form of an array of `struct db_val' that 
 * contains the attribute specification and the respective value.
 *
 * Some words about the callback function `func':
 * The argument `ctx' is the same pointer like the `ctx' given to db_iterate().
 * `ctx' is inteded to point to some context structure one might want to have
 * when treating a tuple. `ctx' can be NULL.
 * `cnt' is the count of attributes the tuple has and is obviously for each
 * tuple the same.
 * `vals' is an array with `cnt' elements and each element is a db_val
 * structure that contains the attribute's owning relation, its name, domain,
 * size and the concrete value of the tuple in the respective attribute. */
int db_iterate(DB_RESULT result, void *ctx,
		void (*func)(void *ctx, unsigned short cnt,
			const struct db_val *vals));

/* Closes all opened relations and frees all allocated memory.
 * Do not invoke this function as long as any result of a db_*() function is in
 * use.
 * This does not replace db_free_iterator() or db_free_result()! */
void db_cleanup(void);

#ifdef __cplusplus
}
}
#endif

#endif

